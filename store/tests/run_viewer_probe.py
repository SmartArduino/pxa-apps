#!/usr/bin/env python3
"""Offline production Host/AOT screenshot viewer checks and PNG captures."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def chunk(kind, data):
    return (struct.pack('>I', len(data)) + kind + data +
            struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff))


def write_png(path, width, height, rows, alpha=False):
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
        chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6 if alpha else 2, 0, 0, 0)) +
        chunk(b'IDAT', zlib.compress(b''.join(b'\0' + row for row in rows))) + chunk(b'IEND', b''))


def fixtures(root):
    root.mkdir(exist_ok=True)
    for n, (width, height, color) in enumerate([
        (120, 240, (150, 42, 37)), (240, 120, (35, 91, 180)), (240, 240, (30, 130, 80))], 1):
        rows = [bytes(v for x in range(width)
                      for v in ((235, 235, 235) if x % 24 == 0 or y % 24 == 0 else color))
                for y in range(height)]
        write_png(root / f'{n}.png', width, height, rows)


def convert_captures(root):
    for path in root.glob('*.bgra'):
        meta = json.loads(path.with_suffix('.json').read_text())
        data = path.read_bytes()
        rows = []
        for y in range(meta['height']):
            row = bytearray(data[y * meta['stride']:y * meta['stride'] + meta['width'] * 4])
            row[0::4], row[2::4] = row[2::4], row[0::4]
            rows.append(bytes(row))
        write_png(path.with_suffix('.png'), meta['width'], meta['height'], rows, alpha=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--publisher', type=Path, required=True)
    parser.add_argument('--ime-package', type=Path, required=True,
                        help='signed IME provider enables production text-input support')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    fixture_root = root / 'fixtures'
    fixtures(fixture_root)
    source_root = Path(__file__).resolve().parent
    probe = root / 'viewer-probe'
    subprocess.run([sys.executable, str(source_root / 'build_browse_probe.py'),
                    str(args.build.resolve()), str(probe), '--source',
                    str(source_root / 'store_viewer_probe.c')], check=True)
    profiles = [
        ('mosaico', 480, 480, 305, 1, 40), ('pai', 296, 240, 130, 1, 16),
        ('watcher', 412, 412, 284, 2, 206), ('small', 240, 240, 160, 0, 0),
        ('low-dpi', 240, 240, 80, 0, 0), ('high-dpi', 720, 720, 480, 1, 60)]

    def run(profile):
        name, width, height, dpi, shape, radius = profile
        out = root / name
        out.mkdir(exist_ok=True)
        env = dict(os.environ, STORE_PROBE_WIDTH=str(width), STORE_PROBE_HEIGHT=str(height),
                   STORE_PROBE_DPI=str(dpi), STORE_PROBE_SHAPE=str(shape), STORE_PROBE_RADIUS=str(radius),
                   PXA_IME_PACKAGE=str(args.ime_package.resolve()))
        with tempfile.TemporaryDirectory(prefix=f'{name}-state-', dir=root) as state:
            with (out / 'run.log').open('w') as log:
                subprocess.run([str(probe), str(args.package.resolve()), str(args.publisher.resolve()),
                                str(out), str(fixture_root), state], env=env, stdout=log,
                               stderr=subprocess.STDOUT, check=True, timeout=45)
        convert_captures(out)
        result = json.loads((out / 'report.json').read_text())
        print(name, result, flush=True)
        return name, result

    with ThreadPoolExecutor(max_workers=3) as pool:
        results = dict(pool.map(run, profiles))
    (root / 'reports.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
