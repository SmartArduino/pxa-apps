#!/usr/bin/env python3
"""Reproduce compact source PNGs; package compilation expands them to INDEX8."""
import binascii
import json
import struct
import zlib
from pathlib import Path

root = Path(__file__).resolve().parent
out = root / 'resources'
out.mkdir(exist_ok=True)
palette = [(i * 251) & 65535 for i in range(256)]
(out / 'palette.json').write_text(json.dumps(palette) + '\n')

def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', binascii.crc32(kind + data) & 0xffffffff)

def rgb(c):
    return bytes((((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31))

assets = []
for i in range(20):
    rows = b''.join(b'\0' + b''.join(rgb(palette[1 + (i + (x // 32 + y // 32) % 2 * 20) % 255]) for x in range(256)) for y in range(256))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 256, 256, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')
    (out / f't{i:02}.png').write_bytes(png)
    assets.append(dict(kind='texture', source=f'resources/t{i:02}.png', palette='resources/palette.json', path=f'assets/t{i:02}.pxr'))
assets.append(dict(kind='palette', source='resources/palette.json', path='assets/p.pxr'))
(root / 'resources.json').write_text(json.dumps(dict(assets=assets), indent=2) + '\n')

# Guest READ example: raw data stays outside AOT and is block-authenticated.
(root / "assets").mkdir(exist_ok=True)
(root / "assets/map.bin").write_bytes(bytes(i % 251 for i in range(8193)))

(root / "assets/click.pcm").write_bytes(bytes([160])*160)
