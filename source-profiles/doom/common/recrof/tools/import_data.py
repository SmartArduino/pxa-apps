#!/usr/bin/env python3
"""Import owned game data into local staging outside the signed engine package.

Quake requires a converted MG24Quake 2.0 PAK; Duke requires a converted MG24
GRP. Run the upstream converters first (see the port README).
"""
import argparse, hashlib, json, os, re, shutil, tempfile
from pathlib import Path

APPS = ('quake', 'quake2', 'duke3d', 'command-conquer', 'red-alert')

def collect(app, source):
    files = [source] if source.is_file() else sorted(p for p in source.rglob('*') if p.is_file())
    result = {}
    for file in files:
        if file.is_symlink():
            raise ValueError(f'Symlinks are not accepted: {file}')
        relative = Path(file.name) if source.is_file() else file.relative_to(source)
        parts = [p.lower() for p in relative.parts]
        if 'baseq2' in parts:
            parts = parts[parts.index('baseq2'):]
        if app == 'quake':
            if file.name.lower() not in ('pak0.pak', 'pak0conv.pak'):
                continue
            parts = ['pak0.pak']
        elif app == 'duke3d':
            if file.name.lower() not in ('duke3d.grp', 'duke3dconv.grp'):
                continue
            parts = ['duke3d.grp']
        elif app == 'quake2':
            if not file.suffix.lower() in ('.pak', '.cfg'):
                continue
            if parts[0] != 'baseq2':
                parts = ['baseq2', *parts]
        elif file.suffix.lower() not in ('.mix', '.ini', '.aud', '.vqa', '.pal', '.bin'):
            continue
        if any(not re.fullmatch(r'[a-z0-9._-]{1,64}', p) or p in ('.', '..') for p in parts):
            raise ValueError(f'Unsupported asset name: {relative}')
        target = '/'.join(parts)
        if target in result:
            raise ValueError(f'Case-insensitive filename collision: {relative}')
        result[target] = file
    required = {'quake':'pak0.pak', 'quake2':'baseq2/pak0.pak', 'duke3d':'duke3d.grp',
                'command-conquer':'conquer.mix', 'red-alert':'redalert.mix'}[app]
    if required not in result and not (app == 'red-alert' and 'local.mix' in result) and not (app == 'command-conquer' and 'demo.mix' in result):
        raise ValueError(f'Missing required file: {required}')
    return result

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def import_files(apps_root, app, source):
    if source.is_symlink():
        raise ValueError(f'Symlinks are not accepted: {source}')
    files = collect(app, source.resolve(strict=True))
    dest = apps_root / '.game-data' / app / 'files'
    rows = []
    # Validate the entire import before changing any existing resources.
    for name, src in files.items():
        dst = dest / name
        checksum = digest(src)
        if dst.is_symlink() or any(p.is_symlink() for p in dst.parents):
            raise ValueError(f'Symlinks are not accepted: {dst}')
        if dst.exists() and digest(dst) != checksum:
            raise ValueError(f'Different file already exists: {dst}; remove it explicitly first')
        rows.append({'path': name, 'bytes': src.stat().st_size, 'sha256': checksum})
    for name, src in files.items():
        dst = dest / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        if not dst.exists():
            with tempfile.NamedTemporaryFile(dir=dst.parent, prefix='.import-', delete=False) as output:
                temporary = Path(output.name)
            try:
                with temporary.open('wb') as output, src.open('rb') as stream:
                    shutil.copyfileobj(stream, output, 256 * 1024)
                if digest(temporary) != next(r['sha256'] for r in rows if r['path'] == name):
                    raise ValueError(f'Resource changed during import: {src}')
                try:
                    os.link(temporary, dst)  # Atomic no-replace publication.
                except FileExistsError:
                    if digest(dst) != digest(temporary):
                        raise ValueError(f'Different file appeared during import: {dst}')
            finally:
                temporary.unlink(missing_ok=True)
    metadata = dest.parent / 'imported-game-data.json'
    metadata.write_text(json.dumps({'app': app, 'files': rows}, indent=2) + '\n')
    return dest, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', choices=APPS)
    parser.add_argument('source', type=Path)
    parser.add_argument('--apps-root', type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    try:
        dest, rows = import_files(args.apps_root, args.app, args.source)
        print(f'Imported {len(rows)} files, {sum(r["bytes"] for r in rows)} bytes into {dest}')
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')

if __name__ == '__main__':
    main()
