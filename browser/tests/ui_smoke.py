#!/usr/bin/env python3
"""Exercise custom home sites, input, scrollbar dragging/hiding and menu exit.

Use a PXA_BROWSER_TRACE=1 package starting at fixture.py's root. All state is
isolated. TEXT edits the focused LVGL text input through normal change events.
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
from geometry import geometry, navigation


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('simulator', 'package', 'publisher-key', 'ime-package', 'output'):
        p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--fixture-url', default='http://127.0.0.1:31878/')
    p.add_argument('--width', type=int, default=296)
    p.add_argument('--height', type=int, default=240)
    p.add_argument('--dpi', type=int, default=160)
    p.add_argument('--corner', type=int, default=58)
    p.add_argument('--round', action='store_true')
    a = p.parse_args();a.output.mkdir(parents=True, exist_ok=True)
    def dp(value):return max(1,(value*a.dpi+159)//160)
    with tempfile.TemporaryDirectory(prefix='browser-ui-') as work:
        state = Path(work)/'state';state.mkdir()
        control = Path(work)/'c.sock'
        logs = a.output/'runtime.log'
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
                   PXA_IME_PACKAGE=str(a.ime_package.resolve()))
        args = [str(a.simulator.resolve()), '--package', str(a.package.resolve()),
                '--publisher-key', str(a.publisher_key.resolve()), '--state-root', str(state),
                '--width', str(a.width), '--height', str(a.height), '--density-dpi', str(a.dpi),
                '--pxadb-control-socket', str(control)]
        args += ['--round'] if a.round else ['--corner-radius',str(a.corner)]
        process = None
        with logs.open('wb') as log:
            def text():return logs.read_text(errors='replace')
            def wait(predicate, timeout=15):
                end = time.monotonic()+timeout
                while not predicate():
                    assert process.poll() is None, text()
                    assert time.monotonic()<end, text()
                    time.sleep(.04)
                time.sleep(.15)
            def launch():
                nonlocal process
                mark = logs.stat().st_size
                process = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT)
                wait(lambda:control.exists() and 'BROWSER DONE status=200' in text()[mark:])
            def command(value):
                with socket.socket(socket.AF_UNIX) as c:
                    c.settimeout(20);c.connect(str(control));c.sendall((value+'\n').encode());data=b''
                    while block:=c.recv(65536):data+=block
                assert not data.startswith(b'ERR'), (value,data)
                return data
            def tap(x,y,hold=.12):
                command(f'TOUCH DOWN 0 {x} {y}');time.sleep(hold)
                command(f'TOUCH UP 0 {x} {y}');time.sleep(.25)
            def nav(index):tap(*navigation(geometry(text()),index))
            def shot(name):
                header,data=command('SCREENSHOT').split(b'\n',1)
                assert header==f'PNG {len(data)}'.encode()
                path=a.output/(name+'.png');path.write_bytes(data)
                return Image.open(path).convert('RGB')
            def enter(value):
                for at in range(0,len(value),30):
                    command('TEXT '+value[at:at+30]);time.sleep(.12)
                shot('editor')
                x,y,w,h=geometry(text())['address'];tap(x+w-dp(15),y+h//2)
            def add(value):
                nav(4)
                x,y,w,h=geometry(text())['content'];tap(x+w//4,y+dp(16))
                wait(lambda:'PXA IME load status=0' in text())
                enter(value)
            def saved():
                files=list(state.rglob('bookmarks.txt'));assert len(files)<=1,files
                return files[0].read_text() if files else ''
            def exit_menu():
                nav(4)
                x,y,w,h=geometry(text())['content'];tap(x+w*3//4,y+dp(16))
                assert process.wait(timeout=12)==0,text()
            def drag_bar(bottom):
                x,y,w,h=geometry(text())['content'];edge=x+w-dp(4)
                command(f'TOUCH DOWN 0 {edge} {y+(h-dp(4) if not bottom else dp(4))}')
                time.sleep(.12)
                command(f'TOUCH MOVE 0 {edge} {y+(h-1 if bottom else 0)}');time.sleep(.12)
                command(f'TOUCH UP 0 {edge} {y+(h-1 if bottom else 0)}');time.sleep(.25)
            def volume_option():
                nav(4)
                x,y,w,h=geometry(text())['content']
                command(f'TOUCH DOWN 0 {x+w//2} {y+h-12}');time.sleep(.1)
                command(f'TOUCH MOVE 0 {x+w//2} {y+12}');time.sleep(.12)
                command(f'TOUCH UP 0 {x+w//2} {y+12}');time.sleep(.25)
                count=sum(bool(line) and not line.startswith('#') for line in saved().splitlines())
                offset=min(h-dp(24),max(0,(count+9)*(dp(32)+dp(3))-h))
                tap(x+w//2,y+4*(dp(32)+dp(3))-offset+dp(16))
                shot('volume-option');nav(4)
            try:
                launch();nav(2);shot('empty-home')
                root=a.fixture_url.rstrip('/')
                add(root+'/reading')
                wait(lambda:root+'/reading\n' in saved())
                shot('custom-home')
                assert saved().count(root+'/reading\n')==1
                add(root+'/reading')
                assert saved().count(root+'/reading\n')==1,'duplicate website'
                # Invalid schemes remain editable and never reach persistence.
                add('javascript:alert(1)')
                shot('invalid-site');assert 'javascript:' not in saved()
                command('KEY BACK');time.sleep(.3)
                # Long press removes the saved site; adding it again persists.
                x,y,w,h=geometry(text())['content'];tap(x+w//2,y+dp(8)+2*(dp(32)+dp(6))+dp(16),.7)
                wait(lambda:root+'/reading\n' not in saved())
                shot('removed-site');add(root+'/reading')
                # A real page is required to verify scrollbar movement.
                x,y,w,h=geometry(text())['address'];tap(x+w//2,y+h//2)
                enter(root+'/large');wait(lambda:'BROWSER NAV '+root+'/large' in text())
                time.sleep(1.1);top=shot('scrollbar-hidden-top')
                x,y,w,h=geometry(text())['content'];body=(x,y,x+w-dp(10),y+h)
                assert '#volume-scroll' not in saved()
                command('KEY VOLUME_DOWN');time.sleep(1.1)
                assert not ImageChops.difference(top.crop(body),shot('volume-disabled').crop(body)).getbbox()
                volume_option();wait(lambda:'#volume-scroll 1\n' in saved())
                command('KEY VOLUME_DOWN');time.sleep(.25)
                assert ImageChops.difference(top.crop(body),shot('volume-next').crop(body)).getbbox()
                command('KEY VOLUME_UP');time.sleep(1.1)
                assert not ImageChops.difference(top.crop(body),shot('volume-previous').crop(body)).getbbox()
                command('KEY_DOWN VOLUME_DOWN');time.sleep(.2);pressed=shot('volume-held-start')
                time.sleep(.5);repeated=shot('volume-held-repeat')
                assert ImageChops.difference(pressed.crop(body),repeated.crop(body)).getbbox()
                command('KEY_UP VOLUME_DOWN');time.sleep(.3);released=shot('volume-released')
                time.sleep(.5)
                assert not ImageChops.difference(released.crop(body),shot('volume-release-stopped').crop(body)).getbbox()
                # Focusing a real input releases capture, even while the app is visible.
                x0,y0,w0,h0=geometry(text())['address'];tap(x0+w0//2,y0+h0//2)
                command('KEY_DOWN VOLUME_DOWN');time.sleep(.5);command('KEY_UP VOLUME_DOWN')
                command('KEY BACK');time.sleep(.4)
                assert not ImageChops.difference(released.crop(body),shot('volume-input-suspended').crop(body)).getbbox()
                drag_bar(False);time.sleep(1.1)
                x,y,w,h=geometry(text())['content'];edge=x+w-dp(4)
                command(f'TOUCH DOWN 0 {edge} {y+dp(8)}');time.sleep(.15)
                command(f'TOUCH MOVE 0 {edge} {y+h-1}');time.sleep(.25)
                dragging=shot('scrollbar-dragging-bottom')
                command(f'TOUCH UP 0 {edge} {y+h-1}');time.sleep(.15)
                visible=shot('scrollbar-visible-bottom')
                time.sleep(1.15);hidden=shot('scrollbar-hidden-bottom')
                strip=(x+w-dp(8),y,x+w,y+h)
                assert ImageChops.difference(visible.crop(strip),hidden.crop(strip)).getbbox()
                body=(x,y,x+w-dp(10),y+h)
                assert not ImageChops.difference(visible.crop(body),hidden.crop(body)).getbbox()
                # Drag back to the top, checking the boundary without page taps.
                command(f'TOUCH DOWN 0 {edge} {y+h-dp(8)}');time.sleep(.12)
                command(f'TOUCH MOVE 0 {edge} {y}');time.sleep(.12)
                command(f'TOUCH UP 0 {edge} {y}');time.sleep(1.1)
                returned=shot('scrollbar-returned-top')
                assert not ImageChops.difference(top.crop(body),returned.crop(body)).getbbox()
                exit_menu()
                launch();nav(2);shot('restored-home')
                assert saved().count(root+'/reading\n')==1
                assert '#volume-scroll 1\n' in saved()
                x,y,w,h=geometry(text())['content'];tap(x+w//2,y+dp(8)+2*(dp(32)+dp(6))+dp(16))
                wait(lambda:'BROWSER NAV '+root+'/reading' in text())
                shot('saved-site-opened');exit_menu()
                assert 'BROWSER DRAW ERROR' not in text() and 'BROWSER ERROR' not in text(),text()
                (a.output/'results.json').write_text(json.dumps(dict(passed=True,
                    width=a.width,height=a.height,dpi=a.dpi,round=a.round,
                    manual_site=True,duplicate_rejected=True,invalid_kept_editable=True,
                    long_press_remove=True,persistent_restart=True,
                    scrollbar_drag=True,scrollbar_hide=True,menu_exit=True,
                    volume_default_off=True,volume_up_down=True,volume_hold_repeat=True,
                    volume_release_stops=True,volume_input_suspended=True,
                    volume_persistent=True),indent=2)+'\n')
                print('browser UI smoke PASS',flush=True)
            finally:
                if process is not None and process.poll() is None:
                    process.terminate()
                    try:process.wait(timeout=8)
                    except subprocess.TimeoutExpired:process.kill();process.wait()


if __name__=='__main__':main()
