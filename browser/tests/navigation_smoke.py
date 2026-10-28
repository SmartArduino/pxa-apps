#!/usr/bin/env python3
"""Exercise reading-position history using the real simulator and input path.

Build with PXA_BROWSER_TRACE=1 and PXA_BROWSER_START_URL pointing at the
fixture base URL, then run fixture.py. --output belongs in local/artifacts.
No application state or history is injected by this test.
"""
import argparse
import json
from pathlib import Path
import re
import time
from urllib.parse import urlencode
from urllib.request import urlopen
from uuid import uuid4

from harness import Browser


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--fixture-url', default='http://127.0.0.1:31878/')
    args = parser.parse_args()
    root = args.fixture_url.rstrip('/')
    query = '?' + urlencode({'run': uuid4().hex})
    main_url = root + '/navigation/main' + query
    other_url = root + '/navigation/other' + query
    reading_url = root + '/navigation/reading' + query
    browser = Browser(args.simulator, args.package, args.publisher_key,
                      args.ime_package, args.output)
    results = []

    def scroll():
        values = re.findall(r'BROWSER SCROLL y=(\d+)', browser.logs())
        assert values, 'test package must trace BROWSER SCROLL y=<position>'
        return int(values[-1])

    def counts():
        with urlopen(root + '/navigation/counts' + query, timeout=5) as response:
            return json.load(response)

    def checked(name, expected, requests=None):
        browser.wait(lambda: scroll() == expected)
        observed_requests = counts()
        if requests is not None:
            assert observed_requests == requests, (name, requests, observed_requests)
        assert 'BROWSER ERROR' not in browser.logs(), browser.logs()[-6000:]
        assert 'BROWSER DRAW ERROR' not in browser.logs(), browser.logs()[-6000:]
        browser.shot(name)
        results.append({'name': name, 'expected_scroll': expected,
                        'scroll': scroll(), 'requests': observed_requests})
        (args.output / 'results.json').write_text(json.dumps(results, indent=2))
        print(name, 'scroll=' + str(expected), flush=True)

    def clear_address():
        # harness.navigate() first goes home, which would save the old
        # position and hide the address-bar navigation regression.
        mark = len(browser.logs())
        before = scroll()
        requests = counts()
        x, y, width, height = browser.box('address')
        browser.tap(x + width // 2, y + height // 2)
        # Use the visible clear action beside Done. This preserves the
        # current page/history and keeps the ordinary native input focused.
        x, y, width, height = browser.box('address')
        browser.tap(x + width - browser.dp(45), y + height // 2)
        browser.wait(lambda: scroll() == before)
        no_fetch(mark, requests)
        return mark

    def address(url, network=True):
        mark = clear_address()
        browser.enter(url)
        if network:
            loaded(mark)
        return mark

    def loaded(mark):
        def ready():
            recent = browser.logs()[mark:]
            done = recent.rfind('BROWSER DONE status=200')
            return done >= 0 and 'BROWSER SCROLL y=' in recent[done:]
        browser.wait(ready)

    def history(index, network=False):
        mark = len(browser.logs())
        browser.nav(index)
        if network:
            loaded(mark)
        return mark

    def drag_down():
        before = scroll()
        x, y, width, height = browser.box('content')
        center = x + width // 2
        start = y + height - 16
        distance = min(72, height - 32)
        browser.command(f'TOUCH DOWN 0 {center} {start}')
        time.sleep(.06)
        for step in range(1, 13):
            browser.command(f'TOUCH MOVE 0 {center} {start-distance*step//12}')
            time.sleep(.025)
        browser.command(f'TOUCH UP 0 {center} {start-distance}')
        browser.wait(lambda: scroll() > before)
        return scroll()

    def top():
        x, y, width, _ = browser.box('content')
        # The scrollbar handles track taps even when its transient thumb
        # has faded. Its top maps to an exact position of zero.
        browser.tap(x + width - 2, y + 1)
        browser.wait(lambda: scroll() == 0)

    def no_fetch(mark, requests):
        assert 'BROWSER NAV ' not in browser.logs()[mark:], browser.logs()[mark:]
        assert counts() == requests, (requests, counts())

    try:
        browser.wait(lambda: 'BROWSER DONE status=200' in browser.logs()
                     and 'BROWSER SCROLL y=' in browser.logs())

        address(main_url)
        assert scroll() == 0, 'plain page should start at the top'
        saved = drag_down()
        saved = drag_down()
        requests = counts()
        mark = clear_address()
        browser.command('KEY BACK')
        time.sleep(.25)
        checked('clear-address-cancel-keeps-page', saved, requests)
        no_fetch(mark, requests)
        address(other_url)
        history(0, network=True)
        checked('address-back-restores-position', saved)

        address(main_url + '#one', network=False)
        browser.wait(lambda: scroll() > saved)
        anchor_one = scroll()
        one = drag_down()
        assert one > anchor_one
        requests = counts()

        mark = address(main_url + '#two', network=False)
        browser.wait(lambda: scroll() > one)
        no_fetch(mark, requests)
        anchor_two = scroll()
        two = drag_down()
        assert two > anchor_two
        checked('same-document-second-position', two, requests)

        mark = history(0)
        checked('same-document-back-restores-position', one, requests)
        no_fetch(mark, requests)
        mark = history(0)
        checked('same-document-back-to-fragmentless-position', saved, requests)
        no_fetch(mark, requests)
        mark = history(1)
        checked('same-document-forward-to-first-position', one, requests)
        no_fetch(mark, requests)
        mark = history(1)
        checked('same-document-forward-restores-position', two, requests)
        no_fetch(mark, requests)

        # Repeating the current fragment does not append a history entry or
        # discard the still-reusable document behind its forward entry.
        mark = history(0)
        checked('repeat-visit-back-to-first-position', one, requests)
        no_fetch(mark, requests)
        mark = address(main_url + '#one', network=False)
        checked('repeat-current-fragment', anchor_one, requests)
        no_fetch(mark, requests)
        mark = history(1)
        checked('repeat-visit-preserves-forward-document', two, requests)
        no_fetch(mark, requests)

        # A deliberate refresh must fetch even for the current fragment URL,
        # while retaining its reading position rather than jumping to #two.
        requests = dict(requests)
        requests['/navigation/main'] += 1
        mark = len(browser.logs())
        browser.nav(3)
        loaded(mark)
        checked('fragment-reload-fetches-and-retains-position', two, requests)

        address(other_url)
        history(0, network=True)
        checked('cross-document-fragment-back-restores-position', two)

        top()
        address(other_url)
        history(0, network=True)
        checked('fragment-history-zero-stays-at-top', 0)

        address(reading_url)
        assert scroll() > 0, 'new reading page should initially find mw-content-text'
        top()
        address(other_url)
        history(0, network=True)
        checked('reading-history-zero-stays-at-top', 0)
        print('navigation smoke PASS:', len(results), 'checks', flush=True)
    finally:
        browser.close()


if __name__ == '__main__':
    main()
