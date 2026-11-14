#!/usr/bin/env python3
"""Real PXA image failures: bounded automatic retries, explicit recovery and DOM reorder."""
import argparse
import json
from pathlib import Path
import re
import time
from PIL import Image
from harness import Browser
from image_failure_fixture import Fixture, oversized


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--width', type=int, default=480)
    p.add_argument('--height', type=int, default=480)
    a = p.parse_args(); f = Fixture().start()
    root = 'http://127.0.0.1:' + str(f.server.server_port)
    b = Browser(a.simulator, a.package, a.publisher_key, a.ime_package, a.output,
                width=a.width, height=a.height, corner=0)
    rows = []

    def load(path):
        mark = len(b.logs()); b.navigate(root + path)
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs()[mark:])
        return mark

    def image(mark, index, success):
        pattern = r'BROWSER IMAGE ' + ('DONE' if success else 'ERROR') + r' index=' + str(index) + r'\b'
        b.wait(lambda: re.search(r'BROWSER IMAGE (?:DONE|ERROR) index=' + str(index) + r'\b', b.logs()[mark:]), timeout=35)
        assert re.search(pattern, b.logs()[mark:]), b.logs()[mark:][-3000:]

    def open_reader():
        b.nav(4); time.sleep(.3)
        x, y, w, h = b.box('content'); b.tap(x+w-2, y+h-1)
        mark = len(b.logs()); b.tap(x+w//2, y+h-b.dp(18))
        b.wait(lambda: 'BROWSER COMIC index=' in b.logs()[mark:])
        return mark

    def turn(index, page):
        mark = len(b.logs()); b.nav(index)
        b.wait(lambda: re.search(r'BROWSER COMIC[^\n]*page=' + str(page) + r' count=2\b', b.logs()[mark:]))
        return mark

    def retry():
        mark = len(b.logs()); x, y, w, h = b.box('content'); b.tap(x+w//2, y+h//2)
        return mark

    def unchanged(path, expected):
        time.sleep(.7)
        assert f.counts[path] == expected, (path, expected, dict(f.counts))

    def toggle_gif():
        b.nav(4); x, y, w, h = b.box('content')
        offset = min(max(0, 13*35-h), max(0, 7*35-h//2))
        remaining = offset
        while remaining:
            amount = min(remaining, h-20); cx, start = x+w//2, y+h-10
            b.command(f'TOUCH DOWN 0 {cx} {start}'); time.sleep(.08)
            for step in range(1, 9):
                b.command(f'TOUCH MOVE 0 {cx} {start-amount*step//8}'); time.sleep(.025)
            b.command(f'TOUCH UP 0 {cx} {start-amount}'); time.sleep(.2); remaining -= amount
        b.tap(x+w//2, y+7*35-offset+16); b.nav(4)

    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        for page, source, code in (('/permanent', '/unsupported.jpg', -3),
                                   ('/limited', '/oversized.jpg', -16)):
            mark = load(page); image(mark, 0, False)
            assert f'BROWSER IMAGE ERROR index=0 code={code}' in b.logs()[mark:]
            attempts = f.counts[source]; b.shot(page[1:] + '-webpage')
            open_reader(); unchanged(source, attempts); b.shot(page[1:] + '-reader')
            rows.append(dict(flow=page[1:] + ' open retains failure', requests=attempts))
            mark = turn(1, 2); image(mark, 1, True)
            turn(0, 1); unchanged(source, attempts)
            rows.append(dict(flow=page[1:] + ' returning avoids duplicate request'))
            mark = retry(); image(mark, 0, False); unchanged(source, attempts + 1)
            mark = len(b.logs()); b.nav(3); image(mark, 0, False); unchanged(source, attempts + 2)
            rows.append(dict(flow=page[1:] + ' explicit retry and quality each request once'))
            b.nav(4); open_reader(); unchanged(source, attempts + 2)
            b.shot(page[1:] + '-reopened'); b.nav(4)
            rows.append(dict(flow=page[1:] + ' close and reopen retains reason'))

        mark = load('/transient'); image(mark, 0, False)
        mark = open_reader(); image(mark, 0, True)
        assert f.counts['/flaky.jpg'] == 2
        b.shot('transient-recovered'); b.nav(4)
        rows.append(dict(flow='transient server failure recovers on intentional open', requests=2))

        mark = load('/manual'); image(mark, 0, False)
        mark = retry(); image(mark, 0, True)
        assert f.counts['/manual.jpg'] == 2
        b.shot('webpage-manual-recovered')
        rows.append(dict(flow='webpage placeholder supports manual retry', requests=2))

        mark = load('/cancelled'); b.wait(lambda: f.counts['/slow.jpg'] >= 1)
        open_reader(); b.wait(lambda: f.counts['/slow.jpg'] >= 2)
        turn(1, 2); mark = turn(0, 1); image(mark, 0, True)
        assert f.counts['/slow.jpg'] == 3, dict(f.counts)
        b.shot('cancelled-resumed'); b.nav(4)
        rows.append(dict(flow='cancelled page resumes when revisited', requests=3))

        attempts = f.counts['/unsupported.jpg']
        mark = load('/reorder'); image(mark, 0, False)
        b.wait(lambda: 'BROWSER JS UPDATE' in b.logs()[mark:])
        b.shot('reorder-before'); im = Image.open(a.output/'reorder-before.png').convert('RGB')
        x, y, w, h = b.box('content')
        points = [(px, py) for py in range(y, y+h) for px in range(x, x+w)
                  if all(abs(c-v) < 9 for c, v in zip(im.getpixel((px, py)), (208, 96, 32)))]
        assert len(points) > 15
        mark = len(b.logs()); b.tap((min(p[0] for p in points)+max(p[0] for p in points))//2,
                                  (min(p[1] for p in points)+max(p[1] for p in points))//2)
        b.wait(lambda: 'BROWSER JS UPDATE' in b.logs()[mark:]); image(mark, 0, True)
        open_reader(); turn(1, 2); unchanged('/unsupported.jpg', attempts + 1)
        b.shot('reorder-retains-reason')
        rows.append(dict(flow='DOM reorder preserves permanent failure by source URL'))

        # Direct-image navigation uses a separate decode path from HTML images.
        b.nav(4); mark = len(b.logs()); b.navigate(root + '/streamed-oversized.jpg'); image(mark, 0, False)
        code = int(re.findall(r'BROWSER IMAGE ERROR index=0 code=(-?\d+)', b.logs()[mark:])[-1])
        # Host input scratch can run out before the streamed byte cap; resource
        # exhaustion remains retryable while an explicit size-limit failure does not.
        assert code in (-9, -16), code
        attempts = f.counts['/streamed-oversized.jpg']; mark = open_reader()
        if code == -9: image(mark, 0, False)
        unchanged('/streamed-oversized.jpg', attempts + (code == -9))
        b.shot('direct-stream-failure')
        rows.append(dict(flow='direct streamed image propagates resource or byte limit', code=code))
        b.nav(4); mark = len(b.logs()); b.navigate(root + '/invalid.jpg'); image(mark, 0, False)
        assert 'BROWSER IMAGE ERROR index=0 code=-3' in b.logs()[mark:]
        attempts = f.counts['/invalid.jpg']; open_reader(); unchanged('/invalid.jpg', attempts)
        b.shot('direct-unsupported-retains-reason')
        rows.append(dict(flow='direct unsupported image retains failure without another request'))
        b.nav(4); mark = len(b.logs()); b.navigate(root + '/ok.jpg'); image(mark, 0, True)
        attempts = f.counts['/ok.jpg']; open_reader(); unchanged('/ok.jpg', attempts)
        b.shot('direct-success-reader'); b.nav(4); unchanged('/ok.jpg', attempts)
        rows.append(dict(flow='direct image opens and closes comic reader using cached decode'))
        toggle_gif(); load('/gif-disabled'); open_reader(); unchanged('/page.gif', 0)
        b.shot('gif-disabled-reader'); b.nav(4)
        mark = len(b.logs()); toggle_gif(); open_reader(); image(mark, 0, True)
        assert f.counts['/page.gif'] == 1
        b.shot('gif-enabled-recovered')
        rows.append(dict(flow='disabled GIF avoids requests and recovers after enabling'))
        assert all(error not in b.logs() for error in ('BROWSER JS ERROR', 'BROWSER DRAW ERROR', 'PXA Guest exception:'))
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows, requests=dict(f.counts),
            oversized_encoded_bytes=len(oversized())), indent=2)+'\n')
        print('Image failure simulator PASS:', len(rows), 'flows', flush=True)
    finally: b.close(); f.close()


if __name__ == '__main__': main()
