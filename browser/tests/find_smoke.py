#!/usr/bin/env python3
"""Exercise page find through real pointer input, native text entry and Canvas.

Use a TRACE package starting at fixture.py's root, with isolated simulator state.
Outputs belong under local/artifacts/. No external sites or credentials are used.
"""
import argparse
import json
from pathlib import Path
import re
import time

from PIL import Image
from harness import Browser
from geometry import TOOL_ROWS, navigation


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--fixture-url', default='http://127.0.0.1:31878/')
    parser.add_argument('--width', type=int, default=296)
    parser.add_argument('--height', type=int, default=240)
    parser.add_argument('--dpi', type=int, default=160)
    parser.add_argument('--corner', type=int, default=0)
    parser.add_argument('--round', action='store_true')
    args = parser.parse_args()
    browser = Browser(args.simulator, args.package, args.publisher_key,
                      args.ime_package, args.output, width=args.width, height=args.height,
                      dpi=args.dpi, corner=args.corner, round_screen=args.round)
    results = []

    def result():
        matches = re.findall(r'BROWSER FIND result=(\d+) count=(\d+) offset=(\d+)', browser.logs())
        return tuple(map(int, matches[-1])) if matches else None

    def scroll():
        return int(re.findall(r'BROWSER SCROLL y=(\d+)', browser.logs())[-1])

    def checked(name, ordinal, count, highlight=True):
        browser.wait(lambda: result() and result()[:2] == (ordinal, count))
        browser.shot(name)
        yellow = highlight_pixels(args.output / (name + '.png'))
        assert bool(yellow) == highlight, (name, yellow)
        assert 'BROWSER DRAW ERROR' not in browser.logs()
        results.append(dict(name=name, result=result(), scroll=scroll(), highlight_pixels=yellow))

    def toolbar(index):
        browser.tap(*navigation({'toolbar': browser.box('toolbar')}, index, count=4))

    def highlight_pixels(path):
        data = Image.open(path).convert('RGB').tobytes()
        return sum(r > 230 and 180 < g < 240 and b < 170
                   for r, g, b in zip(data[0::3], data[1::3], data[2::3]))

    def open_find():
        browser.nav(4)
        x, y, width, height = browser.box('content')
        # The final two rows are Page Find and Comic Reader.
        browser.tap(x + width - 2, y + height - 1)
        menu_height = TOOL_ROWS * (browser.dp(32) + browser.dp(3))
        menu_scroll = max(0, menu_height - height)
        browser.tap(x + width // 2,
                    y + (TOOL_ROWS - 2) * (browser.dp(32) + browser.dp(3))
                    - menu_scroll + browser.dp(16))

    def edit_query(value):
        # Existing text is selected by normal TextInput focus. Clear explicitly
        # through the visible X to also verify its non-navigation behavior.
        x, y, width, height = browser.box('address')
        browser.tap(x + width - browser.dp(45), y + height // 2)
        browser.enter(value)

    try:
        browser.wait(lambda: 'BROWSER DONE status=200' in browser.logs())
        mark = len(browser.logs())
        browser.navigate(args.fixture_url.rstrip('/') + '/find')
        browser.wait(lambda: 'BROWSER DONE status=200' in browser.logs()[mark:])
        nav_count = browser.logs().count('BROWSER NAV ')
        open_find()
        browser.enter('needle')
        checked('ascii-first', 1, 3)
        first = result()[2]
        toolbar(1)
        checked('ascii-next', 2, 3)
        assert result()[2] > first and scroll() > 0
        toolbar(1)
        checked('ascii-last', 3, 3)
        toolbar(1)
        checked('ascii-wrap', 1, 3)
        assert result()[2] == first
        toolbar(0)
        checked('ascii-previous-wrap', 3, 3)

        # Cancel an edit without changing the old result or reading position.
        before = scroll()
        toolbar(2)
        browser.command('TEXT cancelled-query')
        browser.command('KEY BACK')
        time.sleep(.3)
        checked('cancel-edit-keeps-result', 3, 3)
        assert scroll() == before

        toolbar(2)
        edit_query('x' * 129)
        browser.shot('overlong-query-stays-in-editor')
        browser.command('KEY BACK')
        time.sleep(.3)
        checked('invalid-edit-keeps-result', 3, 3)
        assert scroll() == before

        toolbar(2)
        edit_query('中文跨行查找')
        checked('chinese-first', 1, 3)
        toolbar(1)
        checked('chinese-inline-link-and-wrap', 2, 3)
        toolbar(2)
        edit_query('never-present-token')
        checked('no-matches', 0, 0, highlight=False)
        before = scroll()
        toolbar(0)
        toolbar(1)
        assert result()[:2] == (0, 0) and scroll() == before

        toolbar(2)
        edit_query('needle')
        checked('find-again', 1, 3)
        before = scroll()
        toolbar(3)
        browser.shot('close-keeps-position')
        assert highlight_pixels(args.output / 'close-keeps-position.png') == 0
        assert scroll() == before
        open_find()
        browser.enter('needle')
        checked('reopen-find', 1, 3)
        before = scroll()
        browser.command('KEY BACK')
        time.sleep(.3)
        browser.shot('back-closes-find')
        assert highlight_pixels(args.output / 'back-closes-find.png') == 0
        assert scroll() == before
        assert browser.logs().count('BROWSER NAV ') == nav_count, 'find must not refetch the document'

        # Closing find restores ordinary navigation and address input.
        mark = len(browser.logs())
        browser.navigate(args.fixture_url.rstrip('/') + '/next')
        browser.wait(lambda: 'BROWSER DONE status=200' in browser.logs()[mark:])
        browser.shot('ordinary-navigation-restored')
        (args.output / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2))
        print('page find simulator PASS:', len(results), 'checks; no document refetch', flush=True)
    finally:
        browser.close()


if __name__ == '__main__':
    main()
