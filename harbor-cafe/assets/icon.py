#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Draw the original Harbor Cafe icon using only Python's standard library."""
from pathlib import Path
import struct
import zlib
N=64
NAVY=(30,53,67);CREAM=(255,241,205);SEA=(63,148,169);SKY=(179,221,220);WOOD=(154,94,68);GOLD=(244,196,91)
pixels=[[NAVY for _ in range(N)] for _ in range(N)]
def rect(x,y,w,h,c):
    for yy in range(max(0,y),min(N,y+h)):
        for xx in range(max(0,x),min(N,x+w)):
            pixels[yy][xx]=c
rect(3,3,58,58,CREAM)
rect(6,6,52,25,SKY);rect(6,21,52,12,SEA)
for x in range(9,57,12):
    rect(x,25,7,1,CREAM);rect(x+3,29,6,1,CREAM)
rect(44,13,3,10,WOOD)
for y in range(9):
    rect(47,13+y,1+y//2,1,CREAM)
rect(39,23,16,2,NAVY);rect(42,25,10,1,NAVY)
rect(10,37,43,16,WOOD);rect(8,35,47,4,GOLD)
rect(16,31,27,5,NAVY);rect(17,19,21,13,NAVY);rect(19,21,17,9,CREAM)
rect(38,22,7,8,NAVY);rect(39,24,4,4,CREAM)
rect(20,23,15,5,WOOD);rect(20,29,15,2,CREAM)
rect(22,10,2,6,CREAM);rect(24,8,2,3,CREAM);rect(30,8,2,7,CREAM)
rect(14,41,33,2,CREAM);rect(14,47,6,7,NAVY);rect(41,47,6,7,NAVY)
rect(5,55,54,3,SEA)
raw=b"".join(b"\0"+bytes(v for c in row for _ in range(2) for v in c) for row in pixels for _ in range(2))
def chunk(kind,data):
    return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data)&0xffffffff)
png=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",128,128,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(raw,9))+chunk(b"IEND",b"")
Path(__file__).with_name("icon.png").write_bytes(png)
