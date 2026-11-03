#!/usr/bin/env python3
"""Exercise the portable file layer against independent host files and writable overlays.

This validates seek/read/write/rename/error semantics, not a full PXA device.
All fixtures, executables and logs are written below --output.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
port = Path(__file__).resolve().parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
raw = (port / 'assets/jinyong.bundle').read_bytes()
assert hashlib.sha256(raw).hexdigest() == 'adb7971b6d2314d181b67b2d16b4eb41838b53ab61dc6504220ce198e8aabe0f'
index = (port / 'engine/jy_asset_index.c').read_text()
pattern = r'\{"([^"]+)","assets/jinyong.bundle",(\d+)u,(\d+)u,(\d+)u\}'
entries = re.findall(pattern, index)
assert len(entries) == 1371
for name, size, offset, total in entries:
    assert int(total) == len(raw) and int(offset) + int(size) <= len(raw)
    target = out / 'reference/assets' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(raw[int(offset):int(offset) + int(size)])
# The host oracle intentionally uses separate physical files, independent of
# the production bundle backend tested by the real Guest.
index = re.sub(pattern, lambda m: '{"' + m[1] + '","assets/' + m[1] + '",' + m[2] + 'u,0u,0u}', index)
(out / 'index.c').write_text(index)
exe = out / 'filesystem-test'
with (out / 'build.log').open('w') as log:
    subprocess.run(['cc', '-std=c11', '-O2', '-I' + str(port / 'fs'),
                    str(port / 'fs/jy_fs.c'), str(port / 'fs/native_fixture.c'),
                    str(port / 'fs/redirect_fixture.c'), str(out / 'index.c'),
                    '-o', str(exe)], stdout=log, stderr=subprocess.STDOUT, check=True)
for name, extra in [('files', []), ('clone', ['--clone-only'])]:
    with (out / (name + '.log')).open('w') as log:
        subprocess.run([str(exe), str(out / 'reference'), str(out / ('overlay-' + name)), *extra],
                       cwd=out, stdout=log, stderr=subprocess.STDOUT, check=True)
    print(name + ' passed')

# Exercise the actual private-file block-store transport independently of
# the filesystem oracle above (whose backing callbacks are plain host files).
block_out = out / 'block-store'
block_out.mkdir(exist_ok=True)
sdk = port.parents[2] / 'deps/pxa-system/sdk/guest-c/include'
block_exe = out / 'block-store-test'
with (out / 'block-store-build.log').open('w') as log:
    subprocess.run(['cc', '-std=c11', '-O2', '-Wno-attributes',
                    '-ffunction-sections', '-fdata-sections', '-I' + str(sdk),
                    '-I' + str(port / 'fs'), '-I' + str(port),
                    str(port / 'tests/block_store.c'), str(port / 'fs/jy_fs.c'),
                    str(port / 'engine/jy_asset_index.c'),
                    *[str(port / 'deps/zlib' / (name + '.c')) for name in
                      ['adler32','crc32','deflate','inflate','inffast','inftrees','trees','zutil','compress','uncompr']],
                    '-I' + str(port / 'deps/zlib'), '-DNO_GZIP', '-Wl,--gc-sections',
                    '-o', str(block_exe)], stdout=log, stderr=subprocess.STDOUT, check=True)
with (out / 'block-store.log').open('w') as log:
    subprocess.run([str(block_exe), str(block_out)], stdout=log,
                   stderr=subprocess.STDOUT, check=True)
print('private file block-store passed')

calls_exe = out / 'guest-calls-test'
with (out / 'guest-calls-build.log').open('w') as log:
    subprocess.run(['cc', '-std=c11', '-O2', '-Wno-attributes',
                    '-I' + str(sdk), '-I' + str(port),
                    str(port / 'guest_calls.c'), str(port / 'tests/guest_calls.c'),
                    '-o', str(calls_exe)], stdout=log, stderr=subprocess.STDOUT, check=True)
with (out / 'guest-calls.log').open('w') as log:
    subprocess.run([str(calls_exe)], stdout=log, stderr=subprocess.STDOUT, check=True)
print('Host call scheduling passed')

span_exe = out / 'write-spans-test'
with (out / 'write-spans-build.log').open('w') as log:
    subprocess.run(['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I' + str(port / 'fs'),
                    str(port / 'fs/jy_fs.c'), str(port / 'tests/write_spans.c'),
                    '-o', str(span_exe)], stdout=log, stderr=subprocess.STDOUT, check=True)
with (out / 'write-spans.log').open('w') as log:
    subprocess.run([str(span_exe)], stdout=log, stderr=subprocess.STDOUT, check=True)
print('bounded FILE write spans passed')
