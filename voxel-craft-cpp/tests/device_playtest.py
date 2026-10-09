"""Deterministic interaction fixture, only the isolated UI2 test identity."""
import argparse,json,re,sys,time
from pathlib import Path
root=next((p for p in Path(__file__).resolve().parents if (p/'tools/pxadb/pxadb.py').is_file()),None)
if root is None:raise SystemExit('Run this harness from a pxa-projects workspace checkout.')
sys.path.insert(0,str(root/'tools/pxadb'));import pxadb
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--log',type=Path);p.add_argument('--device',action='store_true');p.add_argument('--resume',action='store_true');p.add_argument('--tools',action='store_true');p.add_argument('--expected',type=Path);p.add_argument('--local-heap',action='store_true');p.add_argument('--no-screenshots',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
if a.local_heap and not a.device:p.error('--local-heap requires a physical device')
monitoring=False
state={};ui={};bag={};edits={};craft={};tools={};events=[];sounds=[];log_offset=0;log_epoch=0;dropped=False;report={'passed':False,'scenes':[]};started=time.monotonic()
with pxadb.PxaDbClient(a.port,15)as c,(a.output/'serial.jsonl').open('w',buffering=1)as log:
 def record(kind,v):log.write(json.dumps({'seconds':time.monotonic()-started,'kind':kind,'payload':v})+'\n')
 def parse(line):
  stamp=re.match(r'^(\d+)\t',line)
  if a.device and log_epoch and stamp and int(stamp.group(1))<log_epoch:return
  if '[PXA app='in line and 'app=pxa-voxel-craft-cpp-ui2 'not in line:return
  v={k:int(x)for k,x in re.findall(r'(\w+)=(-?\d+)',line)}
  if 'Runtime permission prompt queued:'in line and ':pxa-voxel-craft-cpp-ui2 permission=音频播放'in line:ui['audio_prompt']=1
  if 'VOXEL-STATE 'in line:state.update(v);ui['screen']=1
  if 'VOXEL-BAG 'in line:bag.update(v)
  if 'VOXEL-TOOLS 'in line:tools.update(v)
  if 'VOXEL-CRAFT 'in line:
   craft.update(v)
   for key in ['grid','bag','quick','output','back']:
    m=re.search(key+r'=(\d+),(\d+)',line)
    if m:craft[key]=tuple(map(int,m.groups()))
  if 'VOXEL-EDIT 'in line:edits.update(v)
  if 'VOXEL-UI 'in line:
   ui.update(v)
   for key in ['menu','bag']:
    m=re.search(key+r'=(\d+),(\d+)',line)
    if m:ui[key]=tuple(map(int,m.groups()))
  if any(x in line for x in ['VOXEL-MINED ','VOXEL-SAVE ','VOXEL-LOAD ','Voxel audio']):
   if line not in events:events.append(line)
  if 'VOXEL-SOUND 'in line:sounds.append(v)
 def receive(f):
  global dropped
  record(f.kind,f.payload);parse(f.payload)
  if f.kind=='DROP':dropped=True
 c.log_callback=receive;c.raw_callback=lambda b:record('RAW',b.decode('utf-8','replace'))
 def req(cmd):
  record('COMMAND',cmd);quiet=a.device and cmd.startswith('MEMORY')
  if quiet:c.unsubscribe_logs()
  r=c.request(cmd,timeout=60)
  if quiet:c.subscribe_logs()
  record('RESPONSE',[(f.kind,f.payload)for f in r if f.kind!='DATA']);return r
 def pump(seconds):
  global log_offset,dropped
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   c.pump_logs()
   if a.device and dropped:
    dropped=False;c.unsubscribe_logs();c.subscribe_logs()
   if a.log:
    data=a.log.read_text()
    for line in data[log_offset:].splitlines():parse(line)
    log_offset=len(data)
   time.sleep(.02)
 def until(condition,seconds=15):
  end=time.monotonic()+seconds
  while not condition()and time.monotonic()<end:pump(.1)
  assert condition(),(ui,state,bag,events[-8:])
 def screen(n):until(lambda:ui.get('screen')==n);pump(.3)
 def tap(x,y):
  req(f'INPUT POINTER DOWN {int(x)} {int(y)} 0');req('INPUT SYNC');pump(.18);req(f'INPUT POINTER UP {int(x)} {int(y)} 0');req('INPUT SYNC');pump(.5)
 def hold(x,y,seconds):
  req(f'INPUT POINTER DOWN {int(x)} {int(y)} 0');req('INPUT SYNC');pump(seconds);req(f'INPUT POINTER UP {int(x)} {int(y)} 0');req('INPUT SYNC');pump(.7)
 def snap(name):
  if a.no_screenshots:
   row={'name':name,'ui':dict(ui),'state':dict(state),'bag':dict(bag),'edits':dict(edits)};report['scenes'].append(row);return row
  cap=pxadb.screenshot_capture(req('SCREENSHOT JPEG'if a.device else'SCREENSHOT'));pxadb.write_screenshot(cap,a.output/(name+('.jpg'if a.device else'.png')))
  row={'name':name,'ui':dict(ui),'state':dict(state),'bag':dict(bag),'edits':dict(edits),'metadata':cap.metadata};report['scenes'].append(row);return row
 def memory(cmd='MEMORY'):
  values={}
  for f in req(cmd):
   if f.kind=='DATA':
    v=pxadb.info_properties(f.payload);scope=v.pop('scope');name=v.pop('name','');values[scope+(':'+name if name else '')]=v
  return values
 try:
  report['hello']=c.hello()
  if a.device:
   c.subscribe_logs();req('INPUT KEY HOME');req('INPUT CANCEL')
   req('INPUT POINTER DOWN 148 216 0');req('INPUT SYNC');pump(.08)
   for y in [185,150,115,80,45]:req(f'INPUT POINTER MOVE 148 {y} 0');req('INPUT SYNC');pump(.06)
   req('INPUT POINTER UP 148 45 0');req('INPUT SYNC');pump(.8);req('INPUT KEY HOME')
   for app in ['pxa-voxel-craft-cpp','pxa-voxel-craft-cpp-ui2']:
    try:req('PACKAGE stop '+app);pump(3)
    except pxadb.PxaDbError as e:
     if str(e)!='package_not_running':raise
   sync=req('INPUT SYNC');log_epoch=int(pxadb.info_properties(sync[-1].payload)['device_us'])//1000
   ui.clear();state.clear();bag.clear();tools.clear();events.clear();sounds.clear()
   if a.local_heap:report['memory_before']=memory();req('MEMORY START');monitoring=True
   req('PACKAGE run pxa-voxel-craft-cpp-ui2')
  if a.device:
   until(lambda:ui.get('audio_prompt')or ui.get('screen')==0)
   if ui.get('audio_prompt'):
    # "queued" precedes the LVGL owner's actual modal paint/activation.
    # Avoid sending the approval tap to the fading launch animation.
    pump(1);snap('audio-permission');tap(199,183);ui['audio_prompt']=0
  screen(0);pump(1)
  until(lambda:ui.get('audio')==1 or any('Voxel audio ready'in x for x in events));snap('title')
  b=ui['button'];s=b/30;pad=max(3,int(6*s));top=ui['menu'][1]-b//2-pad+max(4,int(7*s));x=148
  h=max(20,int(29*s));gap=max(3,int(6*s));titleys=[top+int(66*s)+h/2+i*(h+gap)for i in range(3)]
  slotys=[top+int(43*s)+h/2+i*(h+gap)for i in range(4)]
  pauseys=[top+int(42*s)+h/2+i*(h+gap)for i in range(4)]
  if a.tools:
   names=['wpick','waxe','wshovel','spick','saxe','sshovel']
   if a.resume:
    tap(x,titleys[1]);screen(4);tap(x,slotys[0]);screen(1)
    until(lambda:all(tools.get(k)==1 for k in names)and tools.get('wear')==59)
    if a.expected:assert bag['hash']==json.loads(a.expected.read_text())['saved_inventory_hash']
    snap('restored-tools');report['restored_inventory_hash']=bag['hash']
   else:
    tap(x,titleys[0]);screen(1);until(lambda:state.get('block')==12)
    action_x=296-pad-b/2;menu_y=ui['menu'][1]-b//2;bag_y=ui['bag'][1]-int(23*s)//2
    action_y=max(menu_y+b+pad,(bag_y-3*b-2*pad+menu_y+b)//2)+b/2
    for index,name in enumerate(names):
     tap(action_x,action_y);screen(7)
     cell=craft['cell'];space=craft['space'];gx,y0=craft['grid'];qx,qy=craft['quick']
     material=1 if index<3 else 0
     shape=index%3
     material_cells=[0,1,2]if shape==0 else[0,1,3]if shape==1 else[0]
     stick_cells=[4,7]if shape<2 else[3,6]
     tap(qx+material*(cell+space)+cell/2,qy+cell/2)
     for at in material_cells:tap(gx+(at%3)*(cell+space)+cell/2,y0+(at//3)*(cell+space)+cell/2)
     tap(qx+2*(cell+space)+cell/2,qy+cell/2)
     for at in stick_cells:tap(gx+(at%3)*(cell+space)+cell/2,y0+(at//3)*(cell+space)+cell/2)
     snap(name+'-recipe');tap(*craft['output']);tap(*craft['back']);screen(1)
     until(lambda:tools.get(name)==1)
    snap('six-crafted-tools')
    assert tools['stick']==20
    slot=max(12,int(23*s));space_game=max(1,int(3*s));bar_x=(296-(10*slot+9*space_game))/2
    def select(index):tap(bar_x+index*(slot+space_game)+slot/2,ui['bag'][1])
    select(4);hold(action_x,action_y,1.20);until(lambda:state.get('block')==3 and tools.get('wear')==59)
    # An axe cannot silently accelerate stone; releasing cancels its progress.
    hold(action_x,action_y,.65);until(lambda:bag.get('progress')==0);assert state['block']==3
    for index,block,seconds,following,wear in [(3,3,2.05,3,59),(6,3,1.22,5,131),(7,5,.82,2,131),(8,2,.58,0,131)]:
     select(index);hold(action_x,action_y,seconds)
     until(lambda:tools.get('wear')==wear)
     if following:until(lambda:state.get('block')==following)
     snap('mined-with-'+names[index-3])
    req('INPUT POINTER DOWN 78 140 0');req('INPUT SYNC');pump(.12)
    req('INPUT POINTER MOVE 78 90 0');req('INPUT SYNC');pump(.4);req('INPUT CANCEL');pump(1.2)
    until(lambda:state.get('block')==2);select(5);hold(action_x,action_y,.76);until(lambda:tools.get('wear')==59)
    assert bag['stone']==27 and bag['plank']==25 and bag['wood']==1 and bag['dirt']==2 and bag['table']==1
    mined=[{k:int(v)for k,v in re.findall(r'(\w+)=(\d+)',e)}for e in events if 'VOXEL-MINED 'in e]
    for item,block,duration in [(34,12,800),(33,3,1600),(36,3,800),(37,5,400),(38,2,162),(35,2,325)]:
     matches=[v['elapsed_ms']for v in mined if v['tool']==item and v['block']==block]
     assert len(matches)==1 and duration-2<=matches[0]<=duration+35,(item,matches)
    report['tool_mining_durations']=mined;report['saved_inventory_hash']=bag['hash'];snap('tools-after-mining')
    tap(*ui['menu']);screen(2);tap(x,pauseys[1]);screen(5);tap(x,slotys[0])
    until(lambda:any('VOXEL-SAVE slot=0 generation=1 ok=1'in e for e in events),30)
   assert all(v['ok']==1 for v in sounds),sounds
   report['tools']=dict(tools)
  elif a.resume:
   tap(x,titleys[1]);screen(4);tap(x,slotys[0]);screen(1);pump(2);snap('restored-inventory');report['restored_bag']=dict(bag)
   assert bag['bush']==1 and bag['leaves']==1 and bag['dirt']==1 and bag['table']==1 and bag['stone']==1 and bag['wood']==0
   if a.device:
    # Gestures remain independent of invisible joysticks and action buttons.
    before_pose=dict(state);req('INPUT POINTER DOWN 78 140 0');req('INPUT SYNC');pump(.12)
    req('INPUT POINTER MOVE 78 90 0');req('INPUT SYNC');pump(.9);req('INPUT CANCEL');pump(1.2)
    assert (state['xq8'],state['zq8'])!=(before_pose['xq8'],before_pose['zq8']);snap('movement')
    before_pose=dict(state);req('INPUT POINTER DOWN 175 115 0');req('INPUT SYNC');pump(.12)
    req('INPUT POINTER MOVE 220 130 0');req('INPUT SYNC');pump(.2);req('INPUT CANCEL');pump(1.2)
    assert (state['yawq10'],state['pitchq10'])!=(before_pose['yawq10'],before_pose['pitchq10']);snap('look')
    frozen_bag={k:bag[k]for k in ['bush','leaves','dirt','wood','plank','table','stone']}
    req('INPUT POINTER DOWN 78 140 0');req('INPUT SYNC');pump(.12)
    req('INPUT POINTER MOVE 78 90 0');req('INPUT SYNC');pump(.2)
    req('INPUT KEY HOME');pump(1);req('PACKAGE run pxa-voxel-craft-cpp-ui2');pump(2)
    assert ui.get('screen')==1 and bag['progress']==0 and all(bag[k]==v for k,v in frozen_bag.items())
    pose=dict(state);pump(1.2);assert all(state[k]==pose[k]for k in ['xq8','zq8','yawq10','pitchq10']);snap('foreground-controls-cancelled')
    report['movement_look_background_checks']=True
  else:
   tap(x,titleys[0]);screen(1);until(lambda:state.get('block')==16);snap('empty-inventory-hud');assert bag['bush']==bag['leaves']==bag['dirt']==bag['wood']==0
   action_x=296-pad-b/2;menu_y=ui['menu'][1]-b//2;bag_y=ui['bag'][1]-int(23*s)//2
   action_y=max(menu_y+b+pad,(bag_y-3*b-2*pad+menu_y+b)//2)+b/2
   for block,name,seconds,next_block in [(16,'ground-bush',.62,6),(6,'leaves',.86,2),(2,'dirt',1.06,5),(5,'wood',2.02,3)]:
    until(lambda:state.get('block')==block);hold(action_x,action_y,seconds);until(lambda:state.get('block')==next_block);snap('mined-'+name)
   assert bag['bush']==bag['leaves']==bag['dirt']==bag['wood']==1
   tap(action_x,action_y+b+pad);pump(1.2);until(lambda:bag.get('bush')==0);assert state['block']==16;snap('placed-consumed-bush')
   hold(action_x,action_y,.62);until(lambda:bag.get('bush')==1 and state.get('block')==3)
   tap(*ui['bag']);screen(6);snap('normal-backpack')
   cell=craft['cell'];space=craft['space'];gx,y0=craft['grid'];qx,qy0=craft['quick'];bag_x,bag_y=craft['bag']
   tap(qx+3*(cell+space)+cell/2,qy0+cell/2);tap(bag_x+cell/2,bag_y+cell/2);snap('moved-wood-to-backpack')
   # Reserve one log from the bag, then take the four-plank preview.
   tap(bag_x+cell/2,bag_y+cell/2);tap(gx+cell/2,y0+cell/2);snap('wood-crafting-preview')
   tap(*craft['output']);snap('crafted-four-planks')
   # A shared stack backs four separate 2x2 ingredient cells.
   tap(qx+3*(cell+space)+cell/2,qy0+cell/2)
   for cy,cx in [(0,0),(0,1),(1,0),(1,1)]:tap(gx+cx*(cell+space)+cell/2,y0+cy*(cell+space)+cell/2)
   snap('workbench-crafting-preview');tap(*craft['output']);snap('crafted-workbench')
   tap(*craft['back']);screen(1);pump(1.2);assert bag['wood']==bag['plank']==0 and bag['table']==1
   # Crafting puts the table in empty hotbar slot three; select and place it.
   slot_game=max(12,int(23*s));space_game=max(1,int(3*s));bar_width=10*slot_game+9*space_game;bar_x=(296-bar_width)/2
   tap(bar_x+3*(slot_game+space_game)+slot_game/2,ui['bag'][1]);tap(action_x,action_y+b+pad);until(lambda:state.get('block')==12);snap('placed-workbench')
   tap(action_x,action_y);screen(7);snap('workbench-use');tap(*craft['back']);screen(1)
   hold(action_x,action_y,2.02);until(lambda:bag.get('table')==1 and state.get('block')==3)
   # Stone remains intact while progress/cracks are visible; cancellation resets.
   req(f'INPUT POINTER DOWN {int(action_x)} {int(action_y)} 0');req('INPUT SYNC');pump(1.3);snap('stone-mining-progress');assert not any('VOXEL-MINED block=3 'in e for e in events)
   req('INPUT CANCEL');pump(1.2);assert bag['progress']==0 and state['block']==3
   hold(action_x,action_y,3.68);until(lambda:bag.get('stone')==1);snap('stone-collected')
   mined=[{k:int(v)for k,v in re.findall(r'(\w+)=(\d+)',e)}for e in events if 'VOXEL-MINED 'in e]
   for block,min_ms in [(16,200),(6,450),(2,650),(5,1600),(3,3200),(12,1600)]:
    matches=[v['elapsed_ms']for v in mined if v['block']==block];assert matches and all(min_ms-2<=t<=min_ms+35 for t in matches),(block,matches)
   assert any(v.get('action')==0 for v in sounds)and any(v.get('action')==1 for v in sounds)and any(v.get('action')==2 for v in sounds)
   assert all(v['ok']==1 for v in sounds);report['mined_durations']=mined
   tap(*ui['menu']);screen(2);tap(x,pauseys[1]);screen(5)
   for i in range(3):
    tap(x,slotys[i]);until(lambda:any(f'VOXEL-SAVE slot={i} generation=1 ok=1'in e for e in events),30);snap('saved-slot-'+str(i+1))
   report['saved_bag']=dict(bag);report['saved_pose']=dict(state)
  report['sound_commands']=sounds;report['events']=events;report['passed']=True
 finally:
  req('INPUT CANCEL')
  if a.device:
   if monitoring:report['launch_gameplay_peak']=memory('MEMORY STOP');monitoring=False
   report['memory']=memory();req('PERF CLEAR');req('PACKAGE stop pxa-voxel-craft-cpp-ui2');pump(3);report['memory_after_stop']=memory()
  (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('Gameplay fixture mining/quantity/placement/backpack/recipes/save/audio checks passed.',flush=True)
