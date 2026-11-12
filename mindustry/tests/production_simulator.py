#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise mixed smelter input and one-generator factory brownouts via real UI."""
import argparse,json,os,re,signal,subprocess,time
from pathlib import Path
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';state.mkdir(exist_ok=True);sock=Path(f'/tmp/md-production-{os.getpid()}.sock');logpath=a.output/'simulator.log'
 argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','480','--height','480','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
  try:
   end=time.monotonic()+35
   while time.monotonic()<end:
    assert proc.poll() is None,logpath.read_text(errors='replace')[-2500:]
    if sock.exists() and 'glyph cache ready' in logpath.read_text(errors='replace'):break
    time.sleep(.05)
   else:raise TimeoutError('production simulator')
   def tap(x,y):
    command(sock,f'TOUCH DOWN 0 {x} {y}');time.sleep(.05);command(sock,f'TOUCH UP 0 {x} {y}');time.sleep(.15)
   def shot(name):
    head,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and head==f'PNG {len(png)}'.encode();(a.output/f'{name}.png').write_bytes(png)
   hud=Hud(480,480,tap)
   def place(cat,index,count,x,y):
    hud.choose(cat,index,count);hud.jump(x,y);tap(248,hud.cy+8);hud.confirm();hud.cancel()
   def supply(x,y,count=1):
    hud.jump(x,y);tap(248,hud.cy+8);hud.mode='inspect'
    for _ in range(count):hud.supply()
    hud.cancel()
   tap(240,129+61+2*28+12)
   place(3,1,3,27,31);supply(27,31)
   place(4,0,4,26,31);supply(26,31)
   hud.jump(27,31);tap(248,hud.cy+8);hud.mode='inspect';shot('smelter-working');hud.cancel()
   place(6,0,2,27,35);supply(27,35,2)
   place(4,0,4,26,36);supply(26,36)
   hud.jump(27,35);tap(248,hud.cy+8);hud.mode='inspect';shot('factory-83-percent');hud.cancel()
   time.sleep(10);supply(26,36)
   time.sleep(10);hud.jump(27,35);shot('unit-produced')
   hud.pause();hud.menu(3);time.sleep(.5);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
   latest=max(read_saves(state),key=lambda s:s['ticks']);assert latest['units']==1,latest
   smelter=next(b for b in latest['details'] if b['type']==9)
   assert smelter['items'].get(2)==5 and smelter['items'].get(3,0)==0 and smelter['items'].get(5)==5,smelter
   factory=next(b for b in latest['details'] if b['type']==21)
   assert factory['items'].get(1)==10 and factory['items'].get(5)==10,factory
   text=logpath.read_text(errors='replace');assert not re.search('Guest exception|out of bounds memory|invalid save|submit failed',text,re.I)
   (a.output/'result.json').write_text(json.dumps(latest,indent=2)+'\n');print('Real WAMR production passed: mixed smelter input, five silicon, 83% power and one Dagger')
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)
if __name__=='__main__':main()
