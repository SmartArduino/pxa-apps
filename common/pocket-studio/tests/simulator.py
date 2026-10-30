#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Real signed PXA packages: native text, touch, menus, save/reload and safe areas."""
import argparse
import io
import json
import os
from pathlib import Path
import re
import signal
import socket
import struct
import subprocess
import time
import zlib
from PIL import Image,ImageChops

APPS=('game-2048','pocket-sokoban','marble-islands','lantern-grove','harbor-cafe')
FAULT=re.compile(r'Guest exception|callback failed|out of bounds|unreachable instruction|Pocket: .*failed|exit flush timed out',re.I)
def check(ok,why):
    if not ok:raise AssertionError(why)
def command(sock,value):
    with socket.socket(socket.AF_UNIX) as c:
        c.settimeout(8);c.connect(str(sock));c.sendall((value+'\n').encode());data=bytearray()
        while part:=c.recv(65536):data.extend(part)
    check(not data.startswith(b'ERR'),f'{value}: {data[:200]!r}');return bytes(data)
def save_blob(state,app):
    values=[];key='pxa-'+app+'.save.v1'
    for f in state.glob('**/.pxa-kv-*'):
        raw=f.read_bytes()
        if len(raw)<26:continue
        magic,version,_,generation,size,crc=struct.unpack_from('<4sHHQII',raw);body=raw[24:]
        if magic!=b'PXKV' or version!=1 or len(body)!=size or zlib.crc32(body)!=crc:continue
        at=2
        for _ in range(struct.unpack_from('<H',body)[0]):
            n=body[at];at+=1;k=body[at:at+n].decode();at+=n
            n=struct.unpack_from('<H',body,at)[0];at+=2;v=body[at:at+n];at+=n
            if k==key:values.append((generation,v))
    check(values,'no durable game save');return max(values,key=lambda p:p[0])[1]
class Session:
    def __init__(self,args,name,w,h,radius=0):
        self.args,self.name,self.w,self.h=args,name,w,h
        self.label=f'{name}-{w}x{h}'+('-round' if radius else '')
        self.out=args.output/self.label;self.out.mkdir(parents=True,exist_ok=True)
        self.state=self.out/'state';self.state.mkdir(exist_ok=True)
        self.sock=Path(f'/tmp/pxa-studio-{os.getpid()}-{name}.sock')
        self.radius=radius;self.proc=None
    def logs(self):return self.logpath.read_text(errors='replace')
    def __enter__(self):
        self.sock.unlink(missing_ok=True);self.logpath=self.out/('runtime-reload.log' if (self.out/'runtime.log').exists() else 'runtime.log');self.log=self.logpath.open('wb')
        argv=[str(self.args.simulator),'--package',str(self.args.packages/('pxa-'+self.name)),'--publisher-key',str(self.args.publisher_key),'--state-root',str(self.state),'--width',str(self.w),'--height',str(self.h),'--corner-radius',str(self.radius),'--safe-insets','8,10,8,10' if self.radius else '0,0,0,0','--pxadb-control-socket',str(self.sock)]
        self.proc=subprocess.Popen(argv,stdout=self.log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
        end=time.monotonic()+25
        while time.monotonic()<end:
            check(self.proc.poll() is None,self.logs())
            if self.sock.exists() and 'Pocket: ready app=' in self.logs():break
            time.sleep(.05)
        else:raise TimeoutError('ready')
        m=re.findall(r'viewport canvas=(\d+)x(\d+) origin=(\d+),(\d+) size=(\d+)x(\d+)',self.logs());check(m,'viewport log')
        self.cw,self.ch,self.vx,self.vy,self.vw,self.vh=map(int,m[-1]);time.sleep(.25)
        return self
    def pointer(self,phase,x,y):
        px=self.vx+round(x*self.vw/self.cw);py=self.vy+round(y*self.vh/self.ch)
        return command(self.sock,f'TOUCH {phase} 1 {px} {py}')
    def tap(self,x,y,delay=.15):
        self.pointer('DOWN',x,y);time.sleep(.04);self.pointer('UP',x,y);time.sleep(delay)
    def key(self,key,delay=.15):
        command(self.sock,'KEY_DOWN '+key);time.sleep(.04);command(self.sock,'KEY_UP '+key);time.sleep(delay)
    def shot(self,label):
        check(not FAULT.search(self.logs()),self.logs())
        raw=command(self.sock,'SCREENSHOT AFTER_PRESENT');header,_,png=raw.partition(b'\n');check(header==f'PNG {len(png)}'.encode(),'PNG framing')
        im=Image.open(io.BytesIO(png)).convert('RGB');check(im.size==(self.w,self.h),'frame size')
        # Raw frames are saved directly; no crop/retouch is used for deliverables.
        (self.out/(label+'.png')).write_bytes(png)
        check(len(im.getcolors(self.w*self.h) or [])>30,'blank/low-color frame')
        return im
    def __exit__(self,*exc):
        if self.proc and self.proc.poll() is None:
            try:command(self.sock,'TOUCH CANCEL_ALL')
            except OSError:pass
            os.killpg(self.proc.pid,signal.SIGTERM)
            try:self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:os.killpg(self.proc.pid,signal.SIGKILL);self.proc.wait()
        self.log.close();self.sock.unlink(missing_ok=True)
def exercise(args,name,w,h,radius,report):
    with Session(args,name,w,h,radius) as s:
        initial=s.shot('01-home-zh')
        # Verify actual surface art independently of the overlaid font colors.
        if name in ('lantern-grove','harbor-cafe'):
            patch=initial.crop((s.vx+int(20*s.vw/288),s.vy+int(55*s.vh/224),s.vx+int(270*s.vw/288),s.vy+int(140*s.vh/224)))
            check(len(patch.getcolors(patch.width*patch.height) or [])>100,'painted scene was occluded')
        elif name in ('game-2048','pocket-sokoban'):
            c=initial.getpixel((s.vx+int(10*s.vw/288),s.vy+int(45*s.vh/224)))
            check(sum(c)>120,'board surface was occluded')
        # Exercise each game's actual actions, including a cancelled touch.
        if name=='game-2048':
            for key in ['LEFT','DOWN','RIGHT','UP','LEFT','DOWN']:s.key(key,.16)
            time.sleep(.3);before_hint=save_blob(s.state,name)
            s.key('Z');s.shot('11-direction-hint')
            check(save_blob(s.state,name)==before_hint,'advice changed board/score/undo')
            s.shot('02-play');s.key('X');s.shot('03-undo')
            time.sleep(.25);before_prompt=save_blob(s.state,name)
            s.key('A');s.shot('09-new-game-confirm');check(save_blob(s.state,name)==before_prompt,'new-game request changed save')
            s.pointer('DOWN',190,155);s.pointer('CANCEL',190,155);time.sleep(.15)
            check(save_blob(s.state,name)==before_prompt,'cancelled confirmation changed game')
            s.key('X');check(save_blob(s.state,name)==before_prompt,'B should keep the current game')
            s.key('A');s.tap(190,155,.35);fresh=save_blob(s.state,name)
            check(struct.unpack_from('<I',fresh,16)[0]==0 and struct.unpack_from('<I',fresh,8)[0]==0,'confirmed new game did not reset moves/score')
            check(struct.unpack_from('<I',fresh,12)[0]==struct.unpack_from('<I',before_prompt,12)[0],'new game lost the best score')
            s.key('LEFT');s.key('DOWN')
        elif name=='pocket-sokoban':
            # A five-step walk to an empty tile must never push the nearby box.
            s.tap(30,66,.02);s.shot('11-auto-walk');time.sleep(.85)
            walked=save_blob(s.state,name)
            check(walked[24]==11 and struct.unpack_from('<I',walked,8)[0]==5 and struct.unpack_from('<I',walked,12)[0]==0,'tap walking failed or pushed a box')
            s.tap(220,137,.3);s.tap(156,192,.02)
            s.pointer('DOWN',80,130);s.pointer('CANCEL',80,130);time.sleep(.3)
            stopped=save_blob(s.state,name);time.sleep(.6)
            check(save_blob(s.state,name)==stopped,'cancelled walking kept running')
            s.tap(220,137,.3)
            s.key('RIGHT');s.key('RIGHT');s.shot('02-solved');s.key('Z');s.shot('03-next-level');s.key('X')
            time.sleep(.25);before_picker=save_blob(s.state,name)
            s.tap(180,12);s.shot('09-level-selector');s.tap(121,55);s.shot('10-level-locked')
            check(save_blob(s.state,name)==before_picker,'locked selector changed progress')
            s.pointer('DOWN',32,55);s.pointer('CANCEL',32,55);time.sleep(.1)
            s.key('X');check(save_blob(s.state,name)==before_picker,'cancelled selector changed board')
            s.tap(180,12);s.tap(78,55);check(save_blob(s.state,name)==before_picker,'choosing current level reset its state')
        elif name=='marble-islands':
            s.key('Z');s.pointer('DOWN',120,110);s.pointer('MOVE',120,90);time.sleep(.2);s.pointer('CANCEL',120,90);time.sleep(.15)
            s.shot('02-race');s.key('X');s.shot('03-islands');s.pointer('DOWN',40,60);s.pointer('CANCEL',40,60);time.sleep(.1);s.shot('09-cancelled-island');s.key('X')
        elif name=='lantern-grove':
            before_fly=save_blob(s.state,name);taps=struct.unpack_from('<I',before_fly,56)[0]
            s.pointer('DOWN',72,102);s.pointer('CANCEL',72,102);time.sleep(.1)
            s.tap(72,102,.3);s.shot('11-firefly-catch')
            check(struct.unpack_from('<I',save_blob(s.state,name),56)[0]==taps+1,'visible firefly did not collect exactly once')
            s.tap(60,26);s.shot('12-goal-journal');s.tap(220,26)
            s.tap(48,183);s.tap(140,183);s.tap(140,61);s.shot('02-build');s.tap(220,26);s.tap(230,183);s.shot('03-journal');s.tap(220,26)
        else:
            time.sleep(.7);s.tap(156,117);s.tap(142,180);s.tap(142,180);time.sleep(2.7);s.shot('09-ready-to-serve');s.pointer('DOWN',63,120);s.pointer('CANCEL',63,120);time.sleep(.1);check('harbor event=5' not in s.logs(),'cancelled tray served');s.tap(63,120);s.shot('11-smart-tray');check('harbor event=5' in s.logs(),'cafe serving did not complete');s.shot('02-served');s.tap(154,200);s.shot('03-pantry');s.tap(60,202)
        s.pointer('DOWN',80,180);s.pointer('CANCEL',80,180)
        s.tap(252,10,.3);check('Pocket: menu opened' in s.logs(),'touch menu')
        zh=s.shot('04-pause-zh');time.sleep(.3)
        before=save_blob(s.state,name)
        s.tap(140,174,.3);en=s.shot('05-pause-en')
        check('Pocket: language en' in s.logs(),'English language preference')
        check(ImageChops.difference(zh,en).getbbox() is not None,'native system text did not change')
        check(save_blob(s.state,name)==before,'changing language mutated paused game save')
        s.tap(140,85,.25);s.shot('06-play-en');s.tap(252,10,.25)
        if name=='game-2048':
            before=save_blob(s.state,name);s.tap(140,85,.1)
            board=[list(before[20+4*x:24+4*x]) for x in range(4)]
            chosen=None
            for direction in ['right','down','left','up']:
                possible=False
                for lane in range(4):
                    row=[board[(3-k if direction=='right' else k) if direction in ('right','left') else lane][(3-k if direction=='down' else k) if direction in ('up','down') else lane] for k in range(4)]
                    compact=[v for v in row if v];merged=[];k=0
                    while k<len(compact):
                        if k+1<len(compact) and compact[k]==compact[k+1]:merged.append(compact[k]+1);k+=2
                        else:merged.append(compact[k]);k+=1
                    possible|=row!=merged+[0]*(4-len(merged))
                if possible:chosen=direction;break
            check(chosen is not None,'test board has no legal move')
            dx,dy={'right':(65,0),'down':(0,65),'left':(-65,0),'up':(0,-65)}[chosen]
            s.pointer('DOWN',90,100);s.pointer('UP',90+dx,100+dy);time.sleep(.07)
            s.shot('07-animation');time.sleep(.25);s.tap(252,10,.25)
            check(save_blob(s.state,name)!=before,'swipe failed to move/serialize')
        s.pointer('DOWN',140,145)
        try:s.proc.wait(timeout=4)
        except subprocess.TimeoutExpired:raise AssertionError('save and exit did not close runner')
        check(s.proc.returncode==0,'nonzero exit')
        durable=save_blob(s.state,name)
        check(not FAULT.search(s.logs()),'runtime fault')
        record={'game':name,'width':w,'height':h,'radius':radius,'save_bytes':len(durable),'viewport':[s.vx,s.vy,s.vw,s.vh],'passed':True}
    with Session(args,name,w,h,radius) as s:
        check('Pocket: load completed' in s.logs(),'reload did not restore save')
        s.shot('08-restored');s.tap(252,10,.15);s.pointer('DOWN',140,145);s.proc.wait(timeout=4);check(s.proc.returncode==0,'reload exit')
    report.append(record);print(json.dumps(record),flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for flag in ['simulator','packages','publisher-key','output']:p.add_argument('--'+flag,type=Path,required=True)
    a=p.parse_args()
    for key in ['simulator','packages','publisher_key','output']:setattr(a,key,getattr(a,key).resolve())
    check('local/artifacts/' in str(a.output),'output must be in local/artifacts')
    a.output.mkdir(parents=True,exist_ok=True);report=[]
    try:
        for w,h,radius in [(480,320,0),(320,240,0),(296,240,58)]:
            for name in APPS:exercise(a,name,w,h,radius,report)
    finally:(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
