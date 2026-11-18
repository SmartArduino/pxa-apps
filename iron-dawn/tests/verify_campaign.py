#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise campaign pages, large maps, radio persistence and construction.

Uses the signed production app in the official simulator. Fixture saves are
controlled QA states; captures belong under local/artifacts, never store uploads.
"""
from pathlib import Path
import argparse,json,os,re,socket,struct,subprocess,time,traceback
import numpy as np

p=argparse.ArgumentParser()
for name in ('simulator','package','publisher-key','fixture','save-layout','output'):p.add_argument('--'+name,type=Path,required=True)
p.add_argument("--scenarios",nargs="+");args=p.parse_args()
for name in ("simulator","package","publisher_key","fixture","save_layout","output"):setattr(args,name,getattr(args,name).resolve())
args.output.mkdir(parents=True,exist_ok=False);schema=json.loads(args.save_layout.read_text());results=[]

class Host:
 def __init__(self,label,width=854,height=480,audio=False):
  self.out=args.output/label;self.out.mkdir();self.state=self.out/'state';self.state.mkdir();self.sock=Path('/tmp')/f'iron-story-{os.getpid()}-{label}.sock';self.log=self.out/'run.log';self.stream=self.log.open('w');self.proc=None;self.pw=width;self.ph=height;self.audio=audio
 def command(self,value):
  with socket.socket(socket.AF_UNIX) as c:
   c.settimeout(30);c.connect(str(self.sock));c.sendall((value+'\n').encode());f=c.makefile('rb');head=f.readline()
   if head.startswith((b'PNG ',b'JSON ')):
    data=f.read(int(head.split()[1]));assert len(data)==int(head.split()[1]);return data
   assert head==b'OK\n',(value,head)
 def launch(self):
  n=self.log.read_text().count('Iron Dawn ready');env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='disk' if self.audio else 'dummy',PXA_SIM_NAV_MODE='gesture-no-bar')
  if self.audio:env.update(SDL_DISKAUDIOFILE=str(self.out/'mix.raw'),SDL_DISKAUDIODELAY='32')
  self.proc=subprocess.Popen([str(args.simulator),'--package',str(args.package),'--publisher-key',str(args.publisher_key),'--state-root',str(self.state),'--width',str(self.pw),'--height',str(self.ph),'--density-dpi','160','--safe-insets','0,0,0,0','--corner-radius','0','--pxadb-control-socket',str(self.sock)],env=env,stdout=self.stream,stderr=subprocess.STDOUT)
  end=time.monotonic()+45
  while self.log.read_text().count('Iron Dawn ready')<=n:
   assert self.proc.poll() is None and time.monotonic()<end,self.log.read_text()[-2500:];time.sleep(.1)
  m=re.findall(r'canvas=(\d+)x(\d+) wide=\d world=(\d+) top=(\d+) bottom=(\d+)',self.log.read_text());assert m;self.w,self.h,self.ww,self.top,self.bottom=map(int,m[-1]);time.sleep(.15)
 def tap(self,x,y,delay=.18):self.command(f'TAP {round(x*self.pw/self.w)} {round(y*self.ph/self.h)}');time.sleep(delay)
 def shot(self,name):
  data=self.command('SCREENSHOT');assert struct.unpack('>II',data[16:24])==(self.pw,self.ph);(self.out/(name+'.png')).write_bytes(data)
  if not (self.out/'runtime-info.json').exists():(self.out/'runtime-info.json').write_bytes(self.command('RUNTIME INFO'))
 def closed(self):
  self.proc.wait(timeout=15);assert self.proc.returncode==0,self.log.read_text()[-2000:];self.proc=None
 def bootstrap(self,mode=None,legacy=False):
  self.launch();self.command('KEY BACK');self.closed();files=list((self.state/'private-files').rglob('iron-dawn-*.save'));assert files
  for file in files:
   if legacy:file.write_bytes((Path(__file__).parent/'fixtures/save-v6.bin').read_bytes())
   else:subprocess.run([str(args.fixture),mode,str(file)],check=True)
  self.launch();assert 'Iron Dawn restored' in self.log.read_text()
 def resume(self):self.tap((self.w-480)//2+365,max(214,self.h-128)+20)
 def pause(self):self.tap(self.w-42,24);time.sleep(.4)
 def leave(self):self.tap(self.w/2,(self.h-248)/2+216);self.command('KEY BACK');self.closed()
 def save(self):
  end=time.monotonic()+4
  while time.monotonic()<end:
   files=[]
   for path in (self.state/'private-files').rglob('iron-dawn-*.save'):
    data=path.read_bytes()
    if len(data)==schema['size'] and struct.unpack_from('<II',data)==(0x49445737,7):files.append((struct.unpack_from('<I',data,16)[0],data))
   if files:return max(files)[1]
   time.sleep(.1)
  raise AssertionError('No V7 save')
 def cleanup(self):
  if self.proc and self.proc.poll() is None:self.proc.terminate();self.proc.wait(timeout=10)
  self.sock.unlink(missing_ok=True);self.stream.close()
  text=self.log.read_text();assert not any(x in text for x in ('double free','corruption','out of bounds','Exception:','save open failed','surface create error','resize failed')),text[-2000:]

def scenario(label,run,width=854,height=480,audio=False):
 if args.scenarios and label not in args.scenarios:return
 host=Host(label,width,height,audio);result={'scenario':label,'passed':False}
 try:details=run(host);result.update(passed=True,**(details or {}))
 except Exception:result['error']=traceback.format_exc()
 try:host.cleanup()
 except Exception:result.update(passed=False,cleanup_error=traceback.format_exc())
 results.append(result);print(label,'passed' if result['passed'] else result,flush=True)

def campaign(host):
 host.bootstrap('3');offset=(host.w-480)//2
 for chapter in range(3,6):
  host.tap(offset+75,max(214,host.h-128)+64)
  if chapter==3:
   host.shot('archive-second-act');host.tap(87,host.h-68);host.shot('archive-first-act');host.tap(host.w-87,host.h-68)
  row=min(112,(host.h-165)//3);host.tap(host.w-66,74+(chapter-3)*row+32);host.shot('briefing-'+str(chapter+1))
  for page in range(6):
   host.tap(180,host.h-44)
   if re.search(r'chapter='+str(chapter)+r' screen=3',host.log.read_text()):break
   host.shot(f'briefing-{chapter+1}-page-{page+2}')
  else:raise AssertionError('Briefing did not deploy')
  host.shot('battle-'+str(chapter+1));host.tap(100,host.top+18);assert 'objective focus' in host.log.read_text();host.shot('objective-'+str(chapter+1));host.pause();saved=host.save();assert struct.unpack_from('<i',saved,schema['side'])[0]==(40,44,48)[chapter-3];host.tap(host.w/2,(host.h-248)/2+216)
 host.command('KEY BACK');host.closed();return {'campaign_pages':2,'large_maps':[40,44,48],'normal_ui_deployment':True}

def legacy(host):
 host.bootstrap(legacy=True);host.tap((host.w-480)//2+100,max(214,host.h-128)+20);host.shot('migrated-next-chapter')
 for i in range(6):
  host.tap(180,host.h-44)
  if re.search(r'chapter=3 screen=3',host.log.read_text()):break
 else:raise AssertionError('Completed legacy career did not unlock chapter four')
 host.pause();data=host.save();assert struct.unpack_from('<I',data,20)[0]==3;host.leave();return {'actual_v6_restore':True,'completed_act_one_unlocks_act_two':True}

def radio(host):
 host.bootstrap('radio');host.resume();time.sleep(.5);host.shot('radio-paused');a=host.save();time.sleep(.6);b=host.save();assert struct.unpack_from('<f',a,schema['time'])[0]==struct.unpack_from('<f',b,schema['time'])[0]
 raw=np.frombuffer((host.out/'mix.raw').read_bytes()[-8000:],dtype='<i2');assert len(raw) and np.max(np.abs(raw.astype(np.int32)))==0
 for i in range(4):
  host.tap(host.w/2,host.h-49);time.sleep(.15)
  files=list((host.state/'private-files').rglob('*.save'))
  if any(len(x.read_bytes())==schema['size'] and struct.unpack_from('<I',x.read_bytes(),schema['mission']+4)[0]&8 for x in files):break
 else:raise AssertionError('Radio acknowledgment not saved')
 host.pause();host.leave();host.launch();host.resume();time.sleep(.4);host.shot('radio-restored-read');host.pause();data=host.save();assert struct.unpack_from('<I',data,schema['mission']+4)[0]&8;assert struct.unpack_from('<f',data,schema['time'])[0]>struct.unpack_from('<f',a,schema['time'])[0];host.leave();return {'simulation_paused':True,'audio_silent':True,'read_state_survives_restart':True}

def construction(host,stage,complete=False):
 host.bootstrap('construction'+str(stage));host.resume();host.shot('construction')
 if complete:time.sleep(10);host.shot('completed')
 host.pause();data=host.save();found=[];trucks=0
 for i in range(100):
  at=schema['entities']+i*56;kind,team=data[at+50:at+52];hp=struct.unpack_from('<f',data,at+24)[0];progress=struct.unpack_from('<f',data,at+32)[0]
  if hp>0 and team==0 and kind==4:trucks+=1
  if hp>0 and team==0 and 6<=kind<=10 and progress>0:found.append(kind)
 if complete:assert not found and trucks>=2
 else:assert len(found)==5,found
 host.leave();return {'construction_stage':stage,'unfinished_buildings':len(found),'completion_and_refinery_spawn':complete}

def far(host):
 host.bootstrap('far');host.resume();host.tap(100,host.top+18);assert 'objective focus x=38.0 y=35.0 side=48' in host.log.read_text();host.shot('far-objective');host.pause();host.leave();return {'focus_beyond_old_bounds':[38,35]}

scenario('campaign-wide',campaign)
scenario('campaign-small',campaign,296,240)
scenario('legacy-career',legacy)
scenario('radio-small',radio,296,240,True)
scenario('far-map',far)
for stage in range(4):scenario('construction-'+str(stage),lambda h,s=stage:construction(h,s,complete=s==0))
(args.output/'result.json').write_text(json.dumps({'passed':all(x['passed'] for x in results),'scenarios':results,'capture':'signed production app, official SDK simulator and controlled fixture saves'},ensure_ascii=False,indent=2)+'\n')
raise SystemExit(0 if all(x['passed'] for x in results) else 1)
