#!/usr/bin/env python3
"""Generate packaged PCM sound-effect files from Shattered Pixel Dungeon."""

import argparse
import os
import subprocess
from pathlib import Path

APP = Path(__file__).resolve().parents[1]
RATE = 16000

SOUNDS = [
    ('hit', 0.27),
    ('miss', 0.21),
    ('hit_crush', 0.30),
    ('death', 0.54),
    ('drink', 0.42),
    ('eat', 0.42),
    ('gold', 0.36),
    ('descend', 0.84),
    ('door_open', 0.42),
    ('trap', 0.30),
    ('levelup', 0.96),
    ('item', 0.30),
    ('read', 0.42),
    ('shatter', 0.48),
    ('unlock', 0.36),
    ('health_warn', 0.36),
    ('step', 0.24),
    ('grass', 0.30),
    ('trample', 0.36),
    ('water', 0.36),
]


def decode(path, seconds):
    command = [
        'ffmpeg', '-v', 'error', '-i', str(path),
        '-t', '%.3f' % seconds, '-ac', '1', '-ar', str(RATE),
        '-f', 'u8', '-',
    ]
    return subprocess.run(command, stdout=subprocess.PIPE, check=True).stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--spd-assets', default=os.environ.get('PXA_SPD_ASSETS'))
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if not args.spd_assets:
        raise SystemExit('pass --spd-assets /path/to/shattered-pixel-dungeon')
    sounds = Path(args.spd_assets) / 'core/src/main/assets/sounds'
    if not sounds.is_dir():
        raise SystemExit('not a Shattered Pixel Dungeon checkout: %s' % sounds)

    for asset_root in (APP / 'assets', APP / 'assets-esp32s3'):
        sound_root = asset_root / 'sfx'
        if not args.check:
            sound_root.mkdir(parents=True, exist_ok=True)
        for source, seconds in SOUNDS:
            original = sounds / (source + '.mp3')
            if not original.is_file():
                raise SystemExit('missing sound: %s' % original)
            pcm = decode(original, seconds)
            target = sound_root / (source + '.pcm')
            if args.check:
                if not target.exists() or target.read_bytes() != pcm:
                    raise SystemExit('%s is out of date' % target)
            else:
                target.write_bytes(pcm)
    print('packaged sound effects: %d' % len(SOUNDS))


if __name__ == '__main__':
    main()
