# SPDX-License-Identifier: GPL-3.0-only
"""Capture and restore an unmounted app-private block image through PXADB."""
import json
import zlib
from pathlib import Path
from simulator import read_saves
import pxadb


def capture(client, volume, output):
    fs = pxadb.NormalFsClient(client)
    remote = volume.strip('/') + '/PXA/apps/lucinhu/pxa-mindustry/data'
    output.mkdir(parents=True, exist_ok=True)
    def copy_tree(path, relative):
        for kind, _, name in fs.list(path):
            if kind == 'D':
                copy_tree(path + '/' + name, relative / name)
            elif kind == 'F':
                dest = output / relative / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                fs.get(path + '/' + name, dest)
    copy_tree(remote, Path('.'))
    snapshots = []
    decoded = output / 'decoded'
    decoded.mkdir(exist_ok=True)
    candidate = decoded / '.pxa-kv-candidate'
    # LittleFS can retain old worlds with larger tick counts. Select the
    # newest CRC-validated PXKV storage generation, rather than game time.
    for block in output.rglob('b????????'):
        raw = block.read_bytes()
        start = 0
        while (start := raw.find(b'PXKV', start)) >= 0:
            header = raw[start:start+24]
            start += 4
            if len(header) != 24 or header[4:8] != b'\x01\x00\x00\x00':
                continue
            size = int.from_bytes(header[16:20], 'little')
            body = raw[start+20:start+20+size]
            if len(body) != size or zlib.crc32(body) != int.from_bytes(header[20:24], 'little'):
                continue
            generation = int.from_bytes(header[8:16], 'little')
            pos = 2
            try:
                for _ in range(int.from_bytes(body[:2], 'little')):
                    key_size = body[pos];pos += 1
                    key = body[pos:pos+key_size];pos += key_size
                    value_size = int.from_bytes(body[pos:pos+2], 'little');pos += 2
                    value = body[pos:pos+value_size];pos += value_size
                    if key != b'pxa-mindustry.save.v1':
                        continue
                    candidate.write_bytes(value)
                    for snapshot in read_saves(decoded):
                        snapshot['storage_generation'] = generation
                        snapshots.append(snapshot)
            except (AssertionError, IndexError, ValueError):
                pass
    candidate.unlink(missing_ok=True)
    latest = max(snapshots, key=lambda s: s['storage_generation']) if snapshots else None
    if latest:
        (output / 'latest.json').write_text(json.dumps(latest, indent=2) + '\n')
    return latest


def restore(client, volume, source):
    fs = pxadb.NormalFsClient(client)
    source = Path(source)
    metas = list(source.rglob('private.blocks/image.meta'))
    if not metas:
        raise RuntimeError('No original private block backup')
    for meta in metas:
        local = meta.parent
        remote = (volume.strip('/') + '/PXA/apps/lucinhu/pxa-mindustry/data/'
                  + str(local.relative_to(source)))
        names = {p.name for p in local.iterdir() if p.is_file()}
        for name in sorted(names, key=lambda n: n == 'image.meta'):
            staged = remote + '/' + name + '.restore'
            fs.put(local / name, staged)
            fs.remove(remote + '/' + name)
            fs.move(staged, remote + '/' + name)
        for kind, _, name in fs.list(remote):
            if kind == 'F' and name not in names:
                fs.remove(remote + '/' + name)
