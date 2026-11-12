#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Load a genuine game-save fixture, mine remotely, restart, and deposit cargo."""
import argparse,json,os,re,signal,struct,subprocess,time,zlib
from pathlib import Path
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--position',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';store=state/'app-data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171-pxa-mindustry';store.mkdir(parents=True,exist_ok=True)
 key=b'pxa-mindustry.save.v1';v=a.fixture.read_bytes();body=struct.pack('<HB',1,len(key))+key+struct.pack('<H',len(v))+v
 (store/'.pxa-kv-a').write_bytes(b'PXKV'+struct.pack('<HHQII',1,0,1,len(body),zlib.crc32(body))+body)
 x,y=map(int,a.position.read_text().split());results=[]
 for stage in range(2):
  sock=Path(f'/tmp/md-player-{os.getpid()}-{stage}.sock');logpath=a.output/f'simulator-{stage}.log'
  argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','480','--height','480','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
  with logpath.open('wb') as log:
   proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
   try:
    end=time.monotonic()+35
    while time.monotonic()<end:
     assert proc.poll() is None,logpath.read_text(errors='replace')[-2500:]
     if sock.exists() and 'glyph cache ready' in logpath.read_text(errors='replace'):break
     time.sleep(.05)
    else:raise TimeoutError('player simulator')
    assert 'save restored' in logpath.read_text(errors='replace')
    def tap(x,y):
     command(sock,f'TOUCH DOWN 0 {int(x)} {int(y)}');time.sleep(.05);command(sock,f'TOUCH UP 0 {int(x)} {int(y)}');time.sleep(.15)
    def shot(name):
     head,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and head==f'PNG {len(png)}'.encode();(a.output/f'{stage}-{name}.png').write_bytes(png)
    hud=Hud(480,480,tap);tap(240,201)
    def tile(x,y):
     hud.jump(x,y);tap(240+(x+.5-max(3,min(45,x)))*16,hud.cy+(y+.5-max(3,min(45,y)))*16)
    if not stage:
     tile(x,y);time.sleep(1);shot('mining-beam');time.sleep(4.8);shot('cargo-full')
    else:
     hud.jump(x,y);shot('cargo-restored');tile(24,38);time.sleep(16)
     tile(24,36);hud.mode='inspect';shot('before-deposit');hud.supply();shot('deposited')
    hud.pause();hud.menu(3);time.sleep(.5);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
    latest=max(read_saves(state),key=lambda s:s['ticks'])
    assert latest['carry']==([0,30] if not stage else [255,0]),latest
    assert not re.search('Guest exception|out of bounds memory|invalid save|submit failed',logpath.read_text(errors='replace'),re.I)
    results.append(latest)
   finally:
    if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
    sock.unlink(missing_ok=True)
 (a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n');print('Real WAMR player passed: sustained mining, full cargo, restart and core deposit')
if __name__=='__main__':main()
