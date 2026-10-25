#!/usr/bin/env python3
"""Embed gpSP's GPL open BIOS without target-specific incbin assembly."""
from pathlib import Path
import sys

data = Path(sys.argv[1]).read_bytes()
if len(data) != 16384:
    raise SystemExit("Open GBA BIOS must contain exactly 16384 bytes")
lines = ["/* Generated from the GPL open BIOS; sources: vendor/gpsp/bios/source. */",
         "unsigned char open_gba_bios_rom[16384] = {"]
lines += ["    " + ",".join(f"0x{x:02x}" for x in data[i:i+16]) + ","
          for i in range(0, len(data), 16)]
Path(sys.argv[2]).write_text("\n".join(lines + ["};", ""]))
