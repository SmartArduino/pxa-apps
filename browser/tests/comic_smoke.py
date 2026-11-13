#!/usr/bin/env python3
"""Real PXA comic reader: navigation, detail reload, pan, cancel and chapters."""
import argparse
import json
from pathlib import Path
import re
import time
from urllib.request import urlopen

from PIL import Image, ImageChops
from harness import Browser
from comic_fixture import Fixture


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--fixture-url', help='Use an existing fixture; otherwise start an isolated local server')
    p.add_argument('--width', type=int, default=296)
    p.add_argument('--height', type=int, default=240)
    p.add_argument('--dpi', type=int, default=160)
    p.add_argument('--round', action='store_true')
    a = p.parse_args()
    fixture = None if a.fixture_url else Fixture().start()
    b = Browser(a.simulator, a.package, a.publisher_key, a.ime_package, a.output,
                width=a.width, height=a.height, dpi=a.dpi, corner=0, round_screen=a.round)
    root = a.fixture_url.rstrip('/') if a.fixture_url else 'http://127.0.0.1:' + str(fixture.server.server_port)
    rows = []

    def counts():
        with urlopen(root + '/counts', timeout=5) as response: return json.load(response)

    def toolbar(index): b.nav(index) # reader has five controls

    def decoded(mark, index):
        b.wait(lambda: re.search(r'BROWSER IMAGE DONE index=' + str(index) + r'\b', b.logs()[mark:]), timeout=30)
        match = re.findall(r'BROWSER IMAGE DONE index=' + str(index) +
                           r'\b[^\n]*pixels=(\d+)x(\d+) resident=(\d+)', b.logs()[mark:])[-1]
        return dict(width=int(match[0]), height=int(match[1]), resident_bytes=int(match[2]))

    def reader(expected, count=5):
        b.wait(lambda: re.search(r'BROWSER COMIC[^\n]*page=' + str(expected) +
                                r' count=' + str(count) + r'\b', b.logs()))

    def pan():
        x, y, w, h = b.box('content'); cx, top, distance = x+w//2, y+h*4//5, h*3//5
        b.command(f'TOUCH DOWN 0 {cx} {top}')
        for step in range(1, 13):
            b.command(f'TOUCH MOVE 0 {cx} {top-distance*step//12}'); time.sleep(.025)
        b.command(f'TOUCH UP 0 {cx} {top-distance}')

    def shot(name): b.shot(name)

    try:
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs())
        mark = len(b.logs()); b.navigate(root + '/chapter1')
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs()[mark:])
        b.nav(4); time.sleep(.4)
        x, y, w, h = b.box('content'); b.tap(x+w-2, y+h-1)
        mark = len(b.logs()); b.tap(x+w//2, y+h-b.dp(18))
        reader(1)
        normal = decoded(0, 1); shot('page1-width-fit') # cached thumbnails need no refetch
        assert normal['width'] * normal['height'] <= 32768
        requests = counts()
        mark = len(b.logs()); toolbar(3)
        detailed = decoded(mark, 1); shot('page1-detail')
        assert 32768 < detailed['width'] * detailed['height'] <= 262144, detailed
        assert counts()['/page1.jpg'] == requests['/page1.jpg'] + 1
        rows.append(dict(check='detail reload uses more source pixels', normal=normal, detailed=detailed))
        pan(); shot('page1-panned')
        diff = ImageChops.difference(Image.open(a.output/'page1-detail.png').convert('RGB'),
                                    Image.open(a.output/'page1-panned.png').convert('RGB'))
        assert diff.getbbox(), 'width-fit page did not pan'
        x, y, w, h = b.box('address'); b.tap(x+w//2, y+h//2); shot('page1-zoom2')
        assert ImageChops.difference(Image.open(a.output/'page1-panned.png').convert('RGB'),
                                    Image.open(a.output/'page1-zoom2.png').convert('RGB')).getbbox()
        toolbar(2); shot('page1-fit-screen'); toolbar(2); shot('page1-fit-width-again')
        rows.append(dict(check='width fit, vertical pan and screen fit'))
        mark = len(b.logs()); toolbar(1); reader(2)
        png = decoded(mark, 2); shot('page2-png-detail')
        assert png['width'] * png['height'] > 32768
        mark = len(b.logs()); toolbar(1); reader(3)
        decoded(mark, 3); shot('long-page-top'); pan(); shot('long-page-panned')
        rows.append(dict(check='JPEG/PNG pages and long strips keep document order'))
        # Cancel a slow download by moving to the next page, then recover from
        # a real decoder failure without leaving or reloading the chapter.
        toolbar(1); reader(4); mark = len(b.logs()); toolbar(1); reader(5)
        b.wait(lambda: 'BROWSER IMAGE ERROR index=5' in b.logs()[mark:])
        shot('broken-page'); rows.append(dict(check='slow cancellation and broken image recovery'))
        mark = len(b.logs()); toolbar(1)
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs()[mark:] and
               'BROWSER COMIC index=0 page=1 count=1' in b.logs()[mark:])
        decoded(mark, 0); shot('next-chapter')
        mark = len(b.logs()); toolbar(0)
        b.wait(lambda: 'BROWSER DONE status=200' in b.logs()[mark:] and
               'BROWSER COMIC index=1 page=1 count=5' in b.logs()[mark:])
        decoded(mark, 1); shot('previous-chapter')
        rows.append(dict(check='declared rel=next/prev chapters reopen the reader'))
        mark = len(b.logs()); toolbar(4); time.sleep(.4); shot('back-to-webpage')
        assert 'BROWSER NAV ' not in b.logs()[mark:], 'closing reader refetched the chapter'
        assert 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        rows.append(dict(check='close retains webpage without another chapter request'))
        (a.output/'results.json').write_text(json.dumps(rows, indent=2))
        print('comic reader simulator PASS:', len(rows), 'flows', flush=True)
    finally:
        b.close()
        if fixture: fixture.close()


if __name__ == '__main__': main()
