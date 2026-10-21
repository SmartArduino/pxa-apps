#!/usr/bin/env python3
"""Plan or deploy imported resources to the identity's fixed SD media root.

No remote-directory option: the target is derived from the public signing
identity and the installed application ID. The Host resolves the selected volume
and its authoritative directory binding; no mount name is guessed.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from import_data import APPS, collect


def digest(file):
    with file.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def plan(apps_root, game, publisher_id):
    manifest = json.loads((apps_root / game / 'package.json').read_text())
    identity = manifest['id']
    if not re.fullmatch(r'[0-9a-f]{64}', publisher_id):
        raise ValueError('Expected a lowercase SHA-256 publisher key ID')
    if identity != 'pxa-' + game:
        raise ValueError('Canonical game manifest has an unexpected application ID')
    files = collect(game, apps_root / '.game-data' / game / 'files')
    return {'app_id': identity, 'publisher_key_id': publisher_id,
            'media_root': 'host-resolved at deployment',
            'files': [{'path': name, 'source': str(source),
                       'bytes': source.stat().st_size, 'sha256': digest(source)}
                      for name, source in sorted(files.items())]}


def ensure_parent(fs, remote):
    parent = ''
    for segment in remote.split('/')[:-1]:
        entries = {name: kind for kind, _size, name in fs.list(parent)}
        if segment in entries:
            if entries[segment] != 'D':
                raise ValueError('Resource parent is a file: ' + segment)
        else:
            if not parent:
                raise ValueError('SD storage is unavailable; refusing an internal-folder fallback')
            fs.mkdir(parent + '/' + segment)
        parent = parent + '/' + segment if parent else segment


def package_paths(client, identity):
    paths = {}
    for frame in client.request('PACKAGE paths ' + identity):
        if frame.kind == 'PATH':
            kind, path = frame.payload.split('\t', 1)
            if path.startswith('/') or any(p in ('', '.', '..') for p in path.split('/')):
                raise ValueError('Host returned an invalid storage path')
            paths[kind] = path
    if 'private' not in paths:
        raise ValueError('Firmware does not support authoritative package storage paths')
    return paths


def installed_identity(client, layout):
    expected = layout['publisher_key_id'] + ':' + layout['app_id']
    found = False
    for frame in client.request('PACKAGES'):
        if frame.kind != 'PKG':
            continue
        identity, _name, _version, flags = frame.payload.split('\t', 3)
        if identity.endswith(':' + layout['app_id']):
            if identity != expected:
                raise ValueError('Installed application has a different publisher')
            found = 'installed=1' in flags
        if 'active=1' in flags:
            raise ValueError('Stop the active application before transferring resources')
    if not found:
        raise ValueError('Install the canonical signed engine package first')
    return expected


def deploy(pxadb, port, serial, layout):
    with pxadb.open_client(port, 15) as client:
        properties = pxadb.info_properties(client.device_info)
        if properties.get('serial') != serial:
            raise ValueError('Device serial differs from --expected-serial')
        identity = installed_identity(client, layout)
        paths = package_paths(client, identity)
        if 'media' not in paths:
            raise ValueError('SD storage is unavailable; refusing an internal-folder fallback')
        layout['media_root'] = paths['media']
        fs = pxadb.NormalFsClient(client)
        for row in layout['files']:
            source = Path(row['source'])
            if digest(source) != row['sha256']:
                raise ValueError('Resource changed after planning: ' + str(source))
            remote = paths['media'] + '/' + row['path']
            ensure_parent(fs, remote)
            fs.put(source, remote)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', choices=APPS)
    parser.add_argument('--apps-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--publisher-key', type=Path, help='Publisher root public key in DER (the signing key without key rotation)')
    parser.add_argument('--deploy', action='store_true', help='Transfer resources; default only prints the plan')
    parser.add_argument('--port')
    parser.add_argument('--expected-serial')
    parser.add_argument('--pxadb-dir', type=Path, default=Path(__file__).resolve().parents[5] / 'tools/pxadb')
    args = parser.parse_args()
    try:
        public_key = args.publisher_key or args.apps_root / '.dev-signing/publisher-public.der'
        layout = plan(args.apps_root, args.game, digest(public_key))
        if args.deploy:
            if not args.port or not args.expected_serial:
                parser.error('--deploy requires --port and --expected-serial')
            sys.path.insert(0, str(args.pxadb_dir.resolve()))
            import pxadb
            try:
                deploy(pxadb, args.port, args.expected_serial, layout)
            except pxadb.PxaDbError as error:
                parser.exit(1, f'{error}\n')
        print(json.dumps(dict(layout, transfer_verified=bool(args.deploy)), indent=2))
    except (OSError, ValueError, KeyError, ImportError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
