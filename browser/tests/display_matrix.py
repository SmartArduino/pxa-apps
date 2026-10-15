#!/usr/bin/env python3
"""Capture the real package at different DPI, shapes and safe margins."""
import argparse, json, os, socket, subprocess, tempfile, time
from pathlib import Path
from PIL import Image
from geometry import geometry, navigation

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['simulator','package','publisher-key','ime-package','output']:
        p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);rows=[]
    profiles=[('pai-touch',296,240,160,58,False),('fractional-dpi',296,240,130,0,False),
              ('high-dpi',480,480,320,60,False),('round',240,240,160,120,True),
              ('portrait',240,320,160,20,False),('wide',640,360,240,24,False)]
    for name,w,h,dpi,corner,circle in profiles:
        with tempfile.TemporaryDirectory(prefix='pxa-view-') as work:
            state=Path(work)/'state';state.mkdir();control=Path(work)/'c.sock'
            log_path=a.output/(name+'.log')
            env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',
                     PXA_IME_PACKAGE=str(a.ime_package.resolve()))
            args=[str(a.simulator.resolve()),'--package',str(a.package.resolve()),'--publisher-key',str(a.publisher_key.resolve()),
                  '--state-root',str(state),'--width',str(w),'--height',str(h),'--density-dpi',str(dpi),'--pxadb-control-socket',str(control)]
            args+=['--round'] if circle else ['--corner-radius',str(corner)]
            with log_path.open('wb') as log:
                process=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT)
                try:
                    deadline=time.monotonic()+15
                    while not control.exists():
                        assert process.poll() is None,log_path.read_text(errors='replace')
                        assert time.monotonic()<deadline,'control timeout';time.sleep(.05)
                    time.sleep(.7)
                    def command(text):
                        with socket.socket(socket.AF_UNIX) as c:
                            c.settimeout(20);c.connect(str(control));c.sendall((text+'\n').encode());data=b''
                            while block:=c.recv(65536):data+=block
                        return data
                    def shot(suffix):
                        header,data=command('SCREENSHOT').split(b'\n',1);assert header==f'PNG {len(data)}'.encode()
                        (a.output/(name+'-'+suffix+'.png')).write_bytes(data)
                    layout=geometry(log_path.read_text(errors='replace'))
                    hx,hy=navigation(layout,2)
                    command(f'TOUCH DOWN 0 {hx} {hy}');time.sleep(.12)
                    command(f'TOUCH UP 0 {hx} {hy}');time.sleep(.25)
                    shot('home')
                    x,y=navigation(layout,4)
                    with Image.open(a.output/(name+'-home.png')) as picture:
                        icon=picture.convert('RGB').crop((x-8,y-8,x+9,y+9))
                        ink=sum(r<100 and g<150 and b>40
                                for r,g,b in (icon.getpixel((i,j)) for j in range(17) for i in range(17)))
                        assert ink>=8,(name,'More icon strokes disappeared',ink)
                    command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.12);command(f'TOUCH UP 0 {x} {y}');time.sleep(.3)
                    shot('menu')
                    if name == 'pai-touch':
                        def tap(x,y):
                            command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.12)
                            command(f'TOUCH UP 0 {x} {y}');time.sleep(.2)
                        tap(layout['content'][0]+layout['content'][2]//2,layout['content'][1]+70+16)  # Select DuckDuckGo in More.
                        saved=list(state.rglob('bookmarks.txt'))
                        assert len(saved)==1 and '#prefs 15 1\n' in saved[0].read_text(),saved
                        tap(*navigation(layout,2));tap(145,layout['content'][1]+24)
                        deadline=time.monotonic()+10
                        while 'PXA IME load status=0' not in log_path.read_text(errors='replace'):
                            assert process.poll() is None
                            assert time.monotonic()<deadline,log_path.read_text(errors='replace')
                            time.sleep(.05)
                        command('KEY BACK');time.sleep(.2)
                    text=log_path.read_text(errors='replace');assert 'BROWSER ERROR' not in text and 'BROWSER DRAW ERROR' not in text,text
                    rows.append({'profile':name,'width':w,'height':h,'dpi':dpi,'corner':corner,'round':circle,'passed':True})
                    (a.output/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2));print(name,flush=True)
                finally:
                    process.terminate()
                    try:process.wait(timeout=8)
                    except subprocess.TimeoutExpired:process.kill();process.wait()
    print('display matrix PASS',flush=True)
if __name__=='__main__':main()
