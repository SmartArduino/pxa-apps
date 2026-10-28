#!/usr/bin/env python3
"""Build the app's Host probe with an existing simulator's CMake recipes."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', required=True, type=Path)
    parser.add_argument('--artifacts', required=True, type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    artifacts = args.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    entries = json.loads((build / 'compile_commands.json').read_text())
    entry = next(e for e in entries if Path(e['file']).name == 'product_runner.c'
                 and 'pxsys_product_simulator.dir' in e['command'])
    command = shlex.split(entry['command'])
    command[command.index('-o')+1] = str(artifacts/'audio-probe.o')
    command[command.index(entry['file'])] = str(Path(__file__).with_name('audio_handoff_probe.c').resolve())
    command.append('-DPXA_PRODUCT_RUNNER="'+entry['file']+'"')
    subprocess.run(command, cwd=entry['directory'], check=True)
    link = shlex.split((build/'CMakeFiles/pxsys_product_simulator.dir/link.txt').read_text())
    index = next(i for i, value in enumerate(link) if value.endswith('/product_runner.c.o'))
    link[index] = str(artifacts/'audio-probe.o')
    link[link.index('-o')+1] = str(artifacts/'xiaozhi-audio-probe')
    subprocess.run(link, cwd=build, check=True)
    print(artifacts/'xiaozhi-audio-probe')


if __name__ == '__main__':
    main()
