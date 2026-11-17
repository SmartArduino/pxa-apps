#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise drawers, landscape input and save recovery in official PXA hosts.

Write evidence under local/artifacts. These are QA captures; use the official
verify --store-screenshots workflow for marketplace uploads.
"""
from pathlib import Path
import argparse,concurrent.futures,json,os,re,socket,struct,subprocess,time
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--profiles',nargs='+');p.add_argument('--density-dpi',type=int,default=160);args=p.parse_args()
for name in ('simulator','package','publisher_key','output'):setattr(args,name,getattr(args,name).resolve())
args.output.mkdir(parents=True,exist_ok=False);profiles={'pai-touch':(296,240),'doit-gamekit-2':(320,240),'esp-mosaico':(480,480),'espp4-a':(480,854)}
if args.profiles:profiles={n:profiles[n] for n in args.profiles}
def run(item):
 name,(native_w,native_h)=item;out=args.output/name;out.mkdir();state=out/'state';state.mkdir();sock=Path('/tmp')/f'iron-hud-{os.getpid()}-{name}.sock';log=out/'run.log';proc=None;shots=[];pw=ph=w=h=0
 def command(value):
  with socket.socket(socket.AF_UNIX) as c:
   c.settimeout(30);c.connect(str(sock));c.sendall((value+'\n').encode());f=c.makefile('rb');head=f.readline()
   if head.startswith((b'PNG ',b'JSON ')):
    data=f.read(int(head.split()[1]));assert len(data)==int(head.split()[1]);return data
   assert head==b'OK\n',(name,value,head)
 def stop():
  nonlocal proc
  if proc:
   proc.terminate()
   try:proc.wait(timeout=10)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
   proc=None
 def launch():
  nonlocal proc,pw,ph,w,h
  previous=log.read_text().count('Iron Dawn ready') if log.exists() else 0
  proc=subprocess.Popen([str(args.simulator),'--package',str(args.package),'--publisher-key',str(args.publisher_key),'--state-root',str(state),'--width',str(native_w),'--height',str(native_h),'--density-dpi',str(args.density_dpi),'--safe-insets','0,0,0,0','--corner-radius','0','--pxadb-control-socket',str(sock)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),stdout=stream,stderr=subprocess.STDOUT)
  end=time.monotonic()+45
  while time.monotonic()<end:
   assert proc.poll() is None,log.read_text()[-2000:];text=log.read_text()
   if text.count('Iron Dawn ready')>previous and sock.exists():break
   time.sleep(.1)
  else:raise RuntimeError('Readiness timed out')
  time.sleep(.25);data=command('SCREENSHOT');pw,ph=struct.unpack('>II',data[16:24]);assert pw>=ph and (pw,ph)==(max(native_w,native_h),min(native_w,native_h));m=re.findall(r'Iron Dawn display \d+x\d+ canvas=(\d+)x(\d+)',text);assert m;w,h=map(int,m[-1]);assert abs(w/h-pw/ph)<.005
 def tap(x,y):command(f'TAP {round(x*pw/w)} {round(y*ph/h)}');time.sleep(.25)
 def shot(label):
  file=out/(label+'.png');file.write_bytes(command('SCREENSHOT'));image=Image.open(file);assert image.size==(pw,ph);shots.append(str(file));print(name,label,flush=True)
 def swipe(x,y,xx,yy):
  command(f'TOUCH DOWN 0 {round(x*pw/w)} {round(y*ph/h)}');time.sleep(.04)
  for i in range(1,13):
   t=i/12;command(f'TOUCH MOVE 0 {round((x+(xx-x)*t)*pw/w)} {round((y+(yy-y)*t)*ph/h)}');time.sleep(.04)
  command(f'TOUCH UP 0 {round(xx*pw/w)} {round(yy*ph/h)}');time.sleep(.3)
 def saves():
  found=[]
  for file in (state/'private-files').rglob('*.save'):
   b=file.read_bytes();assert len(b)==13448 and struct.unpack_from('<II',b)==(0x49445736,6);crc=2166136261
   for byte in b[12:]:crc=((crc^byte)*16777619)&0xffffffff
   assert crc==struct.unpack_from('<I',b,8)[0];found.append((struct.unpack_from('<I',b,16)[0],b))
  assert found;return max(found)[1]
 try:
  with log.open('w') as stream:
   launch();offset=(w-480)//2;row=max(28,(30*h+ph-1)//ph);line=max(20,(14*h+ph-1)//ph);picture=max(30,(20*h+ph-1)//ph);card=12+picture+line*2;header=line+8;bottom=h-row-10;panel=bottom-header-12-card;tactics=bottom-header-12-row*2-6;mid=(50+bottom)/2
   def dock(i):tap(offset+52+94*i,bottom+5+row/2)
   def production(i):tap(offset+52+94*i,panel+header+4+card/2)
   def tactic(i):tap(offset+63+118*(i%4),tactics+header+4+(i//4)*(row+6)+row/2)
   shot('landscape-home');tap(offset+185,max(214,h-128)+64);tap(180,h-44);shot('battle-default');dock(0);shot('unit-catalogue');production(2);shot('queued');dock(1);shot('building-catalogue');production(0);shot('placement');tap(w/2-90,mid-65);shot('power-placed');dock(4);shot('tactics');swipe(w/2,tactics+10,w/2+30,tactics+10);shot('header-drag');tactic(7);dock(4);tactic(0);shot('selection-armed');swipe(w/2-75,mid-4,w/2+35,mid+35);shot('selection-complete');assert re.search(r'Iron Dawn state .*selected=4',log.read_text());tap(w/2+20,mid+70);time.sleep(1);shot('movement');tap(w-42,24);time.sleep(.8);assert re.search(r'Iron Dawn state .*screen=4',log.read_text());before=saves()
   # Actual V6 state confirms build/card hits and world movement after rotation.
   assert struct.unpack_from('<I',before,13424)[0]==1
   power_count=0;positions=[]
   for i in range(100):
    at=44+i*56;kind,team=before[at+50:at+52];hp=struct.unpack_from('<f',before,at+24)[0]
    power_count+=hp>0 and kind==6 and team==0
    if i in [5,6,7,8]:positions.append(struct.unpack_from('<ff',before,at))
   assert power_count==3,power_count;original=[(8.5,11),(9.5,12),(8,13),(10.5,11.5)];assert all(abs(x-a)+abs(y-b)>.1 for (x,y),(a,b) in zip(positions,original)),positions
   stop();launch();assert 'Iron Dawn restored' in log.read_text();tap(offset+365,max(214,h-128)+20);shot('restored-default');dock(4);tactic(3);shot('research');tap(w/2,h-32);tap(w-42,24);time.sleep(.8);saves();stop()
  text=log.read_text();assert not any(x in text for x in ['surface create error','resize failed','save open failed','save publish failed','save write failed','out of bounds memory','Exception:'])
  return {'profile':name,'passed':True,'native':[native_w,native_h],'landscape':[pw,ph],'canvas':[w,h],'density_dpi':args.density_dpi,'drawer_touch_and_placement':True,'four_unit_selection_and_movement':True,'save_relaunch_and_research':True,'screenshots':shots}
 except Exception as e:return {'profile':name,'passed':False,'error':str(e),'screenshots':shots}
 finally:stop();sock.unlink(missing_ok=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,profiles.items()))
r={'passed':all(x['passed'] for x in results),'profiles':results,'capture':'official product simulator, rectangular QA viewport'};(args.output/'result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps(r,ensure_ascii=False),flush=True);raise SystemExit(0 if r['passed'] else 1)
