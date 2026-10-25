#!/usr/bin/env python3
"""Check startup raster feedback and the real RTS filesystem adapters."""
import argparse
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
args=p.parse_args()
here=Path(__file__).resolve().parent
source=here.parent
root=here.parents[4]
out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
base=['cc','-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-attributes',
      '-Wno-misleading-indentation','-Wno-format-truncation',
      '-fsanitize=address,undefined','-fno-omit-frame-pointer',
      '-ffunction-sections','-fdata-sections','-I'+str(source),'-I'+str(here/'fs-stubs'),
      '-I'+str(root/'deps/pxa-system/sdk/guest-c/include')]
rts=['-DPORT_RTS=1','-DPORT_EXTERNAL_GAME_DATA=1','-DPORT_CNC_CANONICAL_PATHS=1']
for name,units,flags in [
    ('loading',['tests/loading_test.c','framebuffer.c','font8x8.c'],[]),
    ('missing-paths',['tests/missing_paths_test.c','filesystem.c'],rts),
    ('missing-paths-loading',['tests/missing_paths_test.c','filesystem.c'],rts+['-DPORT_MISSING_PATHS_CAPACITY=64']),
    ('media-handles',['tests/media_handles_test.c','filesystem.c'],rts+['-DPORT_CNC_MEDIA_HANDLES=1']),
    ('external-read-cache',['tests/external_read_cache_test.c','filesystem.c'],rts)]:
    subprocess.run(base+flags+[str(source/u) for u in units]+['-Wl,--gc-sections','-o',str(out/name)],check=True)
    subprocess.run([str(out/name)]+([str(out/'loading.ppm')] if name=='loading' else []),check=True)
