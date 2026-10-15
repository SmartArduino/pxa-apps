#!/usr/bin/env python3
"""Build the optional GB18030 mapping asset from pinned WHATWG indexes.

Downloaded inputs are verified; ordinary app builds use the checked-in asset
and require no network or Python codec version. See assets/encoding-license.txt.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import urllib.request

INDEXES = {
    'index-gb18030.txt': '746b3c55f1a8ec4b90b451f384437a17fd37cd51cd668456a28071a758d10784',
    'index-gb18030-ranges.txt': '874c6b6f6f74cf7d427ad228d5b41ddd9354fffd92a2259bf429f86e6baa7a1e',
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]/'assets/gb18030.bin')
    args = parser.parse_args()
    args.index_dir.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, digest in INDEXES.items():
        path = args.index_dir/name
        if not path.exists():
            with urllib.request.urlopen('https://encoding.spec.whatwg.org/'+name, timeout=30) as r:
                path.write_bytes(r.read())
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != digest:
            raise SystemExit('Unexpected WHATWG index version: '+name)
        rows.append([(int(a),int(b,16)) for line in raw.decode().splitlines()
                     if line and not line.startswith('#') for a,b,*_ in [line.split()]])
    assert [i for i,c in rows[0]] == list(range(23940))
    assert len(rows[1]) == 207
    data = b''.join(struct.pack('<H', c) for _,c in rows[0])
    data += b''.join(struct.pack('<II', i,c) for i,c in rows[1])
    args.output.write_bytes(data)
    print(f'{len(data)} bytes; sha256={hashlib.sha256(data).hexdigest()}')

if __name__ == '__main__': main()
