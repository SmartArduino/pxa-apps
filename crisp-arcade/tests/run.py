#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build only native tests; keep every generated file in ignored artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=ROOT / 'local/artifacts/pocket-play/20261024-015231/crisp-tests')
parser.add_argument('--sanitize', action='store_true', help='Enable AddressSanitizer and UBSan')
args = parser.parse_args()
output = args.output.resolve()
artifact_root = (ROOT / 'local/artifacts').resolve()
if artifact_root not in output.parents:
    parser.error('--output must be inside the ignored local/artifacts directory')
output.mkdir(parents=True, exist_ok=True)
provenance = json.loads((APP / 'vendor/provenance.json').read_text())
for name, expected in provenance['files'].items():
    assert hashlib.sha256((APP / 'vendor' / name).read_bytes()).hexdigest() == expected, name
assert (APP / 'assets/LICENSE-crisp-game-lib-portable.txt').read_bytes() == (APP / 'vendor/LICENSE.txt').read_bytes()
print('PASS byte-identical upstream source and packaged MIT license', flush=True)

cc, cxx = os.environ.get('CC','cc'), os.environ.get('CXX','c++')
flags = ['-O1','-g','-fwrapv','-I'+str(APP/'vendor/src/lib'),'-Wall','-Wextra','-Wno-sign-compare','-Wno-unused-variable']
if args.sanitize:
    flags += ['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-pie']
sources = ['core_bridge.c','registration.c'] + ['vendor/src/lib/'+s+'.c' for s in ['menu','particle','random','sound','textPattern','vector']] + ['tests/probe_'+s+'.c' for s in ['cast','grappling','reflector','color']]
objects = []
def run(command):
    environment = os.environ.copy()
    environment.setdefault('UBSAN_OPTIONS', 'halt_on_error=1:print_stacktrace=1')
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=environment)
    if result.stdout:
        print(result.stdout, end='', flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)
for source in sources:
    obj = output / (source.replace('/','_')+'.o')
    run([cc,'-std=c11',*flags,'-c',str(APP/source),'-o',str(obj)])
    objects.append(str(obj))
exe = output / 'crisp-native'
link = [cxx,'-std=c++17',*flags,str(APP/'main.cpp'),str(APP/'backend.cpp'),str(APP/'tests/native.cpp'),*objects,'-lm','-o',str(exe)]
if args.sanitize:
    link.append('-no-pie')
run(link)
for language in ('zh','en'):
    localized_output = output / language
    localized_output.mkdir(exist_ok=True)
    if language == 'en':
        os.environ['POCKET_TEST_EN']='1'
    else:
        os.environ.pop('POCKET_TEST_EN',None)
    run([str(exe),str(localized_output)])
# Convert simple PPM snapshots to PNG with the standard library only.
import struct
import zlib
for ppm in output.rglob('*.ppm'):
    with ppm.open('rb') as stream:
        assert stream.readline() == b'P6\n'
        width,height = map(int,stream.readline().split())
        assert stream.readline() == b'255\n'
        pixels = stream.read()
    def chunk(kind,data):
        return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
    image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR',struct.pack('!IIBBBBB',width,height,8,2,0,0,0))
    image += chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height)),9)) + chunk(b'IEND',b'')
    ppm.with_suffix('.png').write_bytes(image)
print('Native snapshots:',output)
