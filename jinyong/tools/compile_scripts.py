#!/usr/bin/env python3
"""Build Lua 5.1 wasm32 chunks while retaining the original resource bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import sys

ORIGINAL_SHA = "adb7971b6d2314d181b67b2d16b4eb41838b53ab61dc6504220ce198e8aabe0f"
ENTRY = re.compile(r'\{"([^"]+)","assets/jinyong.bundle",(\d+)u,(\d+)u,(\d+)u\}')
CORE = "lapi lcode ldebug ldo lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lauxlib".split()


def write_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == data:
        return
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--cc", required=True)
    args = parser.parse_args()
    if sys.byteorder != "little":
        raise RuntimeError("The wasm32 chunk builder currently requires a little-endian host")
    root, build = args.root.resolve(), args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    lua = root / "engine/lua"
    raw = (root / "assets/jinyong.bundle").read_bytes()
    if hashlib.sha256(raw).hexdigest() != ORIGINAL_SHA:
        raise RuntimeError("Original resource checksum mismatch; run prepare-assets.py")
    index = (root / "engine/jy_asset_index.c").read_text()
    destination = root / "assets/jinyong-scripts.bundle"
    stamp = build / "stamp.json"
    inputs = [Path(__file__).resolve(), root / "tools/lua_compile.c",
              root / "engine/jy_asset_index.c", *sorted(lua.glob("*.[ch]"))]
    digest = hashlib.sha256((ORIGINAL_SHA + args.cc + platform.machine()).encode())
    for source in inputs:
        digest.update(source.read_bytes())
    key = digest.hexdigest()
    if stamp.exists() and destination.exists() and args.metadata.exists():
        cached = json.loads(stamp.read_text())
        if (cached.get("key") == key and
                cached.get("bundle") == hashlib.sha256(destination.read_bytes()).hexdigest() and
                cached.get("metadata") == hashlib.sha256(args.metadata.read_bytes()).hexdigest()):
            return

    # Lua's native dumper otherwise writes an eight-byte size_t on a 64-bit
    # workstation. Change only that serialized field and its header marker;
    # int/instruction/IEEE-double fields already match the wasm32 ABI.
    dumper = (lua / "ldump.c").read_text()
    replacements = {
        "#include <stddef.h>": "#include <stddef.h>\n#include <stdint.h>",
        "size_t size=0;": "uint32_t size=0;",
        "size_t size=s->tsv.len+1;": "uint32_t size=(uint32_t)s->tsv.len+1;",
        "luaU_header(h);": "luaU_header(h);h[8]=4;",
    }
    for before, after in replacements.items():
        if dumper.count(before) != 1:
            raise RuntimeError("Pinned Lua dumper changed: " + before)
        dumper = dumper.replace(before, after)
    write_changed(build / "ldump32.c", dumper.encode())
    compiler = build / "luac51-wasm32"
    subprocess.run([args.cc, "-O2", "-I" + str(lua),
                    str(root / "tools/lua_compile.c"), str(build / "ldump32.c"),
                    *[str(lua / (name + ".c")) for name in CORE],
                    "-lm", "-o", str(compiler)], check=True)

    compiled, chunks = bytearray(), {}
    for match in ENTRY.finditer(index):
        name, size, offset, total = match.groups()
        if not name.endswith(".lua"):
            continue
        size, offset = int(size), int(offset)
        if int(total) != len(raw) or offset + size > len(raw):
            raise RuntimeError("Invalid original script range: " + name)
        source, output = build / "input.lua", build / "output.luac"
        text = raw[offset:offset + size]
        if name == "script/jymain.lua":
            # The title remains in the software screen after the menu commits.
            # Release its cached decoded pixels before allocating a new world.
            # Keep this on the existing source line so debug line numbers match.
            before = b"    if menuReturn == 1 then        --"
            if text.count(before) != 1:
                raise RuntimeError("Pinned start-menu cleanup boundary changed")
            text = text.replace(before,
                b'    lib.LoadPicture("",0,0); if menuReturn == 1 then        --')
            # Attribute dialogs redraw the title. Drop that second cached
            # image before allocating scene buffers after NewGame returns.
            before = b"\t\tNewGame();          --"
            if text.count(before) != 1:
                raise RuntimeError("Pinned new-game cleanup boundary changed")
            text = text.replace(before,
                b'\t\tNewGame(); lib.LoadPicture("",0,0);          --')
            before = (b"        LoadRecord(r);\r\n\t\tCls();\r\n"
                      b"\t\tShowScreen();\r\n\t\tJY.Status=GAME_FIRSTMMAP;")
            if text.count(before) != 1:
                raise RuntimeError("Pinned load-menu cleanup boundary changed")
            text = text.replace(before,
                before.replace(b"LoadRecord(r);",
                    b'lib.LoadPicture("",0,0); LoadRecord(r);', 1))
        source.write_bytes(text)
        subprocess.run([str(compiler), str(source), str(output), "@./" + name], check=True)
        chunk = output.read_bytes()
        if chunk[:12] != bytes.fromhex("1b4c75615100010404040800"):
            raise RuntimeError("Unexpected Lua 5.1 wasm32 chunk ABI: " + name)
        chunks[name] = (len(chunk), len(compiled))
        compiled.extend(chunk)
    if not chunks:
        raise RuntimeError("No original Lua scripts found")

    def replace(match):
        name = match.group(1)
        if name not in chunks:
            return match.group(0)
        size, offset = chunks[name]
        return ('{"' + name + '","assets/jinyong-scripts.bundle",' +
                f"{size}u,{offset}u,{len(compiled)}u}}")

    runtime_index = ("/* Generated Lua bytecode view; original resources remain intact. */\n" +
                     ENTRY.sub(replace, index)).encode()
    write_changed(destination, compiled)
    write_changed(args.metadata, runtime_index)
    write_changed(stamp, (json.dumps({"key": key, "scripts": len(chunks),
        "bundle": hashlib.sha256(compiled).hexdigest(),
        "metadata": hashlib.sha256(runtime_index).hexdigest()}, indent=2) + "\n").encode())
    print(f"Prepared {len(chunks)} Lua 5.1 wasm32 chunks with original debug information")


if __name__ == "__main__":
    main()
