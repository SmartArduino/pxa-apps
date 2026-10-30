#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Real framebuffer checks for automatic touch/controller focus switching."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import time

sys.dont_write_bytecode = True
here = Path(__file__).resolve().parent
sys.path.insert(0, str(here.parents[1] / 'common/pocket-studio/tests'))
from simulator import Session, check, command

spec = importlib.util.spec_from_file_location('calculator_driver', here / 'simulator.py')
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


def gold(image):
    return sum(r > 200 and 145 < g < 220 and 60 < b < 160
               for r, g, b in image.convert('RGB').getdata())


def menu_pixel(session, image, row):
    x = (session.cw - 192) // 2 + 20
    y = ((session.ch - 180) // 2 if session.ch > 224 else 32) + 46 + row * 30
    return image.getpixel((session.vx + round(x * session.vw / session.cw),
                           session.vy + round(y * session.vh / session.ch)))


def case(args, w, h):
    with Session(args, 'calculator', w, h) as s:
        k = driver.Keys(s)
        check(gold(s.shot('01-default')) == 0, 'default focus is visible')
        command(s.sock, 'KEY_UP DOWN')  # Zero-button controller report, no press edge.
        time.sleep(.15)
        check(gold(s.shot('02-idle-controller')) == 0, 'idle report enabled focus')
        k.key('DOWN')
        check(gold(s.shot('03-keyboard')) > 0, 'physical key did not reveal focus')
        s.tap(20, 15)
        check(gold(s.shot('04-touch')) == 0, 'touch did not hide focus')
        s.tap(s.cw - 36, 12)
        check('Pocket: menu opened' in s.logs(), 'touch did not open menu')
        touch = s.shot('05-menu-touch')
        k.key('UP')
        keys = s.shot('06-menu-keys')
        inactive, selected = menu_pixel(s, touch, 3), menu_pixel(s, keys, 3)
        check(selected[1] > inactive[1] + 40 and selected[2] > inactive[2] + 40,
              'shared menu did not reveal focus after key press')
        s.tap(s.cw - 36, 12)
        check(gold(s.shot('07-touch-resume')) == 0, 'touch resume retained focus')
        s.key('ENTER')  # START opens the shared menu without calling game::key.
        start = s.shot('08-start-menu')
        check(menu_pixel(s, start, 0) == selected, 'START menu input mode differs')
        s.key('X')  # Controller B resumes the calculator.
        check(gold(s.shot('09-key-resume')) > 0, 'key resume lost input mode')
        s.tap(20, 15)
        check(gold(s.shot('10-touch-again')) == 0, 'mode switching is not reversible')
        k.close()
    with Session(args, 'calculator', w, h) as s:
        check(gold(s.shot('11-restart')) == 0, 'saved state restored navigation focus')
        driver.Keys(s).close()
    return {'display': [w, h], 'default_hidden': True, 'idle_hidden': True,
            'keys_show': True, 'touch_hides': True, 'shared_menu': True,
            'restart_hidden': True, 'passed': True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['simulator', 'packages', 'publisher-key', 'output']:
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    for name, value in vars(a).items():
        setattr(a, name, value.resolve())
    check('local/artifacts/' in str(a.output), 'use ignored artifacts')
    a.output.mkdir(parents=True, exist_ok=True)
    os.environ['PXA_SIMULATOR_PERF'] = '0'
    results = []
    try:
        for w, h in [(240, 284), (480, 320)]:
            results.append(case(a, w, h))
            print('PASS automatic focus', w, h, flush=True)
    finally:
        (a.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
