#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise drawers, landscape input and save recovery in official PXA hosts.

Write evidence under local/artifacts. These are QA captures; use the official
verify --store-screenshots workflow for marketplace uploads.
"""
from pathlib import Path
import argparse,concurrent.futures,json,math,os,re,socket,struct,subprocess,time,traceback
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--profiles',nargs='+');p.add_argument('--density-dpi',type=int);p.add_argument('--save-layout',type=Path,required=True);args=p.parse_args();schema=json.loads(args.save_layout.read_text())
for name in ('simulator','package','publisher_key','output'):setattr(args,name,getattr(args,name).resolve())
args.output.mkdir(parents=True,exist_ok=False);profiles={'pai-touch':(296,240),'doit-gamekit-2':(320,240),'esp-mosaico':(480,480),'espp4-a':(480,854),'esp32s31-korvo-1':(800,480),'desktop-1024':(1024,600),'tablet':(1280,800),'desktop-hd':(1920,1080)}
if args.profiles:profiles={n:profiles[n] for n in args.profiles}
def run(item):
 name,(native_w,native_h)=item;density=args.density_dpi or (305 if name=='esp-mosaico' else 160);readable=density>=200;out=args.output/name;out.mkdir();state=out/'state';state.mkdir();sock=Path('/tmp')/f'iron-hud-{os.getpid()}-{name}.sock';log=out/'run.log';proc=None;shots=[];pw=ph=w=h=0
 def command(value):
  with socket.socket(socket.AF_UNIX) as c:
   c.settimeout(30);c.connect(str(sock));c.sendall((value+'\n').encode());f=c.makefile('rb');head=f.readline()
   if head.startswith((b'PNG ',b'JSON ')):
    data=f.read(int(head.split()[1]));assert len(data)==int(head.split()[1]);return data
   assert head==b'OK\n',(name,value,head)
 def stop(graceful=False):
  nonlocal proc
  if proc:
   if graceful and proc.poll() is None:
    tap(w/2,pause_save);command('KEY BACK')
    try:proc.wait(timeout=12)
    except subprocess.TimeoutExpired:pass
   if proc.poll() is None:proc.terminate()
   try:proc.wait(timeout=10)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
   proc=None
 def launch():
  nonlocal proc,pw,ph,w,h
  previous=log.read_text().count('Iron Dawn ready') if log.exists() else 0
  proc=subprocess.Popen([str(args.simulator),'--package',str(args.package),'--publisher-key',str(args.publisher_key),'--state-root',str(state),'--width',str(native_w),'--height',str(native_h),'--density-dpi',str(density),'--safe-insets','0,0,0,0','--corner-radius','0','--pxadb-control-socket',str(sock)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),stdout=stream,stderr=subprocess.STDOUT)
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
   b=file.read_bytes();assert len(b)==schema['size'] and struct.unpack_from('<II',b)==(schema['magic'],schema['version']);crc=2166136261
   for byte in b[12:]:crc=((crc^byte)*16777619)&0xffffffff
   assert crc==struct.unpack_from('<I',b,8)[0];found.append((struct.unpack_from('<I',b,16)[0],b))
  assert found;return max(found)[1]
 try:
  with log.open('w') as stream:
   launch();offset=(w-480)//2;wide='wide=1' in log.read_text();readable=readable and not wide
   glyph=12*max(1,int(12*density/160+6)//12);line=max(20,math.ceil((glyph+4 if readable else 14)*h/ph));picture=max(30,math.ceil((32*density/160 if readable else 20)*h/ph));row=max(28,line+8 if readable else 28,min(int(h*.18),math.ceil(30*density/160*h/ph)));card=12+picture+line*2;header=line+8;bottom=h-row-10;panel=bottom-header-12-card;tactics=bottom-header-12-row*2-6;cx=w/2;top=max(50,2*line+8) if readable else 50;mid=(top+bottom)/2
   menu_row=max(row,12*max(1,int(15*density/160+6)//12)+16) if readable else 32
   home_y=h-2*menu_row-26 if readable else max(214,h-128)
   pause_save=(h-(48+24+4*menu_row+80))/2+48+24+36+3*(menu_row+8)+menu_row/2 if readable else (h-248)/2+216
   if wide:
    match=re.findall(r'wide=1 world=(\d+) top=(\d+) bottom=(\d+) scale=(\d+)',log.read_text());world_w,top,bottom,scale=map(int,match[-1]);dp=density/160/scale;row=max(24,int(__import__('math').ceil(40*dp)),24);side=w-world_w;map_budget=h-top-2*row-56-3*36;map_size=max(32,min(112,min(max(h//6,round(96*dp)),map_budget)));tabs=top+map_size+20;grid=tabs+row+8;footer=h-row-8;card_height=min((footer-8-grid-12)//3,32+max(20,min(40,round(32*dp)))+12);cw=(side-22)//2;cx=world_w/2;mid=(top+bottom)/2
    def dock(i):
     if i in [0,1]:tap(world_w+8+i*((side-20)//2+4)+(side-20)//4,tabs+row/2)
    def production(i):tap(world_w+8+(i%2)*(cw+6)+cw/2,grid+(i//2)*(card_height+6)+card_height/2)
    def tactic(i):
     if i==0:tap(8+(world_w-46)//6+6+(world_w-46)//12,bottom+5+row/2)
     elif i==3:tap(world_w+8+(side-20)//4,footer+row/2)
   else:
    def dock(i):tap(offset+52+94*i,bottom+5+row/2)
    def production(i):tap(offset+52+94*i,panel+header+4+card/2)
    def tactic(i):tap(offset+63+118*(i%4),tactics+header+4+(i//4)*(row+6)+row/2)
   shot('landscape-home')
   if readable:
    tap(offset+75,home_y+menu_row+10+menu_row/2);shot('campaign-archive');tap(w-75,104+32);shot('campaign-briefing');command('KEY BACK');time.sleep(.2)
    tap(offset+295,home_y+menu_row+10+menu_row/2);shot('manual-first');tap(w-90,h-menu_row/2-12);shot('manual-next');tap(90,h-menu_row/2-12);shot('manual-previous');command('KEY BACK');time.sleep(.2)
   tap(offset+185,home_y+menu_row+10+menu_row/2 if readable else home_y+64);shot('endless-briefing')
   for briefing_page in range(6):
    tap(180,h-44)
    if re.search(r'Iron Dawn state .*screen=3',log.read_text()):break
   else:raise AssertionError('Briefing did not deploy')
   shot('battle-default')
   tap(cx+80,mid-88);shot('unit-catalogue');production(0);shot('queued')
   dock(1);shot('building-catalogue');production(0);shot('construction')
   time.sleep(5.5);production(0);shot('placement');tap(cx-80,mid-53);shot('power-placed')
   if wide:
    qx=world_w+8+cw+6+cw/2;qy=grid+2*(card_height+6)+card_height/2;swipe(qx,qy,qx+10,qy);shot('sidebar-empty-drag')
   else:
    dock(4);shot('tactics');swipe(w/2,tactics+10,w/2+30,tactics+10);shot('header-drag');tactic(7);dock(4)
   tactic(0);shot('selection-armed');swipe(cx-75,mid-4,cx+35,mid+35);shot('selection-complete');assert re.search(r'Iron Dawn state .*selected=4',log.read_text());tap(cx-40,mid+100);time.sleep(1);shot('movement');tap(w-42,24);time.sleep(.8);assert re.search(r'Iron Dawn state .*screen=4',log.read_text());before=saves()
   deadline=time.monotonic()+15
   while time.monotonic()<deadline and sum(before[schema['entities']+i*56+50]==6 and before[schema['entities']+i*56+51]==0 and struct.unpack_from('<f',before,schema['entities']+i*56+24)[0]>0 for i in range(100))!=3:
    time.sleep(.2);before=saves()
   shot('paused-saved')
   # The persisted state confirms build/card hits and world movement after rotation.
   assert struct.unpack_from('<I',before,schema['extra'])[0]==1
   power_count=0;positions=[]
   for i in range(100):
    at=schema['entities']+i*56;kind,team=before[at+50:at+52];hp=struct.unpack_from('<f',before,at+24)[0]
    power_count+=hp>0 and kind==6 and team==0
    if i in [5,6,7,8]:positions.append(struct.unpack_from('<ff',before,at))
   assert power_count==3,power_count;original=[(8.5,11),(9.5,12),(8,13),(10.5,11.5)];assert all(abs(x-a)+abs(y-b)>.1 for (x,y),(a,b) in zip(positions,original)),positions
   stop(True);launch();assert 'Iron Dawn restored' in log.read_text();tap(offset+365,home_y+menu_row/2 if readable else home_y+20);shot('restored-default');dock(4);tactic(3);shot('research');tap(w/2,h-32);tap(w-42,24);time.sleep(.8);saves();stop(True)
  text=log.read_text();assert not any(x in text for x in ['surface create error','resize failed','save open failed','save publish failed','save write failed','out of bounds memory','Exception:','double free','corruption'])
  return {'profile':name,'passed':True,'native':[native_w,native_h],'landscape':[pw,ph],'canvas':[w,h],'density_dpi':density,'wide_sidebar':wide,'drawer_touch_and_placement':True,'four_unit_selection_and_movement':True,'save_relaunch_and_research':True,'screenshots':shots}
 except Exception as e:return {'profile':name,'passed':False,'error':str(e),'traceback':traceback.format_exc(),'screenshots':shots}
 finally:stop();sock.unlink(missing_ok=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,profiles.items()))
r={'passed':all(x['passed'] for x in results),'profiles':results,'capture':'official product simulator, rectangular QA viewport'};(args.output/'result.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print(json.dumps(r,ensure_ascii=False),flush=True);raise SystemExit(0 if r['passed'] else 1)
