#!/usr/bin/env python3
"""Build pinned C&C demo media, removing music before signing for the Store.

SPDX-License-Identifier: GPL-3.0-or-later
MIX header layout and public-key decoding reference:
OpenRA.Mods.Cnc/FileSystem/MixFile.cs and FileFormats/BlowfishKeyProvider.cs,
Copyright The OpenRA Developers and Contributors, GPL-3.0-or-later.
"""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

POLICY = 'https://www.ea.com/games/command-and-conquer/news/modding-faq'
DISCLAIMER = 'EA has not endorsed and does not support this product.'


def name_hash(name):
    data = name.upper().encode('ascii')
    value = 0
    for i in range(0, len(data), 4):
        value = (((value << 1) | (value >> 31)) +
                 int.from_bytes(data[i:i + 4].ljust(4, b'\0'), 'little')) & 0xffffffff
    return value


def read_mix(data):
    if len(data) < 6:
        raise ValueError('Truncated MIX header')
    modern = struct.unpack_from('<H', data)[0] == 0
    flags = struct.unpack_from('<I', data)[0] if modern else 0
    if modern and flags & ~0x30000:
        raise ValueError('Unsupported MIX flags')
    if flags & 0x20000:
        if len(data) < 92:
            raise ValueError('Truncated encrypted MIX header')
        from Crypto.Cipher import Blowfish
        public = base64.b64decode('AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V')
        modulus = int.from_bytes(public[2:], 'big')
        key = b''.join(pow(int.from_bytes(data[4 + i:44 + i], 'little'),
                          65537, modulus).to_bytes(40, 'little')[:39]
                       for i in (0, 40))[:56]
        cipher = Blowfish.new(key, Blowfish.MODE_ECB)
        count = struct.unpack_from('<H', cipher.decrypt(data[84:92]))[0]
        start = 84 + ((6 + count * 12 + 7) // 8) * 8
        if start > len(data):
            raise ValueError('MIX index exceeds file')
        header = cipher.decrypt(data[84:start])
    else:
        offset = 4 if modern else 0
        header = data[offset:]
        if len(header) < 6:
            raise ValueError('Truncated MIX header')
        count = struct.unpack_from('<H', header)[0]
        start = offset + 6 + count * 12
    size = struct.unpack_from('<I', header, 2)[0]
    trailer = 20 if flags & 0x10000 else 0
    if not count or count > 5000 or len(header) < 6 + count * 12 or start + size + trailer != len(data):
        raise ValueError('Invalid MIX index or payload size')
    if trailer and hashlib.sha1(data[start:start + size]).digest() != data[-20:]:
        raise ValueError('Invalid MIX payload checksum')
    entries = {}
    for i in range(count):
        identity, offset, length = struct.unpack_from('<III', header, 6 + i * 12)
        if identity in entries or offset + length > size:
            raise ValueError('Duplicate or out-of-bounds MIX entry')
        entries[identity] = data[start + offset:start + offset + length]
    return modern, entries


def filter_mix(data, plan):
    """Remove exactly reviewed paths; retained leaf bytes never change."""
    modern, entries = read_mix(data)
    for name, children in plan.items():
        identity = name_hash(name)
        if identity not in entries:
            raise ValueError('Reviewed music path missing: ' + name)
        if children is None:
            del entries[identity]
        else:
            entries[identity] = filter_mix(entries[identity], children)
    if not entries:
        raise ValueError('Refusing an empty MIX archive')
    payload = bytearray()
    index = bytearray()
    for identity, content in entries.items():
        index.extend(struct.pack('<III', identity, len(payload), len(content)))
        payload.extend(content)
    return ((b'\0' * 4 if modern else b'') +
            struct.pack('<HI', len(entries), len(payload)) + index + payload)


def prepare(recipe, source, destination):
    files = {}
    for name, expected in recipe['files'].items():
        if Path(name).name != name or '\\' in name:
            raise ValueError('Invalid recipe filename')
        data = (source / name).read_bytes()
        if len(data) != expected['bytes'] or hashlib.sha256(data).hexdigest() != expected['sha256']:
            raise ValueError('Unreviewed input: ' + name)
        # Validate even files that need no transformation or are excluded.
        if name.endswith('.mix'):
            read_mix(data)
        if not expected.get('exclude'):
            files[name] = filter_mix(data, expected['remove']) if expected.get('remove') else data
    destination.mkdir(parents=True, exist_ok=False)
    for name, data in files.items():
        (destination / name).write_bytes(data)
    (destination / 'LICENSE.txt').write_text(
        DISCLAIMER + '\n\n' + recipe['title'] + '\n'
        'Original game data copyright Electronic Arts / Westwood Studios.\n'
        'Non-commercial community port; distributed free of charge.\n'
        'The engine GPL does not license the game data.\n'
        'Game assets are provided under the EA C&C Franchise Modding Guidelines:\n'
        + POLICY + '\n\n'
        'Original music is excluded, including music inside nested MIX archives.\n'
        'Retained leaf assets are unchanged; modified MIX files only rebuild indexes.\n'
        'This package contains demo assets, not the complete retail game.\n'
        'Original demo: ' + recipe['source_url'] + '\n', encoding='utf-8')


def main():
    recipes = json.loads((Path(__file__).resolve().parents[1] / 'resources/cnc-demo-media.json').read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('application', choices=tuple(recipes['applications']))
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--signing-key', type=Path, required=True)
    args = parser.parse_args()
    recipe = recipes['applications'][args.application]
    output = args.output_root.resolve() / args.application
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='payload-', dir=output) as temporary:
        stage = Path(temporary)
        payload = stage / 'package'
        prepare(recipe, args.input, payload)
        metadata = stage / 'package.json'
        metadata.write_text(json.dumps({'format': 'pxa-package-source-0.2', 'package_type': 'resources',
            'id': 'pxa-' + args.application, 'version': '2026.10.23.1', 'release_sequence': 1,
            'name': recipe['title'], 'min_sdk': [2, 0], 'target_sdk': [2, 0]}, indent=2) + '\n')
        subprocess.run([sys.executable, str(args.sdk_root / 'tools/package/build_resource_package.py'),
            str(metadata), str(payload), str(args.signing_key), str(output / 'resources.pxa'),
            '--descriptor', str(output / 'binding.json')], check=True)
    print((output / 'binding.json').read_text())


if __name__ == '__main__':
    main()
