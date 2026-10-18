#!/usr/bin/env python3
"""Copy owned game data into a PXA app's signed, streaming blob assets.

Quake requires a converted MG24Quake 2.0 PAK; Duke requires a converted MG24
GRP. Run the upstream converters first (see the port README).
"""
import argparse, hashlib, json, re, shutil
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

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', choices=APPS)
    parser.add_argument('source', type=Path)
    parser.add_argument('--apps-root', type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    try:
        if args.source.is_symlink():
            raise ValueError(f'Symlinks are not accepted: {args.source}')
        files = collect(args.app, args.source.resolve(strict=True))
        dest = args.apps_root / args.app / 'assets/game'
        # A different existing import is never silently overwritten.
        for name, src in files.items():
            dst = dest/name
            if dst.exists() and hashlib.sha256(dst.read_bytes()).digest() != hashlib.sha256(src.read_bytes()).digest():
                raise ValueError(f'Different file already exists: {dst}; remove it explicitly first')
        rows = []
        for name, src in files.items():
            dst = dest/name; dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dst)
            rows.append({'path':name, 'bytes':dst.stat().st_size, 'sha256':hashlib.sha256(dst.read_bytes()).hexdigest()})
        (dest/'catalog.txt').write_text(''.join(f"{r['path']}\t{r['bytes']}\n" for r in rows))
        (dest.parent/'imported-game-data.json').write_text(json.dumps({'app':args.app, 'files':rows}, indent=2)+'\n')
        print(f'Imported {len(rows)} files, {sum(r["bytes"] for r in rows)} bytes into {dest}')
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')

if __name__ == '__main__':
    main()
