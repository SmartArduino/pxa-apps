#!/usr/bin/env python3
"""Exercise CDN MIME labels, content coding limits and cancellation on PXA."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import time

from PIL import Image
from harness import Browser
from image_fixtures import fixtures
from image_response_fixture import Fixture


class StableBrowser(Browser):
    def logs(self):
        text = self.log_path.read_text(errors='replace')
        return text[:text.rfind('\n')+1]

    def tap(self, x, y):
        self.command(f'TOUCH DOWN 0 {x} {y}')
        time.sleep(.01)
        self.command(f'TOUCH UP 0 {x} {y}')
        time.sleep(.22)

    def navigate(self, url):
        for _ in range(4):
            mark = len(self.logs())
            self.nav(2)
            self.wait(lambda: any(value in self.logs()[mark:] for value in (
                'BROWSER HIT action=3 index=0', 'BROWSER LONG action=3 index=0')))
            if 'BROWSER HIT action=3 index=0' in self.logs()[mark:]:
                break
        else:
            raise AssertionError('Home tap was not acknowledged')
        x, y, width, height = self.box('address')
        self.tap(x+width//2, y+height//2)
        self.enter(url)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--width', type=int, default=480)
    parser.add_argument('--height', type=int, default=480)
    parser.add_argument('--fixture-port', type=int, default=0)
    parser.add_argument('--large-images', action='store_true',
                        help='Expect a package and Host that support encoded images above 512 KiB')
    args = parser.parse_args()
    fixture = Fixture(args.fixture_port).start()
    origin = 'http://127.0.0.1:' + str(fixture.server.server_port)
    browser = None
    rows = []
    try:
        browser = StableBrowser(args.simulator, args.package, args.publisher_key,
            args.ime_package, args.output, width=args.width, height=args.height, corner=0)
        browser.wait(lambda: 'BROWSER VIEWPORT' in browser.logs())

        def load(route, success, source=None, code=None):
            mark = len(browser.logs())
            browser.navigate(origin+'/'+route)
            browser.wait(lambda: re.search(r'BROWSER IMAGE (DONE|ERROR) index=0\b',
                                          browser.logs()[mark:]), timeout=35)
            text = browser.logs()[mark:]
            expected = 'DONE' if success else 'ERROR'
            assert f'BROWSER IMAGE {expected} index=0' in text, text[-4000:]
            if code is not None:
                assert f'BROWSER IMAGE ERROR index=0 code={code}' in text, text[-4000:]
            browser.shot(route)
            if success:
                frame = Image.open(args.output/(route+'.png')).convert('RGB')
                x, y, width, height = browser.box('content')
                colored = sum(max(rgb)-min(rgb)>65 and min(rgb)<170 for rgb in
                    frame.crop((x, y, x+width, y+height)).getdata())
                assert colored > 5000, (route, colored)
                if source:
                    data = fixture.large_bytes if source == 'large' else fixtures()[source]
                    assert f'received={len(data)} ' in text, text[-4000:]
            assert not list(browser.state.rglob('compressed-image.tmp')), route
            rows.append({'route': route, 'success': success, 'expected_error': code})

        for route in ('small', 'mime', 'plain', 'gzip', 'direct-alias', 'direct-gzip'):
            load(route, True, 'photo.jpg')
        load('deflate', True, 'alpha.png')
        load('large', args.large_images, 'large' if args.large_images else None,
             None if args.large_images else -16)
        load('oversize', False, code=-16)
        load('broken', False)
        load('bomb', False, code=-16)
        load('html', False, code=-3)
        load('mime', True, 'photo.jpg')
        mark = len(browser.logs())
        browser.navigate(origin+'/slow')
        browser.wait(lambda: fixture.counts['/slow.jpg'] > 0)
        time.sleep(.2)
        browser.nav(3)
        time.sleep(.3)
        assert 'BROWSER IMAGE DONE index=0' not in browser.logs()[mark:]
        assert not list(browser.state.rglob('compressed-image.tmp'))
        browser.shot('cancelled')
        rows.append({'route': 'slow', 'cancelled_without_temporary_file': True})
        load('gzip', True, 'photo.jpg')
        mark = len(browser.logs())
        browser.command('KEY HOME')
        browser.wait(lambda: 'PXA RESOURCE BUDGET' in browser.logs()[mark:])
        budget = next(line for line in reversed(browser.logs().splitlines())
                      if 'PXA RESOURCE BUDGET' in line)
        assert 'remaining_internal=0 remaining_external=0' in budget, budget
        result = {'passed': True, 'checks': rows, 'resource_release': budget,
                  'manifest_sha256': hashlib.sha256((args.package/'manifest.pxm').read_bytes()).hexdigest()}
        (args.output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2), flush=True)
    finally:
        if browser is not None:
            browser.close()
        fixture.close()


if __name__ == '__main__':
    main()
