"""External local staging and authoritative Host-selected media deployment."""
import re
import hashlib
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MEDIA = ROOT.parent / '.game-data/openlara/files'
sys.path.insert(0, str(ROOT.parent / 'common/recrof/tools'))
from stage_sd import digest, ensure_parent, installed_identity, package_paths


def external_destination(value):
    destination = Path(value).resolve()
    if destination.is_relative_to(ROOT):
        relative = destination.relative_to(ROOT)
        if relative.parts and relative.parts[0].startswith('assets'):
            raise ValueError('Game media must be outside package assets; use .game-data/openlara/files')
    return destination


def media_plan(directory, manifest, publisher=None):
    directory = external_destination(directory)
    publisher = publisher or digest(ROOT.parent / '.dev-signing/publisher-public.der')
    if not re.fullmatch(r'[0-9a-f]{64}', publisher):
        raise ValueError('Publisher ID must be a lowercase SHA-256 key ID')
    rows = []
    seen = set()
    for entry in manifest['files']:
        name = entry['file']
        if not re.fullmatch(r'[A-Za-z0-9_.-]+', name) or name in ('.', '..') or name in seen:
            raise ValueError('Invalid or duplicate media filename')
        seen.add(name)
        source = directory / name
        if source.is_symlink() or not source.is_file():
            raise ValueError('Missing or linked media file: ' + name)
        if source.stat().st_size != entry['bytes'] or digest(source) != entry['sha256']:
            raise ValueError('Prepared media changed: ' + name)
        rows.append({'path': name, 'source': str(source), 'bytes': entry['bytes'], 'sha256': entry['sha256']})
    manifest_path = directory / 'data-manifest.json'
    if manifest_path.is_file():
        if manifest_path.is_symlink():
            raise ValueError('Linked data manifest')
        rows.append({'path': manifest_path.name, 'source': str(manifest_path),
                     'bytes': manifest_path.stat().st_size, 'sha256': digest(manifest_path)})
    # Publish the catalog only after all referenced media has been transferred.
    rows.sort(key=lambda row: row['path'] == 'levels.idx')
    return {'app_id': 'pxa-openlara', 'publisher_key_id': publisher,
            'media_root': 'host-resolved at deployment', 'files': rows}


def deploy_media(pxadb, port, serial, directory, manifest, publisher=None):
    layout = media_plan(directory, manifest, publisher)
    with pxadb.open_client(port, 15) as client:
        if pxadb.info_properties(client.device_info).get('serial') != serial:
            raise ValueError('Device serial differs from --serial')
        identity = installed_identity(client, layout)
        paths = package_paths(client, identity)
        if 'media' not in paths:
            raise ValueError('Selected media storage is unavailable')
        destination = paths['media']
        fs = pxadb.NormalFsClient(client)
        # Migration only: locate this unchanged identity's historical AppKey
        # directory on the Host-selected volume. New installs never use it.
        media_base, separator, _ = destination.rpartition('/PXA/media/')
        if not separator:
            raise ValueError('Host media layout cannot be migrated safely')
        app = layout['app_id'].encode('ascii')
        legacy_key = hashlib.sha256(b'PXA-APPKEY-1' + bytes.fromhex(layout['publisher_key_id']) +
                                   struct.pack('<H', len(app)) + app).hexdigest()
        ensure_parent(fs, destination + '/levels.idx')
        base = media_base + '/PXA/media'
        legacy = base + '/' + legacy_key
        old_files = {}
        if any(kind == 'D' and name == legacy_key for kind, _, name in fs.list(base)):
            old_files = {name: size for kind, size, name in fs.list(legacy) if kind == 'F'}
        existing = {name for _, _, name in fs.list(destination)}
        for row in layout['files']:
            source = Path(row['source'])
            if digest(source) != row['sha256']:
                raise ValueError('Media changed after planning: ' + row['path'])
            name = row['path']
            remote = destination + '/' + name
            expected = (row['sha256'], row['bytes'])
            if name not in existing and old_files.get(name) == row['bytes'] and \
                    fs._expected_digest(legacy + '/' + name, row['bytes']) == expected:
                # Reuse byte-identical media with a same-volume rename, avoiding
                # megabytes of serial upload. Different old files remain intact.
                fs.move(legacy + '/' + name, remote)
                if fs._expected_digest(remote, row['bytes']) != expected:
                    raise ValueError('Migrated media verification failed: ' + name)
            else:
                fs.put(source, remote)
        return destination
