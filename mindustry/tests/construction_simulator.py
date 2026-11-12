#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify real Alpha construction/demolition and cross-process partial-site saves."""
import argparse,json,os,re,signal,subprocess,time
from pathlib import Path
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';state.mkdir(exist_ok=True);results=[]
 for stage in range(2):
  sock=Path(f'/tmp/md-builder-{os.getpid()}-{stage}.sock');logpath=a.output/f'simulator-{stage}.log'
  argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','480','--height','480','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
  with logpath.open('wb') as log:
   proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
   def wait(predicate):
    deadline=time.monotonic()+35
    while time.monotonic()<deadline:
     assert proc.poll() is None,logpath.read_text(errors='replace')[-3000:]
     if predicate():return
     time.sleep(.025)
    raise TimeoutError('builder simulator')
   try:
    wait(lambda:sock.exists() and 'glyph cache ready' in logpath.read_text(errors='replace'))
    def tap(x,y):
     command(sock,f'TOUCH DOWN 0 {x} {y}');time.sleep(.04);command(sock,f'TOUCH UP 0 {x} {y}');time.sleep(.04)
    def shot(name):
     head,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and head==f'PNG {len(png)}'.encode();(a.output/f'{stage}-{name}.png').write_bytes(png)
    hud=Hud(480,480,tap);tap(240,201)
    if not stage:
     tap(240,65)
     hud.choose(2,0,3);hud.jump(25,32);tap(248,hud.cy+8);hud.confirm();hud.cancel()
     shot('alpha-building');hud.pause();shot('paused-site');hud.menu(3)
    else:
     assert 'save restored' in logpath.read_text(errors='replace')
     time.sleep(1);hud.jump(25,32);shot('completed')
     tap(248,hud.cy+8);hud.mode='inspect';hud.dock(2)
     shot('alpha-dismantling');hud.pause();shot('paused-demolition');hud.menu(3)
    time.sleep(.5);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
    text=logpath.read_text(errors='replace');assert not re.search('Guest exception|out of bounds memory|invalid save|submit failed',text,re.I)
    latest=max(read_saves(state),key=lambda x:x['ticks']);site=next(b for b in latest['details'] if b['type']==6)
    assert site['build_state']==(2 if not stage else 3) and 0<site['progress']<60000,site
    assert len(latest['plans'])==1,latest
    results.append(latest)
   finally:
    if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
    sock.unlink(missing_ok=True)
 (a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n')
 print('Real WAMR Alpha construction, partial-save reload and progressive demolition passed')
if __name__=='__main__':main()
