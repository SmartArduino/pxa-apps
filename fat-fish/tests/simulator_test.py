#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the signed PXA app, animated frames, care, touch cancellation and saves."""
import argparse
import json
from pathlib import Path
import struct
import re
import sys
import time

from PIL import ImageChops

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'common/pocket-studio/tests'))
from simulator import Session, check, command, save_blob


def saved(session):
    raw = save_blob(session.state, 'fat-fish')
    check(len(raw) == 64 and raw[:8] == b'FFP1\x01\0\0\0', 'save header')
    checksum = 2166136261
    for byte in raw[:60]:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    check(struct.unpack_from('<I', raw, 60)[0] == checksum, 'save checksum')
    return raw


def wait_log(s, text, count=1):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        check(s.proc.poll() is None, s.logs())
        check('animation resource failed' not in s.logs(), s.logs())
        if s.logs().count(text) >= count:
            time.sleep(.3)
            return
        time.sleep(.05)
    raise AssertionError(text + '\n' + s.logs())


def action(s, index, delay=.4):
    match = re.findall(r'layout canvas=\d+x\d+ touch=\d+ columns=(\d+) cell=(\d+) top=(\d+) height=(\d+)', s.logs())
    check(match, 'missing control layout')
    columns, cell, top, height = map(int, match[-1])
    if columns == 3:
        if index < 2:
            s.tap(12 + index * cell + (cell - 4) // 2, top + height + 4 + height // 2, delay)
            return
        s.tap(12 + 2 * cell + (cell - 4) // 2, top + height + 4 + height // 2, .1)
    s.tap(12 + (index % columns) * cell + (cell - 4) // 2,
          top + (index // columns) * (height + 4) + height // 2, delay)


def menu(s):
    s.tap(s.cw - 26, 21, .3)


def menu_row(s, row):
    top = ((s.ch - 180) // 2 if s.ch > 224 else 32) + 40
    s.tap(s.cw // 2, top + row * 30 + 14, .3)


def exit_cleanly(s):
    menu(s)
    top = ((s.ch - 180) // 2 if s.ch > 224 else 32) + 40
    # Exit is handled on DOWN and may close the runner before a matching UP.
    s.pointer('DOWN', s.cw // 2, top + 74)
    s.proc.wait(timeout=5)
    check(s.proc.returncode == 0, 'save/exit')


def exercise(args, width, height, radius):
    with Session(args, 'fat-fish', width, height, radius) as s:
        time.sleep(.8)
        first = s.shot('01-room-zh')
        time.sleep(.6)
        second = s.shot('02-animation')
        check(ImageChops.difference(first, second).getbbox() is not None, 'animation did not advance')
        check('animation resource failed' not in s.logs(), s.logs())
        # Cancelled releases may never trigger care.
        s.pointer('DOWN', s.cw // 2, 100)
        s.pointer('CANCEL', s.cw // 2, 100)
        time.sleep(.2)
        check('FatFish: pet ' not in s.logs(), 'cancel triggered pet')
        s.tap(s.cw // 2, 100, 1.1)
        wait_log(s, 'FatFish: pet ')
        check(saved(s)[11] == 1, 'touch did not increase bond exactly once')
        action(s, 1)
        wait_log(s, 'FatFish: feed ')
        check(saved(s)[8] == 92, 'feeding did not update food')
        s.shot('03-feeding')
        time.sleep(3)
        action(s, 2)
        wait_log(s, 'FatFish: play ')
        check(saved(s)[10] <= 80, 'play did not consume energy')
        # Rest and wake must remain available during another action's cooldown.
        action(s, 3)
        wait_log(s, 'FatFish: sleep ')
        check(saved(s)[12] == 1, 'sleep did not persist')
        time.sleep(1)
        s.shot('04-sleep')
        time.sleep(5)
        action(s, 3)
        wait_log(s, 'FatFish: wake ')
        check(saved(s)[12] == 0 and saved(s)[10] >= 83, 'sleep recovery or wake failed')
        s.pointer('DOWN', s.cw // 2, 100)
        s.pointer('MOVE', s.cw // 2 + 25, 100)
        s.pointer('UP', s.cw // 2 + 25, 100)
        wait_log(s, 'FatFish: drag ')
        check(saved(s)[16] >= 86, 'drag position did not save')
        # Rapid clip changes cancel pending asset reads without stale frames.
        for index in (1, 3, 7, 4):
            action(s, 4, .08)
            row_h = (s.ch - 92) // 4
            cell = (s.cw - 28) // 2
            s.tap(12 + (index % 2) * (cell + 4) + cell // 2,
                  53 + (index // 2) * row_h + 8, .08)
        time.sleep(.8)
        check(s.logs().count('FatFish: preview') == 4, 'gallery preview actions')
        action(s, 4)
        s.shot('05-clips-page-one')
        s.tap(s.cw // 2, s.ch - 23)
        s.shot('06-clips-page-two')
        s.tap(40, s.ch - 23)
        action(s, 5)
        s.shot('07-settings')
        row = min(32, (s.ch - 93) // 5)
        for index in range(5):
            s.tap(s.cw // 2, 45 + index * row + (row - 5) // 2)
        s.shot('08-credits')
        s.tap(s.cw // 2, s.ch - 17)
        s.tap(s.cw // 2, s.ch - 17)
        s.shot('09-moonlight')
        menu(s)
        before = saved(s)
        s.shot('10-menu-zh')
        menu_row(s, 3)
        wait_log(s, 'Pocket: language en')
        s.shot('11-menu-en')
        time.sleep(1.1)
        check(saved(s) == before, 'paused care timer advanced')
        menu_row(s, 0)
        s.shot('12-room-en')
        exit_cleanly(s)
        durable = saved(s)
        check(durable[13:16] == b'\x01\0\0', 'room preferences not durable')
        check(durable[19] == 1, 'automatic care preference not durable')
        check('animation resource failed' not in s.logs(), s.logs())
    with Session(args, 'fat-fish', width, height, radius) as s:
        wait_log(s, 'FatFish: loaded ')
        time.sleep(.6)
        s.shot('13-reloaded')
        menu(s)
        restored = saved(s)
        check(restored[8:20] == durable[8:20], 'care/room state lost on reload')
        check(restored[24:28] == durable[24:28], 'pat count lost on reload')
        menu_row(s, 0)
        # Controller normal entry point selects Feed with Right + A.
        s.key('RIGHT')
        s.key('Z')
        wait_log(s, 'FatFish: feed ')
        check(saved(s)[8] == 100, 'controller feeding failed')
        s.shot('14-fed-bubble-en')
        s.key('A')  # X action opens sleep.
        wait_log(s, 'FatFish: sleep ')
        s.shot('15-sleep-bubble-en')
        exit_cleanly(s)
    with Session(args, 'fat-fish', width, height, radius) as s:
        wait_log(s, 'FatFish: loaded ')
        time.sleep(.6)
        s.shot('14-sleep-restored')
        check(saved(s)[12] == 1, 'sleep state not restored')
        s.tap(s.cw // 2, 100)
        wait_log(s, 'FatFish: wake ')
        exit_cleanly(s)
    return {'display': [width, height], 'corner_radius': radius,
            'care_touch_controller_gallery_pause_reload': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'packages', 'publisher-key', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    for key, value in vars(args).items():
        setattr(args, key, value.resolve())
    check('local/artifacts/' in str(args.output), 'use ignored local/artifacts output')
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        for geometry in ((320, 240, 0), (240, 320, 0), (296, 240, 58)):
            results.append(exercise(args, *geometry))
            print(json.dumps(results[-1]), flush=True)
    finally:
        (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
