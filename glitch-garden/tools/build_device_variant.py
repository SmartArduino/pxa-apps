#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the large-display P4 profile without changing the S31/Wasm defaults."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pxa', required=True, help='Pinned DevKit bin/pxa')
    parser.add_argument('--output', required=True, type=Path, help='New ignored artifact directory outside this app')
    parser.add_argument('--define', action='append', default=[])
    args = parser.parse_args()
    app = Path(__file__).resolve().parents[1]
    output = args.output.expanduser().resolve()
    if output.is_relative_to(app) or output.exists():
        parser.error('--output must be a new artifact directory outside the app')
    output.mkdir(parents=True)
    project = output / 'source/glitch-garden'
    shutil.copytree(app, project, ignore=shutil.ignore_patterns('.git', '.pxa', 'dist', '__pycache__'))
    common = output / 'source/common/motion-games'
    common.mkdir(parents=True)
    for name in ('sound.hpp', 'codec.hpp'):
        shutil.copy2(app.parent / 'common/motion-games' / name, common / name)
    metadata_path = project / 'package.json'
    metadata = json.loads(metadata_path.read_text())
    # This is a source packaging profile, before signing. Preserve identity,
    # version, sequence, SDK lock and every other declared requirement.
    metadata['build']['linear_memory'] = {'maximum_bytes': 12 * 1024 * 1024, 'pinned': True}
    metadata_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n')
    prefix = output / 'pxa-glitch-garden-esp32p4'
    command = [args.pxa, 'build', str(project), '--offline', '--target', 'esp32p4', '--aot-only', '--output', str(prefix)]
    for value in args.define:
        command += ['--define', value]
    subprocess.run(command, check=True)
    package = prefix.with_suffix('.pxa')
    subprocess.run([args.pxa, 'publish', str(package), '--check-only'], check=True)
    result = {'profile': 'espp4-a', 'target': 'esp32p4', 'project': str(project),
              'version': metadata['version'], 'release_sequence': metadata['release_sequence'],
              'maximum_bytes': 12 * 1024 * 1024, 'pinned': True,
              'definitions': args.define, 'package': str(package),
              'sha256': hashlib.sha256(package.read_bytes()).hexdigest()}
    (output / 'profile-build.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
