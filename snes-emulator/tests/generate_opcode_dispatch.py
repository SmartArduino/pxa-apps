#!/usr/bin/env python3
"""Derive dispatch IDs and cases from the pinned core's five opcode tables."""
import argparse
from pathlib import Path
import re


def generate(source):
    tables = re.findall(
        r"const SOpcodes (S9xOpcodes\w+)\[256\]\s*=\s*\{(.*?)\};",
        source, re.S)
    expected = {"S9xOpcodesM1X1", "S9xOpcodesM1X0", "S9xOpcodesM0X1",
                "S9xOpcodesM0X0", "S9xOpcodesE1"}
    if len(tables) != 5 or {name for name, _ in tables} != expected:
        raise ValueError("Expected all five 256-entry CPU mode tables")
    handlers = set()
    for name, body in tables:
        entries = re.findall(r"\{SNES_OPCODE_ID_(Op\w+)\}", body)
        if len(entries) != 256 or re.sub(
                r"\{SNES_OPCODE_ID_Op\w+\}|[\s,]", "", body):
            raise ValueError("Invalid opcode table: " + name)
        handlers.update(entries)
    for handler in handlers:
        if not re.search(r"\bvoid\s+" + handler + r"\s*\(void\)", source):
            raise ValueError("Missing instruction handler: " + handler)
    if len(handlers) > 65536:
        raise ValueError("Dispatch IDs do not fit in uint16_t")
    return ("/* Generated from cpuops.c; included with SNES_OPCODE defined. */\n"
            + "".join(f"SNES_OPCODE({name})\n" for name in sorted(handlers)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = generate(args.source.read_text())
    args.output.write_text(result)


if __name__ == "__main__":
    main()
