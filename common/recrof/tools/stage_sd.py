#!/usr/bin/env python3
"""Plan or deploy imported resources to the identity's fixed SD media root.

No remote-directory option: the target is derived from the public signing
identity and the SD package's application ID, using the Host's AppKey contract.
"""
import argparse
import hashlib
import json
import re
import struct
import sys
import time
from pathlib import Path
from import_data import APPS, collect


def app_key(publisher_id, app_id):
    if not re.fullmatch(r'[0-9a-f]{64}', publisher_id):
        raise ValueError('Expected a lowercase SHA-256 publisher key ID')
    if not re.fullmatch(r'[a-z0-9][a-z0-9._-]{0,63}', app_id):
        raise ValueError('Invalid application ID')
    name = app_id.encode('ascii')
    return hashlib.sha256(b'PXA-APPKEY-1' + bytes.fromhex(publisher_id) +
                          struct.pack('<H', len(name)) + name).hexdigest()


def digest(file):
    with file.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def plan(apps_root, game, publisher_id):
    manifest = json.loads((apps_root / (game + '-sd') / 'package.json').read_text())
    identity = manifest['id']
    key = app_key(publisher_id, identity)
    files = collect(game, apps_root / game / 'assets/game')
    return {'app_id': identity, 'publisher_key_id': publisher_id, 'app_key': key,
            'sd_root': 'sdcard/PXA/media/' + key,
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


def deploy(pxadb, port, serial, layout):
    with pxadb.open_client(port, 15) as client:
        properties = pxadb.info_properties(client.device_info)
        if properties.get('serial') != serial:
            raise ValueError('Device serial differs from --expected-serial')
        active = False
        for frame in client.request('PACKAGES'):
            if frame.kind != 'PKG':
                continue
            identity = frame.payload.split('\t', 1)[0]
            publisher, separator, app = identity.partition(':')
            if separator and app == layout['app_id']:
                if publisher != layout['publisher_key_id']:
                    raise ValueError('Installed application has a different publisher')
                active = 'active=1' in frame.payload
        try:
            if active:
                client.request('PACKAGE stop ' + layout['app_id'])
                time.sleep(2)
            fs = pxadb.NormalFsClient(client)
            for row in layout['files']:
                source = Path(row['source'])
                if digest(source) != row['sha256']:
                    raise ValueError('Resource changed after planning: ' + str(source))
                remote = layout['sd_root'] + '/' + row['path']
                ensure_parent(fs, remote)
                fs.put(source, remote)
        finally:
            if active:
                client.request('INPUT SWIPE 160 205 160 30 300 12')
                client.request('PACKAGE run ' + layout['app_id'])


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
