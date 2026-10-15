#!/usr/bin/env python3
"""Opt-in live reading-site checks; external availability is not a CI guarantee."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from PIL import Image, ImageChops

SITES = [
    ('nahan', 'https://zh.wikisource.org/zh-hans/%E5%90%B6%E5%96%8A'),
    ('panghuang', 'https://zh.wikisource.org/zh-hans/%E5%BD%B7%E5%BE%A8'),
    ('alice', 'https://www.cs.cmu.edu/~rgs/alice-I.html'),
]
CHAPTERS = {
    'nahan': ('kuangren', '%E7%8B%82%E4%BA%BA%E6%97%A5%E8%A8%98', 143),
    'panghuang': ('zhufu', '%E7%A5%9D%E7%A6%8F', 120),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['simulator', 'package', 'publisher-key', 'ime-package', 'output']:
        parser.add_argument('--' + key, type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    log_path = args.output / 'runtime.log'
    with tempfile.TemporaryDirectory(prefix='pxa-reading-') as work:
        control = Path(work) / 'c.sock'
        state = Path(work) / 'state'
        state.mkdir()
        command_line = [
            str(args.simulator.resolve()), '--package', str(args.package.resolve()),
            '--publisher-key', str(args.publisher_key.resolve()),
            '--state-root', str(state), '--width', '296', '--height', '240',
            '--density-dpi', '160', '--corner-radius', '58',
            '--pxadb-control-socket', str(control),
        ]
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                   PXA_IME_PACKAGE=str(args.ime_package.resolve()))
        with log_path.open('wb') as log:
            process = subprocess.Popen(command_line, env=env, stdout=log,
                                       stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 15
                while not control.exists():
                    assert process.poll() is None, log_path.read_text(errors='replace')
                    assert time.monotonic() < deadline, 'control timeout'
                    time.sleep(.05)
                time.sleep(.8)

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
                    command(f'TOUCH DOWN 0 {x} {y}')
                    time.sleep(.12)
                    command(f'TOUCH UP 0 {x} {y}')
                    time.sleep(.2)

                def swipe(amount):
                    command('TOUCH DOWN 0 150 160')
                    for step in range(1, 13):
                        y = 160 - round(amount * step / 12)
                        command(f'TOUCH MOVE 0 150 {y}')
                        time.sleep(.015)
                    command(f'TOUCH UP 0 150 {160 - amount}')
                    time.sleep(.2)

                def screenshot(name):
                    header, data = command('SCREENSHOT').split(b'\n', 1)
                    assert header == f'PNG {len(data)}'.encode()
                    path = args.output / (name + '.png')
                    path.write_bytes(data)
                    return path

                def text():
                    return log_path.read_text(errors='replace')

                def wait_done(mark):
                    deadline = time.monotonic() + 30
                    while 'BROWSER DONE status=200' not in text()[mark:]:
                        assert 'BROWSER ERROR' not in text()[mark:], text()[mark:]
                        assert time.monotonic() < deadline, text()[mark:]
                        time.sleep(.05)
                    time.sleep(.3)
                    return [line for line in text()[mark:].splitlines()
                            if 'BROWSER ' in line]

                rows = []
                screenshot('home')
                for index, (name, target) in enumerate(SITES, start=1):
                    tap(150, 206)
                    for _ in range(index):
                        swipe(36)
                    before = screenshot(name + '-home')
                    mark = len(text())
                    tap(145, 131)
                    logs = wait_done(mark)
                    assert any('BROWSER NAV ' + target in line for line in logs), logs
                    screenshot(name)
                    if name in CHAPTERS:
                        chapter, path, y = CHAPTERS[name]
                        mark = len(text())
                        tap(55, y)
                        chapter_logs = wait_done(mark)
                        target = 'BROWSER NAV https://zh.wikisource.org/wiki/' + path
                        assert any(target in line for line in chapter_logs), chapter_logs
                        screenshot(chapter + '-top')
                        swipe(150)
                        swipe(150)
                        screenshot(chapter + '-reading')
                        mark = len(text())
                        tap(50, 206)
                        wait_done(mark)
                        rows.append({'chapter': chapter, 'logs': chapter_logs})
                    tap(50, 206)
                    after = screenshot(name + '-back')
                    with Image.open(before) as first, Image.open(after) as restored:
                        body = (24, 72, 272, 173)
                        difference = ImageChops.difference(first.crop(body),
                                                          restored.crop(body))
                        assert difference.getbbox() is None, 'Home scroll not restored'
                    rows.append({'site': name, 'logs': logs, 'home_scroll_restored': True})
                    print(name, 'live content + back PASS', flush=True)
                (args.output / 'results.json').write_text(
                    json.dumps(rows, ensure_ascii=False, indent=2))
                assert 'BROWSER DRAW ERROR' not in text()
            finally:
                process.terminate()
                try:
                    process.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == '__main__':
    main()
