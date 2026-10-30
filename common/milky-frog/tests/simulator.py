#!/usr/bin/env python3
"""Validate real signed Milky Frog packages, screenshots and Host audio output."""
from array import array
import argparse
import io
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import tempfile
import time
import zlib
from PIL import Image

VIEWPORT=re.compile(r'Pocket: viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) size=(\d+)x(\d+) display=')
FAULT=re.compile(r'Guest exception|callback failed|out of bounds|wasm trap|unreachable instruction|Pocket: .*failed|exit flush timed out|image unavailable|recording preparation deferred',re.I)


class Simulator:
    def __init__(self,args,app,label,width=296,height=240,audio=True,state=None,dpi=160,corner=0,round_screen=False,insets="0,0,0,0"):
        self.args,self.app,self.label=args,app,label
        self.root=args.output/label;self.root.mkdir(parents=True,exist_ok=True)
        self.state=state or self.root/'state';self.state.mkdir(parents=True,exist_ok=True)
        self.log_path=self.root/'simulator.log';self.pcm=self.root/'mixed.s16'
        self.temp=tempfile.TemporaryDirectory(prefix='mfrog-');self.sock=Path(self.temp.name)/'control.sock'
        self.size=(width,height);self.audio=audio;self.dpi=dpi;self.corner=corner;self.round_screen=round_screen;self.insets=insets
    def logs(self):return self.log_path.read_text(errors='replace')
    def wait(self,predicate,label,seconds=25):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            assert self.proc.poll() is None,f'{self.label}: simulator exited: {self.logs()[-1800:]}'
            assert not FAULT.search(self.logs()),f'{self.label}: runtime fault; see {self.log_path}'
            value=predicate()
            if value:return value
            time.sleep(.04)
        raise TimeoutError(f'{self.label}: {label}; {self.logs()[-1400:]}')
    def command(self,text):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(8);client.connect(str(self.sock));client.sendall((text+'\n').encode());data=bytearray()
            while part:=client.recv(65536):data.extend(part)
        assert not data.startswith(b'ERR'),(text,data[:160]);return bytes(data)
    def touch(self,phase,x,y):
        px=self.x+int((x+.5)*self.w/self.cw);py=self.y+int((y+.5)*self.h/self.ch)
        self.command(f'TOUCH {phase} 1 {px} {py}')
    def tap(self,x,y):self.touch('DOWN',x,y);time.sleep(.04);self.touch('UP',x,y);time.sleep(.15)
    def key(self,key):self.command('KEY_DOWN '+key);time.sleep(.06);self.command('KEY_UP '+key);time.sleep(.15)
    def shot(self,name):
        header,_,data=self.command('SCREENSHOT AFTER_PRESENT').partition(b'\n')
        assert header==f'PNG {len(data)}'.encode(),header
        image=Image.open(io.BytesIO(data));assert image.size==self.size
        (self.root/(name+'.png')).write_bytes(data);return image.convert('RGB')
    def __enter__(self):
        folder={'2048':'2048','rain':'rain','miner':'miner'}[self.app]
        package=self.args.packages/folder/('pxa-milkyfrog-'+self.app)
        argv=[str(self.args.simulator),'--package',str(package),'--publisher-key',str(self.args.publisher_key),'--state-root',str(self.state),'--width',str(self.size[0]),'--height',str(self.size[1]),'--density-dpi',str(self.dpi),'--corner-radius',str(self.corner),'--safe-insets',self.insets,'--pxadb-control-socket',str(self.sock)]
        if self.round_screen:argv+=['--round']
        self.log_file=self.log_path.open('wb')
        env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='disk' if self.audio else 'unavailable-test-driver',SDL_DISKAUDIOFILE=str(self.pcm),SDL_DISKAUDIODELAY='32',PXA_SIM_NAV_MODE='gesture-no-bar',PXA_SIMULATOR_PERF='0')
        self.proc=subprocess.Popen(argv,env=env,stdout=self.log_file,stderr=subprocess.STDOUT)
        try:
            self.wait(lambda:self.sock.exists(),'control socket')
            self.wait(lambda:'Pocket: ready app=' in self.logs(),'app startup')
            m=self.wait(lambda:VIEWPORT.search(self.logs()),'viewport');self.cw,self.ch,self.x,self.y,self.w,self.h=map(int,m.groups())
            if self.audio:self.wait(lambda:'Milky: recording ready id=0' in self.logs(),'original recording preparation')
            if self.audio:
                device=re.search(r'format=S16LE channels=(\d+) freq=(\d+)',self.logs())
                assert device,'Missing actual SDL output format';self.channels,self.rate=map(int,device.groups())
            time.sleep(.15);return self
        except BaseException:self.close();raise
    def quiet_or_voice(self,voice):
        self.wait(lambda:self.pcm.exists() and self.pcm.stat().st_size>0,'PCM capture')
        start=self.pcm.stat().st_size;start-=start%2
        wanted=int(self.rate*self.channels*.6)*2
        self.wait(lambda:self.pcm.stat().st_size>=start+wanted,'PCM interval',seconds=8)
        with self.pcm.open('rb') as stream:stream.seek(start);raw=stream.read(wanted)
        values=array('h',raw);nonzero=sum(v!=0 for v in values);peak=max(map(abs,values),default=0)
        if voice:assert nonzero>256 and peak>128,(self.label,nonzero,peak)
        else:assert nonzero==0,(self.label,nonzero,peak)
        return {'nonzero_samples':nonzero,'peak':peak,'offset':start,'samples':len(values),'channels':self.channels,'rate':self.rate,'seconds':len(values)/self.channels/self.rate}
    def close(self):
        if hasattr(self,'proc') and self.proc.poll() is None:
            try:self.command('TOUCH CANCEL_ALL');self.command('KEY HOME');self.proc.wait(timeout=6)
            except Exception:self.proc.terminate();self.proc.wait(timeout=3)
        if hasattr(self,'log_file'):self.log_file.close()
        self.temp.cleanup()
    def __exit__(self,typ,value,tb):
        self.close()
        if typ is None:assert self.proc.returncode==0,(self.label,self.proc.returncode)


def exercise(args,app,report):
    with Simulator(args,app,app+'-audio') as sim:
        sim.shot('initial')
        sim.key('S')
        if app!='2048':sim.tap(140,117) # Default music off for a clean voice-only capture.
        time.sleep(.8)
        sim.tap(140,163);sim.wait(lambda:'Milky: recording play id=0' in sim.logs(),'original laugh playback')
        report['voice']=sim.quiet_or_voice(True);sim.shot('sound-settings')
        sim.tap(276,10);sim.wait(lambda:'Pocket: menu opened' in sim.logs(),'pause');time.sleep(1.1)
        report['paused']=sim.quiet_or_voice(False);sim.shot('paused');sim.tap(140,85)
        sim.tap(140,72);time.sleep(.7);sim.tap(140,163)
        report['muted']=sim.quiet_or_voice(False);sim.shot('muted');time.sleep(.4)
        state=sim.state
    with Simulator(args,app,app+'-restored',state=state) as sim:
        assert 'Pocket: load completed' in sim.logs();sim.key('S');sim.tap(140,163)
        report['restored_mute']=sim.quiet_or_voice(False)
        sim.tap(140,72);sim.tap(140,163);sim.wait(lambda:'Milky: recording play id=0' in sim.logs(),'restored original laugh')
        report['reenabled']=sim.quiet_or_voice(True);sim.key('X') # Controller B returns from sound settings.
        if app=='2048':
            for key in ['LEFT','UP','RIGHT','DOWN']*3:sim.key(key)
            sim.touch('DOWN',70,110);sim.touch('MOVE',145,110);sim.touch('UP',145,110)
            sim.wait(lambda:'Milky2048: move=' in sim.logs(),'controller and touch movement');sim.shot('gameplay')
            sim.tap(sim.cw-35,sim.ch-75);sim.shot('collection')
        elif app=='rain':
            sim.tap(140,170);sim.wait(lambda:'MilkyRain: started' in sim.logs(),'start via touch')
            sim.touch('DOWN',140,185)
            for i in range(70):
                # Follow the lowest saturated falling block in actual Host pixels.
                picture=sim.shot('gameplay' if i==69 else 'tracking')
                target=None
                for y in range(sim.y+int(sim.h*.73),sim.y+int(sim.h*.24),-2):
                    hits=[]
                    for x in range(sim.x+4,sim.x+sim.w-4,2):
                        r,g,b=picture.getpixel((x,y))
                        if r>210 and 145<g<220 and b<120:hits.append(x)
                    if hits:target=sum(hits)//len(hits);break
                if target is not None:sim.command(f'TOUCH MOVE 1 {target} {sim.y+int(sim.h*.8)}')
                time.sleep(.06)
            sim.touch('UP',140,185);sim.key('RIGHT');sim.shot('gameplay')
        else:
            sim.tap(140,177);sim.wait(lambda:'MilkyMiner: started' in sim.logs(),'start via touch')
            sim.key('Z');time.sleep(1.7);sim.shot('gameplay');sim.tap(120,130);time.sleep(.4);sim.key('A');sim.shot('collection')
        time.sleep(.5)
    for width,height in [(240,240),(480,320)]:
        with Simulator(args,app,f'{app}-{width}x{height}',width,height) as sim:sim.shot('initial')
    with Simulator(args,app,app+'-audio-unavailable',audio=False) as sim:
        sim.shot('still-playable');sim.key('Z');sim.key('LEFT');sim.shot('input')
    report['display_sizes']=[[296,240],[240,240],[480,320]];report['audio_unavailable']='playable'


def layout_matrix(args,app,report):
    cases=[('small',240,240,{}),('large',800,480,{}),('portrait',390,844,{}),
           ('square',480,480,{}),('round',412,412,{'round_screen':True}),
           ('dpi-safe',800,480,{'dpi':240,'corner':28,'insets':'12,8,16,8'})]
    report['layouts']=[]
    for name,width,height,options in cases:
        with Simulator(args,app,f'{app}-layout-{name}',width,height,**options) as sim:
            time.sleep(.5);sim.shot('home');sim.key('S');sim.shot('settings');sim.key('X')
            if app=='2048':
                for key in ['LEFT','UP','RIGHT','DOWN']:sim.key(key)
            else:sim.key('Z')
            time.sleep(.7);sim.shot('gameplay');sim.key('ENTER');sim.shot('paused');sim.key('X')
            if app=='miner':sim.key('A');time.sleep(.2);sim.shot('collection')
        assert not FAULT.search(sim.logs()),sim.log_path
        stats=re.search(r'PXA ASSETS [^\n]*? failures=(\d+)\b',sim.logs());assert stats and int(stats[1])==0,sim.log_path
        report['layouts'].append({'name':name,'size':[width,height],**options})
        print('PASS layout',app,name,flush=True)


def unlocked_gallery(args,app,report):
    if app=='rain':return
    state=args.output/(app+'-unlocked-state')
    with Simulator(args,app,app+'-gallery-init',state=state):pass
    save=bytearray(64 if app=='2048' else 32);save[:4]=b'MF48' if app=='2048' else b'MFMN';save[4]=1
    if app=='2048':save[6]=100;save[20]=save[21]=1;save[56:58]=b'\xff\x0f'
    else:save[6]=100;struct.pack_into('<I',save,12,1);struct.pack_into('<Q',save,16,(1<<47)-1)
    checksum=2166136261
    for value in save[:-4]:checksum=((checksum^value)*16777619)&0xffffffff
    struct.pack_into('<I',save,len(save)-4,checksum)
    # Seed an isolated POSIX Host storage fixture, never a user's save.
    key=('pxa-milkyfrog-'+app+'.save.v1').encode();body=struct.pack('<HB',1,len(key))+key+struct.pack('<H',len(save))+save
    encoded=struct.pack('<4sHHQII',b'PXKV',1,0,100,len(body),zlib.crc32(body))+body
    roots=list((state/'app-data').iterdir());assert len(roots)==1
    for slot in ['a','b']:(roots[0]/('.pxa-kv-'+slot)).write_bytes(encoded)
    for width,height in [(800,480),(390,844)]:
        with Simulator(args,app,f'{app}-gallery-{width}x{height}',width,height,state=state) as sim:
            if app=='miner':sim.key('A')
            elif sim.cw<280 or sim.cw*100<sim.ch*118:sim.tap(int(sim.cw*.375),sim.ch-20)
            else:sim.tap(sim.cw-30,sim.ch-80)
            for page in range(4 if app=='miner' else 2):
                time.sleep(.7);sim.shot(f'page-{page+1}');sim.tap(sim.cw-50,sim.ch-20)
            sim.key('X');sim.key('S');sim.key('Z');time.sleep(.2)
        assert not FAULT.search(sim.logs()),sim.log_path
        stats=re.search(r'PXA ASSETS [^\n]*? failures=(\d+)\b',sim.logs());assert stats and int(stats[1])==0
        print('PASS unlocked gallery',app,width,height,flush=True)
    report['unlocked_gallery']='all pages, native photos, original voice, no Assets failures'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--simulator',type=Path,required=True);parser.add_argument('--packages',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--publisher-key',type=Path,default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
    parser.add_argument('--layout-only',action='store_true',help='Run six responsive screen configurations per game.')
    parser.add_argument('--gallery-only',action='store_true',help='Use isolated saves to test every unlocked portrait.')
    parser.add_argument('--app',action='append',choices=['2048','rain','miner'],help='Select one or more games; defaults to all three.')
    args=parser.parse_args()
    for name in ['simulator','packages','output','publisher_key']:setattr(args,name,getattr(args,name).resolve())
    args.output.mkdir(parents=True,exist_ok=True);report={}
    try:
        for app in args.app or ['2048','rain','miner']:
            report[app]={}
            if args.gallery_only:unlocked_gallery(args,app,report[app])
            elif args.layout_only:layout_matrix(args,app,report[app])
            else:exercise(args,app,report[app])
            print('PASS',app,flush=True)
    finally:(args.output/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
