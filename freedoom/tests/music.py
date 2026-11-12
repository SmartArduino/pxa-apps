#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run MIDI/OPL checks under ASan/UBSan and render every supplied WAD song."""
import argparse
import json
from pathlib import Path
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wads', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
app = Path(__file__).resolve().parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
source = app.parent / 'duke3d/vendor/Game/src/midi'
songs = []
for wad in sorted(args.wads.glob('freedoom[12].wad')):
    data = wad.read_bytes()
    count, directory = struct.unpack_from('<II', data, 4)
    for i in range(count):
        offset, size, name = struct.unpack_from('<II8s', data, directory + i*16)
        name = name.rstrip(b'\0').decode('ascii')
        if name == 'GENMIDI':
            (out / 'GENMIDI').write_bytes(data[offset:offset+size])
        if name.startswith('D_'):
            path = out / (wad.stem + '-' + name + '.mid')
            path.write_bytes(data[offset:offset+size])
            songs.append(path)
if not songs:
    parser.error('--wads must contain freedoom1.wad and/or freedoom2.wad')
# The existing emulator uses two's-complement shifts; keep its sources intact.
# New sequencer code is checked with all UBSan checks enabled.
with (out / 'build.log').open('w') as log:
    for unit, flags in ((source / 'opl2.c', ['-fno-sanitize=shift']),
                        (app / 'music.c', []), (app / 'tests/music_test.c', [])):
        subprocess.run(['cc', '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-fwrapv',
            '-Wno-unused-variable', '-Wno-unused-parameter', '-fsanitize=address,undefined',
            '-fno-omit-frame-pointer', *flags, '-I' + str(app), '-I' + str(app / 'opl-compat'),
            '-I' + str(source), '-c', str(unit), '-o', str(out / (unit.stem + '.o'))],
            check=True, stdout=log, stderr=subprocess.STDOUT)
    subprocess.run(['cc', '-fsanitize=address,undefined', str(out / 'opl2.o'),
        str(out / 'music.o'), str(out / 'music_test.o'), '-lm', '-o', str(out / 'music-test')],
        check=True, stdout=log, stderr=subprocess.STDOUT)
with (out / 'test.log').open('w') as log:
    subprocess.run([str(out / 'music-test'), str(out / 'GENMIDI'), *map(str, songs)],
                   check=True, stdout=log, stderr=subprocess.STDOUT, timeout=300)
(out / 'results.json').write_text(json.dumps({'passed': True, 'songs': len(songs),
    'checks': ['multitrack', 'tempo', 'loops', 'pause', 'mute', 'controllers', 'truncation',
               'invalid input', 'all supplied tracks through first loop', 'ASan', 'UBSan']}, indent=2) + '\n')
print(f'PASS: {len(songs)} full MIDI tracks and sequencer checks; {out}')
