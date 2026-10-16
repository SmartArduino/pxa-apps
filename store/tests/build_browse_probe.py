#!/usr/bin/env python3
"""Build the Store-specific integration probe against a configured simulator.

Usage: build_browse_probe.py BUILD_DIRECTORY OUTPUT
The simulator (including pxsys_store_images_test) must already be built.
The build reuses its compiler flags and static libraries without modifying it.
"""
from pathlib import Path
import argparse
import re
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--source', type=Path, default=Path(__file__).with_name('store_browse_probe.c'))
args = parser.parse_args()
build = args.build.resolve()
output = args.output.resolve()
output.parent.mkdir(parents=True, exist_ok=True)
unit = build / 'CMakeFiles/pxsys_store_images_test.dir'
flags = (unit / 'flags.make').read_text()
arguments = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    match = re.search(rf'^{key} = (.*)$', flags, re.MULTILINE)
    if not match:
        raise SystemExit(f'Missing {key} in {unit / "flags.make"}')
    arguments += shlex.split(match[1])
runner = next(Path(x[2:]).parent.parent / 'simulator/desktop/product_runner.c'
              for x in arguments if x.startswith('-I') and x.endswith('/libpxa/include'))
source = args.source.resolve()
link = shlex.split((unit / 'link.txt').read_text())
compiler = link[0]
obj = output.with_suffix('.o')
subprocess.run([compiler, *arguments, f'-DPXA_PRODUCT_RUNNER="{runner}"',
                '-c', str(source), '-o', str(obj)], cwd=build, check=True)
old = 'CMakeFiles/pxsys_store_images_test.dir/tests/store_image_capture_test.c.o'
link[link.index(old)] = str(obj)
link[link.index('-o') + 1] = str(output)
link = [x for x in link if not x.startswith('-Wl,--dependency-file=')]
subprocess.run(link, cwd=build, check=True)
print(output)
