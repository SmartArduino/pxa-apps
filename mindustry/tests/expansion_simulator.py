#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise water, multi-cell conduit gestures, both factories and troop commands.
All buildings, inventories and units are created through normal guest input.
"""
import argparse,io,json,os,re,signal,subprocess,time
from pathlib import Path
from PIL import Image
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';state.mkdir(exist_ok=True);sock=Path(f'/tmp/md-exp-{os.getpid()}.sock');logpath=a.output/'simulator.log'
 argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','320','--height','240','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
  try:
   deadline=time.monotonic()+30
   while time.monotonic()<deadline:
    assert proc.poll() is None,logpath.read_text(errors='replace')[-2500:]
    if sock.exists() and 'Pocket: ready app=' in logpath.read_text(errors='replace'):break
    time.sleep(.1)
   else:raise TimeoutError('guest startup')
   time.sleep(.5)
   match=re.search(r'viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) size=(\d+)x(\d+)',logpath.read_text(errors='replace'));cw,ch,ox,oy,vw,vh=map(int,match.groups())
   def pointer(phase,x,y):
    command(sock,f'TOUCH {phase} 1 {ox+(x*vw+cw-1)//cw} {oy+(y*vh+ch-1)//ch}')
   def tap(x,y):pointer('DOWN',x,y);time.sleep(.06);pointer('UP',x,y);time.sleep(.2)
   def shot(name):
    header,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and header==f'PNG {len(png)}'.encode();assert Image.open(io.BytesIO(png)).size==(320,240);(a.output/f'{name}.png').write_bytes(png)
   hud=Hud(cw,ch,tap)
   def jump(x,y):hud.jump(x,y)
   def cancel():hud.cancel()
   def select(cat,index,count):
    hud.choose(cat,index,count)
   def place(cat,index,count,x,y):
    select(cat,index,count);jump(x,y);tap(cw//2+8,hud.cy+8);hud.confirm();cancel()
   def supply(x,y,count=1):
    jump(x,y);tap(cw//2+8,hud.cy+8);hud.mode="inspect"
    for _ in range(count):hud.supply()
   # Sandbox retains all normal input, liquid, production and power rules.
   yy=max(4,(ch-222)//2);tap(cw//2,yy+61+2*28+12)
   place(5,0,4,16,34) # pump in the new pool
   place(0,1,2,19,33) # pneumatic drill on copper
   select(5,1,4);jump(17,34)
   px,py=cw//2+8,hud.cy+8
   pointer('DOWN',px,py);time.sleep(.06);pointer('MOVE',px+16,py);time.sleep(.06);pointer('UP',px+16,py);time.sleep(.15)
   shot('conduit-line-preview');hud.confirm();cancel();time.sleep(1);jump(18,34);shot('working-water-line')
   place(6,0,2,27,32);supply(27,32)
   place(4,0,4,26,32);supply(26,32)
   place(4,0,4,26,33);supply(26,33)
   place(6,1,2,27,28)
   place(4,0,4,25,32);supply(25,32)
   place(4,1,4,26,30) # connects both factories to all three generators
   # Move Alpha close enough to supply the northern air factory.
   jump(26,29);tap(cw//2+8,hud.cy+8);time.sleep(4)
   supply(27,28,2)
   for x,y in [(26,32),(26,33),(25,32)]:supply(x,y)
   jump(26,30);tap(cw//2+8,hud.cy+8);hud.mode='inspect';shot('power-network');jump(27,31);shot('factories-powered');time.sleep(17);shot('units-produced')
   cancel();hud.tools(1);shot('unit-commands');tap(cw//2,112)
   jump(30,29);tap(cw//2+8,hud.cy+8);time.sleep(5);shot('rally')
   hud.pause();hud.menu(3);time.sleep(.7);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
   text=logpath.read_text(errors='replace');assert not re.search('Guest exception|out of bounds memory|callback failed|invalid save',text,re.I)
   snapshots=read_saves(state);assert snapshots;latest=max(snapshots,key=lambda x:x['ticks'])
   assert latest['units']==2,latest
   assert {u['kind'] for u in latest['roster']}=={0,1},latest
   assert latest['command']==1,latest
   assert sum(b['type']==18 for b in latest['details'])==2,latest
   assert any(b['type']==15 and b['water']>0 for b in latest['details']),latest
   assert all(u['x']>28 for u in latest['roster']),latest
   (a.output/'result.json').write_text(json.dumps({'command':argv,'saved':latest},indent=2)+'\n');print('Real simulator expansion passed: water, conduit drag, both factories, two troop types and rally')
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)
if __name__=='__main__':main()
