#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Camera-follow and remote survival construction across real process restarts."""
import argparse,json,math,os,re,signal,struct,subprocess,time,zlib
from pathlib import Path
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';store=state/'app-data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171-pxa-mindustry';store.mkdir(parents=True,exist_ok=True)
 key=b'pxa-mindustry.save.v1';value=a.fixture.read_bytes();body=struct.pack('<HB',1,len(key))+key+struct.pack('<H',len(value))+value
 (store/'.pxa-kv-a').write_bytes(b'PXKV'+struct.pack('<HHQII',1,0,1,len(body),zlib.crc32(body))+body)
 result=[]
 for stage in range(3):
  logpath=a.output/f'simulator-{stage}.log';sock=Path(f'/tmp/md-camera-{os.getpid()}-{stage}.sock')
  argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','480','--height','480','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
  with logpath.open('wb') as log:
   proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
   def text():return logpath.read_text(errors='replace')
   def wait(predicate):
    end=time.monotonic()+35
    while time.monotonic()<end:
     assert proc.poll() is None,text()[-2500:]
     if predicate():return
     time.sleep(.025)
    raise TimeoutError(text()[-2500:])
   def tap(x,y):
    command(sock,f'TOUCH DOWN 0 {int(x)} {int(y)}');time.sleep(.04);command(sock,f'TOUCH UP 0 {int(x)} {int(y)}');time.sleep(.08)
   def shot(name):
    head,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and head==f'PNG {len(png)}'.encode();(a.output/f'{stage}-{name}.png').write_bytes(png)
   try:
    wait(lambda:sock.exists() and 'glyph cache ready' in text());assert 'save restored' in text();tap(240,201);hud=Hud(480,480,tap)
    if stage:
     hud.jump(25,4);time.sleep(.55 if stage==1 else 6.7);shot('alpha-approaching-and-building' if stage==1 else 'completed')
    else:time.sleep(.25);shot('out-of-range')
    hud.pause();hud.menu(3);time.sleep(.4);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
    latest=max(read_saves(state),key=lambda s:s['ticks']);factory=next(b for b in latest['details'] if b['type']==21)
    if stage==0:assert factory['build_state']==1 and factory['progress']==0 and not factory['items'],factory
    elif stage==1:assert factory['build_state']==2 and 0<factory['progress']<60000 and sum(factory['items'].values())>0,factory
    else:
     assert factory['build_state']==0 and not latest['plans'],factory
     assert math.hypot(latest['player'][0]-25,latest['player'][1]-4)<2,latest
    result.append(latest)
    assert not re.search('Guest exception|out of bounds memory|invalid save|submit failed',text(),re.I)
   finally:
    if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
    sock.unlink(missing_ok=True)
 (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('Real camera/builder passed: out-of-range ghost, camera-follow approach, gradual survival build, consumed materials and restart completion')
if __name__=='__main__':main()
