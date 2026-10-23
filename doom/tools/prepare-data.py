#!/usr/bin/env python3
"""Prepare an external DOOM IWAD; optionally upload it to installed pxa-doom media.

The default download is pinned shareware. Supply your own legally obtained PC
IWAD with --wad. Game data stays outside the engine package and its license.
Only the selected WAD is uploaded; saves and the local manifest are not uploaded.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import urllib.request

APP_ROOT = Path(__file__).resolve().parents[1]
APPS_ROOT = APP_ROOT.parent
DEFAULT_OUTPUT = APPS_ROOT / '.game-data/doom/files'
PXADB_DIR = APP_ROOT.parents[2] / 'tools/pxadb'
COMMIT = '355cfbd16fac119718879239336ee2ea408886bd'
SHAREWARE_URL = f'https://raw.githubusercontent.com/Daivuk/PureDOOM/{COMMIT}/doom1.wad'
SHAREWARE_SHA256 = '1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771'
SHAREWARE_BYTES = 4196020
MAX_WAD_BYTES = 20 * 1024 * 1024
# Prefer a full game when recovering a directory without a usable manifest.
WAD_NAMES = ('doom2.wad', 'plutonia.wad', 'tnt.wad', 'doomu.wad', 'doom.wad', 'doom1.wad')
MANIFEST_NAME = 'data-manifest.json'


def validate_wad(data):
    """Check bounded PC IWAD structure, returning metadata for the exact bytes."""
    size = len(data)
    if not 12 <= size <= MAX_WAD_BYTES:
        raise ValueError('IWAD must be 12 bytes to 20 MiB')
    magic, count, directory = struct.unpack_from('<4sii', data)
    if magic != b'IWAD':
        raise ValueError('Expected an IWAD header (PWAD add-ons are unsupported)')
    if count <= 0 or directory < 12 or directory > size or count > (size - directory) // 16:
        raise ValueError('IWAD directory is outside the file')
    names = set()
    for index in range(count):
        offset, length, name = struct.unpack_from('<ii8s', data, directory + index * 16)
        # Zero-length map/section markers may legitimately have offset zero.
        if offset < 0 or length < 0 or offset > size or length > size - offset:
            raise ValueError(f'IWAD lump {index} is outside the file')
        if length and offset < 12:
            raise ValueError(f'IWAD lump {index} overlaps the header')
        names.add(name.split(b'\0', 1)[0])
    missing = {b'PLAYPAL', b'COLORMAP', b'TITLEPIC'} - names
    if missing:
        raise ValueError('IWAD missing required lumps: ' + ', '.join(sorted(n.decode() for n in missing)))
    if not ({b'E1M1', b'MAP01'} & names):
        raise ValueError('IWAD must contain E1M1 or MAP01')
    return {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': size, 'lumps': count}


def read_wad(path):
    path = Path(path)
    if not path.is_file():
        raise ValueError('Not a regular WAD file: ' + str(path))
    if path.stat().st_size > MAX_WAD_BYTES:
        raise ValueError('IWAD exceeds 20 MiB: ' + str(path))
    with path.open('rb') as source:
        data = source.read(MAX_WAD_BYTES + 1)
    return data, validate_wad(data)


def external_output(path):
    directory = Path(path).resolve()
    if directory == APP_ROOT or APP_ROOT in directory.parents:
        raise ValueError('Game WADs must stay outside the engine package; use ' + str(DEFAULT_OUTPUT))
    return directory


def atomic_write(path, data):
    """Replace one file on its own filesystem, cleaning up on failed writes."""
    path = Path(path)
    if path.is_symlink():
        raise ValueError('Refusing a linked output file: ' + str(path))
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.' + path.name + '.',
                                         suffix='.tmp', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


def previous_entries(directory):
    path = directory / MANIFEST_NAME
    if path.is_symlink():
        raise ValueError('Refusing a linked data manifest')
    if not path.exists():
        return []
    try:
        if path.stat().st_size > 65536:
            return []
        manifest = json.loads(path.read_text(encoding='utf-8'))
        rows = manifest.get('files', [])
        return [row for row in rows if isinstance(row, dict)] if isinstance(rows, list) else []
    except (ValueError, UnicodeError, AttributeError):
        return []


def existing_wad(directory, previous):
    candidates = [row.get('file') for row in previous if row.get('file') in WAD_NAMES]
    errors = []
    for name in dict.fromkeys(candidates + list(WAD_NAMES)):
        path = directory / name
        if path.is_symlink():
            raise ValueError('Refusing a linked output WAD: ' + name)
        if not path.exists():
            continue
        try:
            data, metadata = read_wad(path)
        except (OSError, ValueError) as error:
            errors.append(str(error))
            continue
        source = str(path)
        for row in previous:
            if (row.get('file') == name and row.get('sha256') == metadata['sha256']
                    and isinstance(row.get('source'), str)):
                source = row['source']
                break
        return name, data, metadata, source
    if errors:
        raise ValueError('Existing WAD is invalid; use --wad to replace it: ' + '; '.join(errors))
    return None


def prepare(output=DEFAULT_OUTPUT, wad=None, upstream=None):
    """Select one WAD without deleting other imports; default preserves valid data."""
    if wad is not None and upstream is not None:
        raise ValueError('--wad and --upstream are mutually exclusive')
    directory = external_output(output)
    previous = previous_entries(directory)
    selected = existing_wad(directory, previous) if wad is None else None
    if selected is not None:
        name, data, metadata, source = selected
    elif wad is not None:
        wad = Path(wad)
        name = wad.name.lower()
        if name not in WAD_NAMES:
            raise ValueError('Unsupported WAD filename; expected: ' + ', '.join(WAD_NAMES))
        wad = wad.resolve()
        data, metadata = read_wad(wad)
        source = str(wad)
    else:
        name = 'doom1.wad'
        if upstream is not None:
            location = Path(upstream).resolve()
            if location.is_dir():
                location /= name
            data, metadata = read_wad(location)
            source = str(location)
        else:
            with urllib.request.urlopen(SHAREWARE_URL, timeout=60) as response:
                data = response.read(MAX_WAD_BYTES + 1)
            metadata = validate_wad(data)
            source = SHAREWARE_URL
        if metadata['sha256'] != SHAREWARE_SHA256 or metadata['bytes'] != SHAREWARE_BYTES:
            raise ValueError('Pinned shareware SHA-256/size mismatch')
    manifest = {
        'schema_version': 1,
        'app_id': 'pxa-doom',
        'upstream_commit': COMMIT,
        'scope': 'External game data; not part of the engine package or engine license',
        'files': [dict(file=name, source=source, **metadata)],
    }
    directory.mkdir(parents=True, exist_ok=True)
    destination = directory / name
    if destination.is_symlink():
        raise ValueError('Refusing a linked output WAD: ' + name)
    identical = False
    if destination.is_file() and destination.stat().st_size == len(data):
        with destination.open('rb') as current:
            identical = current.read(MAX_WAD_BYTES + 1) == data
    if not identical:
        atomic_write(destination, data)
    # A failure here leaves a complete validated WAD; the next run repairs metadata.
    atomic_write(directory / MANIFEST_NAME,
                 (json.dumps(manifest, indent=2, ensure_ascii=False) + '\n').encode('utf-8'))
    return manifest


def publisher_id(value=None):
    if value is None:
        # Only the public DER key is read. No signing/private key is needed.
        value = hashlib.sha256((APPS_ROOT / '.dev-signing/publisher-public.der').read_bytes()).hexdigest()
    if not re.fullmatch(r'[0-9a-f]{64}', value):
        raise ValueError('Publisher ID must be a lowercase SHA-256 key ID')
    return value


def installed_identity(pxadb, client, publisher):
    expected = publisher + ':pxa-doom'
    installed = False
    for frame in client.request('PACKAGES'):
        if frame.kind != 'PKG':
            continue
        identity, _name, _version, flags = frame.payload.split('\t', 3)
        properties = pxadb.info_properties(flags)
        if properties.get('active') == '1':
            raise ValueError('Stop the active application before transferring WAD data')
        if identity.endswith(':pxa-doom'):
            if identity != expected:
                raise ValueError('Installed pxa-doom has a different publisher')
            installed = properties.get('installed') == '1'
    if not installed:
        raise ValueError('Install the signed pxa-doom engine package first')
    return expected


def media_path(client, identity):
    paths = {}
    for frame in client.request('PACKAGE paths ' + identity):
        if frame.kind != 'PATH':
            continue
        kind, path = frame.payload.split('\t', 1)
        if (not path or '\\' in path or any(ord(char) < 32 for char in path)
                or any(part in ('', '.', '..') for part in path.split('/'))
                or kind in paths):
            raise ValueError('Host returned an invalid or duplicate storage path')
        paths[kind] = path
    if 'media' not in paths:
        raise ValueError('Selected media storage is unavailable; no fallback is allowed')
    return paths['media']


def ensure_parent(fs, remote):
    parent = ''
    for segment in remote.split('/')[:-1]:
        entries = {name: kind for kind, _size, name in fs.list(parent)}
        if segment in entries:
            if entries[segment] != 'D':
                raise ValueError('Resource parent is a file: ' + segment)
        elif not parent:
            raise ValueError('Selected storage volume is unavailable')
        else:
            fs.mkdir(parent + '/' + segment)
        parent = parent + '/' + segment if parent else segment


def deploy_media(pxadb, port, serial, output, manifest, publisher=None):
    """Check serial and installed publisher before resolving or writing media."""
    if not port or not serial:
        raise ValueError('Deployment requires explicit --port and --serial')
    publisher = publisher_id(publisher)
    directory = external_output(output)
    rows = manifest.get('files', [])
    if len(rows) != 1 or rows[0].get('file') not in WAD_NAMES:
        raise ValueError('Expected one selected WAD in the data manifest')
    row = rows[0]
    source = directory / row['file']
    if source.is_symlink():
        raise ValueError('Refusing a linked output WAD')
    _data, metadata = read_wad(source)
    if any(row.get(key) != value for key, value in metadata.items()):
        raise ValueError('Prepared WAD changed after validation')
    with pxadb.open_client(port, 15) as client:
        if pxadb.info_properties(client.device_info).get('serial') != serial:
            raise ValueError('Device serial differs from --serial')
        identity = installed_identity(pxadb, client, publisher)
        destination = media_path(client, identity)
        fs = pxadb.NormalFsClient(client)
        remote = destination + '/' + row['file']
        ensure_parent(fs, remote)
        _data, current = read_wad(source)
        if current != metadata or source.is_symlink():
            raise ValueError('Prepared WAD changed before upload')
        fs.put(source, remote)
    return remote


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sources = parser.add_mutually_exclusive_group()
    sources.add_argument('--wad', type=Path, help='your own IWAD, using one of the supported PC filenames')
    sources.add_argument('--upstream', type=Path, help='local pinned PureDOOM checkout (or its doom1.wad)')
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT,
                        help='external media directory (default: %(default)s)')
    parser.add_argument('--port', help='explicit PXADB port; omit to prepare files only')
    parser.add_argument('--baud', type=int, default=2000000, help='UART baud rate (default: %(default)s)')
    parser.add_argument('--serial', help='required expected device serial with --port')
    parser.add_argument('--publisher-id', help='installed publisher root SHA-256; defaults to the local public DER key')
    args = parser.parse_args(argv)
    if args.port and not args.serial:
        parser.error('--serial is required with --port')
    if args.baud <= 0:
        parser.error('--baud must be positive')
    try:
        manifest = prepare(args.output, args.wad, args.upstream)
        row = manifest['files'][0]
        print(f"Prepared {row['file']}: {row['bytes']} bytes, {row['lumps']} lumps, SHA-256 {row['sha256']}")
        print('Media directory: ' + str(external_output(args.output)))
        if args.port:
            os.environ['PXADB_BAUD'] = str(args.baud)
            sys.path.insert(0, str(PXADB_DIR))
            import pxadb
            print('Uploaded WAD: ' + deploy_media(pxadb, args.port, args.serial, args.output,
                                                  manifest, args.publisher_id))
    except (OSError, ValueError, RuntimeError, ImportError) as error:
        parser.exit(1, str(error) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
