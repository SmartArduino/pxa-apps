#!/usr/bin/env python3
"""Exercise the real PXA package over the simulator control socket.

Build with PXA_BROWSER_START_URL pointing to tests/fixture.py first. Use the
same fixture address in --fixture-url. Screenshots and checked runtime logs
are saved; no browser automation framework or desktop window is required.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from PIL import Image, ImageChops
from geometry import geometry, content_crop


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--simulator', type=Path, required=True)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--publisher-key', type=Path, required=True)
    p.add_argument('--ime-package', type=Path, required=True)
    p.add_argument('--fixture-url', default='http://127.0.0.1:31878/')
    p.add_argument('--fixture-log', type=Path, help='Optional fixture stdout, to check actual HTTP headers')
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pxa-browser-') as scratch:
        control = Path(scratch) / 'control.sock'
        state = Path(scratch) / 'state'
        state.mkdir()
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                   PXA_IME_PACKAGE=str(a.ime_package.resolve()))
        log_path = a.output / 'runtime.log'
        log_file = log_path.open('wb')
        process = subprocess.Popen([
            str(a.simulator.resolve()), '--package', str(a.package.resolve()),
            '--publisher-key', str(a.publisher_key.resolve()), '--state-root', str(state),
            '--width', '296', '--height', '240', '--density-dpi', '130',
            '--pxadb-control-socket', str(control)], env=env, stdout=log_file,
            stderr=subprocess.STDOUT)
        rows = []
        try:
            deadline = time.monotonic() + 20
            while not control.exists():
                assert process.poll() is None, log_path.read_text(errors='replace')
                assert time.monotonic() < deadline, 'simulator control timeout'
                time.sleep(.05)

            def command(text):
                with socket.socket(socket.AF_UNIX) as client:
                    client.settimeout(20)
                    client.connect(str(control))
                    client.sendall((text + '\n').encode())
                    data = bytearray()
                    while block := client.recv(65536):
                        data.extend(block)
                    return bytes(data)

            def tap(x, y):
                assert command(f'TOUCH DOWN 0 {x} {y}').startswith(b'OK')
                time.sleep(.10)
                assert command(f'TOUCH UP 0 {x} {y}').startswith(b'OK')
                time.sleep(.15)

            def screenshot(name):
                header, data = command('SCREENSHOT').split(b'\n', 1)
                assert header == f'PNG {len(data)}'.encode(), header
                (a.output / f'{name}.png').write_bytes(data)

            def checked(name, fn, expected=None, delay=.25):
                before = len(log_path.read_bytes())
                fn()
                deadline = time.monotonic() + 8
                while expected and expected not in log_path.read_text(errors='replace')[before:]:
                    assert process.poll() is None, log_path.read_text(errors='replace')
                    assert time.monotonic() < deadline, (name, expected, log_path.read_text(errors='replace')[before:])
                    time.sleep(.05)
                time.sleep(delay)
                screenshot(name)
                recent = log_path.read_text(errors='replace')[before:]
                assert 'BROWSER DRAW ERROR' not in recent and 'BROWSER ERROR' not in recent, recent
                rows.append({'name': name, 'expected': expected, 'log': recent})
                (a.output / 'results.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2))
                print(name, flush=True)

            deadline = time.monotonic() + 10
            while 'BROWSER DONE status=200' not in log_path.read_text(errors='replace'):
                assert time.monotonic() < deadline, log_path.read_text(errors='replace')
                time.sleep(.05)
            screenshot('launch')
            root = a.fixture_url.rstrip('/')
            nav = lambda path: 'BROWSER NAV ' + root + path
            # At 130 DPI without corner insets, body starts at y=29 and the
            # bottom toolbar avoids the system strip at y=220..239. All commands use physical pixels.
            checked('next', lambda: tap(50, 64), nav('/next'))
            checked('back', lambda: tap(44, 207), nav('/'))
            checked('forward', lambda: tap(100, 207), nav('/next'))
            checked('back-again', lambda: tap(44, 207), nav('/'))
            checked('form', lambda: tap(50, 82), nav('/form'))
            checked('field-editor', lambda: tap(50, 64), 'PXA IME load status=0')
            checked('field-done', lambda: tap(275, 15))
            checked('post', lambda: tap(50, 108), nav('/echo'))
            checked('post-back', lambda: tap(44, 207), nav('/form'))
            checked('form-back', lambda: tap(44, 207), nav('/'))
            checked('redirect', lambda: tap(40, 99), 'BROWSER DONE status=200')
            checked('redirect-back', lambda: tap(44, 207), nav('/'))
            checked('slow', lambda: tap(100, 99), nav('/slow'), .5)
            checked('cancel', lambda: tap(212, 207))
            checked('cancel-back', lambda: tap(44, 207), nav('/'))
            checked('large', lambda: tap(40, 118), nav('/large'), 1)
            def drag_page():
                assert command('TOUCH DOWN 0 150 170').startswith(b'OK')
                for step in range(1, 25):
                    time.sleep(.02)
                    assert command(f'TOUCH MOVE 0 150 {170-step*3}').startswith(b'OK')
                assert command('TOUCH UP 0 150 98').startswith(b'OK')
            checked('large-scrolled', drag_page, delay=1.1)
            # Toggle More twice to force a full repaint of the same scroll
            # position. It must exactly match the retained partial repaint.
            checked('large-force-full', lambda: (tap(268,207),tap(268,207)))
            with Image.open(a.output/'large.png') as before, \
                 Image.open(a.output/'large-scrolled.png') as partial, \
                 Image.open(a.output/'large-force-full.png') as full:
                before, partial, full = (image.convert('RGB') for image in (before,partial,full))
                body=content_crop(geometry(log_path.read_text(errors='replace')))
                assert ImageChops.difference(before.crop(body),partial.crop(body)).getbbox(), 'drag did not scroll'
                assert not ImageChops.difference(partial.crop(body),full.crop(body)).getbbox(), 'partial redraw left stale pixels'
                layout = geometry(log_path.read_text(errors='replace'))
                for x,y,width,height in (layout['address'],layout['toolbar']):
                    area=(x+1,y+1,x+width-1,y+height-1)
                    assert not ImageChops.difference(before.crop(area),partial.crop(area)).getbbox(), 'scroll changed navigation chrome'
            checked('large-back', lambda: tap(44, 207), nav('/'))
            checked('404', lambda: tap(80, 118), 'BROWSER DONE status=404')
            checked('404-back', lambda: tap(44, 207), nav('/'))
            checked('plain', lambda: tap(40, 135), nav('/plain'))
            checked('plain-back', lambda: tap(44, 207), nav('/'))
            checked('compressed', lambda: tap(100, 135), nav('/gzip'))
            checked('compressed-back', lambda: tap(44, 207), nav('/'))
            checked('menu', lambda: tap(268, 207))
            checked('bookmark', lambda: tap(80, 48))
            checked('font-less', lambda: tap(80, 77))
            checked('font-more', lambda: tap(215, 77))
            saved = list(state.rglob('bookmarks.txt'))
            assert len(saved) == 1, saved
            preferences = saved[0].read_text()
            assert '#prefs 15 0\n' in preferences and root + '/\n' in preferences, preferences
            (a.output / 'saved-preferences.txt').write_text(preferences)
            checked('ua-mobile', lambda: tap(150, 132))
            assert '#ua mobile\n' in saved[0].read_text()
            server_mark = a.fixture_log.stat().st_size if a.fixture_log else 0
            checked('ua-close', lambda: tap(268,207))
            checked('ua-mobile-request', lambda: tap(212,207), 'BROWSER DONE status=200')
            if a.fixture_log:
                recent = a.fixture_log.read_bytes()[server_mark:].decode()
                assert 'Chrome/143' in recent and '"save_data": "on"' in recent, recent
            checked('ua-open', lambda: tap(268,207))
            checked('ua-lightweight', lambda: tap(150,132))
            assert '#ua mobile' not in saved[0].read_text()
            server_mark = a.fixture_log.stat().st_size if a.fixture_log else 0
            checked('ua-close-restored', lambda: tap(268,207))
            checked('ua-lightweight-request', lambda: tap(212,207), 'BROWSER DONE status=200')
            if a.fixture_log:
                recent = a.fixture_log.read_bytes()[server_mark:].decode()
                assert 'TextMode' in recent and '"save_data": "on"' in recent, recent
            checked('home', lambda: tap(156, 207))
            # Seed only this disposable test profile, then exercise bookmark
            # loading after restart and the real mixed-language page layout.
            saved[0].write_text('#prefs 15 0\n'+root+'/reading\n')
            def restart():
                nonlocal process
                arguments = process.args
                process.terminate()
                process.wait(timeout=10)
                process = subprocess.Popen(arguments, env=env, stdout=log_file,
                                           stderr=subprocess.STDOUT)
                deadline = time.monotonic() + 15
                while not control.exists():
                    assert process.poll() is None
                    assert time.monotonic() < deadline, 'restart control timeout'
                    time.sleep(.05)
            checked('reading-restart', restart, 'BROWSER DONE status=200')
            checked('reading-menu', lambda: tap(268,207))
            checked('reading-home', lambda: tap(156,207))
            layout=geometry(log_path.read_text(errors='replace'))
            x,y,w,h=layout['content']
            # Home contains the saved site below its search row. The menu has
            # additional settings and must not be tested with a stale row Y.
            checked('reading', lambda: tap(x+w//2,y+61), 'BROWSER DONE status=200')
            print('simulator browser smoke PASS', flush=True)
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            log_file.close()


if __name__ == '__main__':
    main()
