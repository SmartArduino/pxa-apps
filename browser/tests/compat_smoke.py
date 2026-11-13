#!/usr/bin/env python3
"""Real PXA input -> radio group -> checked-only GET; Atom -> article link."""
import argparse
import json
import re
from pathlib import Path

from harness import Browser


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--fixture-url', default='http://127.0.0.1:31878/')
    args = p.parse_args()
    b = Browser(args.simulator, args.package, args.publisher_key, args.ime_package,
                args.output, width=480, height=480, corner=0)
    root = args.fixture_url.rstrip('/')
    checks = []

    def loaded(mark):
        b.wait(lambda: 'BROWSER DONE status=' in b.logs()[mark:])
        assert 'BROWSER DONE status=200' in b.logs()[mark:], b.logs()[mark:]

    def go(path):
        mark = len(b.logs())
        b.navigate(root + path)
        loaded(mark)
        return mark

    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        for name, path in [('static', '/compat-controls'), ('script', '/compat-controls-script')]:
            mark = go(path)
            if name == 'script':
                b.wait(lambda: 'BROWSER JS READY' in b.logs()[mark:] and
                       'BROWSER JS UPDATE' in b.logs()[mark:])
            b.shot(name + '-form')
            x, y, width, _ = b.box('content')
            mark = len(b.logs())
            b.tap(x + 80, y + 22 + 30 + 15)
            if name == 'script': b.wait(lambda: 'BROWSER JS UPDATE' in b.logs()[mark:])
            b.shot(name + '-selected')
            # Selecting the same radio twice must keep it checked.
            b.tap(x + 80, y + 22 + 30 + 15)
            mark = len(b.logs())
            b.tap(x + 40, y + 22 + 3 * 30 + 10)
            loaded(mark)
            # NAV diagnostics deliberately omit queries. The fixture returns
            # 200 only for exactly mode=en and the explicit empty checkbox.
            assert 'BROWSER NAV ' + root + '/compat-choice' in b.logs()[mark:]
            b.shot(name + '-submitted')
            checks.append(name + ': checked radio only, empty checkbox value, disabled upload ignored')
        go('/compat-atom')
        b.shot('atom-list')
        x, y, _, _ = b.box('content')
        mark = len(b.logs())
        b.tap(x + 60, y + 10)
        loaded(mark)
        assert 'BROWSER NAV ' + root + '/compat-article' in b.logs()[mark:]
        b.shot('atom-article')
        checks.append('Atom MIME, alternate article link, HTML title and summary')
        mark = go('/compat-icon')
        b.wait(lambda: 'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        decoded = re.search(r'BROWSER IMAGE DONE[^\n]*pixels=(\d+)x(\d+) resident=(\d+)', b.logs()[mark:])
        assert decoded and (int(decoded[1]), int(decoded[2])) == (25, 25), b.logs()[mark:]
        assert int(decoded[3]) <= 4096 # decoded pixels plus Host metadata
        b.shot('small-icon')
        checks.append('200px source icon decodes to declared 25px; decoded resource stays below 4 KiB')
        assert 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        (args.output / 'results.json').write_text(json.dumps(checks, indent=2))
        print('compatibility simulator PASS:', len(checks), 'flows', flush=True)
    finally:
        b.close()


if __name__ == '__main__':
    main()
