#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise actual multi-pointer events through PXA/WAMR, with a DPI/safe-area case."""
import math,argparse,json,os,re,signal,subprocess,time
from pathlib import Path
from simulator import command,read_saves
from ui_controls import Hud

def run(a,w,h,dpi=160,inset=0,corner=0):
 out=a.output/f'{w}x{h}-{dpi}dpi';out.mkdir(parents=True,exist_ok=True);sock=Path(f'/tmp/md-touch-{os.getpid()}-{dpi}.sock');state=out/'state';state.mkdir(exist_ok=True);logpath=out/'runtime.log'
 argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width',str(w),'--height',str(h),'--density-dpi',str(dpi),'--corner-radius',str(corner),'--safe-insets',','.join([str(inset)]*4),'--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
  def logs():return logpath.read_text(errors='replace')
  def wait(predicate):
   end=time.monotonic()+30
   while time.monotonic()<end:
    assert proc.poll() is None,logs()[-3000:]
    if predicate():return
    time.sleep(.04)
   raise TimeoutError(logs()[-2500:])
  try:
   wait(lambda:sock.exists() and 'glyph cache ready' in logs())
   m=re.search(r'viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) size=(\d+)x(\d+)',logs());cw,ch,ox,oy,vw,vh=map(int,m.groups())
   b=re.search(r'UI bounds=(\d+),(\d+),(\d+),(\d+)',logs());bounds=tuple(map(int,b.groups()))
   def point(phase,id,x,y):
    command(sock,f'TOUCH {phase} {id} {ox+(int(x)*vw+cw-1)//cw} {oy+(int(y)*vh+ch-1)//ch}');time.sleep(.06)
   def tap(x,y):point('DOWN',0,x,y);point('UP',0,x,y);time.sleep(.18)
   def shot(name):
    header,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and header==f'PNG {len(png)}'.encode();(out/f'{name}.png').write_bytes(png)
   hud=Hud(cw,ch,tap,bounds);yy=max(4,(ch-222)//2);tap(cw//2,yy+61+2*28+12);wait(lambda:'new sandbox' in logs());shot('world')
   cx,cy=cw//2,hud.cy
   transforms=[]
   def gesture(start,end,shift=(0,0)):
    before=len(re.findall(r'Mindustry: view zoom=',logs()))
    point('DOWN',0,cx-start,cy);point('DOWN',1,cx+start,cy)
    point('MOVE',0,cx-end+shift[0],cy+shift[1]);point('MOVE',1,cx+end+shift[0],cy+shift[1])
    point('UP',0,cx-end+shift[0],cy+shift[1]);point('UP',1,cx+end+shift[0],cy+shift[1])
    wait(lambda:len(re.findall(r'Mindustry: view zoom=',logs()))>before)
    match=re.findall(r'Mindustry: view zoom=(\d+) camera=([\d.]+),([\d.]+)',logs())[-1]
    result=[int(match[0]),float(match[1]),float(match[2])];transforms.append(result);return result
   z,x,y=gesture(40,80);assert abs(z-32)<=1 and abs(x-24.5)<.15 and abs(y-35)<.15,(z,x,y);shot('pinch-out')
   z,px,py=gesture(60,60,(24,12));assert abs(z-32)<=1 and abs(px-(x-24/z))<.15 and abs(py-(y-12/z))<.15,(z,px,py);shot('two-finger-pan')
   z,_,_=gesture(60,30);assert abs(z-16)<=1
   wide=min(cx-hud.left-24,hud.right-cx-24)
   z,_,_=gesture(20,wide);assert z==40;shot('zoom-max')
   z,_,_=gesture(wide,8);assert z==8;shot('zoom-min')
   # Existing buttons remain usable after multi-touch, restoring 100% zoom.
   for _ in range(2):hud.tools(2);hud.dock(3)
   hud.choose(2,1,3);z,_,_=gesture(40,60);assert z==24;shot('pinch-during-construction');hud.cancel()
   hud.dock(1) # Recenter; long-hold a starter conveyor with no placement on UP.
   point('DOWN',0,cx-3*z,cy+z//2);time.sleep(.62);point('UP',0,cx-3*z,cy+z//2)
   wait(lambda:'block copied' in logs());shot('copied-conveyor');hud.cancel()
   # A canceled two-finger sequence can be followed by a fresh one.
   point('DOWN',0,cx-40,cy);point('DOWN',1,cx+40,cy);command(sock,'TOUCH CANCEL_ALL');time.sleep(.15)
   z,_,_=gesture(40,40);assert z==24
   hud.pause();hud.menu(3);time.sleep(.7);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
   assert not re.search(r'Guest exception|callback failed|submit failed|glyph failed|out of bounds',logs(),re.I)
   snapshots=read_saves(state);assert snapshots;latest=max(snapshots,key=lambda r:r['ticks']);assert latest['buildings']==4 and math.hypot(latest['player'][0]-latest['target'][0],latest['player'][1]-latest['target'][1])<2,latest
   result={'command':argv,'transforms':transforms,'saved':latest};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');return result
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 results=[run(a,w,h) for w,h in [(320,240),(296,240),(480,320),(240,320),(480,480)]]
 results.append(run(a,480,480,305,12,58));(a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n');print('Real WAMR touch: pinch/pan, zoom bounds, construction capture, long-hold copy and cancellation passed at five sizes plus 305 DPI')
if __name__=='__main__':main()
