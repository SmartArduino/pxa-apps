#!/usr/bin/env python3
"""Convert original PC TR1 PHD levels into PXA fixed-engine media."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from game_data import ROOT, catalog, fingerprint, inspect_pkd


def build_packer(build_dir):
    build_dir.mkdir(parents=True, exist_ok=True)
    target = build_dir / ('openlara-packer.exe' if os.name == 'nt' else 'openlara-packer')
    sources = [ROOT / 'tools/packer.cpp', *(ROOT / 'vendor/packer').glob('*.h')]
    if not target.exists() or any(p.stat().st_mtime > target.stat().st_mtime for p in sources):
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O2',
                        '-fno-strict-aliasing', '-fwrapv', '-Wno-narrowing',
                        '-I', str(ROOT / 'vendor/packer'), str(ROOT / 'tools/packer.cpp'),
                        '-o', str(target)], check=True)
    return target.resolve()


def convert(source, destination, build_dir, allow_partial=False):
    source, destination = source.resolve(), destination.resolve()
    if not source.is_dir():
        raise ValueError(f'game data directory does not exist: {source}')
    files = {}
    for path in source.rglob('*'):
        if path.is_file() and path.suffix.lower() == '.phd':
            name = path.stem.lower()
            if name in files and fingerprint(path) != fingerprint(files[name]):
                raise ValueError(f'conflicting copies of {path.name}; select a single DATA directory')
            files[name] = path
    entries = catalog()
    missing = [e['file'].replace('.pkd', '.PHD') for e in entries if Path(e['file']).stem not in files]
    if missing and not allow_partial:
        raise ValueError('missing original PC data: ' + ', '.join(missing) +
                         '\nNo media was changed. --allow-partial is for development only.')
    available = [e for e in entries if Path(e['file']).stem in files]
    if not available:
        raise ValueError('no recognized TR1 .PHD files found')
    packer = build_packer(build_dir)
    destination.parent.mkdir(parents=True, exist_ok=True)
    converted = []
    with tempfile.TemporaryDirectory(prefix='.openlara-import-', dir=destination.parent) as temporary:
        stage = Path(temporary)
        for entry in available:
            original = files[Path(entry['file']).stem]
            output = stage / entry['file']
            subprocess.run([str(packer), original.stem.upper(), str(original), str(output)], check=True)
            converted.append({**entry, **inspect_pkd(output), 'source': {
                'file': original.name, **fingerprint(original)}})
        manifest = {'format': 'openlara-pxa-data-1', 'upstream_commit':
                    '3b268ca4bf986c380f44a26ffd9218bf6d376249',
                    'edition': 'tr1-pc', 'campaign_data_complete': not missing,
                    'missing': missing, 'scope': 'game assets; separate from the BSD engine license',
                    'levels': converted}
        (stage / 'data-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        destination.mkdir(parents=True, exist_ok=True)
        # Conversion and validation finish before replacing any existing file.
        # The manifest is replaced last so an interrupted import is detectable.
        for entry in converted:
            os.replace(stage / entry['file'], destination / entry['file'])
        os.replace(stage / 'data-manifest.json', destination / 'data-manifest.json')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='original PC game root or DATA directory')
    parser.add_argument('--output', type=Path, default=ROOT / 'assets-simulator/game')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/packer')
    parser.add_argument('--allow-partial', action='store_true')
    args = parser.parse_args()
    try:
        manifest = convert(args.source, args.output, args.build_dir, args.allow_partial)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
