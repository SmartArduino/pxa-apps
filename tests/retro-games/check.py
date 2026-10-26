#!/usr/bin/env python3
"""Validate the imported catalogue, run transport tests and optionally build it."""
import argparse
import concurrent.futures
import datetime
import json
import os
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--workspace', type=Path, default=repo.parents[1])
parser.add_argument('--output', type=Path)
parser.add_argument('--target', choices=('wasm', 'simulator', 'esp32s3', 'esp32s31', 'all'))
parser.add_argument('--apps', help='Comma-separated directory names; default is the full catalogue')
parser.add_argument('--workers', type=int, default=3)
args = parser.parse_args()
workspace = args.workspace.resolve()
output = (args.output or workspace/'local/artifacts/retro-games'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')).resolve()
output.mkdir(parents=True, exist_ok=True)
catalogue = json.loads((repo/'common/retro-games/games.json').read_text())
games = catalogue['games']
assert len({g['directory'] for g in games}) == len(games)
assert len({g['id'] for g in games}) == len(games)
existing = {}
for path in repo.glob('*/package.json'):
    manifest = json.loads(path.read_text())
    app_id = manifest['id']
    assert app_id not in existing, (app_id, existing.get(app_id), str(path))
    existing[app_id] = str(path)
for game in games:
    manifest = json.loads((repo/game['directory']/'package.json').read_text())
    assert manifest['id'] == game['id']
    assert manifest['min_sdk'] == [2, 1]
    assert manifest['publisher']['handle'] == 'lucinhu'
    assert manifest['format'] == 'pxa-package-source-0.2'
    assert manifest['icon'] == 'assets/icon.png'
    assert (repo/game['directory']/manifest['icon']).is_file()
    assert (repo/game['directory']/'assets/icon.svg').is_file()
    for component in manifest['components']:
        assert 'system' not in component['services']
for skip in catalogue.get('existing_games', []):
    assert skip['source'] not in {game['directory'] for game in games}
sdk = workspace/'deps/pxa-system'
for name in ('viewport', 'framebuffer'):
    exe = output/(name+'_test')
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-unknown-attributes']
    if os.getenv('SANITIZE') == '1':
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    with (output/(name+'-build.log')).open('w') as log:
        subprocess.run([os.getenv('CC', 'clang'), *flags, '-I'+str(sdk/'sdk/guest-c/include'),
                        str(repo/f'common/retro-games/tests/{name}_test.c'), '-o', str(exe)],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    subprocess.run([str(exe)], check=True)
if args.apps:
    selected = set(args.apps.split(','))
    assert selected <= {game['directory'] for game in games}, selected
    games = [game for game in games if game['directory'] in selected]

def build(game):
    name = game['directory']
    with (output/(name+'.log')).open('w') as log:
        command = [str(workspace/'tools/app.sh'), 'build', name, '--source-root', str(repo),
                   '--target', args.target, '--output', str(output/'packages'/name)]
        code = subprocess.run(command, cwd=workspace, stdout=log, stderr=subprocess.STDOUT).returncode
    row = {'app': name, 'passed': code == 0}
    print(name, 'PASS' if code == 0 else 'FAIL', flush=True)
    return row

results = []
if args.target:
    with concurrent.futures.ThreadPoolExecutor(max(1, args.workers)) as pool:
        results = list(pool.map(build, games))
report = {'catalogue_apps': len(catalogue['games']), 'native_tests': 2,
          'target': args.target, 'builds': results}
(output/'result.json').write_text(json.dumps(report, indent=2)+'\n')
assert all(row['passed'] for row in results), 'Inspect the failing application logs in '+str(output)
print('PASS:', output)
