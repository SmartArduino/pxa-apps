#!/usr/bin/env python3
"""Prepare the licensed LibreQuake first-level preview for PXA's MCU format 3."""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

from build_demo_resources import relative_path, verify_archive

PAK_HEADER = struct.Struct('<4sII')
PAK_ENTRY = struct.Struct('<56sII')
DEFAULT_CFG = (b'bind "UPARROW" "+forward"\nbind "DOWNARROW" "+back"\n'
               b'bind "LEFTARROW" "+left"\nbind "RIGHTARROW" "+right"\n'
               b'bind "CTRL" "+attack"\nbind "MOUSE1" "+attack"\n'
               b'bind "," "+moveleft"\nbind "." "+moveright"\n'
               b'bind "SPACE" "+jump"\nbind "/" "impulse 10"\n'
               b'bind "e" "+use"\n')


def unpack_pak(data):
    if len(data) < PAK_HEADER.size:
        raise ValueError('Truncated PAK header')
    magic, start, length = PAK_HEADER.unpack_from(data)
    if magic != b'PACK' or start < 12 or length % 64 or start + length > len(data):
        raise ValueError('Invalid PAK directory')
    files, names = {}, set()
    for raw, offset, size in PAK_ENTRY.iter_unpack(data[start:start + length]):
        name = raw.split(b'\0', 1)[0].decode('ascii')
        relative_path(name)
        if name.casefold() in names or offset < 12 or offset + size > start:
            raise ValueError('Invalid or duplicate PAK member: ' + name)
        files[name] = data[offset:offset + size]
        names.add(name.casefold())
    return files


def pack_pak(files):
    output = bytearray(PAK_HEADER.size)
    entries = []
    for name, data in sorted(files.items()):
        relative_path(name)
        encoded = name.encode('ascii')
        if len(encoded) >= 56:
            raise ValueError('PAK name is too long')
        entries.append(PAK_ENTRY.pack(encoded, len(output), len(data)))
        output.extend(data)
    directory = len(output)
    output.extend(b''.join(entries))
    PAK_HEADER.pack_into(output, 0, b'PACK', directory, len(entries) * 64)
    return output


def resize_mdl(data, limit):
    if len(data) < 84 or data[:4] != b'IDPO' or struct.unpack_from('<I', data, 4)[0] != 6:
        raise ValueError('Unsupported alias model')
    skins, width, height, vertices, triangles, frames = struct.unpack_from('<6i', data, 48)
    if not (1 <= skins <= 16 and 4 <= width <= 4096 and 1 <= height <= 480 and
            1 <= vertices <= 1024 and triangles > 0 and 1 <= frames < 256):
        raise ValueError('Alias model exceeds format 3 limits')
    new_width, new_height = min(width, limit), min(height, limit)
    new_width = (new_width + 3) & ~3
    output = bytearray(data[:84])
    struct.pack_into('<ii', output, 52, new_width, new_height)
    offset = 84
    for _ in range(skins):
        if offset + 4 + width * height > len(data):
            raise ValueError('Truncated model skin')
        kind = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        if kind:
            raise ValueError('Grouped model skins require a separate recipe')
        skin = data[offset:offset + width * height]
        offset += width * height
        output.extend(struct.pack('<I', 0))
        # Palette indices stay palette indices. Match nearest-neighbor sampling
        # at pixel centers without introducing a picture-library dependency.
        columns = [min(width - 1, (2 * x + 1) * width // (2 * new_width))
                   for x in range(new_width)]
        for y in range(new_height):
            row = min(height - 1, (2 * y + 1) * height // (2 * new_height)) * width
            output.extend(skin[row + x] for x in columns)
    if offset + vertices * 12 > len(data):
        raise ValueError('Truncated model texture coordinates')
    for _ in range(vertices):
        seam, s, t = struct.unpack_from('<iii', data, offset)
        offset += 12
        output.extend(struct.pack('<iii', seam, s * new_width // width,
                                  t * new_height // height))
    output.extend(data[offset:])  # Geometry and all animation frames are unchanged.
    return bytes(output)


def preview_level(data):
    if len(data) < 124 or struct.unpack_from('<I', data)[0] != 29:
        raise ValueError('Expected a classic BSP29 level')
    output = bytearray(124)
    struct.pack_into('<I', output, 0, 29)
    for index in range(15):
        offset, length = struct.unpack_from('<II', data, 4 + index * 8)
        if length and (offset < 124 or offset + length > len(data)):
            raise ValueError('Truncated BSP lump')
        block = data[offset:offset + length]
        if index == 0:
            # The preview has one complete map. Its exit starts a new run.
            block = block.replace(b'"lq_e0m2"', b'"start"')
        while len(output) % 4:
            output.append(0)
        struct.pack_into('<II', output, 4 + index * 8, len(output), len(block))
        output.extend(block)
    return bytes(output)


def prepare(entry, archive, destination, converter, log):
    verify_archive(archive, entry)
    files = {}
    with zipfile.ZipFile(archive) as source:
        for name, size in entry['pak_members'].items():
            if source.getinfo(name).file_size != size:
                raise ValueError('Unexpected original PAK size')
            files.update(unpack_pak(source.read(name)))
        selected = {}
        for name, data in files.items():
            if name.endswith(('.lit', '.dem', '.cfg')) or name in ('progs.dat', 'progs.lno'):
                continue
            if name.startswith('maps/') and name.endswith('.bsp') and not name.startswith('maps/b_'):
                if name != entry['level']:
                    continue
                name, data = 'maps/start.bsp', preview_level(data)
            if name.endswith('.mdl'):
                data = resize_mdl(data, entry['skin_limit'])
            selected[name] = data
        if 'maps/start.bsp' not in selected:
            raise ValueError('Preview level is missing')
        selected['default.cfg'] = DEFAULT_CFG
        destination.mkdir(parents=True, exist_ok=False)
        intermediate = destination.parent / 'source.pak'
        intermediate.write_bytes(pack_pak(selected))
        subprocess.run([str(converter), str(intermediate), str(destination / 'pak0.pak')],
                       check=True, stdout=log, stderr=subprocess.STDOUT, timeout=90)
        intermediate.unlink()
        for name in entry['license_members']:
            target = destination / 'licenses' / relative_path(name)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read('lite/id1/docs/' + name))
    (destination / 'ABOUT.txt').write_text(
        'LibreQuake free first-level preview\n'
        'Assets: LibreQuake v0.09-beta, BSD-3-Clause; pop.lmp is GPL-2.0.\n'
        'Source: https://github.com/lavenderdotpet/LibreQuake/tree/' + entry['commit'] + '\n'
        'Modifications: model skins limited to 128x128, PXA MCU format 3, and\n'
        'the first Lite level installed as start. Its exit starts a new run.\n'
        'Model geometry and animation frames remain unchanged.\n'
        'This preview uses the engine built-in GPL gamecode, with no external progs.dat.\n'
        'No id Software game data, retail levels, demos or soundtrack are included.\n'
        'All accompanying original licenses and credits are under licenses/.\n')


def main():
    apps = Path(__file__).resolve().parents[3]
    recipe = json.loads((Path(__file__).resolve().parents[1] / 'resources/librequake-media.json').read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--signing-key', type=Path, required=True)
    args = parser.parse_args()
    output = args.output_root.resolve() / 'quake'
    output.mkdir(parents=True, exist_ok=True)
    archive = args.archive
    if archive is None:
        archive = output / recipe['archive']['name']
        if not archive.exists():
            temporary = archive.with_suffix('.part')
            try:
                with urllib.request.urlopen(recipe['archive']['url'], timeout=60) as source, temporary.open('wb') as target:
                    remaining = recipe['archive']['bytes']
                    while remaining:
                        block = source.read(min(65536, remaining))
                        if not block:
                            raise ValueError('Incomplete LibreQuake download')
                        target.write(block)
                        remaining -= len(block)
                    if source.read(1):
                        raise ValueError('LibreQuake download exceeds pinned size')
                verify_archive(temporary, recipe)
                temporary.replace(archive)
            finally:
                temporary.unlink(missing_ok=True)
    verify_archive(archive, recipe)
    with tempfile.TemporaryDirectory(prefix='librequake-', dir=output) as directory:
        stage = Path(directory)
        converter = stage / 'pakconvert'
        with (output / 'prepare.log').open('wb') as log:
            sources = sorted((apps / 'quake/tools/MCUPackConverter').glob('*.c'))
            subprocess.run([os.environ.get('CC', 'cc'), '-O2', *map(str, sources), '-lm', '-o', str(converter)],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
            payload = stage / 'package'
            prepare(recipe, archive, payload, converter, log)
            metadata = stage / 'package.json'
            metadata.write_text(json.dumps({'format': 'pxa-package-source-0.2', 'package_type': 'resources',
                'id': recipe['id'], 'name': recipe['title'], 'version': recipe['version'],
                'release_sequence': recipe['release_sequence'], 'min_sdk': [2, 1], 'target_sdk': [2, 1]}, indent=2) + '\n')
            subprocess.run([sys.executable, str(args.sdk_root / 'tools/package/build_resource_package.py'),
                str(metadata), str(payload), str(args.signing_key), str(output / 'resources.pxa'),
                '--descriptor', str(output / 'binding.json')], check=True, stdout=log, stderr=subprocess.STDOUT)
    print((output / 'binding.json').read_text())


if __name__ == '__main__':
    main()
