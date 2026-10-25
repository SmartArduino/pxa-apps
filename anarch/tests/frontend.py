#!/usr/bin/env python3
"""Build and run native Anarch frontend checks from the workspace root.

All executables, logs and rendered interface fixtures go to --output.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    root = Path.cwd()
    app = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    include = root / "deps/pxa-system/sdk/guest-c/include"
    assert include.is_dir(), "Run from the pxa-projects workspace root"
    flags = ["-O1", "-g", "-fwrapv"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    commands = [
        ["cc", "-std=c11", *flags, "-c", str(app / "audio.c"), "-o", str(output / "audio.o")],
        ["c++", "-std=c++17", *flags, "-Wno-attributes", "-Wall", "-Wextra", "-Werror",
         "-I" + str(include), "-I" + str(root / "deps/pxa-system/libpxa/include"),
         str(app / "tests/frontend.cpp"), str(output / "audio.o"), "-o", str(output / "frontend-test")],
        [str(output / "frontend-test"), str(output)],
    ]
    with (output / "frontend-test.log").open("w") as log:
        for command in commands:
            log.write("$ " + " ".join(command) + "\n")
            log.flush()
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        source = (app / "view.hpp").read_text()
        glyphs = (app.parent / "common/pocket-ui/glyphs.hpp").read_text()
        available = {int(cp, 16) for cp in re.findall(r"\{0x([0-9a-f]+)u", glyphs)}
        needed = {c for c in source if "\u4e00" <= c <= "\u9fff"}
        missing = sorted(c for c in needed if ord(c) not in available)
        assert not missing, f"Missing glyphs: {missing}"
        log.write(f"PASS glyph coverage: {len(needed)} Chinese characters\n")
    # Optional image conversion only affects previews, never contract checks.
    try:
        from PIL import Image
    except ImportError:
        pass
    else:
        for ppm in output.glob("*.ppm"):
            Image.open(ppm).save(ppm.with_suffix(".png"))
    print(f"Anarch frontend checks passed; evidence: {output / 'frontend-test.log'}")


if __name__ == "__main__":
    main()
