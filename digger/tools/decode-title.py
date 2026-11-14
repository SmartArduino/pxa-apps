#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 PXA Digger adapter contributors
"""Losslessly reproduce the pixel table from the upstream zlib title source."""
import argparse
import re
import zlib
from pathlib import Path


def c_array(path, name):
    text = path.read_text()
    match = re.search(r"\b" + name + r"\s*\[[^]]*\]\s*=\s*\{([^}]+)\}", text, re.S)
    if not match:
        raise ValueError("Missing title array: " + str(path))
    return bytes(int(value, 0) for value in re.findall(r"0x[0-9a-fA-F]+|\d+", match[1]))


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=root / "engine/title_gz.c")
    parser.add_argument("--check", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    pixels = zlib.decompress(c_array(args.source, "title_gz"))
    if len(pixels) != 256000:
        raise ValueError("Unexpected upstream title dimensions")
    if args.check:
        if c_array(args.check, "title_pixels") != pixels:
            raise ValueError("Decoded title data differs")
        print("Lossless upstream title data match")
    if args.output:
        header = ("/* Digger Remastered title. Copyright (c) Andrew Jenner 1998-2004.\n"
                  " * Upstream GNU GPL version 2 applies; lossless PXA decoding. */\n"
                  "#include <string.h>\nstatic const unsigned char title_pixels[256000]={\n")
        rows = [",".join(map(str, pixels[i:i+40])) + ",\n" for i in range(0, len(pixels), 40)]
        footer = "};\nvoid gettitle(unsigned char *buf){memcpy(buf,title_pixels,sizeof title_pixels); }\n"
        args.output.write_text(header + "".join(rows) + footer)
    if not args.check and not args.output:
        parser.error("Choose --check or --output")


if __name__ == "__main__":
    main()
