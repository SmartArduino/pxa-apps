#!/usr/bin/env python3
"""Exercise the normal C++ Pixel package on pai-touch; verify real saved turns.

Uses the 296x240 / 160 DPI / 8,10,8,10 inset layout. Requires an empty test
catalog, changes only this game's data, and leaves this application stopped.
"""
import argparse
import json
import re
import struct
import sys
import time
import zlib
from pathlib import Path
sys.path.insert(0, str(Path.cwd() / "tools/pxadb"))
import pxadb

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--port", required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
identity = "pxa-pixel-dungeon-cpp"
report = {"production": True, "passed": False, "checks": []}
logs = []; seen = set()
with (args.output / "serial.jsonl").open("w", buffering=1) as stream:
    with pxadb.PxaDbClient(args.port, 30) as client:
        def record(frame):
            key = (frame.kind, frame.payload)
            if key in seen: return
            seen.add(key); logs.append(frame.payload)
            stream.write(json.dumps({"kind": frame.kind, "payload": frame.payload}) + "\n")
        client.log_callback = record
        def request(command):
            stream.write(json.dumps({"command": command}) + "\n")
            return client.request(command, timeout=30)
        def pump(seconds):
            end = time.monotonic() + seconds
            while time.monotonic() < end: client.pump_logs(); time.sleep(.01)
        def tap(x, y):
            request(f"INPUT POINTER DOWN {x} {y} 0"); request("INPUT SYNC"); pump(.08)
            request(f"INPUT POINTER UP {x} {y} 0"); request("INPUT SYNC"); pump(.8)
        def phase(wanted):
            actual = [int(m.group(1)) for line in logs if (m := re.search(r"PDCPP phase=(\d+)", line))]
            assert actual and actual[-1] == wanted, (wanted, actual)
        def screenshot(name):
            capture = pxadb.screenshot_capture(request("SCREENSHOT JPEG"))
            pxadb.write_screenshot(capture, args.output / (name + ".jpg"))
            assert (capture.metadata["width"], capture.metadata["height"]) == ("296", "240")
        def save(name):
            fs = pxadb.NormalFsClient(client)
            private = "pxa-state/data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171~" + identity + "/.pxa-storage"
            snapshots = []
            for kind, size, filename in fs.list(private):
                if kind != "F" or not filename.startswith(".pxa-kv-"): continue
                path = args.output / (name + "-" + filename.removeprefix(".pxa-"))
                fs.get(private + "/" + filename, path); raw = path.read_bytes()
                if len(raw) < 24: continue
                magic, version, _, generation, length, crc = struct.unpack("<4sHHQII", raw[:24]); body = raw[24:]
                if magic != b"PXKV" or version != 1 or len(body) != length or zlib.crc32(body) != crc: continue
                values = {}; at = 2
                for _ in range(struct.unpack_from("<H", body)[0]):
                    n = body[at]; at += 1; key = body[at:at+n]; at += n
                    n = struct.unpack_from("<H", body, at)[0]; at += 2; values[key] = body[at:at+n]; at += n
                assert at == len(body)
                snapshots.append((generation, values))
            assert snapshots
            payload = max(snapshots, key=lambda row: row[0])[1][b"pixel-dungeon.save"]
            return {"seed": struct.unpack_from("<I", payload, 4)[0], "turn": struct.unpack_from("<H", payload, 28)[0]}
        running = False
        try:
            report["hello"] = client.hello()
            assert not any("active=1" in f.payload for f in request("PACKAGES") if f.kind == "PKG")
            client.subscribe_logs(); request("INPUT KEY HOME")
            request("PACKAGE run " + identity); running = True; pump(7)
            if any("Runtime permission prompt queued:" in line and identity in line for line in logs):
                request("INPUT TAP 214 182"); request("INPUT SYNC"); pump(3)
                request("PACKAGE stop " + identity); running = False; pump(1)
                request("PACKAGE run " + identity); running = True; pump(5)
            assert any("PDCPP ready pixels=296,240 dpi_q16=65536 render=296,240" in line for line in logs)
            tap(148,122); phase(5); tap(148,120); phase(6); tap(62,129); phase(1)
            tap(78,213); tap(183,213); phase(2); screenshot("bag")
            tap(248,44); phase(1); tap(262,34); phase(9)
            tap(148,101); phase(8); screenshot("settings"); tap(231,56); phase(9)
            tap(148,137); phase(0); pump(2)
            first = save("title-save"); assert first["turn"] == 1 and first["seed"] != 0x51ed270b
            report["checks"].append({"name": "normal-new-game-bag-settings-title-save", **first})
            request("PACKAGE stop " + identity); running = False; pump(2)
            for repeat in range(3):
                request("PACKAGE run " + identity); running = True; pump(5)
                tap(148,122); phase(5); tap(148,103); phase(1); screenshot(f"loaded-{repeat}")
                tap(78,213)
                request("INPUT KEY HOME"); pump(2)
                saved = save(f"background-{repeat}")
                assert saved == {"seed": first["seed"], "turn": repeat+2}
                report["checks"].append({"name": "relaunch-load-wait-background-save", "repeat": repeat, **saved})
                request("PACKAGE run " + identity); pump(2)
                screenshot(f"foreground-{repeat}")
                request("PACKAGE stop " + identity); running = False; pump(2)
            assert not any("PDCPP save failed" in line or "resource initialization failed" in line for line in logs)
            report["passed"] = True
        finally:
            request("INPUT CANCEL")
            if running: request("PACKAGE stop " + identity)
            (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
assert report["passed"]
