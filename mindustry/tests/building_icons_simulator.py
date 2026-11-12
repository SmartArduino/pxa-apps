#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Review actual building-drawer icons, factory bodies and four-way previews."""
import argparse,json,os,re,signal,struct,subprocess,time,zlib
from pathlib import Path
from PIL import Image
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--fixture',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 state=a.output/'state';store=state/'app-data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171-pxa-mindustry';store.mkdir(parents=True,exist_ok=True)
 key=b'pxa-mindustry.save.v1';v=a.fixture.read_bytes();body=struct.pack('<HB',1,len(key))+key+struct.pack('<H',len(v))+v
 (store/'.pxa-kv-a').write_bytes(b'PXKV'+struct.pack('<HHQII',1,0,1,len(body),zlib.crc32(body))+body)
 logpath=a.output/'simulator.log';sock=Path(f'/tmp/md-icons-{os.getpid()}.sock');names=[]
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
   command(sock,f'TOUCH DOWN 0 {int(x)} {int(y)}');time.sleep(.04);command(sock,f'TOUCH UP 0 {int(x)} {int(y)}');time.sleep(.12)
  def shot(name):
   head,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and head==f'PNG {len(png)}'.encode();path=a.output/f'{name}.png';path.write_bytes(png);im=Image.open(path);assert im.size==(480,480);assert len(im.convert('RGB').getcolors(480*480))>60;names.append(name)
  try:
   wait(lambda:sock.exists() and 'glyph cache ready' in text());assert 'save restored' in text();tap(240,201);hud=Hud(480,480,tap)
   hud.dock(0);tab_y=hud.bottom-8-3*hud.n-18+hud.n//2
   for c in range(7):tap(hud.left+4+(c+.5)*(hud.width-8)/7,tab_y);shot(f'category-{c}')
   hud.dock(0)
   for _ in range(2):hud.tools(2);hud.dock(3)
   hud.jump(18,10);time.sleep(.7);shot('ground-factory-four-directions')
   hud.jump(18,26);time.sleep(.7);shot('air-factory-four-directions')
   for index,kind in [(0,'ground'),(1,'air')]:
    hud.choose(6,index,2);hud.jump(18,26);tap(252,hud.cy+int(3.5*24))
    for direction in range(4):shot(f'{kind}-preview-{direction}');hud.dock(1)
    hud.cancel()
   hud.pause();hud.menu(3);time.sleep(.5);command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
   assert not re.search('Guest exception|out of bounds memory|invalid save|submit failed|glyph failed',text(),re.I),text()[-3000:]
   latest=max(read_saves(state),key=lambda r:r['ticks']);assert latest['buildings']==9 and not latest['plans'],latest
   result={'passed':True,'categories':7,'factory_body_directions':8,'factory_preview_directions':8,'screenshots':names,'package':str(a.package)};(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)
if __name__=='__main__':main()
