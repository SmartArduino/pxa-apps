#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify full drill icons, placement, mining and reload in the real PXA guest."""
import argparse,json,os,re,signal,subprocess,time
from pathlib import Path
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';state.mkdir(exist_ok=True);sock=Path(f'/tmp/md-drill-{os.getpid()}.sock');logpath=a.output/'simulator.log'
 argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','480','--height','480','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
  def wait(predicate):
   deadline=time.monotonic()+35
   while time.monotonic()<deadline:
    assert proc.poll() is None,logpath.read_text(errors='replace')[-2500:]
    if predicate():return
    time.sleep(.08)
   raise TimeoutError('drill simulator')
  try:
   wait(lambda:sock.exists() and 'glyph cache ready' in logpath.read_text(errors='replace'))
   def point(phase,id,x,y):command(sock,f'TOUCH {phase} {id} {x} {y}');time.sleep(.07)
   def tap(x,y):point('DOWN',1,x,y);point('UP',1,x,y);time.sleep(.25)
   def shot(name):
    header,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and header==f'PNG {len(png)}'.encode();(a.output/f'{name}.png').write_bytes(png)
   hud=Hud(480,480,tap)
   tap(240,129+61+2*28+12)
   wait(lambda:'new sandbox' in logpath.read_text(errors='replace'))
   hud.dock(0);shot('drill-icons');hud.dock(0)
   hud.choose(0,1,2);hud.jump(19,33);tap(248,hud.cy+8);shot('pneumatic-preview');hud.confirm();hud.cancel()
   time.sleep(8)
   tap(256,hud.cy+16);hud.mode='inspect';shot('pneumatic-mining')
   hud.cancel();hud.jump(20,35)
   point('DOWN',0,200,hud.cy);point('DOWN',1,280,hud.cy)
   point('MOVE',0,160,hud.cy);point('MOVE',1,320,hud.cy)
   point('UP',0,160,hud.cy);point('UP',1,320,hud.cy)
   shot('drills-detail')
   hud.pause();hud.menu(3);time.sleep(.6);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
   text=logpath.read_text(errors='replace');assert not re.search('Guest exception|out of bounds memory|invalid save|submit failed',text,re.I)
   snapshots=read_saves(state);assert snapshots;latest=max(snapshots,key=lambda x:x['ticks'])
   gas=next(b for b in latest['details'] if b['type']==15)
   assert gas['x']==19 and gas['y']==33 and gas['items'].get(0,0)>0,gas
   (a.output/'result.json').write_text(json.dumps(latest,indent=2)+'\n')
   print('Real simulator drill icons, blueprint, production and MND5 persistence passed')
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)
if __name__=='__main__':main()
