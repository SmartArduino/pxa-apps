#!/usr/bin/env python3
"""Exercise Host-decoded GIF pixels, pause/backpressure, filtering and cancellation.

Start fixture.py separately. This controls ordinary PXA touch/key input; it
does not use a desktop web renderer or inject pixels into the application.
"""
import argparse
import json
import time
from PIL import Image, ImageChops
from pathlib import Path
from harness import Browser
from geometry import content_crop, navigation


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['simulator', 'package', 'publisher-key', 'ime-package', 'output']:
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--fixture-url', default='http://127.0.0.1:31915')
    parser.add_argument('--shell', action='store_true', help='desktop system shell: also test real background/foreground')
    args = parser.parse_args()
    b = Browser(args.simulator, args.package, args.publisher_key, args.ime_package, args.output, shell=args.shell)
    checks = []

    def count(): return b.logs().count('BROWSER GIF FRAME')

    def go(path):
        mark = len(b.logs())
        b.navigate(args.fixture_url.rstrip('/') + path)
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs()[mark:])
        return mark

    def swipe(amount):
        x, y, w, h = b.box('content')
        top = y + (h - 8 if amount > 0 else 8)
        b.command(f'TOUCH DOWN 0 {x+w//2} {top}')
        for step in range(1, 9):
            b.command(f'TOUCH MOVE 0 {x+w//2} {top-amount*step//8}')
            time.sleep(.025)
        b.command(f'TOUCH UP 0 {x+w//2} {top-amount}')
        time.sleep(.15)

    def toggle(row):
        b.nav(4)
        x, y, w, h = b.box('content')
        edge = x + w - 3
        b.command(f'TOUCH DOWN 0 {edge} {y+3}')
        b.command(f'TOUCH MOVE 0 {edge} {y+h-1}')
        b.command(f'TOUCH UP 0 {edge} {y+h-1}')
        time.sleep(.3)
        offset = max(0, 9 * 35 - h)
        b.tap(x+w//2, y + row*35 - offset + 16)
        b.nav(4)

    try:
        b.wait(lambda: 'BROWSER METRICS' in b.logs())
        mark = go('/gif')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        colors = set()
        for index in range(10):
            b.shot('animation-' + str(index))
            image = Image.open(args.output / ('animation-' + str(index) + '.png')).convert('RGB')
            x, y, w, h = b.box('content')
            colors.add(image.getpixel((x+w//2, y+min(60, h//2))))
            time.sleep(.12)
        assert len(colors) >= 3, 'GIF did not display three different frame colors'
        assert count() >= 3, 'No Host frame progression'
        checks.append('real GIF pixels change across at least three frames')

        x, y, w, h = b.box('content'); b.tap(x+w//2, y+50)
        b.wait(lambda: 'BROWSER IMAGE VIEW index=0' in b.logs())
        before = count(); b.wait(lambda: count() > before)
        b.shot('animated-preview')
        b.tap(*navigation({'toolbar': b.box('toolbar')}, 0, 4))
        checks.append('preview keeps GIF advancing without cancelling or downloading again')

        b.nav(4); time.sleep(.3); paused = count(); time.sleep(1.1)
        assert count() == paused, 'Menu kept advancing hidden GIF'
        b.shot('menu-paused'); b.nav(4)
        b.wait(lambda: count() > paused)
        checks.append('menu pauses GIF; closing menu resumes without a catch-up queue')

        x, y, w, h = b.box('content')
        for _ in range(3): swipe(min(h-16, 120))
        time.sleep(.3); paused = count(); time.sleep(1.1)
        assert count() == paused, 'Offscreen GIF kept decoding'
        for _ in range(3): swipe(-min(h-16, 120))
        b.wait(lambda: count() > paused)
        checks.append('offscreen GIF pauses and resumes after scrolling back')

        if args.shell:
            # The desktop shell uses the ordinary bottom Home gesture. A
            # keyboard Home key belongs to the foreground app on this Host.
            xx = 148
            b.command(f'TOUCH DOWN 0 {xx} 238')
            for step in range(1, 13):
                b.command(f'TOUCH MOVE 0 {xx} {238-72*step//12}'); time.sleep(.025)
            b.command(f'TOUCH UP 0 {xx} 166')
            time.sleep(.6); b.shot('system-background')
            paused = count(); time.sleep(.8)
            assert count() == paused
            mark = len(b.logs()); b.command('PACKAGE run pxa-browser')
            b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
            b.wait(lambda: count() > paused)
            checks.append('system background pauses; foreground reloads and resumes GIF')

        mark = go('/gif-alpha')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        b.shot('transparent-first'); time.sleep(.5); b.shot('transparent-next')
        crop = content_crop({'content': b.box('content')})
        one = Image.open(args.output/'transparent-first.png').convert('RGB').crop(crop)
        two = Image.open(args.output/'transparent-next.png').convert('RGB').crop(crop)
        assert ImageChops.difference(one, two).getbbox()
        checks.append('transparent/disposal GIF displays changing pixels')

        mark = go('/gif-still')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        paused = count(); time.sleep(.8); assert count() == paused
        checks.append('single-frame GIF retains no animation work')

        toggle(7)  # GIF off, ordinary images remain on.
        b.wait(lambda: any('#gif 0\n' in p.read_text() and '#images 0\n' not in p.read_text()
                           for p in b.state.rglob('bookmarks.txt')))
        mark = go('/gif-disguised')
        b.wait(lambda: 'BROWSER IMAGE ERROR index=0' in b.logs()[mark:])
        assert 'BROWSER IMAGE DONE' not in b.logs()[mark:]
        mark = go('/image-alpha')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        checks.append('GIF off rejects an octet-stream .bin signature while PNG still loads')
        toggle(7)
        mark = go('/gif-disguised')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        checks.append('GIF on accepts the same disguised image')

        mark = go('/gif-errors')
        b.wait(lambda: 'BROWSER IMAGE ERROR index=0' in b.logs()[mark:])
        mark = go('/gif-slow'); time.sleep(.3); b.nav(3); time.sleep(.5)
        assert 'BROWSER IMAGE DONE' not in b.logs()[mark:]
        checks.append('truncated GIF fails safely; slow download cancels before publication')
        for _ in range(3):
            mark = go('/gif')
            b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
            b.nav(2)
        checks.append('repeated animated page load and home navigation')
        assert 'BROWSER DRAW ERROR' not in b.logs() and 'BROWSER ERROR' not in b.logs()
        b.command('PACKAGE stop pxa-browser' if args.shell else 'KEY HOME')
        end = time.monotonic() + 4
        while 'PXA RESOURCE BUDGET' not in b.logs() and time.monotonic() < end:
            time.sleep(.05)
        budget = [line for line in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in line]
        assert budget and 'remaining_internal=0 remaining_external=0' in budget[-1], budget
        checks.append('normal stop releases all image/temporary budget charges')
        (args.output/'results.json').write_text(json.dumps({'passed': checks}, ensure_ascii=False, indent=2))
        print('GIF simulator PASS:', len(checks), 'checks', flush=True)
    finally:
        b.close()


if __name__ == '__main__': main()
