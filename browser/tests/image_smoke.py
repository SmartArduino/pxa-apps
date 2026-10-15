#!/usr/bin/env python3
"""Verify real streamed PNG/JPEG drawing and scrolling with a fixture-start package.

Build with PXA_BROWSER_START_URL=http://127.0.0.1:31878/images (or the route
matching --case), and start fixture.py separately. No external website required.
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
from geometry import geometry, navigation, content_crop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['simulator', 'package', 'publisher-key', 'ime-package', 'output']:
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--case', choices=['images', 'errors', 'capacity', 'slow', 'tall', 'direct', 'display', 'viewer', 'linked'], default='images')
    parser.add_argument('--width', type=int, default=296)
    parser.add_argument('--height', type=int, default=240)
    parser.add_argument('--dpi', type=int, default=160)
    parser.add_argument('--corner', type=int, default=58)
    parser.add_argument('--round', action='store_true')
    args = parser.parse_args();args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pxa-images-') as work:
        control = Path(work)/'c.sock'
        log_path = args.output/'runtime.log'
        command_line = [str(args.simulator.resolve()), '--package', str(args.package.resolve()),
                        '--publisher-key', str(args.publisher_key.resolve()), '--state-root', work,
                        '--width', str(args.width), '--height', str(args.height),
                        '--density-dpi', str(args.dpi), '--corner-radius', str(args.corner),
                        '--pxadb-control-socket', str(control)]
        if args.round: command_line.append('--round')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                   PXA_IME_PACKAGE=str(args.ime_package.resolve()))
        with log_path.open('wb') as log:
            process = subprocess.Popen(command_line, env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                def logs(): return log_path.read_text(errors='replace')
                def wait_for(predicate, timeout=15):
                    deadline = time.monotonic()+timeout
                    while not predicate():
                        assert process.poll() is None, logs()
                        assert time.monotonic()<deadline, logs()
                        time.sleep(.05)
                    time.sleep(.2)
                wait_for(control.exists)
                def command(text):
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(20);client.connect(str(control));client.sendall((text+'\n').encode())
                        data = bytearray()
                        while block := client.recv(65536): data.extend(block)
                    return bytes(data)
                def tap(x, y, hold=.12):
                    command(f'TOUCH DOWN 0 {x} {y}');time.sleep(hold)
                    command(f'TOUCH UP 0 {x} {y}');time.sleep(.2)
                def swipe(amount):
                    x = args.width//2
                    y = args.height*(2 if amount>0 else 1)//3
                    command(f'TOUCH DOWN 0 {x} {y}')
                    for step in range(1, 13):
                        command(f'TOUCH MOVE 0 {x} {y-round(amount*step/12)}');time.sleep(.02)
                    command(f'TOUCH UP 0 {x} {y-amount}');time.sleep(.25)
                def screenshot(name):
                    header, data = command('SCREENSHOT').split(b'\n', 1)
                    assert header == f'PNG {len(data)}'.encode()
                    path = args.output/(name+'.png');path.write_bytes(data)
                    return Image.open(path).convert('RGB')
                def colored(image):
                    # Excludes text and chrome: actual image pixels must be present.
                    return sum(max(rgb)-min(rgb)>65 and min(rgb)<170
                               for rgb in image.crop((args.width//5, args.height//3,
                                                      args.width*4//5, args.height*2//3)).getdata())
                if args.case == 'slow':
                    wait_for(lambda: 'BROWSER DONE' in logs())
                    time.sleep(.4);screenshot('loading')
                    tap(*navigation(geometry(logs()), 3))  # Stop; foreground coroutine must release HTTP and decoder.
                    time.sleep(.4);screenshot('cancelled')
                    assert 'BROWSER IMAGE DONE' not in logs()
                    tap(*navigation(geometry(logs()), 2));screenshot('home')
                elif args.case == 'errors':
                    wait_for(lambda: 'BROWSER IMAGE ERROR index=0' in logs())
                    screenshot('failed')
                    for _ in range(7): swipe(100)
                    time.sleep(.4)
                    assert 'BROWSER IMAGE DONE' not in logs()
                    assert all(f'BROWSER IMAGE ERROR index={i}' in logs() for i in range(4))
                    screenshot('errors-bottom')
                else:
                    wait_for(lambda: 'BROWSER IMAGE DONE index=0' in logs())
                    first = screenshot('loaded')
                    assert colored(first)>150, 'placeholder was drawn instead of decoded pixels'
                    if args.case in ('viewer', 'linked'):
                        layout = geometry(logs())
                        left, cy, safe_width, ch = layout['content']
                        # Choose the first image's longest visible colour band
                        # instead of assuming the title or image's height.
                        bands, start = [], None
                        for y in range(cy,cy+ch):
                            count = sum(max(first.getpixel((x,y)))-min(first.getpixel((x,y)))>65
                                        and min(first.getpixel((x,y)))<170
                                        for x in range(left,left+safe_width))
                            if count>safe_width//4:
                                if start is None:start=y
                            elif start is not None:bands.append((start,y));start=None
                        if start is not None:bands.append((start,cy+ch))
                        top,bottom_band = max(bands,key=lambda band:band[1]-band[0])
                        image_y = (top+bottom_band)//2
                        def viewer_button(index):tap(*navigation(layout,index,4))
                        mark = logs().count('BROWSER IMAGE DONE index=0')
                        tap(args.width//2,image_y, .7 if args.case=='linked' else .12)
                        assert 'BROWSER IMAGE VIEW index=0' in logs()
                        fitted = screenshot('viewer-fit')
                        viewer_button(3);zoomed = screenshot('viewer-zoom')
                        crop = content_crop(layout, 4)
                        assert ImageChops.difference(fitted.crop(crop),zoomed.crop(crop)).getbbox()
                        start_x, start_y = args.width//2, cy+ch*2//3
                        command(f'TOUCH DOWN 0 {start_x} {start_y}')
                        for step in range(1,13):
                            command(f'TOUCH MOVE 0 {start_x-step*2} {start_y-step*max(1,ch//60)}')
                            time.sleep(.02)
                        command(f'TOUCH UP 0 {start_x-24} {start_y-12*max(1,ch//60)}');time.sleep(.25)
                        panned = screenshot('viewer-pan')
                        assert ImageChops.difference(zoomed.crop(crop),panned.crop(crop)).getbbox()
                        viewer_button(2);refit = screenshot('viewer-refit')
                        assert not ImageChops.difference(fitted.crop(crop),refit.crop(crop)).getbbox()
                        viewer_button(0);returned = screenshot('viewer-returned')
                        assert not ImageChops.difference(first.crop(crop),returned.crop(crop)).getbbox()
                        assert logs().count('BROWSER IMAGE DONE index=0')==mark, 'preview downloaded its image again'
                        if args.case=='linked':
                            tap(args.width//2,image_y)
                            wait_for(lambda:'BROWSER NAV http://127.0.0.1:31878/next' in logs())
                            screenshot('linked-navigation')
                    elif args.case == 'images':
                        for _ in range(3): swipe(90)
                        wait_for(lambda: 'BROWSER IMAGE DONE index=1' in logs())
                        screenshot('jpeg')
                        # Force a full repaint at the current scroll position.
                        if (args.width,args.height,args.dpi)==(296,240,160):
                            time.sleep(1.1);before = screenshot('partial');tap(*navigation(geometry(logs()),4));tap(*navigation(geometry(logs()),4))
                            full = screenshot('full')
                            assert not ImageChops.difference(before.crop(content_crop(geometry(logs()))),
                                                             full.crop(content_crop(geometry(logs())))).getbbox()
                        for _ in range(4): swipe(-90)
                        returned = screenshot('returned')
                        assert colored(returned)>150
                    elif args.case == 'capacity':
                        for _ in range(12): swipe(90)
                        wait_for(lambda: 'BROWSER IMAGE DONE index=4' in logs())
                        for _ in range(12): swipe(-90)
                        wait_for(lambda: logs().count('BROWSER IMAGE DONE index=0')>=2)
                        screenshot('evicted-and-reloaded')
                    elif args.case == 'tall':
                        for _ in range(4): swipe(90)
                        middle = screenshot('middle')
                        assert colored(middle)>150, 'tall image disappeared while its top was offscreen'
                assert 'BROWSER DRAW ERROR' not in logs() and 'BROWSER ERROR' not in logs(), logs()
                (args.output/'results.json').write_text(json.dumps(
                    dict(case=args.case, width=args.width, height=args.height, dpi=args.dpi,
                         passed=True, image_logs=[line for line in logs().splitlines() if 'BROWSER IMAGE' in line]),
                    ensure_ascii=False, indent=2))
                print(args.case, args.width, args.height, args.dpi, 'PASS', flush=True)
            finally:
                process.terminate()
                try: process.wait(timeout=8)
                except subprocess.TimeoutExpired: process.kill();process.wait()


if __name__ == '__main__': main()
