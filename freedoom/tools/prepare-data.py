#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare unchanged Freedoom IWADs and notices outside the application package.

Downloads are pinned by SHA-256. --source-wads imports make's wads/ output from
an upstream checkout; these builds may differ from the official release.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

APP = Path(__file__).resolve().parents[1]
APPS = APP.parent
RECIPE = APP / 'upstream.json'
MAX_WAD = 64 * 1024 * 1024
MAX_ARCHIVE = 64 * 1024 * 1024
NOTICES = ('COPYING.txt', 'CREDITS.txt', 'CREDITS-MUSIC.txt')


def atomic_write(path, data):
    path = Path(path)
    if path.is_symlink():
        raise ValueError('Refusing linked output: ' + str(path))
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(data)
            stream.close()
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)


def validate_wad(data, phase):
    # Use the same structural checks as the sibling engine's data importer.
    spec = importlib.util.spec_from_file_location('doom_data', APPS / 'doom/tools/prepare-data.py')
    doom = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(doom)
    metadata = doom.validate_wad(data, max_bytes=MAX_WAD)
    import struct
    count, directory = struct.unpack_from('<II', data, 4)
    names = {data[directory + i * 16 + 8:directory + i * 16 + 16].rstrip(b'\0') for i in range(count)}
    expected = b'E1M1' if phase == 1 else b'MAP01'
    if expected not in names or b'FREEDOOM' not in names or count > 65536:
        raise ValueError('Not a supported Freedoom Phase ' + str(phase) + ' IWAD')
    return metadata


def verified_bytes(path, entry, limit):
    path = Path(path)
    if path.stat().st_size != entry['bytes'] or path.stat().st_size > limit:
        raise ValueError('Pinned file size mismatch: ' + str(path))
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise ValueError('Pinned SHA-256 mismatch: ' + str(path))
    return data


def prepare(output, phases=(2,), archive=None, source_wads=None):
    recipe = json.loads(RECIPE.read_text())
    output = Path(output).resolve()
    if output == APP or APP in output.parents:
        raise ValueError('Media and generated data must stay outside freedoom/')
    if archive and source_wads:
        raise ValueError('--archive and --source-wads are mutually exclusive')
    output.mkdir(parents=True, exist_ok=True)
    payload = {}
    if source_wads:
        upstream = APP / 'upstream'
        commit = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
        # Refuse modified upstream source; builds only add ignored generated files.
        if subprocess.check_output(['git', '-C', str(upstream), 'status', '--porcelain', '--untracked-files=no']):
            raise ValueError('Upstream has tracked changes; restore it before preparing media')
        for phase in phases:
            name = f'freedoom{phase}.wad'
            path = Path(source_wads) / name
            if path.stat().st_size > MAX_WAD:
                raise ValueError('IWAD exceeds 64 MiB')
            payload[name] = path.read_bytes()
        for original, target in (('COPYING.adoc', 'COPYING.txt'), ('CREDITS', 'CREDITS.txt'),
                                 ('CREDITS-MUSIC', 'CREDITS-MUSIC.txt')):
            payload['licenses/' + target] = (upstream / original).read_bytes()
        origin = str(Path(source_wads).resolve())
    else:
        commit = recipe['commit']
        entry = recipe['archive']
        if archive is None:
            archive = output.parent / entry['name']
            if not archive.exists():
                with urllib.request.urlopen(entry['url'], timeout=60) as response:
                    data = response.read(entry['bytes'] + 1)
                if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
                    raise ValueError('Downloaded archive differs from pinned release')
                atomic_write(archive, data)
        verified_bytes(archive, entry, MAX_ARCHIVE)
        with zipfile.ZipFile(archive) as source:
            prefix = 'freedoom-' + recipe['version'] + '/'
            for name in [*(f'freedoom{p}.wad' for p in phases), *NOTICES]:
                member = source.getinfo(prefix + name)
                expected = recipe['members'][name]
                if member.file_size != expected['bytes']:
                    raise ValueError('Unexpected archive member size: ' + name)
                data = source.read(member)
                if hashlib.sha256(data).hexdigest() != expected['sha256']:
                    raise ValueError('Archive member SHA-256 mismatch: ' + name)
                payload[name if name.endswith('.wad') else 'licenses/' + name] = data
        origin = entry['url']
    records = []
    for phase in phases:
        name = f'freedoom{phase}.wad'
        records.append(dict(file=name, **validate_wad(payload[name], phase)))
    # Prevent silent precedence changes: explicitly prepare both to install both.
    for phase in (1, 2):
        if phase not in phases and (output / f'freedoom{phase}.wad').exists():
            raise ValueError('Output contains the other phase; choose a fresh --output directory or --phase both')
    for name, data in payload.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        atomic_write(target, data)
    manifest = dict(schema_version=1, app_id='pxa-freedoom', upstream_commit=commit,
                    source=origin, modified=False, files=records)
    atomic_write(output / 'data-manifest.json', (json.dumps(manifest, indent=2) + '\n').encode())
    return manifest


def resource_package(media, output, sdk, signing_key, bind=False):
    output = Path(output).resolve()
    if output == APP or APP in output.parents:
        raise ValueError('Resource packages must stay outside freedoom/')
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        payload = Path(temporary) / 'payload'
        shutil.copytree(media, payload)
        metadata = Path(temporary) / 'package.json'
        recipe = json.loads(RECIPE.read_text())
        metadata.write_text(json.dumps(dict(format='pxa-package-source-0.2', package_type='resources',
            id='pxa-freedoom', version=recipe['version'], release_sequence=1, name='Freedoom game data',
            min_sdk=[2, 1], target_sdk=[2, 1])))
        subprocess.run([sys.executable, str(Path(sdk) / 'tools/package/build_resource_package.py'),
            str(metadata), str(payload), str(signing_key), str(output / 'resources.pxa'),
            '--descriptor', str(output / 'binding.json')], check=True)
    binding = json.loads((output / 'binding.json').read_text())
    if bind:
        path = APP / 'package.json'
        package = json.loads(path.read_text())
        package['resources'] = binding['resources']
        atomic_write(path, (json.dumps(package, indent=2) + '\n').encode())
    return binding


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('1', '2', 'both'), default='2')
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--source-wads', type=Path)
    parser.add_argument('--output', type=Path, default=APPS / '.game-data/freedoom/files')
    parser.add_argument('--resource-output', type=Path)
    parser.add_argument('--sdk-root', type=Path)
    parser.add_argument('--signing-key', type=Path)
    parser.add_argument('--bind-package', action='store_true')
    args = parser.parse_args()
    if (args.resource_output and not (args.sdk_root and args.signing_key)) or (args.bind_package and not args.resource_output):
        parser.error('--resource-output requires --sdk-root and --signing-key; --bind-package requires --resource-output')
    try:
        phases = (1, 2) if args.phase == 'both' else (int(args.phase),)
        result = prepare(args.output, phases, args.archive, args.source_wads)
        if args.resource_output:
            result['binding'] = resource_package(args.output, args.resource_output, args.sdk_root,
                                                  args.signing_key, args.bind_package)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
