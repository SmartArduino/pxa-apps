#!/usr/bin/env python3
"""Reuse a configured product simulator's exact compiler flags and libraries.

Builds no production code changes. Run from the PXA workspace root after
`bash tools/simulator.sh build --profile pai-touch` and game package builds.
"""
import argparse
import json
from pathlib import Path
import shlex
import statistics
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--simulator-build', type=Path, required=True)
parser.add_argument('--artifacts', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--lifecycle', action='store_true', help='Check C++ clock and music pause/resume')
args = parser.parse_args()
build = args.simulator_build.resolve()
artifacts = args.artifacts.resolve()
output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
source = Path(__file__).resolve().with_name('aot_probe.c')
root = Path.cwd()
target = build/'CMakeFiles/pxsys_voxel_resources_test.dir'
variables = {}
for line in (target/'flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1); variables[key] = shlex.split(value)
obj = output/'aot_probe.o'; executable = output/'aot_probe'
compiler = shlex.split((target/'link.txt').read_text())[0]
subprocess.run([compiler, *variables['C_DEFINES'], *variables['C_INCLUDES'],
    *variables['C_FLAGS'], '-DPXA_PRODUCT_RUNNER="'+str(root/'deps/pxa-system/simulator/desktop/product_runner.c')+'"',
    '-c', str(source), '-o', str(obj)], check=True, cwd=build)
link = shlex.split((target/'link.txt').read_text())
link = [str(obj) if value.endswith('/voxel_resource_test.c.o') else value
        for value in link if not value.startswith('-Wl,--dependency-file=')]
link[link.index('-o')+1] = str(executable)
subprocess.run(link, check=True, cwd=build)
key = root/'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'
report = []
for game, identity in [('jump', 'pxa-jump-jump-3d'), ('pixel', 'pxa-pixel-dungeon')]:
    for repeat in range(3):
        for cpp in [False, True]:
            if args.lifecycle and not cpp: continue
            case = f'{game}-{"cpp" if cpp else "c"}-{repeat}'
            state = output/(case+'-state'); state.mkdir(exist_ok=True)
            package = artifacts/(game+'-all' if cpp else game+'-c-baseline')/(identity+('-cpp' if cpp else ''))
            rows = output/(case+'.jsonl')
            with (output/(case+'.log')).open('w') as log:
                subprocess.run([str(executable), str(package), str(key), str(state), str(rows),
                    *(['static'] if game=='pixel' else []),
                    *(['lifecycle'] if args.lifecycle else [])],
                    stdout=log, stderr=subprocess.STDOUT, check=True, timeout=25)
            data = [json.loads(line) for line in rows.read_text().splitlines()]
            lifecycle = [row for row in data if row['phase']=='lifecycle']
            if args.lifecycle: assert lifecycle==[{'phase':'lifecycle', 'passed':True}]
            data = [row for row in data if row['phase']!='lifecycle']
            frames = [row for row in data if row['phase']=='frame']
            assert len(frames)==(60 if game=='jump' else 0)
            row = {'game':game, 'cpp':cpp, 'repeat':repeat,
                'cpu_p50_us':statistics.median(r['cpu_us'] for r in frames) if frames else None,
                'raster_p50_us':statistics.median(r['raster_us'] for r in frames) if frames else None,
                'draw_bytes':statistics.mean(r['draw_bytes'] for r in frames) if frames else data[-1]['draw_bytes'],
                'warm':data[0], 'end':data[-1], 'lifecycle_passed':bool(lifecycle)}
            if frames:
                row['desktop_visible_fps'] = 1e6*data[-1]['visible_delta']/data[-1]['capture_us']
                assert row['desktop_visible_fps']>=45, 'clock/simulation coupling dropped presentation frames'
            report.append(row); print(case, row['cpu_p50_us'], row['raster_p50_us'], flush=True)
            (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
if not args.lifecycle:
    jump_c = statistics.median(r['desktop_visible_fps'] for r in report if r['game']=='jump' and not r['cpp'])
    jump_cpp = statistics.median(r['desktop_visible_fps'] for r in report if r['game']=='jump' and r['cpp'])
    assert jump_cpp>=jump_c*.95, 'C++ presentation cadence regressed'
