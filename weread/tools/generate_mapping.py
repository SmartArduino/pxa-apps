#!/usr/bin/env python3
"""Derive character codes, not font glyphs, from the pinned public site bundle.

No network, account state or JavaScript execution. The inverse permits the
system's existing Chinese font to display server-supplied authorized content.
"""
import argparse
from decimal import Decimal
import hashlib
from pathlib import Path
import re
import struct

SHA256 = '7c055cd6a1aed4771b3aa06b811630b03db1de41ea04da6a383540f64b087593'
SOURCE = 'https://cdn.weread.qq.com/web/wrwebsimplenjlogic/server/2018e7e.js'


def generate(source: bytes) -> bytes:
    if hashlib.sha256(source).hexdigest() != SHA256:
        raise ValueError('Site bundle changed; review its character mapping before updating the pin')
    text = source.decode('utf-8')
    end = text.index('},n={};for(var o in r)')
    start = text.rfind('var r={', 0, end) + len('var r={')
    pairs = text[start:end]
    if not re.fullmatch(r'\d+(?:e\d+)?:"\d+"(?:,\d+(?:e\d+)?:"\d+")*', pairs):
        raise ValueError('Unexpected mapping syntax')
    mapping = {int(Decimal(k)): int(v) for k, v in re.findall(r'(\d+(?:e\d+)?):"(\d+)"', pairs)}
    codes = set(range(0x4e00, 0xa000))
    if not set(mapping).issubset(codes):
        raise ValueError('Unexpected code point')
    mapping = {code: mapping.get(code, code) for code in codes}
    if set(mapping.values()) != codes:
        raise ValueError('Expected a complete bijection of CJK unified ideographs')
    inverse = [0] * len(codes)
    for original, encoded in mapping.items():
        inverse[encoded - 0x4e00] = original
    return struct.pack('<20992H', *inverse)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help=SOURCE)
    parser.add_argument('--output', type=Path, default=Path(__file__).parents[1]/'assets/weread-cjk.bin')
    args = parser.parse_args()
    output = generate(args.source.read_bytes())
    args.output.write_bytes(output)
    print(f'{len(output)} B; SHA256 {hashlib.sha256(output).hexdigest()}')
