#!/usr/bin/env python3
"""Opt-in public-site reading checks in the real PXA simulator.

Network/content changes are recorded as failures, not hidden by a local replay.
This verifies entry pages and scrolling; it does not establish account login,
all linked pages or full script compatibility. Store output in local/artifacts/.
"""
import argparse
import json
from pathlib import Path
import re
import time

from harness import Browser

SITES = [
    ('poetry', 'https://m.gushiwen.cn/'),
    ('it-news', 'https://www.ithome.com/rss/'),
    ('developer-blogs', 'https://feed.cnblogs.com/blog/sitehome/rss'),
    ('npr-text', 'https://text.npr.org/'),
    ('wikisource', 'https://zh.wikisource.org/zh-hans/%E9%A6%96%E9%A1%B5'),
]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--site', action='append', help='Optional name=https://url; repeatable')
    args = p.parse_args()
    sites = [tuple(item.split('=', 1)) for item in args.site] if args.site else SITES
    b = Browser(args.simulator, args.package, args.publisher_key, args.ime_package,
                args.output, width=480, height=480, corner=0)
    rows = []
    try:
        b.wait(lambda: 'BROWSER VIEWPORT' in b.logs())
        for name, url in sites:
            assert re.fullmatch(r'[a-z0-9-]+', name) and url.startswith(('http://', 'https://'))
            mark = len(b.logs())
            row = dict(name=name, url=url, passed=False)
            try:
                b.navigate(url)
                b.wait(lambda: 'BROWSER DONE status=' in b.logs()[mark:] or
                       'BROWSER ERROR' in b.logs()[mark:], timeout=45)
                time.sleep(3)
                recent = b.logs()[mark:]
                loaded = re.search(r'BROWSER DONE status=(\d+).*?text=(\d+)', recent)
                assert loaded and int(loaded[1]) == 200 and int(loaded[2]) > 100, recent[-4000:]
                assert 'BROWSER DRAW ERROR' not in recent and 'PXA Guest exception:' not in recent
                b.shot(name + '-top')
                x, y, w, h = b.box('content')
                for _ in range(2):
                    cx, start, distance = x + w // 2, y + h * 4 // 5, h * 3 // 5
                    b.command(f'TOUCH DOWN 0 {cx} {start}')
                    for step in range(1, 13):
                        b.command(f'TOUCH MOVE 0 {cx} {start-distance*step//12}')
                        time.sleep(.025)
                    b.command(f'TOUCH UP 0 {cx} {start-distance}')
                time.sleep(.4)
                b.shot(name + '-reading')
                row.update(passed=True, status=int(loaded[1]), text_bytes=int(loaded[2]),
                           script_messages=re.findall(r'BROWSER JS[^\n\x1b]*', recent))
            except AssertionError as error:
                row['error'] = str(error)[-4000:]
                b.shot(name + '-failed')
            rows.append(row)
            (args.output / 'results.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2))
            print(name, 'PASS' if row['passed'] else 'FAIL', flush=True)
        assert all(row['passed'] for row in rows), rows
        print('live sites reading PASS:', len(rows), 'entry pages and scrolling', flush=True)
    finally:
        b.close()


if __name__ == '__main__':
    main()
