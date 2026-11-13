#!/usr/bin/env python3
"""Opt-in original-site comic reading in the real PXA simulator.

Uses live author pages, native input, and original image responses. A changed or
unavailable site fails explicitly. Write output below local/artifacts/.
"""
import argparse
import json
from pathlib import Path
import re
import time

from harness import Browser

PEPPER = 'https://www.peppercarrot.com/cn/webcomic/ep01_Potion-of-Flight.html'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    a = p.parse_args()
    b = Browser(a.simulator, a.package, a.publisher_key, a.ime_package, a.output,
                width=480, height=480, corner=0)
    rows = []

    def loaded(mark):
        b.wait(lambda: 'BROWSER DONE status=' in b.logs()[mark:] or
               'BROWSER ERROR' in b.logs()[mark:], timeout=50)
        assert 'BROWSER DONE status=200' in b.logs()[mark:], b.logs()[mark:][-3000:]

    def reader(mark):
        b.wait(lambda: 'BROWSER COMIC index=' in b.logs()[mark:], timeout=30)
        return tuple(map(int, re.findall(r'BROWSER COMIC index=(\d+) page=(\d+) count=(\d+)',
                                        b.logs()[mark:])[-1]))

    def decoded(mark, index):
        b.wait(lambda: re.search(r'BROWSER IMAGE (?:DONE|ERROR) index=' + str(index) + r'\b',
                                b.logs()[mark:]), timeout=50)
        matches = re.findall(r'BROWSER IMAGE DONE index=' + str(index) +
                             r'\b[^\n]*pixels=(\d+)x(\d+) resident=(\d+)', b.logs()[mark:])
        assert matches, b.logs()[mark:][-3000:]
        w, h, size = map(int, matches[-1])
        return dict(width=w, height=h, resident_bytes=size)

    def open_reader():
        b.nav(4); time.sleep(.5)
        x, y, w, h = b.box('content')
        b.tap(x+w-2, y+h-1)
        mark = len(b.logs())
        b.tap(x+w//2, y+h-b.dp(18))
        return (*reader(mark), mark)

    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        mark = len(b.logs()); b.navigate(PEPPER); loaded(mark)
        b.shot('pepper-original-page')
        index, page, count, opened = open_reader()
        assert page == 1 and count == 5, (index, page, count)
        decoded(0, index); b.shot('pepper-chapter-header')
        mark = len(b.logs()); b.nav(1)
        index, page, count = reader(mark)
        assert page == 2
        normal = decoded(mark, index)
        mark = len(b.logs()); b.nav(3)
        detailed = decoded(mark, index)
        assert normal['width'] * normal['height'] <= 32768
        assert 32768 < detailed['width'] * detailed['height'] <= 262144
        b.shot('pepper-page1-detail')
        rows.append(dict(site='peppercarrot', url=PEPPER, check='original Chinese page and detail reload',
                         normal=normal, detailed=detailed))
        x, y, w, h = b.box('address'); b.tap(x+w//2, y+h//2)
        b.shot('pepper-page1-zoom2')
        for expected in (3, 4, 5):
            mark = len(b.logs()); b.nav(1)
            index, page, count = reader(mark)
            assert page == expected
            image = decoded(mark, index)
            b.shot('pepper-page' + str(expected-1))
            rows.append(dict(site='peppercarrot', check='original chapter image', page=expected-1, **image))
        mark = len(b.logs()); b.nav(1); loaded(mark)
        index, page, count = reader(mark)
        assert page == 1 and 'ep02_Rainbow-potions.html' in b.logs()[mark:]
        image = decoded(mark, index); b.shot('pepper-next-chapter')
        rows.append(dict(site='peppercarrot', check='original rel=next chapter opens in reader', **image))
        b.nav(4)
        mark = len(b.logs()); b.navigate('https://xkcd.com/#comic'); loaded(mark)
        index, page, count, opened = open_reader()
        assert index == 2, 'xkcd #comic no longer selects the expected comic image: ' + str(index)
        # Detail mode persists in the session. Wait for its fresh decode,
        # rather than accepting the ordinary thumbnail loaded before opening.
        image = decoded(opened, index); b.shot('xkcd-original-comic')
        assert image['width'] > 64 and image['height'] > 64
        rows.append(dict(site='xkcd', url='https://xkcd.com/#comic',
                         check='fragment-selected original comic image', **image))
        assert 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        print('live comic reading PASS:', len(rows), 'checks', flush=True)
    finally:
        (a.output/'results.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2))
        b.close()


if __name__ == '__main__': main()
