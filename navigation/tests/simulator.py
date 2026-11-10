#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the signed app in the real simulator. Save unaltered screenshots."""
import argparse
import os
from pathlib import Path
import socket
import subprocess
import time

p=argparse.ArgumentParser()
p.add_argument('--package',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--runner',type=Path,default=Path('build/simulator/pai-touch/pxsys_product_simulator'))
p.add_argument('--key',type=Path,default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
p.add_argument('--width',type=int,default=480)
p.add_argument('--height',type=int,default=480)
p.add_argument('--density-dpi',type=int,default=160)
p.add_argument('--arrival',action='store_true',help='Also complete the demo and start it again')
p.add_argument('--zoom-range',action='store_true',help='Exercise the entire illustrative zoom range')
p.add_argument('--safe-insets',default='0,0,0,0',help='Physical pixels: top,right,bottom,left')
a=p.parse_args();out=a.output.resolve()
if '/local/artifacts/' not in str(out):raise SystemExit('Output must be under local/artifacts/')
out.mkdir(parents=True,exist_ok=True);state=out/'state';state.mkdir(exist_ok=True)
sock=f'/tmp/pxa-navigation-qa-{os.getpid()}.sock'
scale=a.density_dpi/160
width=int(a.width/scale);height=int(a.height/scale)
top,right,bottom,left=[int(int(v)/scale) for v in a.safe_insets.split(',')]
safe_w=width-left-right;safe_h=height-top-bottom;compact=safe_h<400
margin=10 if compact else 16
env=os.environ.copy();env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='none',PXA_SIMULATOR_PERF='0')
def command(text,png=None):
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as c:
        c.settimeout(10);c.connect(sock);c.sendall((text+'\n').encode());f=c.makefile('rb');h=f.readline()
        if png:
            assert h.startswith(b'PNG '),h
            n=int(h.split()[1]);data=f.read(n);assert len(data)==n;(out/png).write_bytes(data);return data
        assert h.startswith(b'OK'),(text,h)
def tap(x,y):command(f'TAP {round(x*scale)} {round(y*scale)}');time.sleep(.3)
def back():command('KEY BACK');time.sleep(.3)
def drag(x,y,dx,dy):
    def point(phase,xx,yy):command(f'POINTER {phase} {round(xx*scale)} {round(yy*scale)}')
    point('DOWN',x,y)
    for i in range(1,11):point('MOVE',x+dx*i/10,y+dy*i/10);time.sleep(.03)
    point('UP',x+dx,y+dy);time.sleep(.6)
def shot(name):return command('SCREENSHOT AFTER_PRESENT',name+'.png')
with (out/'simulator.log').open('w') as log:
    process=subprocess.Popen([str(a.runner.resolve()),'--package',str(a.package.resolve()),'--publisher-key',str(a.key.resolve()),
        '--state-root',str(state),'--pxadb-control-socket',sock,'--width',str(a.width),'--height',str(a.height),
        '--density-dpi',str(a.density_dpi),'--safe-insets',a.safe_insets,'--locale','zh-CN'],stdout=log,stderr=subprocess.STDOUT,env=env)
    try:
        for _ in range(150):
            if process.poll() is not None:raise RuntimeError('Simulator exited; inspect simulator.log')
            if Path(sock).exists() and 'Navigation: ready' in (out/'simulator.log').read_text(errors='replace'):break
            time.sleep(.1)
        else:raise RuntimeError('App did not become ready')
        time.sleep(.4);shot('home')
        primary_x=width-right-margin-12-(55 if compact else 75)
        footer_y=height-bottom-margin-30
        tap(primary_x,footer_y);time.sleep(.8);first=shot('demo-start')
        time.sleep(1);assert first!=shot('demo-moving'),'Demo position did not move'
        if a.arrival:
            time.sleep(53);arrived=shot('arrived');time.sleep(1)
            assert arrived==shot('arrived-idle'),'Navigation did not stop on arrival'
            tap(primary_x,footer_y);time.sleep(.5);again=shot('demo-restarted')
            time.sleep(1);assert again!=shot('demo-restarted-moving'),'Arrived demo could not restart'
        deck_y=height-bottom-margin-(76 if compact else 116)
        tap(left+44,deck_y+48 if compact else footer_y);book=shot('book')
        tap(width-right-47,height-bottom-29);page=shot('book-next');assert page!=book,'Roadbook did not advance'
        time.sleep(1);assert shot('book-reading')==page,'Live fixes interrupted manual roadbook reading'
        back();back();time.sleep(4.3)
        before=shot('preview');drag(left+safe_w//2,top+78,40,12)
        assert shot('map-dragged')!=before,'Map drag did not change the camera'
        if a.zoom_range:
            for _ in range(17):tap(width-right-29 if compact else width-right-margin-19,height-bottom-153 if compact else top+139)
            shot('map-wide')
            for _ in range(17):tap(width-right-67 if compact else width-right-margin-19,height-bottom-153 if compact else top+105)
            shot('map-close')
        tap(width-right-margin-20,top+margin+(20 if compact else 24));shot('settings-map')
        tap(left+safe_w*5/6,top+70);shot('settings-city')
        tap(left+safe_w/2,top+70);shot('settings-location')
        tap(left+safe_w*3/4,top+107);shot('settings-forward')
        back()
        tap(left+safe_w//2,top+30);shot('search-empty')
        tap(left+90,top+70);command('TEXT 114.054000,22.542000');tap(width-right-38,top+72);shot('search-coordinate')
        tap(left+safe_w//2,top+133);shot('plan')
        tap(width-right-40,top+120) if compact else tap(left+safe_w//2,top+253)
        time.sleep(.5);shot('missing-wifi')
        assert process.poll() is None
        text=(out/'simulator.log').read_text(errors='replace')
        assert not any(word in text for word in ('Exception:', 'PXA task failed', 'map frame rejected')),text[-3000:]
        print(f'PASS: {a.width}x{a.height} @ {a.density_dpi}dpi; navigation, roadbook, drag, native inputs, settings, missing Wi-Fi')
    finally:
        process.terminate()
        try:process.wait(10)
        except subprocess.TimeoutExpired:process.kill();process.wait()
        Path(sock).unlink(missing_ok=True)
