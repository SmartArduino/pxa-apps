#!/usr/bin/env python3
"""Run OPR regressions and a five-floor campaign; write only to --output."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
port = Path(__file__).resolve().parents[1]
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
sources = sorted((port / 'engine/src').glob('*.c'))
flags = ['cc', '-O2', '-std=c11', '-ffp-contract=off', '-Drand=opr_rand',
         '-I' + str(port / 'engine/include'), '-I' + str(port)]
for name, files in [('regressions', ['tests/regressions.c']),
                    ('campaign', ['tests/native.c', 'portable-rng.c', 'session.c', 'ui.c', 'render.c'])]:
    executable = out / name
    with (out / (name + '-build.log')).open('w') as log:
        subprocess.run([*flags, *[str(port / f) for f in files], *map(str, sources),
                        '-o', str(executable)], stdout=log, stderr=subprocess.STDOUT, check=True)
    for args in ([['single'], ['boss']] if name == 'regressions' else [[]]):
        with (out / (name + '-' + ''.join(args) + '.log')).open('w') as log:
            subprocess.run([str(executable), *args], cwd=out, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
    print(name + ' passed')
