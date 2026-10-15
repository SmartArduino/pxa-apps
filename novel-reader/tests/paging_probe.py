"""Compare real cached-book page turns in an isolated full-system simulator.

The same deterministic UTF-8 text, preferences and Host are used for old/new
packages. Timings include socket input and desktop screenshot polling; they
are simulator observations, not device touch-to-photon measurements.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import socket
import statistics
import subprocess
import tempfile
import time
from PIL import Image

ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'tools/app.sh').exists())
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--binary', type=Path, default=ROOT / 'build/simulator/pai-touch/pxsys_desktop_simulator')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--width', type=int, default=296)
p.add_argument('--height', type=int, default=240)
p.add_argument('--dpi', type=int, default=160)
p.add_argument('--round', action='store_true')
p.add_argument('--turns', type=int, default=24)
a = p.parse_args()
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=True)
key = ROOT / 'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'
publisher = hashlib.sha256(key.read_bytes()).hexdigest()
book = ''.join(f'第{i}段 山间来信\n清晨的风沿着山路吹来，林舟打开书页，记下今天的旅程。白鸟带来消息，河边的书屋又亮起了灯。\n\n' for i in range(1, 1601)).encode()
url = 'http://127.0.0.1:18764/paging-book.txt'
value = 14695981039346656037
for byte in url.encode():
    value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
with tempfile.TemporaryDirectory(prefix='reader-paging-') as scratch:
    state = Path(scratch) / 'state'
    control = Path(scratch) / 'control.sock'
    binary = a.binary.resolve()
    state.mkdir()
    subprocess.run([str(binary.parent / 'pxsys_package_installer'), '--storage-root', str(state),
                    '--publisher-key', str(key), '--source', str(a.package.resolve()),
                    '--expected-id', 'pxa-novel-reader-qa'], check=True, capture_output=True)
    private = state / 'private-files' / f'{publisher}-pxa-novel-reader-qa'
    private.mkdir(parents=True, exist_ok=True)
    (private / f'b{value:x}.txt').write_bytes(book)
    saved = dict(version=2, settings=dict(font=16, gap=6, theme=3, seconds=15),
                 sources=[dict(name='经典直读', url='https://www.gutenberg.org/cache/epub/', kind=2, enabled=True)],
                 shelf=[dict(title='林间长篇', author='原创 CC0', url=url, toc='', position=0,
                             total=len(book), complete=True, validator='')])
    (private / 'reader-state.json').write_text(json.dumps(saved, ensure_ascii=False))
    log_path = a.output / 'runtime.log'
    def logs():
        text = re.sub(r'\x1b\[[0-9;]*m', '', log_path.read_text(errors='replace'))
        return re.sub(r'READER-HITS ([0-9,;]+)', lambda m: '\n'.join(
            'READER-HIT action=%s arg=%s x=%s y=%s' % tuple(row.split(','))
            for row in m[1].split(';') if row), text)
    def wait(predicate, timeout=30):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            text = logs()
            assert process.poll() is None, text[-4000:]
            assert 'READER-ERROR' not in text and 'unreachable' not in text, text[-4000:]
            if predicate(text):
                return text
            time.sleep(.002)
        raise AssertionError(logs()[-4000:])
    def command(value):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(15)
            client.connect(str(control))
            client.sendall((value + '\n').encode())
            result = b''
            while data := client.recv(65536):
                result += data
        assert not result.startswith(b'ERR'), result
        return result
    def capture():
        header, data = command('SCREENSHOT').split(b'\n', 1)
        assert header == f'PNG {len(data)}'.encode()
        frame = Image.open(io.BytesIO(data)).convert('RGB')
        digest = hashlib.sha256(frame.crop((0, a.height // 5, a.width, a.height * 4 // 5)).tobytes()).hexdigest()
        return digest, data
    def hit(action):
        tail = logs()[logs().rfind('READER-UI '):]
        match = re.search(rf'READER-HIT action={action} arg=0 x=(\d+) y=(\d+)', tail)
        assert match, tail
        command(f'TAP {match[1]} {match[2]}')
    args = [str(binary), '--width', str(a.width), '--height', str(a.height), '--density-dpi', str(a.dpi),
            '--safe-insets', '12,12,12,12', '--locale', 'zh-CN', '--light', '--gestures',
            '--installed-packages-root', str(state / 'packages'), '--product-runner', str(binary.parent / 'pxsys_product_simulator'),
            '--publisher-key', str(key), '--state-root', str(state), '--pxadb-control-socket', str(control),
            '--launch', 'pxa-novel-reader-qa']
    args += ['--round'] if a.round else ['--corner-radius', '58']
    with log_path.open('w') as log:
        process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT,
                                   env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'))
        try:
            wait(lambda s: 'READER-HIT action=4 arg=0 ' in s)
            hit(4)
            wait(lambda s: 'READER-PAGES ' in s and 'READER-PAGE ' in s, 60)
            time.sleep(.5)
            previous, png = capture()
            (a.output / 'first-page.png').write_bytes(png)
            samples = []
            for index in range(a.turns + a.turns // 2):
                direction = 1 if index < a.turns else -1
                before = len(logs())
                begin = time.monotonic()
                command(f'TAP {a.width * (5 if direction > 0 else 1) // 6} {a.height // 2}')
                text = wait(lambda s: 'READER-PAGE ' in s[before:])
                page_ms = (time.monotonic() - begin) * 1000
                while True:
                    current, png = capture()
                    if current != previous:
                        break
                    assert time.monotonic() - begin < 10, logs()[-4000:]
                    time.sleep(.003)
                samples.append(dict(direction=direction, page_ms=page_ms,
                                    visible_ms=(time.monotonic() - begin) * 1000,
                                    offset=int(re.findall(r'READER-PAGE offset=(\d+)', text)[-1]), body_sha256=current))
                previous = current
                # Prevent the control driver's synthetic TAP release from the
                # previous operation overlapping the next press.
                time.sleep(.025)
            (a.output / 'last-page.png').write_bytes(png)
            time.sleep(.8) # Let an outstanding page-number lookup settle.
            before = len(logs())
            time.sleep(1.5)
            idle_redraws = logs()[before:].count('READER-NUMBER ')
            assert idle_redraws == 0, 'Idle reader keeps redrawing'
            for i in range(a.turns // 2):
                assert samples[a.turns + i]['body_sha256'] == samples[a.turns - 2 - i]['body_sha256'], 'Back page differs'
            def summary(key):
                values = sorted(s[key] for s in samples)
                return dict(median=statistics.median(values), p95=values[min(len(values)-1, int(len(values)*.95))], maximum=max(values))
            report = dict(passed=True, width=a.width, height=a.height, dpi=a.dpi, round=a.round,
                          package_sha256=hashlib.sha256(a.package.read_bytes()).hexdigest(),
                          book_sha256=hashlib.sha256(book).hexdigest(), samples=samples,
                          page_ms=summary('page_ms'), visible_ms=summary('visible_ms'), idle_redraws=idle_redraws,
                          reading_canvas_presents=logs().count('READER-NUMBER '),
                          observed_linear_pages=max(map(int,re.findall(r'linear_pages=(\d+)',logs())),default=0),
                          reader_stats=re.findall(r'READER-(?:CACHE|RENDER|MEM) [^\n]+', logs()))
            (a.output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
            print(json.dumps({k:v for k,v in report.items() if k not in ('samples','reader_stats')}, ensure_ascii=False))
        finally:
            process.terminate()
            process.wait(timeout=10)
