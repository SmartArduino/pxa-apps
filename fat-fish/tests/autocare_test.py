#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Load signed-app save fixtures and exercise autonomous care in the simulator."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import time
import zlib

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from simulator_test import Session, check, saved, wait_log, menu, menu_row, exit_cleanly


def seed(session, publisher_key, *, food=72, joy=80, energy=86, sleeping=False,
         automatic_sleep=False, enabled=0):
    data = bytearray(64)
    data[:5] = b'FFP1\x01'
    data[8:20] = bytes((food, joy, energy, 5, sleeping, 0, 0, 0, 64, 0, 1, enabled))
    struct.pack_into('<I', data, 24, 7)
    data[32] = automatic_sleep
    checksum = 2166136261
    for byte in data[:60]:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    struct.pack_into('<I', data, 60, checksum)
    key = b'pxa-fat-fish.save.v1'
    body = struct.pack('<H', 1) + bytes((len(key),)) + key + struct.pack('<H', len(data)) + data
    block = struct.pack('<4sHHQII', b'PXKV', 1, 0, 1, len(body), zlib.crc32(body)) + body
    publisher = hashlib.sha256(publisher_key.read_bytes()).hexdigest()
    root = session.state / 'app-data' / (publisher + '-pxa-fat-fish')
    root.mkdir(parents=True, exist_ok=True)
    (root / '.pxa-kv-a').write_bytes(block)


def exercise(args, label, fixture, event=None, expected=None):
    args = argparse.Namespace(**vars(args), case=label)
    args.output = args.output / label
    session = Session(args, 'fat-fish', 320, 240)
    seed(session, args.publisher_key, **fixture)
    with session as s:
        wait_log(s, 'FatFish: loaded ')
        if event:
            wait_log(s, 'FatFish: ' + event + ' ')
            s.shot(label)
            raw = saved(s)
            for offset, value in expected.items():
                check(raw[offset] == value, f'{label} save offset {offset}: {raw[offset]} != {value}')
            check(raw[11] == 5 and struct.unpack_from('<I', raw, 24)[0] == 7, 'care changed pat progress')
        else:
            time.sleep(6)
            check('FatFish: auto-' not in s.logs(), label + ' unexpectedly ran care')
        exit_cleanly(s)
    print(json.dumps({'case': label, 'passed': True}), flush=True)
    return session


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'packages', 'publisher-key', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    for key, value in vars(args).items():
        setattr(args, key, value.resolve())
    check('local/artifacts/' in str(args.output), 'use ignored local/artifacts output')
    exercise(args, 'hungry-legacy-save', {'food': 10}, 'auto-feed', {8: 30, 19: 2})
    exercise(args, 'tired', {'energy': 15}, 'auto-sleep', {12: 1, 32: 1})
    exercise(args, 'rested-auto-sleep', {'energy': 90, 'sleeping': True, 'automatic_sleep': True},
             'auto-wake', {12: 0, 32: 0})
    exercise(args, 'bored', {'joy': 15, 'energy': 80}, 'auto-play', {9: 30, 10: 74})
    exercise(args, 'disabled', {'food': 0, 'joy': 0, 'energy': 0, 'enabled': 1})
    exercise(args, 'manual-sleep', {'energy': 100, 'sleeping': True})
    blocked = argparse.Namespace(**vars(args))
    blocked.output = args.output / 'pause-and-chat'
    session = Session(blocked, 'fat-fish', 320, 240)
    seed(session, args.publisher_key, food=10)
    with session as s:
        menu(s)
        time.sleep(6)
        check('FatFish: auto-' not in s.logs(), 'care ran while paused')
        menu_row(s, 0)
        s.key('TAB')
        time.sleep(6)
        check('FatFish: auto-' not in s.logs(), 'care ran while chatting')
        s.key('X')
        wait_log(s, 'FatFish: auto-feed ')
        exit_cleanly(s)
    print(json.dumps({'case': 'pause-and-chat', 'passed': True}), flush=True)


if __name__ == '__main__':
    main()
