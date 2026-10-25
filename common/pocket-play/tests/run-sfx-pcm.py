#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Native real-app SFX/idle PCM regression. Outputs only to local/artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
BASE = HERE.parents[2]
ROOT = BASE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--apps', nargs='+', default=['pet-garden', 'marble-islands', 'lantern-grove', 'harbor-cafe', 'tiny-elements', 'crisp-arcade'])
args = parser.parse_args()
out = args.output.resolve()
if ROOT / 'local/artifacts' not in out.parents:
    parser.error('--output must be inside local/artifacts')
out.mkdir(parents=True, exist_ok=True)
audio = (HERE.parent / 'audio.inc').read_text()
prefix = audio.split('void service_sound() {', 1)[0]
assert 'void render_sound()' in prefix and 'void add_tone(' in prefix
(out / 'audio-render-prefix.inc').write_text(prefix)
flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-fwrapv']
env = os.environ.copy()
env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
rows = []
for name in args.apps:
    app = BASE / name
    dest = out / name
    dest.mkdir(exist_ok=True)
    commands = []
    sources = [app / 'main.cpp']
    if name == 'crisp-arcade':
        sources.append(app / 'backend.cpp')
        c_sources = ['core_bridge.c', 'registration.c'] + ['vendor/src/lib/' + s + '.c' for s in ['menu', 'particle', 'random', 'sound', 'textPattern', 'vector']] + ['vendor/src/games/game' + s + '.c' for s in ['CastN', 'GrapplingH', 'Reflector', 'ColorRoll']]
        for c_source in c_sources:
            obj = dest / (c_source.replace('/', '_') + '.o')
            commands.append(['cc', '-std=c11', *flags, '-I' + str(app / 'vendor/src/lib'), '-c', str(app / c_source), '-o', str(obj)])
            sources.append(obj)
    else:
        sources.append(app / 'core.cpp')
        if name == 'marble-islands':
            sources.append(app / 'renderer.cpp')
    commands.append(['c++', '-std=c++17', *flags, '-no-pie', '-Wall', '-Wextra', '-Werror', '-Wno-unused-variable', '-DAPP_' + name.replace('-', '_').upper(), '-I' + str(out), str(HERE / 'sfx_pcm.cpp'), *map(str, sources), '-lm', '-o', str(dest / 'sfx-pcm')])
    commands.append([str(dest / 'sfx-pcm'), str(dest)])
    text = ''
    code = 0
    with (dest / 'run.log').open('w') as log:
        for command in commands:
            log.write('COMMAND ' + json.dumps(command) + '\n')
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
            log.write(result.stdout + '\nEXIT ' + str(result.returncode) + '\n')
            log.flush()
            text += result.stdout
            if result.returncode:
                code = result.returncode
                break
    row = {'app': name, 'exit': code, 'log': str(dest / 'run.log')}
    for line in text.splitlines():
        if line.startswith('PCM_RESULT '):
            row.update(json.loads(line.removeprefix('PCM_RESULT ')))
    rows.append(row)
    print(json.dumps(row), flush=True)
report = {'audio_inc_sha256': hashlib.sha256(audio.encode()).hexdigest(), 'render_prefix_sha256': hashlib.sha256(prefix.encode()).hexdigest(), 'results': rows}
(out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
sys.exit(0 if all(row['exit'] == 0 for row in rows) else 1)
