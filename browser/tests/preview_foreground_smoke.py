#!/usr/bin/env python3
"""Real desktop Home gesture and foreground return restore an animated preview."""
import argparse
import json
from pathlib import Path
import time
from PIL import Image
from geometry import navigation, content_crop
from harness import Browser
from image_failure_fixture import Fixture


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--desktop-simulator', type=Path, help='Default: pxsys_desktop_simulator next to --simulator')
    a = p.parse_args(); f = Fixture().start()
    root = 'http://127.0.0.1:' + str(f.server.server_port)
    b = Browser(a.desktop_simulator or a.simulator.with_name('pxsys_desktop_simulator'),
                a.package, a.publisher_key, a.ime_package, a.output,
                width=296, height=240, corner=0, shell=True,
                environment={'PXA_SIM_NAV_MODE':'gesture'})
    rows = []

    def red(name):
        b.shot(name); im = Image.open(a.output/(name+'.png')).convert('RGB')
        left, top, right, bottom = content_crop({'content':b.box('content'), 'status':b.box('status')})
        return [(x, y) for y in range(top, bottom) for x in range(left, right)
                if all(abs(c-v) < 9 for c, v in zip(im.getpixel((x, y)), (220, 45, 65)))]

    def preview_button(index):
        # The desktop's enabled system gesture takes the bottom strip. The
        # ordinary app button still has a visible, tappable area above it.
        x = navigation({'toolbar':b.box('toolbar')}, index, 4)[0]
        b.tap(x, b.box('toolbar')[1]+3)

    try:
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs())
        mark = len(b.logs()); x, y, w, h = b.box('address'); b.tap(x+w//2, y+h//2)
        b.tap(x+w-b.dp(45), y+h//2); b.enter(root + '/resume-preview')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        points = red('loaded'); assert len(points) > 150
        b.tap((min(p[0] for p in points)+max(p[0] for p in points))//2,
              (min(p[1] for p in points)+max(p[1] for p in points))//2)
        b.wait(lambda: 'BROWSER IMAGE VIEW index=0' in b.logs()[mark:])
        preview_button(3); zoomed = red('zoomed')
        x, y, w, h = b.box('content'); cx, cy = x+w*3//4, y+h//2
        b.command(f'TOUCH DOWN 0 {cx} {cy}')
        for step in range(1, 9):
            b.command(f'TOUCH MOVE 0 {cx-24*step//8} {cy+12*step//8}'); time.sleep(.025)
        b.command(f'TOUCH UP 0 {cx-24} {cy+12}')
        before = red('panned-before-background'); assert len(before) > 150 and before != zoomed
        assert b.command('PACKAGE active pxa-browser').strip() == b'OK 1'
        # Exercise the system gesture, rather than injecting a synthetic Core event.
        b.command('TOUCH DOWN 0 148 238')
        for step in range(1, 13):
            b.command(f'TOUCH MOVE 0 148 {238-72*step//12}'); time.sleep(.025)
        b.command('TOUCH UP 0 148 166')
        time.sleep(.6); b.shot('after-home-gesture')
        assert b.command('PACKAGE active pxa-browser').strip() == b'OK 1'
        b.shot('system-background'); assert f.counts['/resume.gif'] == 1
        mark = len(b.logs()); b.command('PACKAGE run pxa-browser')
        b.wait(lambda: b.command('PACKAGE active pxa-browser').strip() == b'OK 1')
        b.shot('foreground-returned')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        after = red('foreground-reloaded'); assert before == after, (len(before), len(after))
        assert f.counts['/resume.gif'] == 2 and f.counts['/resume-preview'] == 1, dict(f.counts)
        rows.append(dict(flow='foreground reload preserves preview zoom and pan'))
        b.wait(lambda: 'BROWSER GIF FRAME index=0' in b.logs()[mark:])
        b.shot('animation-resumed')
        assert 'BROWSER COMIC' not in b.logs()
        rows.append(dict(flow='animation resumes with one reload and no document refetch'))
        preview_button(0); b.shot('returned-to-page')
        assert f.counts['/resume.gif'] == 2 and f.counts['/resume-preview'] == 1, dict(f.counts)
        rows.append(dict(flow='close preview keeps its reloaded image'))
        b.command('PACKAGE stop pxa-browser')
        b.wait(lambda: 'PXA RESOURCE BUDGET' in b.logs())
        budget = [line for line in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in line][-1]
        assert 'remaining_internal=0 remaining_external=0' in budget, budget
        assert all(error not in b.logs() for error in ('BROWSER DRAW ERROR', 'PXA Guest exception:'))
        rows.append(dict(flow='normal stop releases background and foreground image charges'))
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows, requests=dict(f.counts)), indent=2)+'\n')
        print('Preview foreground simulator PASS:', len(rows), 'flows', flush=True)
    finally: b.close(); f.close()


if __name__ == '__main__': main()
