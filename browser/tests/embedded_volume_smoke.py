#!/usr/bin/env python3
"""Verify Guest volume capture and system fallback in the desktop system shell.

Use a trace-enabled browser fixture-start package. All installed packages,
application preferences and system volume changes stay in a temporary profile.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
from geometry import geometry, navigation


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','output'):
        parser.add_argument('--'+key,type=Path,required=True)
    a=parser.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='embedded-volume-') as scratch:
        work=Path(scratch);packages=work/'packages';packages.mkdir()
        state=work/'state';state.mkdir()
        shutil.copytree(a.package,packages/'pxa-browser')
        (state/'user-preferences').write_text('1 1 0 60 80\n')
        control=work/'control.sock';logpath=a.output/'runtime.log'
        env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
        args=[str(a.simulator.resolve()),'--width','296','--height','240',
              '--density-dpi','160','--corner-radius','58','--gestures',
              '--installed-packages-root',str(packages),'--publisher-key',
              str(a.publisher_key.resolve()),'--state-root',str(state),
              '--launch','pxa-browser','--pxadb-control-socket',str(control)]
        with logpath.open('wb') as log:
            process=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
            def command(value):
                with socket.socket(socket.AF_UNIX) as c:
                    c.settimeout(20);c.connect(str(control));c.sendall((value+'\n').encode())
                    data=b''
                    while block:=c.recv(65536):data+=block
                assert not data.startswith(b'ERR'),(value,data)
                return data
            def tap(x,y):
                command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.12)
                command(f'TOUCH UP 0 {x} {y}');time.sleep(.3)
            def swipe(x,y0,y1):
                command(f'TOUCH DOWN 0 {x} {y0}');time.sleep(.1)
                for i in range(1,25):
                    command(f'TOUCH MOVE 0 {x} {y0+(y1-y0)*i//24}');time.sleep(.02)
                command(f'TOUCH UP 0 {x} {y1}');time.sleep(.5)
            def shot(name):
                header,data=command('SCREENSHOT').split(b'\n',1)
                assert header==f'PNG {len(data)}'.encode()
                (a.output/(name+'.png')).write_bytes(data)
            def volume():return int((state/'user-preferences').read_text().split()[3])
            try:
                end=time.monotonic()+20
                while not control.exists() or 'BROWSER DONE status=200' not in logpath.read_text(errors='replace'):
                    assert process.poll() is None,logpath.read_text(errors='replace')
                    assert time.monotonic()<end,logpath.read_text(errors='replace')
                    time.sleep(.05)
                time.sleep(.5);layout=geometry(logpath.read_text(errors='replace'))
                command('KEY VOLUME_DOWN');time.sleep(.5);assert volume()==50
                tap(*navigation(layout,4));x,y,w,h=layout['content']
                swipe(x+w//2,y+h-8,y+8)
                offset=min(max(0,175-h),h-16)
                tap(x+w//2,y+140-offset+16);shot('option')
                prefs=list(state.rglob('bookmarks.txt'));assert len(prefs)==1,prefs
                assert '#volume-scroll 1\n' in prefs[0].read_text()
                tap(*navigation(layout,4));time.sleep(.5)
                command('KEY VOLUME_DOWN');time.sleep(.5);assert volume()==50
                swipe(148,238,166)
                command('KEY VOLUME_DOWN');time.sleep(.5);assert volume()==40
                shot('background')
                (a.output/'report.json').write_text(json.dumps(dict(passed=True,layout=layout,
                    uncaptured_changes_volume=True,captured_keeps_volume=True,
                    background_releases_volume=True),indent=2)+'\n')
                print('embedded volume PASS',flush=True)
            finally:
                process.terminate()
                try:process.wait(timeout=10)
                except subprocess.TimeoutExpired:process.kill();process.wait()


if __name__=='__main__':main()
