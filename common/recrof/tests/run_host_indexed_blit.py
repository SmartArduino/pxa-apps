#!/usr/bin/env python3
"""Compare borrowed indexed resolve with the unchanged game-frame adapter."""
import os
import argparse
from contextlib import nullcontext
from pathlib import Path
import subprocess
import tempfile
here = Path(__file__).resolve().parent
sdk = next(p / 'deps/pxa-system/libpxa' for p in here.parents if (p / 'deps/pxa-system/libpxa').is_dir())
common = here.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path)
parser.add_argument('--gather-blit', action='store_true', help='use the C&C S31 gather profile')
args = parser.parse_args()
if args.output:
    args.output.mkdir(parents=True, exist_ok=True)
with (nullcontext(str(args.output.resolve())) if args.output else
      tempfile.TemporaryDirectory(prefix='pxa-indexed-blit-')) as name:
    command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-g', '-Wall', '-Wextra',
        '-Werror', '-ffp-contract=off', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
        '-I' + str(common), '-I' + str(sdk / 'include'), str(here / 'host_indexed_blit_test.c'),
        str(common / 'framebuffer.c'), str(common / 'font8x8.c')]
    if args.gather_blit:
        command.append('-DPORT_CNC_GATHER_BLIT=1')
    command += [str(sdk / 'src/services/game_render' / source) for source in
        ['raster_dispatch.c', 'raster_blit.c', 'raster_priority_tiles.c']]
    command += ['-o', str(Path(name) / 'test')]
    subprocess.run(command, check=True)
    subprocess.run([str(Path(name) / 'test')], check=True,
        env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1', ASAN_OPTIONS='detect_leaks=1'))
