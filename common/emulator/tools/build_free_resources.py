#!/usr/bin/env python3
"""Build signed media from reviewed, hash-pinned free-game recipes."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import urllib.request


def fetch(item, cache):
    path = cache / item['sha256']
    if not path.exists():
        temporary = path.with_suffix('.part')
        try:
            with urllib.request.urlopen(item['url'], timeout=60) as source, temporary.open('wb') as target:
                remaining = item['bytes']
                while remaining:
                    data = source.read(min(65536, remaining))
                    if not data:
                        raise ValueError('Incomplete resource: ' + item['url'])
                    target.write(data)
                    remaining -= len(data)
                if source.read(1):
                    raise ValueError('Resource exceeds pinned size: ' + item['url'])
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != item['sha256']:
                raise ValueError('Resource digest changed: ' + item['url'])
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)
    if path.stat().st_size != item['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
        raise ValueError('Invalid cached resource: ' + str(path))
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('application')
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--signing-key', type=Path, default=os.environ.get('PXA_SIGNING_KEY'))
    args = parser.parse_args()
    if args.signing_key is None:
        parser.error('--signing-key or PXA_SIGNING_KEY is required')
    recipes = json.loads((Path(__file__).resolve().parents[1] / 'resources/free-games.json').read_text())
    entry = recipes['applications'][args.application]
    output = args.output_root.resolve()
    cache = output / 'cache'
    cache.mkdir(parents=True, exist_ok=True)
    package = output / args.application / 'package'
    # A fresh payload prevents removed games from leaking into later generations.
    if package.exists():
        shutil.rmtree(package)
    package.mkdir(parents=True)
    for item in entry['files']:
        name = PurePosixPath(item['path'])
        if name.is_absolute() or '..' in name.parts or not name.parts:
            raise ValueError('Unsafe resource path')
        target = package / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(fetch(item, cache), target)
    (package / 'ABOUT.txt').write_text(
        f"{entry['title']}\n{entry['author']}\nLicense: {entry['license']}\n{entry['project_url']}\n\n"
        'Free homebrew game. This is not an official Nintendo release.\n'
        'Original notices and complete source are included under licenses/ and sources/.\n', encoding='utf-8')
    metadata = output / args.application / 'package.json'
    metadata.write_text(json.dumps({'format': 'pxa-package-source-0.2', 'package_type': 'resources',
        'id': entry['id'], 'version': entry['version'], 'release_sequence': entry['release_sequence'],
        'name': entry['title'], 'min_sdk': [2, 1], 'target_sdk': [2, 1]}, indent=2) + '\n')
    subprocess.run([sys.executable, str(args.sdk_root / 'tools/package/build_resource_package.py'),
        str(metadata), str(package), str(args.signing_key),
        str(output / f"pxa-{args.application}-resources.pxa"),
        '--descriptor', str(output / args.application / 'binding.json')], check=True)
    print((output / args.application / 'binding.json').read_text())


if __name__ == '__main__':
    main()
