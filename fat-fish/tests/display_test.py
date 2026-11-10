#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Render the signed app across densities and verify icon hit regions."""
import argparse
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

from PIL import Image

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'common/pocket-studio/tests'))
from simulator import check, command, FAULT


def exercise(args, width, height, dpi, radius):
    out = args.output / f'{width}x{height}-{dpi}dpi'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'state').mkdir(exist_ok=True)
    sock = Path(f'/tmp/fat-fish-display-{os.getpid()}.sock')
    sock.unlink(missing_ok=True)
    path = out / 'runtime.log'
    log = path.open('wb')
    process = subprocess.Popen([
        str(args.simulator), '--package', str(args.package), '--publisher-key', str(args.publisher_key),
        '--state-root', str(out / 'state'), '--width', str(width), '--height', str(height),
        '--density-dpi', str(dpi), '--corner-radius', str(radius),
        '--safe-insets', '8,10,8,10' if radius else '0,0,0,0', '--pxadb-control-socket', str(sock)
    ], stdout=log, stderr=subprocess.STDOUT, env=dict(os.environ,
        SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', PXA_SIM_NAV_MODE='gesture-no-bar'))
    try:
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            text = path.read_text(errors='replace')
            check(process.poll() is None, text)
            if sock.exists() and 'FatFish: layout ' in text:
                break
            time.sleep(.05)
        else:
            raise TimeoutError(text)
        geometry = re.findall(r'viewport canvas=(\d+)x(\d+) origin=(\d+),(\d+) size=(\d+)x(\d+)', text)
        cw, ch, vx, vy, vw, vh = map(int, geometry[-1])
        layout = re.findall(r'layout canvas=\d+x\d+ touch=(\d+) columns=(\d+) cell=(\d+) top=(\d+) height=(\d+) icon=(\d+)', text)
        touch, columns, cell, top, button_height, icon = map(int, layout[-1])
        check(columns in (3, 6) and icon >= 14 and top > 100, 'invalid control layout')

        def check_placement(room):
            placements = re.findall(r'placement room=(\d+) rug=(\d+),(\d+) feet=(\d+),(\d+) crop=(\d+),(\d+),(\d+),(\d+) travel=(\d+)',
                                    path.read_text(errors='replace'))
            check(placements, 'missing room placement')
            current, rx, ry, fx, fy, sx, sy, sw, sh, travel = map(int, placements[-1])
            check(current == room and (rx, ry) == (fx, fy), 'feet must align with rug center')
            check(sx + sw <= 384 and sy + sh <= 256, 'room crop outside source')
            check(ch * 2 // 3 < ry < ch * 4 // 5 and 0 <= travel <= 64, 'room standing area')

        def tap(x, y):
            px = vx + (int(x) * vw + cw - 1) // cw
            py = vy + (int(y) * vh + ch - 1) // ch
            command(sock, f'TOUCH DOWN 1 {px} {py}')
            time.sleep(.04)
            command(sock, f'TOUCH UP 1 {px} {py}')
            time.sleep(.2)

        def action(index):
            if columns == 3:
                if index < 2:
                    tap(12 + index * cell + (cell - 4) // 2, top + button_height + 4 + button_height // 2)
                    return
                tap(12 + 2 * cell + (cell - 4) // 2, top + button_height + 4 + button_height // 2)
            tap(12 + index % columns * cell + (cell - 4) // 2,
                top + index // columns * (button_height + 4) + button_height // 2)

        def shot(name):
            time.sleep(.3)
            header, _, png = command(sock, 'SCREENSHOT AFTER_PRESENT').partition(b'\n')
            check(header == f'PNG {len(png)}'.encode(), 'PNG framing')
            image = Image.open(io.BytesIO(png))
            check(image.size == (width, height), 'screenshot size')
            (out / (name + '.png')).write_bytes(png)

        time.sleep(.6)
        check_placement(0)
        shot('01-icons')
        tap(cw - 20, 29)
        check('Pocket: menu opened' in path.read_text(), 'aligned menu button lower hit region')
        shot('01-menu')
        tap(cw - 26, 21)
        check('Pocket: menu closed' in path.read_text(), 'aligned menu button close')
        visible_top = top + button_height + 4 if columns == 3 else top
        tap(cw - 26, visible_top - 15)
        shot('02-dock-hidden')
        tap(cw - 29, ch - 29)
        action(0)
        check('FatFish: pet ' in path.read_text(), 'pet icon mapping')
        action(3)
        check('FatFish: sleep ' in path.read_text(), 'sleep icon mapping')
        shot('02-sun-icon')
        action(3)
        check('FatFish: wake ' in path.read_text(), 'wake icon mapping')
        action(4)
        shot('03-gallery')
        tap(45, 65)
        check('FatFish: preview' in path.read_text(), 'gallery icon mapping')
        action(5)
        shot('04-settings')
        tap(cw // 2, 57)
        check('FatFish: settings' in path.read_text(), 'settings icon mapping')
        tap(cw // 2, ch - 17)
        check_placement(1)
        shot('05-moonlight')
        action(5)
        tap(cw // 2, 57)
        tap(cw // 2, ch - 17)
        check_placement(2)
        shot('06-aquarium')
        text = path.read_text(errors='replace')
        check(not FAULT.search(text) and 'animation resource failed' not in text, text)
        return {'display': [width, height], 'dpi': dpi, 'radius': radius,
                'canvas': [cw, ch], 'columns': columns, 'touch_units': touch,
                'button_height': button_height, 'icon_size': icon, 'passed': True}
    finally:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        log.close()
        sock.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('simulator', 'package', 'publisher-key', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    for name, value in vars(args).items():
        setattr(args, name, value.resolve())
    check('local/artifacts/' in str(args.output), 'use ignored local/artifacts output')
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        for geometry in ((320, 240, 160, 0), (240, 320, 240, 0), (480, 480, 320, 0),
                         (480, 320, 240, 0), (296, 240, 160, 58), (720, 720, 320, 0)):
            results.append(exercise(args, *geometry))
            print(json.dumps(results[-1]), flush=True)
    finally:
        (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
