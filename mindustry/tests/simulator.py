#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the signed guest in the real PXA simulator, retaining original PNGs."""
import argparse,io,json,os,re,signal,socket,subprocess,time
from pathlib import Path
from PIL import Image,ImageChops
from ui_controls import Hud

def command(sock,text):
 with socket.socket(socket.AF_UNIX) as client:
  client.settimeout(12);client.connect(str(sock));client.sendall((text+'\n').encode());data=bytearray()
  while chunk:=client.recv(65536):data.extend(chunk)
 if data.startswith(b'ERR'):raise RuntimeError(bytes(data[:200]))
 return bytes(data)

def read_saves(state):
 snapshots=[]
 for kv in state.rglob('.pxa-kv-*'):
  content=kv.read_bytes();indices=[content.find(magic) for magic in (b'MND3',b'MND4',b'MND5',b'MND6',b'MND7',b'MND8')];index=min((i for i in indices if i>=0),default=-1)
  if index<0:continue
  p=content[index:];count=p[51];end=52;buildings=[]
  for _ in range(count):
   kind,x,y,direction=p[end:end+4];mode=direction>>2;direction&=3;end+=6;mask=p[end];end+=1
   inventory={}
   for item in range(8):
    if mask&(1<<item):inventory[item]=p[end];end+=1
   timer=int.from_bytes(p[end:end+2],'little');end+=5
   water=int.from_bytes(p[end:end+2],'little');progress=int.from_bytes(p[end+2:end+4],'little');end+=4
   buildings.append({'type':kind,'x':x,'y':y,'water':water,'progress':progress,'items':inventory,'build_state':mode})
  enemies=p[end];end+=1+7*enemies;units=p[end];end+=1
  roster=[]
  for _ in range(units):
   roster.append({'x':int.from_bytes(p[end:end+2],'little')/256,'y':int.from_bytes(p[end+2:end+4],'little')/256,'kind':p[end+6]});end+=10
  plans=[]
  if p[:4] in (b'MND5',b'MND6',b'MND7',b'MND8'):
   jobs=p[end];end+=1;plans=list(p[end:end+jobs]);end+=jobs
  carry=None;mining=None;target=None
  if p[:4] in (b'MND7',b'MND8'):
   carry=list(p[end:end+2]);mining=list(p[end+2:end+4]);target=[int.from_bytes(p[end+6:end+8],'little')/256,int.from_bytes(p[end+8:end+10],'little')/256];end+=10
  check=2166136261
  for byte in p[:end]:check=((check^byte)*16777619)&0xffffffff
  assert int.from_bytes(p[end:end+4],'little')==check
  snapshots.append({'format':p[:4].decode(),'muted':bool(p[21]&8),'music_mode':(p[21]>>4) if p[:4]==b'MND8' else 0,'ticks':int.from_bytes(p[8:12],'little'),'wave':int.from_bytes(p[12:14],'little'),'buildings':count,'enemies':enemies,'units':units,'roster':roster,'player':[int.from_bytes(p[40:42],'little')/256,int.from_bytes(p[42:44],'little')/256],'command':p[46],'plans':plans,'details':buildings,'carry':carry,'mining':mining,'target':target})
 return snapshots

def run(args,width,height,stage,restore=False):
 output=args.output; logpath=output/f'{stage}.log';sock=Path(f'/tmp/md-{os.getpid()}-{stage}.sock');state=output/f'state-{width}x{height}'
 state.mkdir(parents=True,exist_ok=True)
 argv=[str(args.simulator),'--package',str(args.package),'--publisher-key',str(args.publisher_key),'--state-root',str(state),'--width',str(width),'--height',str(height),'--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
  def wait(predicate):
   end=time.monotonic()+35
   while time.monotonic()<end:
    assert proc.poll() is None,logpath.read_text(errors='replace')[-4000:]
    if predicate():return
    time.sleep(.08)
   raise TimeoutError(stage)
  try:
   wait(lambda:sock.exists() and 'Pocket: ready app=' in logpath.read_text(errors='replace'))
   if 'GameRender direct-scanout' in logpath.read_text(errors='replace'):wait(lambda:'glyph cache ready' in logpath.read_text(errors='replace'))
   time.sleep(.5)
   text=logpath.read_text(errors='replace');match=re.search(r'viewport canvas=(\d+)x(\d+) origin=(-?\d+),(-?\d+) size=(\d+)x(\d+)',text);assert match,text
   cw,ch,ox,oy,vw,vh=map(int,match.groups())
   if 'GameRender direct-scanout' in text:
    header,sep,data=command(sock,'UI INSPECT').partition(b'\n');assert sep and header==f'JSON {len(data)}'.encode()
    inspected=json.loads(data);assert inspected['nodes'] and all(n['type']=='root' for n in inspected['nodes']),inspected
    (output/f'{stage}-ui.json').write_text(json.dumps(inspected,indent=2)+'\n')
   def tap(x,y):
    px=ox+(x*vw+cw-1)//cw;py=oy+(y*vh+ch-1)//ch
    command(sock,f'TOUCH DOWN 1 {px} {py}');time.sleep(.08);command(sock,f'TOUCH UP 1 {px} {py}');time.sleep(.35)
   def shot(name):
    result=command(sock,'SCREENSHOT AFTER_PRESENT');header,sep,png=result.partition(b'\n');assert sep and header==f'PNG {len(png)}'.encode()
    path=output/f'{stage}-{name}.png';path.write_bytes(png)
    im=Image.open(io.BytesIO(png));assert im.size==(width,height);assert len(im.convert('RGB').getcolors(width*height))>60
    return im.convert('RGB')
   hud=Hud(cw,ch,tap)
   shot('title');yy=max(4,(ch-222)//2)
   if restore:
    wait(lambda:'Mindustry: save restored' in logpath.read_text(errors='replace'));tap(cw//2,yy+72);shot('restored')
   else:
    tap(cw//2,yy+72);shot('sectors');tap(cw//2,60);world=shot('world')
    # The construction drawer folds after picking a block.
    hud.choose(2,1,3)
    worldx=cw//2-int(24.5*16);worldy=hud.cy-int(35*16)
    tap(worldx+22*16+8,worldy+34*16+8)
    shot('blueprint');hud.confirm();shot('built');assert 'Mindustry: building queued' in logpath.read_text(errors='replace')
    hud.pause();shot('pause');assert 'Mindustry: paused' in logpath.read_text(errors='replace');hud.menu(1);shot('research');hud.back();hud.menu(0)
    hud.cancel();hud.tools(0);shot('research-direct');assert 'Mindustry: research opened' in logpath.read_text(errors='replace');hud.back()
    if width==320:
     hud.choose(2,0,3)
     tap(worldx+25*16+8,worldy+32*16+8);hud.confirm();hud.cancel()
     tap(worldx+25*16+8,worldy+32*16+8);hud.mode='inspect'
     for _ in range(3):hud.supply()
     shot('turret-supplied')
    hud.wave();time.sleep(1);shot('wave');assert 'Mindustry: wave started' in logpath.read_text(errors='replace')
    if width==320:
     hud.jump(24,25);time.sleep(21);shot('battle')
    # Frozen manual pause allows a pixel-exact cross-process save comparison.
    hud.pause();shot('saved-pause');hud.menu(3)
    time.sleep(.8)
   command(sock,'KEY HOME');proc.wait(timeout=8);assert proc.returncode==0
   errors=re.findall(r'.*(?:Guest exception|out of bounds memory|callback failed|invalid save|UI.*failed|submit failed|glyph failed).*',logpath.read_text(errors='replace'),re.I)
   assert not errors,errors
   if not restore:
    assert 'Milky: recording ready' in logpath.read_text(errors='replace'),'Original SFX never loaded'
    assert 'Milky: recording play' in logpath.read_text(errors='replace'),'Original SFX never played'
   snapshots=read_saves(state)
   assert snapshots,'No validated game payload persisted'
   latest=max(snapshots,key=lambda x:x['ticks']);assert latest['buildings']==(6 if width==320 else 5) and latest['wave']==1,latest
   return {'saved':latest,'profile':f'{width}x{height}','canvas':[cw,ch],'command':argv,'restored':restore,'exit':proc.returncode}
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 result=[]
 for w,h in [(320,240),(296,240),(480,320),(240,320),(480,480)]:
  result.append(run(a,w,h,f'{w}x{h}'));result.append(run(a,w,h,f'{w}x{h}-restart',True))
 (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('Real WAMR simulator: five sizes, folded HUD, construction, research, waves, pause and restart passed')
if __name__=='__main__':main()
