#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check real rasterized conveyor joints, including opposite creation orders."""
import argparse,json,os,re,signal,struct,subprocess,time,zlib
from pathlib import Path
from PIL import Image
from simulator import command
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);p.add_argument('--baseline',action='store_true');a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';store=state/'app-data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171-pxa-mindustry';store.mkdir(parents=True,exist_ok=True)
 key=b'pxa-mindustry.save.v1';value=a.fixture.read_bytes();body=struct.pack('<HB',1,len(key))+key+struct.pack('<H',len(value))+value
 (store/'.pxa-kv-a').write_bytes(b'PXKV'+struct.pack('<HHQII',1,0,1,len(body),zlib.crc32(body))+body)
 logpath=a.output/'simulator.log';sock=Path(f'/tmp/md-belts-{os.getpid()}.sock')
 argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','480','--height','480','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
  def text():return logpath.read_text(errors='replace')
  def wait(predicate):
   end=time.monotonic()+35
   while time.monotonic()<end:
    assert proc.poll() is None,text()[-3000:]
    if predicate():return
    time.sleep(.03)
   raise TimeoutError(text()[-2500:])
  def tap(x,y):
   command(sock,f'TOUCH DOWN 0 {int(x)} {int(y)}');time.sleep(.05);command(sock,f'TOUCH UP 0 {int(x)} {int(y)}');time.sleep(.15)
  def screenshot(name):
   head,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and head==f'PNG {len(png)}'.encode();path=a.output/f'{name}.png';path.write_bytes(png);return Image.open(path).convert('RGB')
  try:
   wait(lambda:sock.exists() and 'glyph cache ready' in text());assert 'save restored' in text();tap(240,201)
   hud=Hud(480,480,tap);hud.jump(24,29);time.sleep(.2);im=screenshot('joined-belts')
   # Shadow RGB565(28,28,33) expands to (24,28,32). It must never
   # overwrite the opaque interior of a straight conveyor tile.
   regions={}
   for name,(x0,y0,x1,y1) in {'vertical-up':(22,25,23,32),'vertical-other-order':(26,25,27,32),'horizontal-right':(23,23,26,24),'horizontal-left':(23,21,26,22)}.items():
    left=240+int((x0-24)*16);right=240+int((x1-24)*16)
    top=hud.cy+int((y0-29)*16);bottom=hud.cy+int((y1-29)*16)
    count=sum(im.getpixel((x,y))==(24,28,32) for y in range(top,bottom) for x in range(left,right))
    regions[name]=count
   if a.baseline:assert sum(regions.values())>0,regions
   else:assert all(v==0 for v in regions.values()),regions
   hud.jump(24,19);time.sleep(.2);screenshot('turns-and-merges');command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
   assert not re.search('Guest exception|out of bounds memory|invalid save|submit failed',text(),re.I)
   report={'passed':True,'baseline':a.baseline,'shadow_pixels_inside_belts':regions,'package':str(a.package)};(a.output/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)
if __name__=='__main__':main()
