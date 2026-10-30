#!/usr/bin/env python3
"""Extract native ZLabs CN pixels without rasterization or resampling.

The OFL subset is named Starsoil UI 12. Original shapes and metrics are
preserved. KBitX codec follows kbitfont 0.0.4 (MIT, Copyright 2025 TakWolf),
see art/FONT-CODEC-LICENSE.txt; the font license is art/FONT-LICENSE.txt.
"""
from pathlib import Path
import argparse
import base64
import hashlib
import json
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = 'https://github.com/Astro-2539/ZLabs-Pixel-12px'
COMMIT = 'c4d5b7f7d95f04abcb330d012e4b38becb2de99b'


def decode_bitmap(encoded):
    data = iter(base64.b64decode(encoded + '==='))
    def number():
        value, shift = 0, 0
        while True:
            b = next(data)
            value |= (b & 127) << shift
            if b < 128:
                return value
            shift += 7
    height, width = number(), number()
    pixels = []
    while len(pixels) < width * height:
        token = next(data)
        count = (token & 31) << (5 if token & 32 else 0)
        if not count:
            raise ValueError('Invalid KBitX run')
        mode = token & 192
        if mode == 192:
            pixels.extend(next(data) for _ in range(count))
        else:
            color = 0 if mode == 0 else 255 if mode == 64 else next(data)
            pixels.extend([color] * count)
    if len(pixels) != width * height:
        raise ValueError('Invalid KBitX pixel count')
    return width, height, pixels


def collect(source):
    document = ET.parse(source).getroot()
    ascent = int(document.find("prop[@id='emAscent']").get('value'))
    requested = set(range(32, 127))
    for name in ('main.cpp', 'main_menu.inc', 'core.cpp', 'items.hpp', 'runtime.cpp'):
        requested.update(ord(c) for c in (ROOT / name).read_text() if ord(c) >= 128)
    glyphs = {int(g.get('u')): g for g in document.findall('g') if g.get('u')}
    missing = requested - glyphs.keys()
    if missing:
        raise ValueError('Missing source glyphs: ' + ''.join(chr(cp) for cp in sorted(missing)))
    result = []
    for cp in sorted(requested):
        g = glyphs[cp]
        width, height, pixels = decode_bitmap(g.get('d'))
        x_offset, top = int(g.get('x')), ascent - int(g.get('y'))
        advance = int(g.get('w'))
        rows = [0] * 12
        if not 0 < advance <= 12:
            raise ValueError(f'Unexpected advance for U+{cp:04X}: {advance}')
        for y in range(height):
            for x in range(width):
                if pixels[y * width + x] <= 127:
                    continue
                xx, yy = x + x_offset, y + top
                if not 0 <= xx < 12 or not 0 <= yy < 12:
                    raise ValueError(f'Clipped native glyph U+{cp:04X}')
                rows[yy] |= 1 << (11 - xx)
        result.append((cp, advance, rows))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font', type=Path, default=ROOT / 'art/fonts/ZLabsPixel_12px_M_CN.kbitx')
    args = parser.parse_args()
    glyphs = collect(args.font)
    with (ROOT / 'font.hpp').open('w') as out:
        out.write('// SPDX-License-Identifier: OFL-1.1\n')
        out.write('// Starsoil UI 12, subset of ZLabs Pixel 12px CN.\n')
        out.write('// Copyright (c) 2023-2026 Astro_2539. See art/FONT-LICENSE.txt.\n')
        out.write('// Generated from original KBitX pixels by tools/build_font.py.\n')
        out.write('#pragma once\n#include <cstdint>\nnamespace font {\n')
        out.write('inline constexpr int Height=12, CellWidth=12;\n')
        out.write('struct Glyph { uint32_t cp; uint8_t advance; uint16_t rows[12]; };\n')
        out.write('inline constexpr Glyph glyphs[]={\n')
        for cp, advance, rows in glyphs:
            out.write('{'+str(cp)+','+str(advance)+',{'+','.join(map(str,rows))+'}},\n')
        out.write('};\n}\n')
    provenance = {
        'family': 'Starsoil UI 12', 'upstream_family': 'ZLabs Pixel 12px M CN',
        'source_url': UPSTREAM, 'commit': COMMIT,
        'source_file': 'fonts/ZLabsPixel_12px_M_CN.kbitx',
        'sha256': hashlib.sha256(args.font.read_bytes()).hexdigest(),
        'license': 'OFL-1.1', 'license_file': 'FONT-LICENSE.txt',
        'native_height': 12, 'cjk_advance': 12, 'latin_advance': 6,
        'glyph_count': len(glyphs), 'conversion': 'Exact original bitmap pixels; subset only',
    }
    (ROOT / 'art/font-source.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2)+'\n')
    (ROOT / 'assets/font-notice.txt').write_text(
        'Starsoil UI 12 is an embedded subset of ZLabs Pixel 12px M CN.\n'
        'Copyright (c) 2023-2026 Astro_2539 (https://github.com/Astro-2539).\n'
        'Source: '+UPSTREAM+'\nCommit: '+COMMIT+'\n'
        'Licensed under SIL Open Font License 1.1. See font-license.txt.\n'
        'Reserved Font Names remain with their original owner.\n')
    print(f'{len(glyphs)} native bitmap glyphs; 12px baseline; no resampling')


if __name__ == '__main__':
    main()
