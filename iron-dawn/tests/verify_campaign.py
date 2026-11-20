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
 def tap(self,x,y,delay=.28):self.command(f'TAP {round(x*self.pw/self.w)} {round(y*self.ph/self.h)}');time.sleep(delay)
 def shot(self,name):
  data=self.command('SCREENSHOT');assert struct.unpack('>II',data[16:24])==(self.pw,self.ph);(self.out/(name+'.png')).write_bytes(data)
  if not (self.out/'runtime-info.json').exists():(self.out/'runtime-info.json').write_bytes(self.command('RUNTIME INFO'))
 def closed(self):
  self.proc.wait(timeout=15);assert self.proc.returncode==0,self.log.read_text()[-2000:];self.proc=None
 def bootstrap(self,mode=None,legacy=None):
  self.launch();self.command('KEY BACK');self.closed();files=list((self.state/'private-files').rglob('iron-dawn-*.save'));assert files
  for file in files:
   if legacy:file.write_bytes((Path(__file__).parent/f'fixtures/save-v{legacy}.bin').read_bytes())
   else:subprocess.run([str(args.fixture),mode,str(file)],check=True)
  self.launch();assert 'Iron Dawn restored' in self.log.read_text()
 def resume(self):self.tap((self.w-480)//2+365,max(214,self.h-128)+20)
 def wait_log(self,needle,after=0):
  end=time.monotonic()+6
  while time.monotonic()<end:
   if needle in self.log.read_text()[after:]:return
   time.sleep(.08)
  raise AssertionError('Missing log message: '+needle)
 def objective(self,needle='objective focus'):
  after=len(self.log.read_text());self.tap(100,self.top+18);self.wait_log(needle,after)
 def pause(self):
  after=len(self.log.read_text());self.tap(self.w-42,24);self.wait_log('screen=4',after)

 def leave(self):self.tap(self.w/2,(self.h-248)/2+216);self.command('KEY BACK');self.closed()
 def save(self,condition=lambda _:True):
  end=time.monotonic()+4
  while time.monotonic()<end:
   files=[]
   for path in (self.state/'private-files').rglob('iron-dawn-*.save'):
    data=path.read_bytes()
    if len(data)==schema['size'] and struct.unpack_from('<II',data)==(schema['magic'],schema['version']):files.append((struct.unpack_from('<I',data,16)[0],data))
   if files:
    data=max(files)[1]
    if condition(data):return data
   time.sleep(.1)
  raise AssertionError('No current save')
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

def campaign(host, first=0, last=6):
 host.bootstrap('3');offset=(host.w-480)//2
 for chapter in range(first,last):
  host.tap(offset+75,max(214,host.h-128)+64)
  if chapter==first:
   host.shot('archive-second-act');host.tap(87,host.h-68);host.shot('archive-first-act')
  if chapter>=3:host.tap(host.w-87,host.h-68)
  else:host.tap(87,host.h-68)
  row=min(112,(host.h-165)//3);host.tap(host.w-66,74+(chapter%3)*row+32);host.shot('briefing-'+str(chapter+1))
  for page in range(6):
   host.tap(180,host.h-44)
   if re.search(r'chapter='+str(chapter)+r' screen=3',host.log.read_text()):break
   host.shot(f'briefing-{chapter+1}-page-{page+2}')
  else:raise AssertionError('Briefing did not deploy')
  host.shot('battle-'+str(chapter+1));host.objective();host.shot('objective-'+str(chapter+1));host.pause();saved=host.save();assert struct.unpack_from('<i',saved,schema['side'])[0]==(36,44,48,54,60,64)[chapter];host.tap(host.w/2,(host.h-248)/2+216)
 host.command('KEY BACK');host.closed();return {'campaign_pages':2,'maps':[36,44,48,54,60,64][first:last],'normal_ui_deployment':True}

def legacy(host):
 host.bootstrap(legacy=6);host.tap((host.w-480)//2+100,max(214,host.h-128)+20);host.shot('migrated-next-chapter')
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
 host.bootstrap('far');host.resume();host.objective('objective focus x=46.0 y=43.0 side=64');host.shot('far-objective');host.pause();host.leave();return {'focus_beyond_old_bounds':[46,43]}

def production_flow(host):
 host.bootstrap('0');host.resume()
 width=host.ww
 # First chapter's deployment camera sits at (9, 23.5).
 def field(x,y,roof=0):
  host.tap(width/2+(x-y+14.5)*20,(host.top+host.bottom)/2+(x+y-32.5)*10-roof)
 def card(index):
  host.tap(width+59+(index%2)*107,242+(index//2)*62)
 def entities(data):
  return [(i,tuple(data[at+50:at+52]),struct.unpack_from('<2f',data,at))
          for i in range(100) if struct.unpack_from('<f',data,(at:=schema['entities']+56*i)+24)[0]>0]
 host.shot('production-without-selection')
 field(10,25,29);host.shot('barracks-infantry');card(0)
 producer=next(i+1 for i,kind,pos in entities(host.save()) if kind==(8,0) and pos==(10,25))
 host.save(lambda data:data[schema['jobs']+1]==producer)
 field(7,25,30);card(3)
 data=host.save(lambda data:data[schema['construction']]==9)
 assert data[schema['construction']]==9 and not any(kind==(9,0) for _,kind,_ in entities(data))
 host.shot('factory-under-construction')
 time.sleep(10)
 host.pause();data=host.save()
 assert struct.unpack_from('<f',data,schema['construction']+4)[0]==0
 assert not any(kind==(9,0) for _,kind,_ in entities(data))
 host.leave();host.launch();host.resume()
 host.tap(width+165,185);host.shot('factory-ready-after-restart');card(3)
 host.shot('deployment-grid')
 field(7,25);assert host.save()[schema['construction']]==9
 host.tap(width+165,185);card(3)  # Cancel placement, keep the finished factory.
 field(5,21);data=host.save(lambda data:data[schema['construction']]==0)
 assert data[schema['construction']]==0
 assert any(kind==(9,0) and pos==(5,21) for _,kind,pos in entities(data))
 field(5,21,28);host.shot('factory-vehicles');card(0)
 host.save(lambda data:data[schema['jobs']]==2)
 host.pause();host.leave()
 return {'selected_barracks':True,'factory_restored_ready':True,'blocked_and_cancelled_placement':True,'selected_factory':True}

def squads(host):
 host.bootstrap('squads');host.resume()
 wide=host.ww<host.w
 if wide:
  opener=(host.ww+165,134)
  recall=(host.ww+59,242)
  assign=(host.ww+165,242)
  select_all=(host.ww/12,host.bottom+24)
 else:
  row=max(28,min(int(host.h*.18),int(np.ceil(30*host.h/host.ph))))
  line=max(20,int(np.ceil(14*host.h/host.ph)))
  panel=host.bottom-(line+8+12+row*2+6)
  opener=(host.w-50,panel+4+line/2)
  recall=((host.w-480)/2+63,panel+line+12+row/2)
  assign=(recall[0],recall[1]+row+6)
  select_all=((host.w-480)/2+240,host.bottom+5+row/2)
 def open_squads():
  if not wide:host.tap((host.w-480)/2+428,host.bottom+5+row/2)
  host.tap(*opener)
 open_squads();host.tap(*assign);host.shot('squad-assigned')
 data=host.save(lambda data:data[schema['squads']+5:schema['squads']+7]==b'\x01\x01')
 assert [i for i in range(100) if data[schema['squads']+i]==1]==[5,6]
 host.tap(*select_all)
 open_squads();host.tap(*recall)
 host.wait_log('selection ids=5,6,')
 host.tap(*recall);host.wait_log('Iron Dawn squad focused')
 host.shot('squad-recalled');host.pause();host.leave()
 host.launch();host.resume();open_squads();host.shot('squad-restored')
 assert host.save()[schema['squads']+5:schema['squads']+7]==b'\x01\x01'
 host.pause();host.leave()
 return {'slots':[5,6],'recall_and_camera_focus':True,'restored':True}

def tactical_orders(host):
 host.bootstrap('tactics');host.resume()
 data=host.save()
 entity=schema['entities']
 truck=next(i for i in range(100) if data[entity+i*56+50:entity+i*56+52]==b'\x04\x02')
 truck_x,truck_y=struct.unpack_from('<2f',data,entity+truck*56)
 def target(snapshot):
  return struct.unpack_from('<h',snapshot,entity+5*56+schema['target_offset'])[0]
 def field(x,y,height=0):
  # The convoy chapter opens with the camera at (49, 13).
  host.tap(host.ww/2+(x-y-36)*20,(host.top+host.bottom)/2+(x+y-62)*10-height)
 def arm(retreat=False):
  if host.ww<host.w:
   host.tap(165+78*retreat,host.top+44)
  else:
   row=max(28,min(int(host.h*.18),int(np.ceil(30*host.h/host.ph))))
   line=max(20,int(np.ceil(14*host.h/host.ph)))
   panel=host.bottom-(line+8+12+row*2+6)
   host.tap((host.w-480)/2+428,host.bottom+5+row/2)
   host.shot('tactical-drawer')
   host.tap(host.w-218+82*retreat,panel+4+line/2)
 arm();field(truck_x,truck_y,10)
 host.wait_log('escort order truck='+str(truck))
 host.save(lambda current:target(current)==-4-truck)
 host.shot('escorting');host.pause();host.leave();host.launch();host.resume()
 assert target(host.save())==-4-truck
 arm(True)
 field(51,10)
 assert 'Iron Dawn retreat order' not in host.log.read_text()
 field(51,15.5)
 host.wait_log('Iron Dawn retreat order')
 host.save(lambda current:target(current)==-3)
 host.shot('retreating');host.pause();host.leave()
 return {'truck':truck,'escort_order_restored':True,'retreat_rejects_building':True}

def combined_arms(host,mode):
 host.bootstrap(mode);host.resume()
 data=host.save()
 entity=schema['entities']
 view={'combined-water':(34,24),'combined-sky':(11.5,38.5),'combined-hero':(52.5,50.5)}[mode]
 def find(kind,team=0,snapshot=None):
  if snapshot is None:snapshot=host.save()
  return next(i for i in range(100) if snapshot[entity+i*56+50:entity+i*56+52]==bytes((kind,team))
              and struct.unpack_from('<f',snapshot,entity+i*56+24)[0]>0)
 def position(i,snapshot=None):
  return struct.unpack_from('<2f',snapshot or data,entity+i*56)
 def field(x,y,z=0):
  cx,cy=view
  host.tap(host.ww/2+(x-y-cx+cy)*20,(host.top+host.bottom)/2+(x+y-cx-cy)*10-z)
 def card(index):
  host.tap(host.ww+59+(index%2)*106,242+(index//2)*72)
 if mode=='combined-water':
  dock=find(16,snapshot=data);boat=find(13,snapshot=data)
  field(*position(dock),30);host.shot('shipyard')
  card(0);host.save(lambda state:state[schema['jobs']:schema['jobs']+2]==bytes((13,dock+1)))
  map_x,map_y=host.ww+12,host.top+12
  bx,by=position(boat)
  px,py=round(bx*96/44),round(by*96/44)
  host.tap(map_x+px,map_y+py)
  view=(px*44/96,py*44/96)
  field(bx,by,12);field(23.5,20.5)
  host.shot('patrol-order')
  time.sleep(8)
  host.pause()
  advanced=host.save(lambda state:position(boat,state)[1]<24)
  assert sum(advanced[entity+i*56+50:entity+i*56+52]==bytes((13,0)) for i in range(100))>=2
  host.leave()
  return {'completed_boat':True,'sailed_under_bridge':True}
 if mode=='combined-sky':
  pad=find(17,snapshot=data)
  # Move the camera to the helipad using the normal 854-by-480 minimap.
  map_x,map_y=host.ww+12,host.top+12
  px,py=9,59
  host.tap(map_x+px,map_y+py)
  view=(px*54/96,py*54/96)
  field(*position(pad),24);host.shot('helipad')
  card(0);host.save(lambda state:state[schema['jobs']:schema['jobs']+2]==bytes((14,pad+1)))
  host.tap(host.ww+165,185);card(5);host.shot('combined-construction')
  card(2);host.save(lambda state:state[schema['construction']]==18)
  time.sleep(7)
  card(2);field(6.5,35.5)
  placed=host.save(lambda state:state[schema['construction']]==0)
  assert find(18,snapshot=placed)>=0
  host.shot('air-defences');host.pause();host.leave()
  return {'rotorcraft_queue':True,'built_anti_air':True}
 hero=find(15,snapshot=data)
 field(*position(hero),18)
 host.shot('selected-commander')
 if host.ww<host.w:host.tap(323,host.top+44)
 else:
  row=max(28,min(int(host.h*.18),int(np.ceil(30*host.h/host.ph))))
  line=max(20,int(np.ceil(14*host.h/host.ph)))
  panel=host.bottom-(line+8+12+row*2+6)
  host.tap((host.w-480)/2+428,host.bottom+5+row/2)
  host.tap(host.w-302,panel+4+line/2)
 def cooldown(state):
  return struct.unpack_from('<f',state,schema['mission']+20)[0]
 result=host.save(lambda state:cooldown(state)>0)
 assert struct.unpack_from('<f',result,entity+5*56+24)[0]>195
 host.shot('support-cooldown');host.pause();host.leave()
 host.launch();host.resume();assert cooldown(host.save())>0
 host.pause();host.leave()
 return {'hero_support':True,'cooldown_restored':True}

scenario('campaign-wide',campaign)
scenario('campaign-small',lambda host:campaign(host,3,6),296,240)
def legacy_convoy(host):
 host.bootstrap(legacy=7);host.resume();host.objective('objective focus x=21.0 y=18.0 side=44')
 host.pause();data=host.save();assert struct.unpack_from('<i',data,schema['side'])[0]==44
 host.leave();return {'actual_v7_restore':True,'legacy_convoy_map':44}
scenario('legacy-convoy',legacy_convoy)
def legacy_production(host):
 host.bootstrap(legacy=8);host.resume();host.pause()
 data=host.save()
 assert struct.unpack_from('<i',data,schema['side'])[0]==60
 assert data[schema['construction']]==6 and data[schema['jobs']]==2
 assert not any(data[schema['squads']:schema['squads']+100])
 host.leave();return {'actual_v8_restore':True,'construction_and_training_preserved':True}
scenario('legacy-production',legacy_production)
scenario('legacy-career',legacy)
scenario('radio-small',radio,296,240,True)
scenario('far-map',far)
scenario('production-flow',production_flow)
scenario('squads-wide',squads)
scenario('squads-small',squads,296,240)
scenario('tactical-wide',tactical_orders)
scenario('tactical-small',tactical_orders,296,240)
for mode in ['combined-water','combined-sky','combined-hero']:
 scenario(mode,lambda host,mode=mode:combined_arms(host,mode))
scenario('combined-hero-small',lambda host:combined_arms(host,'combined-hero'),296,240)
for stage in range(4):scenario('construction-'+str(stage),lambda h,s=stage:construction(h,s,complete=s==0))
(args.output/'result.json').write_text(json.dumps({'passed':all(x['passed'] for x in results),'scenarios':results,'capture':'signed production app, official SDK simulator and controlled fixture saves'},ensure_ascii=False,indent=2)+'\n')
raise SystemExit(0 if all(x['passed'] for x in results) else 1)
