#!/usr/bin/env python3
"""Migrate a retired -sd identity to its canonical game, with verified save backup.

Default is read-only planning. --apply requires a local --backup directory.
The Host chooses paths; exact publisher identity and device serial are mandatory.
Install the new signed engine first. No package is archived or uninstalled here.
"""
import argparse
import json
import sys
from pathlib import Path
from import_data import APPS
from stage_sd import digest, ensure_parent, installed_identity, package_paths


def inventory(fs, root):
    result = []
    for kind, size, name in fs.list(root):
        if name in ('.', '..') or '/' in name or '\\' in name:
            raise ValueError('Unsafe remote filename')
        if kind == 'D':
            for row in inventory(fs, root + '/' + name):
                result.append(dict(row, path=name + '/' + row['path']))
        elif kind == 'F':
            checksum, actual = fs._expected_digest(root + '/' + name, size)
            if checksum is None or actual != size:
                raise ValueError('Firmware must support SHA-256 file verification')
            result.append({'path': name, 'bytes': size, 'sha256': checksum})
        else:
            raise ValueError('Unsupported remote file kind')
    return sorted(result, key=lambda row: row['path'])


def child_exists(fs, root, name):
    return any(item[2] == name for item in fs.list(root))


def verify(fs, root, rows):
    if inventory(fs, root) != rows:
        raise ValueError('Contents changed or verification failed: ' + root)


def save_journal(path, record):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(record, indent=2) + '\n')
    temp.replace(path)


def migrate(client, pxadb, game, publisher, serial, backup=None, apply=False):
    if pxadb.info_properties(client.device_info).get('serial') != serial:
        raise ValueError('Device serial differs from --expected-serial')
    app = 'pxa-' + game
    target = installed_identity(client, {'publisher_key_id': publisher, 'app_id': app})
    source = target + '-sd'
    old = package_paths(client, source)
    new = package_paths(client, target)
    if 'media' not in old or 'media' not in new:
        raise ValueError('Selected SD media volume is unavailable')
    fs = pxadb.NormalFsClient(client)
    # A persisted plan also supports retry after data/ was moved successfully.
    journal = backup / 'migration.json' if backup else None
    if journal and journal.exists():
        record = json.loads(journal.read_text())
        for key, value in [('serial', serial), ('source', source), ('target', target), ('old', old), ('new', new)]:
            if record[key] != value:
                raise ValueError('Backup belongs to a different device, identity or storage binding')
    else:
        private = old['private'] + '/data'
        saves = inventory(fs, private) if child_exists(fs, old['private'], 'data') else None
        record = {'serial': serial, 'source': source, 'target': target, 'old': old, 'new': new,
                  'saves': saves, 'media': inventory(fs, old['media']), 'complete': False}
    if not apply:
        return record
    if backup is None:
        raise ValueError('--apply requires --backup')
    backup.mkdir(parents=True, exist_ok=True)
    save_journal(journal, record)
    # Back up every save, verifying local content before any remote mutation.
    for row in record['saves'] or []:
        local = backup / 'saves' / row['path']
        if local.exists():
            if digest(local) != row['sha256']:
                raise ValueError('Existing backup differs: ' + str(local))
        else:
            local.parent.mkdir(parents=True, exist_ok=True)
            fs.get(old['private'] + '/data/' + row['path'], local)
            if digest(local) != row['sha256']:
                raise ValueError('Save changed during backup: ' + row['path'])
    # Preflight every conflict before moving any media. Identical destinations
    # are accepted on retry; different files are never overwritten.
    remaining = []
    for row in record['media']:
        remote = new['media'] + '/' + row['path']
        ensure_parent(fs, remote)
        parent, _, name = remote.rpartition('/')
        if child_exists(fs, parent, name):
            checksum, size = fs._expected_digest(remote, row['bytes'])
            if (checksum, size) != (row['sha256'], row['bytes']):
                raise ValueError('Different destination already exists: ' + remote)
        else:
            checksum, size = fs._expected_digest(old['media'] + '/' + row['path'], row['bytes'])
            if (checksum, size) != (row['sha256'], row['bytes']):
                raise ValueError('Source changed: ' + row['path'])
            remaining.append(row)
    if record['saves'] is not None:
        # The owner rechecks idle state and publisher, handles a durable receipt,
        # and refuses any destination created since this plan was captured.
        client.request('PACKAGE move-data ' + source + ' ' + target, timeout=15)
        verify(fs, new['private'] + '/data', record['saves'])
    for row in remaining:
        fs.move(old['media'] + '/' + row['path'], new['media'] + '/' + row['path'])
        checksum, size = fs._expected_digest(new['media'] + '/' + row['path'], row['bytes'])
        if (checksum, size) != (row['sha256'], row['bytes']):
            raise ValueError('Moved resource verification failed: ' + row['path'])
    record['complete'] = True
    save_journal(journal, record)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', choices=APPS)
    parser.add_argument('--port', required=True)
    parser.add_argument('--expected-serial', required=True)
    parser.add_argument('--apply', action='store_true')
    parser.add_argument('--backup', type=Path)
    parser.add_argument('--apps-root', type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument('--publisher-key', type=Path)
    parser.add_argument('--pxadb-dir', type=Path, default=Path(__file__).resolve().parents[5] / 'tools/pxadb')
    args = parser.parse_args()
    if args.apply and not args.backup:
        parser.error('--apply requires --backup')
    sys.path.insert(0, str(args.pxadb_dir.resolve()))
    import pxadb
    try:
        publisher = digest(args.publisher_key or args.apps_root / '.dev-signing/publisher-public.der')
        with pxadb.open_client(args.port, 15) as client:
            record = migrate(client, pxadb, args.game, publisher, args.expected_serial, args.backup, args.apply)
        print(json.dumps(record, indent=2))
    except (OSError, ValueError, KeyError, pxadb.PxaDbError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
