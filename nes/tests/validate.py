#!/usr/bin/env python3
"""Native regression runner. Optional CPU ROMs are downloaded only on request."""
import argparse
import concurrent.futures
import json
import re
import pathlib
import subprocess
import sys
import urllib.request

app = pathlib.Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--output', type=pathlib.Path, required=True)
p.add_argument('--cpu-roms', type=pathlib.Path)
p.add_argument('--download', action='store_true')
p.add_argument('--sanitize', action='store_true')
args = p.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
def run(command):
    print(' '.join(map(str, command)), flush=True)
    return subprocess.run(list(map(str, command)), check=True, capture_output=True, text=True)

configure = ['cmake', '-S', app, '-B', out/'build', '-DNES_NATIVE_TEST=ON']
if args.sanitize:
    configure += ['-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer',
                  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
run(configure)
run(['cmake', '--build', out/'build', '-j8'])
run([sys.executable, app/'tests/make_demo.py', out/'demo.nes'])
exe = out/'build/nes_core_test'
report = []
def check(rom, *extra):
    result = run([exe, rom, *extra])
    reference=run([out/'build/nes_core_reference_test',rom,*extra])
    signature=re.search(r'SIGNATURE ([0-9a-f]+)',result.stdout).group(1)
    reference_signature=re.search(r'SIGNATURE ([0-9a-f]+)',reference.stdout).group(1)
    if signature!=reference_signature:raise SystemExit(f'OAM differential failed: {rom}')
    report.append({'rom': pathlib.Path(rom).name, 'stdout': result.stdout, 'stderr': result.stderr, 'reference_signature': reference_signature})
check(out/'demo.nes', 600)
demo = (out/'demo.nes').read_bytes()
pal=bytearray(demo);pal[9]=1
(out/'pal.nes').write_bytes(pal);check(out/'pal.nes',600)
trainer=bytearray(demo[:16]);trainer[6]|=4
(out/'trainer.nes').write_bytes(trainer+bytes(range(256))*2+demo[16:]);check(out/'trainer.nes',600)
for mapper in (1, 2, 3, 4, 7, 66):
    header = bytearray(demo[:16])
    header[4] = 2
    header[6] = (mapper & 15)*16+2
    header[7] = mapper & 240
    rom = out/f'mapper-{mapper}.nes'
    rom.write_bytes(header+bytes([234])*16384+demo[16:])
    check(rom, 600)
if args.download:
    cache = args.cpu_roms or out/'cpu-roms'
    cache.mkdir(parents=True, exist_ok=True)
    url = 'https://api.github.com/repos/christopherpow/nes-test-roms/contents/instr_test-v5/rom_singles'
    entries = json.load(urllib.request.urlopen(url, timeout=30))
    def fetch(entry):
        if entry['name'].endswith('.nes'):
            (cache/entry['name']).write_bytes(urllib.request.urlopen(entry['download_url'], timeout=30).read())
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(fetch, entries))
    args.cpu_roms = cache
if args.cpu_roms:
    cases = sorted(args.cpu_roms.glob('*.nes'))
    if len(cases) != 16:
        raise SystemExit('Expected the 16 instr_test-v5/rom_singles ROMs')
    for rom in cases:
        check(rom, 3000, 'blargg')
(out/'results.json').write_text(json.dumps(report, indent=2)+'\n')
print(f'PASS: {len(report)} cases; evidence: {out / "results.json"}')
