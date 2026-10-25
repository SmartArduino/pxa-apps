#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Original Pet Garden pixel icon. Python stdlib only; no downloaded artwork.
Run: python3 local/pxa-apps/pet-garden/assets/icon.py
"""
from pathlib import Path
import struct
import zlib

SIZE = 64
pixels = [[(255, 247, 221) for _ in range(SIZE)] for _ in range(SIZE)]
INK = (54, 48, 73)
GREEN = (100, 163, 116)
BODY = (181, 217, 153)
PINK = (244, 143, 165)
GOLD = (243, 196, 98)
CREAM = (255, 247, 221)


def rect(x, y, w, h, color):
    for yy in range(max(0, y), min(SIZE, y + h)):
        for xx in range(max(0, x), min(SIZE, x + w)):
            pixels[yy][xx] = color


def box(x, y, w, h, color):
    rect(x + 2, y, w - 4, h, INK)
    rect(x, y + 2, w, h - 4, INK)
    rect(x + 2, y + 2, w - 4, h - 4, color)


rect(3, 3, 58, 58, (173, 220, 230))
rect(3, 44, 58, 17, GREEN)
for x in range(6, 62, 10):
    rect(x, 40, 4, 12, CREAM)
rect(3, 46, 58, 2, CREAM)
box(15, 14, 12, 24, BODY)
box(37, 12, 12, 24, BODY)
rect(19, 18, 4, 13, PINK)
rect(41, 16, 4, 15, PINK)
box(12, 28, 40, 28, BODY)
rect(16, 51, 32, 3, GREEN)
box(14, 51, 12, 8, BODY)
box(38, 51, 12, 8, BODY)
rect(22, 37, 5, 7, INK)
rect(37, 37, 5, 7, INK)
rect(23, 38, 2, 2, CREAM)
rect(38, 38, 2, 2, CREAM)
rect(17, 44, 7, 3, PINK)
rect(41, 44, 7, 3, PINK)
rect(30, 44, 4, 2, INK)
rect(31, 46, 2, 3, INK)
rect(28, 49, 8, 2, INK)
rect(31, 22, 2, 7, GREEN)
rect(25, 20, 7, 4, GREEN)
rect(33, 17, 7, 4, GREEN)
rect(54, 53, 2, 7, INK)
rect(51, 50, 8, 4, PINK)
rect(53, 48, 4, 8, PINK)
rect(54, 51, 2, 2, GOLD)
# Crisp 2x pixels; canonical PNG is 128x128 RGB.
raw = b"".join(b"\0" + bytes(c for x in row for _ in range(2) for c in x)
               for row in pixels for _ in range(2))


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 128, 128, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
Path(__file__).with_name("icon.png").write_bytes(png)
