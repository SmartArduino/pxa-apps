#!/usr/bin/env python3
"""Build signed media from a pinned, unmodified, redistributable demo archive."""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import urllib.request
import zipfile


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def verify_archive(path, entry):
    archive = entry['archive']
    if path.stat().st_size != archive['bytes'] or digest(path) != archive['sha256']:
        raise ValueError('Demo archive does not match the reviewed original')


def relative_path(name):
    parts = name.split('/')
    if not name or '\\' in name or any(p in ('', '.', '..') for p in parts):
        raise ValueError('Unsafe archive member: ' + name)
    path = PurePosixPath(name)
    if path.is_absolute() or ':' in name:
        raise ValueError('Unsafe archive member: ' + name)
    return path


def prepare(entry, archive, destination):
    """Keep the entire original and copy runtime files without altering bytes."""
    verify_archive(archive, entry)
    if 'runtime_files' in entry:
        with zipfile.ZipFile(archive) as source:
            license_bytes = source.read(entry['license_member'])
            nested = None
            if 'nested_zip' in entry:
                member = source.getinfo(str(relative_path(entry['nested_zip'])))
                if member.file_size != entry['nested_zip_bytes']:
                    raise ValueError('Unexpected original installer size')
                nested = zipfile.ZipFile(io.BytesIO(source.read(member)))
            runtime = nested if nested is not None else source
            selected = []
            names = set()
            for original, target in entry['runtime_files'].items():
                member = runtime.getinfo(str(relative_path(original)))
                name = relative_path(target)
                if member.is_dir() or stat.S_ISLNK(member.external_attr >> 16) or str(name).casefold() in names:
                    raise ValueError('Ambiguous or symbolic runtime member: ' + original)
                expected = entry['required_files'][target]
                if member.file_size != expected['bytes']:
                    raise ValueError('Runtime file size differs: ' + target)
                names.add(str(name).casefold())
                selected.append((member, name))
            destination.mkdir(parents=True, exist_ok=False)
            original = destination / 'original' / relative_path(entry['archive']['name'])
            original.parent.mkdir(parents=True)
            shutil.copyfile(archive, original)
            licenses = destination / 'licenses'
            licenses.mkdir()
            (licenses / 'demo-license.txt').write_bytes(license_bytes)
            for member, name in selected:
                target = destination / name
                target.parent.mkdir(parents=True, exist_ok=True)
                with runtime.open(member) as data, target.open('wb') as output:
                    shutil.copyfileobj(data, output, 65536)
                expected = entry['required_files'][str(name)]
                if digest(target) != expected['sha256']:
                    raise ValueError('Runtime file differs from original: ' + str(name))
            if nested is not None:
                nested.close()
            if 'level_index' in entry:
                catalog = entry['level_index']
                (destination / 'levels.idx').write_bytes(struct.pack('<4sIII', b'OLCI', 1,
                    catalog['mask'], catalog['count']))
        (destination / 'ABOUT.txt').write_text(
            entry['title'] + '\n' + entry['copyright'] + '\n' + entry['license'] + '\n\n'
            'The complete original archive is under original/. All its files remain unaltered.\n'
            + entry.get('runtime_notice',
                'Runtime PHD and PCX files are byte-for-byte copies from that archive.\n'
                'levels.idx is an engine catalog, not a modified game asset.\n'
                'The PCX title is decoded by the engine at runtime.\n'
                'No GBA title, GBA music bank or retail campaign is included.\n') +
            'The original distribution notice accompanies the package under licenses/.\n'
            'These demo assets are separate from the open-source engine license.\n')
        return
    if 'runtime_file' in entry:
        name = relative_path(entry['runtime_file'])
        license_name = relative_path(entry['license_file'])
        license_path = Path(__file__).resolve().parents[1] / 'resources' / license_name
        if digest(license_path) != entry['license_sha256']:
            raise ValueError('Demo license does not match the reviewed notice')
        destination.mkdir(parents=True, exist_ok=False)
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(archive, target)
        licenses = destination / 'licenses'
        licenses.mkdir()
        shutil.copyfile(license_path, licenses / 'demo-license.txt')
        (destination / 'ABOUT.txt').write_text(
            entry['title'] + '\n' + entry['copyright'] + '\n' + entry['license'] + '\n\n'
            'The runtime IWAD is the complete, unmodified original shareware file.\n'
            'No retail data, PWAD, conversion or patch is included.\n'
            'The original terms and redistribution clarification accompany this package.\n'
            'Game data is not covered by the engine GPL license.\n'
            'This is episode one shareware, not the full retail game.\n'
            'Source: ' + entry['archive']['url'] + '\n'
            'License notice: ' + entry['license_source'] + '\n', encoding='utf-8')
        return
    prefix = entry['runtime_prefix']
    root = relative_path(entry['runtime_root'])
    with zipfile.ZipFile(archive) as source:
        selected = []
        names = set()
        for member in source.infolist():
            if member.is_dir() or not member.filename.startswith(prefix):
                continue
            name = root / relative_path(member.filename[len(prefix):])
            if stat.S_ISLNK(member.external_attr >> 16) or str(name).casefold() in names:
                raise ValueError('Ambiguous or symbolic archive member: ' + member.filename)
            names.add(str(name).casefold())
            selected.append((member, name))
        if not selected:
            raise ValueError('Demo has no runtime files')
        # Read the original agreement before creating a payload. Preserve CRLF.
        license_bytes = source.read(entry['license_member'])
        destination.mkdir(parents=True, exist_ok=False)
        original = destination / 'original' / relative_path(entry['archive']['name'])
        original.parent.mkdir(parents=True)
        shutil.copyfile(archive, original)
        licenses = destination / 'licenses'
        licenses.mkdir()
        (licenses / 'demo-license.txt').write_bytes(license_bytes)
        for member, name in selected:
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.open(member) as data, target.open('wb') as output:
                shutil.copyfileobj(data, output, 65536)
        for name, expected in entry['required_files'].items():
            target = destination / relative_path(name)
            if target.stat().st_size != expected['bytes'] or digest(target) != expected['sha256']:
                raise ValueError('Runtime file does not match the original: ' + name)
    (destination / 'ABOUT.txt').write_text(
        entry['title'] + '\n' + entry['copyright'] + '\n' + entry['license'] + '\n\n'
        'The complete, unmodified original demo is retained under original/.\n'
        'Runtime files are byte-for-byte copies from that demo.\n'
        'The original agreement accompanies this package under licenses/.\n'
        'This data is not covered by the engine GPL license.\n'
        'For free, non-commercial electronic distribution only; not for hardware bundles.\n'
        'This is a demo, not the full retail game.\n', encoding='utf-8')


def main():
    recipes = json.loads((Path(__file__).resolve().parents[1] / 'resources/demo-media.json').read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('application', choices=tuple(recipes['applications']))
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--output-root', type=Path, required=True)
    parser.add_argument('--archive', type=Path, help='reuse the exact original archive without downloading')
    parser.add_argument('--signing-key', type=Path, default=os.environ.get('PXA_SIGNING_KEY'))
    args = parser.parse_args()
    if args.signing_key is None:
        parser.error('--signing-key or PXA_SIGNING_KEY is required')
    entry = recipes['applications'][args.application]
    output = args.output_root.resolve() / args.application
    output.mkdir(parents=True, exist_ok=True)
    archive = args.archive
    if archive is None:
        archive = output / entry['archive']['name']
        if not archive.exists():
            temporary = archive.with_suffix('.part')
            try:
                with urllib.request.urlopen(entry['archive']['url'], timeout=45) as source, temporary.open('wb') as target:
                    remaining = entry['archive']['bytes']
                    while remaining:
                        block = source.read(min(65536, remaining))
                        if not block:
                            raise ValueError('Incomplete demo download')
                        target.write(block)
                        remaining -= len(block)
                    if source.read(1):
                        raise ValueError('Demo exceeds pinned archive size')
                verify_archive(temporary, entry)
                temporary.replace(archive)
            finally:
                temporary.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='payload-', dir=output) as temporary:
        stage = Path(temporary)
        payload = stage / 'package'
        prepare(entry, archive, payload)
        metadata = stage / 'package.json'
        metadata.write_text(json.dumps({'format': 'pxa-package-source-0.2', 'package_type': 'resources',
            'id': entry['id'], 'version': entry['version'], 'release_sequence': entry['release_sequence'],
            'name': entry['title'], 'min_sdk': [2, 1], 'target_sdk': [2, 1]}, indent=2) + '\n')
        subprocess.run([sys.executable, str(args.sdk_root / 'tools/package/build_resource_package.py'),
            str(metadata), str(payload), str(args.signing_key),
            str(output / 'resources.pxa'), '--descriptor', str(output / 'binding.json')], check=True)
    print((output / 'binding.json').read_text())


if __name__ == '__main__':
    main()
