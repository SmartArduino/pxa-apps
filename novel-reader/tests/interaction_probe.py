"""Check held presses and list navigation through real Host pointer events.

Private QA storage contains fifteen books, eight sources and a local cached
37-chapter book. No internet, user library or physical device is required.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time

ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'tools/app.sh').exists())
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--binary', type=Path, default=ROOT / 'build/simulator/reader-validation/pxsys_desktop_simulator')
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
binary = a.binary.resolve()
key = ROOT / 'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'
publisher = hashlib.sha256(key.read_bytes()).hexdigest()
records = []

for width, height, dpi, circle in [(296, 240, 160, False), (480, 480, 320, True),
                                    (240, 240, 160, True), (800, 480, 160, False)]:
    name = f'{width}x{height}-{dpi}' + ('-round' if circle else '')
    output = a.output / name
    output.mkdir(exist_ok=True)
    log_path = output / 'runtime.log'
    with tempfile.TemporaryDirectory(prefix='reader-interaction-') as tmp:
        state = Path(tmp) / 'state'
        state.mkdir()
        control = Path(tmp) / 'control.sock'
        subprocess.run([str(binary.parent / 'pxsys_package_installer'), '--storage-root', str(state),
                        '--publisher-key', str(key), '--source', str(a.package.resolve()),
                        '--expected-id', 'pxa-novel-reader-qa'], check=True, capture_output=True)
        private = state / 'private-files' / f'{publisher}-pxa-novel-reader-qa'
        private.mkdir(parents=True)
        url = 'https://fixture.invalid/chapters.txt'
        value = 14695981039346656037
        for byte in url.encode():
            value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
        body = ''.join(f'第{i}章 山间来信\n清晨的风吹过书屋，白鸟带来新的旅程。\n\n' for i in range(1, 38)).encode()
        (private / f'b{value:x}.txt').write_bytes(body)
        books = [dict(title=f'书屋 {i + 1}', author='原创 CC0', url=f'sample:book-{i}', toc='',
                      position=0, total=100, complete=True) for i in range(15)]
        books[1].update(title='三十七章验收书', url=url, total=len(body))
        saved = dict(version=4, sourceRevision=2,
                     settings=dict(font=16, gap=6, theme=3, seconds=15), shelf=books,
                     sources=[dict(name=f'自定义书源 {i + 1}', url=f'https://fixture.invalid/source-{i}',
                                   kind=1, enabled=True) for i in range(8)])
        (private / 'reader-state.json').write_text(json.dumps(saved, ensure_ascii=False))

        def logs():
            text = re.sub(r'\x1b\[[0-9;]*m', '', log_path.read_text(errors='replace'))
            return re.sub(r'READER-HITS ([0-9,;]+)', lambda m: '\n'.join(
                'READER-HIT action=%s arg=%s x=%s y=%s' % tuple(row.split(','))
                for row in m[1].split(';') if row), text)

        def screen():
            text = logs()
            return text[text.rfind('READER-UI '):]

        def wait(predicate, timeout=15):
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                text = logs()
                assert process.poll() is None, text[-4000:]
                assert 'READER-ERROR' not in text and 'unreachable' not in text, text[-4000:]
                if predicate(text):
                    return text
                time.sleep(.01)
            raise AssertionError(logs()[-6000:])

        def cmd(command):
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(15)
                client.connect(str(control))
                client.sendall((command + '\n').encode())
                data = b''
                while chunk := client.recv(65536):
                    data += chunk
            assert not data.startswith(b'ERR'), data
            return data

        def hit(action, arg=0):
            match = re.search(rf'READER-HIT action={action} arg={arg} x=(\d+) y=(\d+)', screen())
            assert match, (action, arg, screen())
            return int(match[1]), int(match[2])

        def tap(action, arg=0):
            x, y = hit(action, arg)
            before = len(logs())
            cmd(f'TAP {x} {y}')
            wait(lambda text: 'READER-UI ' in text[before:])
            time.sleep(.12)

        def capture(label):
            time.sleep(.15)
            header, png = cmd('SCREENSHOT').split(b'\n', 1)
            assert header == f'PNG {len(png)}'.encode()
            (output / (label + '.png')).write_bytes(png)

        def on_screen(number):
            return f'READER-UI screen={number} ' in screen()

        def ids(action):
            return tuple(map(int, re.findall(rf'READER-HIT action={action} arg=(\d+) ', screen())))

        def pages(action, count, label):
            seen = []
            while True:
                current = ids(action)
                assert current and current not in seen, (label, seen, current)
                seen.append(current)
                if current[-1] == count - 1:
                    break
                tap(24)
            assert [i for page in seen for i in page] == list(range(count)), (label, seen)
            capture(label + '-last')
            tap(24)
            assert ids(action) == seen[-1], 'Next passed the end'
            for previous in reversed(seen[:-1]):
                tap(23)
                assert ids(action) == previous, (label, seen, ids(action), previous)
            tap(23)
            assert ids(action) == seen[0], 'Previous passed the beginning'
            return len(seen)

        args = [str(binary), '--width', str(width), '--height', str(height), '--density-dpi', str(dpi),
                '--safe-insets', '12,12,12,12', '--locale', 'zh-CN', '--light', '--gestures',
                '--installed-packages-root', str(state / 'packages'), '--product-runner',
                str(binary.parent / 'pxsys_product_simulator'), '--publisher-key', str(key),
                '--state-root', str(state), '--pxadb-control-socket', str(control),
                '--launch', 'pxa-novel-reader-qa'] + (['--round'] if circle else ['--corner-radius', '58'])
        with log_path.open('w') as log:
            process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT,
                                       env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'))
            try:
                wait(lambda text: 'READER-UI screen=0 busy=0 ' in text)
                time.sleep(.25)
                capture('shelf')
                x, y = hit(4)
                began = time.monotonic()
                cmd(f'POINTER DOWN {x} {y}')
                time.sleep(.28)
                assert on_screen(0), 'Long press fired too early'
                wait(lambda text: on_screen(8), 2)
                held_ms = round((time.monotonic() - began) * 1000, 1)
                assert 480 <= held_ms < 1000, held_ms
                capture('held-open-menu')  # Finger is still down here.
                cmd(f'POINTER UP {x} {y}')
                time.sleep(.3)
                assert on_screen(8), 'Release clicked through the opened menu'
                tap(16)

                # A drag that returns to its origin must not become a tap/hold.
                x, y = hit(4)
                cmd(f'POINTER DOWN {x} {y}')
                time.sleep(.08)
                cmd(f'POINTER MOVE {x + 32} {y}')
                time.sleep(.05)
                cmd(f'POINTER MOVE {x} {y}')
                time.sleep(.65)
                assert on_screen(0)
                cmd(f'POINTER UP {x} {y}')
                time.sleep(.2)
                assert on_screen(0), 'Cancelled drag opened a book'

                # A cancelled touch must also cancel the pending timer.
                cmd(f'TOUCH DOWN 0 {x} {y}')
                time.sleep(.1)
                cmd(f'TOUCH CANCEL 0 {x} {y}')
                time.sleep(.7)
                assert on_screen(0), 'Cancelled touch opened a book'

                shelf_pages = pages(4, 15, 'shelf')
                tap(4)  # Short tap still opens normally.
                wait(lambda text: 'READER-PAGE ' in text)
                assert on_screen(3)
                cmd(f'TAP {width // 2} {height // 2}')
                wait(lambda text: on_screen(4))
                time.sleep(.15)
                tap(0)
                tap(2)
                capture('sources')
                source_pages = pages(21, 8, 'sources')
                tap(0)
                # Book 1 can be on the next page on very small displays.
                while 1 not in ids(4):
                    tap(24)
                tap(4, 1)
                wait(lambda text: on_screen(3))
                time.sleep(.5)
                cmd(f'TAP {width // 2} {height // 2}')
                wait(lambda text: on_screen(4))
                time.sleep(.15)
                tap(8)
                wait(lambda text: '共 37 章' in screen())
                capture('toc')
                toc_pages = pages(25, 37, 'toc')
                assert all(label not in logs() for label in ['text=上一组', 'text=下一组'])
                text = logs()
                record = dict(profile=name, passed=True, held_opens_before_release=True, held_ms=held_ms,
                              release_no_clickthrough=True, drag_cancels=True, touch_cancels=True,
                              tap_opens=True, shelf_pages=shelf_pages, source_pages=source_pages,
                              toc_pages=toc_pages, all_items_reachable=True, short_last_page_previous_correct=True,
                              max_linear_pages=max(map(int, re.findall(r'linear_pages=(\d+)', text))),
                              pool_peak=max(map(int, re.findall(r'pool_peak=(\d+)', text))),
                              allocation_failures=max(map(int, re.findall(r'failures=(\d+)', text))))
                assert record['allocation_failures'] == 0
                records.append(record)
                print(json.dumps(record), flush=True)
            finally:
                process.terminate()
                process.wait(timeout=10)

(a.output / 'report.json').write_text(json.dumps(dict(passed=True, cases=records,
    package_sha256=hashlib.sha256(a.package.read_bytes()).hexdigest()), ensure_ascii=False, indent=2) + '\n')
