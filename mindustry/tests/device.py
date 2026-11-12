#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Deploy and exercise the real folded HUD, fonts and presentation on PXA hardware.
Run from the firmware workspace root. Starts a sandbox through the normal UI and backs up the previous save. Never flashes firmware.
"""
import argparse,json,re,statistics,sys,time
from pathlib import Path
from ui_controls import Hud
sys.path.insert(0,str(Path.cwd()/'tools/pxadb'))
import pxadb

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--audio',action='store_true');p.add_argument('--skip-install',action='store_true');p.add_argument('--production',action='store_true');p.add_argument('--mining',action='store_true');p.add_argument('--restore-from',type=Path);p.add_argument('--port',required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 report={'package':str(a.package),'passed':False};messages=[];audio_events=[];running=False;recording=False
 with pxadb.PxaDbClient(a.port,20) as c,(a.output/'serial.jsonl').open('w',buffering=1) as log:
  def record(kind,payload):log.write(json.dumps({'time':time.time(),'kind':kind,'payload':payload},ensure_ascii=False)+'\n')
  def onlog(f):
   messages.append(f.payload);record(f.kind,f.payload)
   if 'Mindustry: music' in f.payload:audio_events.append(f.payload)
  c.log_callback=onlog;c.raw_callback=lambda b:record('RAW',b.decode('utf-8','replace'))
  def req(cmd):
   record('COMMAND',cmd)
   quiet=c.log_subscribed and cmd.startswith(('PERF','MEMORY','SCREENSHOT'))
   if quiet:c.unsubscribe_logs(timeout=3)
   frames=c.request(cmd,timeout=45)
   if quiet:c.subscribe_logs()
   record('RESPONSE',[{'kind':f.kind,'payload':f.payload} for f in frames if f.kind!='DATA'])
   return frames
  def pump(seconds):
   end=time.monotonic()+seconds
   while time.monotonic()<end:c.pump_logs();time.sleep(.005)
  def wait(predicate):
   end=time.monotonic()+35
   while time.monotonic()<end:
    if predicate():return
    pump(.025)
   raise TimeoutError('guest startup / font warmup')
  def shot(name):
   for attempt in range(3):
    try:
     cap=pxadb.screenshot_capture(req('SCREENSHOT JPEG'));break
    except pxadb.PxaDbError as error:
     if attempt==2 or not pxadb.screenshot_retryable(error):raise
     pump(.4)
   pxadb.write_screenshot(cap,a.output/(name+'.jpg'))
   report.setdefault('screenshots',{})[name]=cap.metadata
   assert cap.metadata['source'] in ('surface','surface-direct'),cap.metadata
  def tap(x,y):
   req(f'INPUT POINTER DOWN {x} {y} 0');req('INPUT SYNC');pump(.09)
   req(f'INPUT POINTER UP {x} {y} 0');req('INPUT SYNC');pump(.35)
  try:
   report['hello']=c.hello();info=pxadb.info_properties(req('INFO')[-1].payload);report['info']=info;volumes=pxadb.device_volumes(c)
   candidates=[v for v in volumes if v.get('relative')=='nand'] or [v for v in volumes if int(v.get('capabilities','0'))&67==67 and v.get('state')=='2']
   assert candidates,'No writable application volume'
   args=pxadb.build_parser().parse_args(['package','install',str(a.package),'--yes','--via','auto','--volume',candidates[0]['epoch'],'--port',a.port,'--timeout','30'])
   report['packages_before']=[f.payload for f in req('PACKAGES') if f.kind=='PKG']
   for payload in report['packages_before']:
    if 'active=1' in payload:req('PACKAGE stop '+payload.split('\t')[0])
   pump(.5)
   fs=pxadb.NormalFsClient(c);backups=[]
   # Volume-local app data is a leased block image, not a legacy Flash KV path.
   candidates_data=[candidates[0]['relative'].strip('/')+'/PXA/apps/lucinhu/pxa-mindustry/data',
     'pxa-state/data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171~pxa-mindustry']
   def copy_tree(remote,relative):
    for kind,_,name in fs.list(remote):
     if kind=='D':copy_tree(remote+'/'+name,relative/name)
     elif kind=='F':
      destination=a.output/'save-backup'/relative/name;destination.parent.mkdir(parents=True,exist_ok=True)
      fs.get(remote+'/'+name,destination);backups.append(str(destination))
   for remote in candidates_data:
    try:copy_tree(remote,Path('.'))
    except pxadb.PxaDbRejectedError as error:
     if 'path_not_found' not in str(error):raise
    if backups:break
   report['save_backups']=backups
   if not a.skip_install:pxadb.command_package_install(args,connection=c)
   c.subscribe_logs();pump(.5);req('INPUT KEY HOME');pump(.3)
   width=int(info['width']);height=int(info['height'])
   req(f'INPUT SWIPE {width//2} {height-60} {width//2} {height//4} 400 8');pump(1)
   messages.clear();req('PACKAGE run pxa-mindustry');running=True
   wait(lambda:any('glyph cache ready' in x for x in messages));cold=list(messages)
   def stamp(pattern):
    line=next(x for x in cold if pattern in x);return int(line.split('\t',1)[0])
   report['title_font_ready_ms']=stamp('glyph cache ready')-stamp('Pocket: viewport')
   text='\n'.join(messages);match=re.search(r'Mindustry UI bounds=(\d+),(\d+),(\d+),(\d+)',text);assert match,text[-2000:]
   bounds=tuple(map(int,match.groups()));width=int(info['width']);height=int(info['height'])
   hud=Hud(width,height,tap,bounds);pump(.4);shot('title')
   yy=(height-222)//2
   if a.audio:
    wait(lambda:any('music event id=0 state=1 status=0' in x for x in messages))
    report['menu_music_ready']=True
    # Old saves migrate to original music mode; use the normal settings UI to
    # enable continuous playback for the following rendering/production run.
    tap(width//2,yy+61+4*28+12)
    tap(width//2,hud.top+12+hud.n+2*(hud.n+12)+hud.n//2)
    hud.back()
   # Preserve the previous app save before starting the test sandbox via its UI.
   has_save=any('save restored' in x for x in cold)
   if has_save:assert report.get('save_backups'),'Existing save could not be backed up'
   tap(width//2,yy+61+2*28+12)
   if has_save:hud.confirm_new()
   wait(lambda:any('new sandbox' in x for x in messages))
   pump(4);shot('world')
   if a.audio:
    wait(lambda:any(re.search(r'music event id=[1-6] state=1 status=0',x) for x in messages))
    report['game_music_ready']=True

   # Both icons use their complete original layer stack. Build the pneumatic
   # drill through the same UI and return the camera for the gesture checks.
   hud.dock(0);shot('drill-icons');hud.dock(0)
   hud.choose(0,1,2);hud.jump(19,33);tap(width//2+8,hud.cy+8)
   shot('pneumatic-preview');hud.confirm();hud.cancel();hud.dock(1)
   caps=pxadb.info_properties(req('INPUT CAPABILITIES')[-1].payload)
   assert int(caps['max_pointers'])>=2,caps
   def point(phase,id,x,y):
    req(f'INPUT POINTER {phase} {int(x)} {int(y)} {id}');req('INPUT SYNC');pump(.09)
   def nav_events():
    events={}
    for line in messages:
     m=re.search(r'^(\d+)\t.*Mindustry: view zoom=(\d+) camera=([\d.]+),([\d.]+)',line)
     if m:events[int(m[1])]=[int(m[2]),float(m[3]),float(m[4])]
    return events
   def gesture(start,end,shift=(0,0)):
    before=max(nav_events(),default=0);cx,cy=width//2,hud.cy
    point('DOWN',0,cx-start,cy);point('DOWN',1,cx+start,cy)
    point('MOVE',0,cx-end+shift[0],cy+shift[1]);point('MOVE',1,cx+end+shift[0],cy+shift[1])
    point('UP',0,cx-end+shift[0],cy+shift[1]);point('UP',1,cx+end+shift[0],cy+shift[1])
    wait(lambda:max(nav_events(),default=0)>before)
    events=nav_events();value=events[max(events)];report.setdefault('gestures',[]).append(value);return value
   z,x,y=gesture(40,80);assert abs(z-32)<=1 and abs(x-24.5)<.15 and abs(y-35)<.15,(z,x,y);shot('pinch-out')
   z,px,py=gesture(60,60,(24,12));assert abs(px-(x-24/z))<.15 and abs(py-(y-12/z))<.15,(z,px,py);shot('two-finger-pan')
   z,_,_=gesture(60,30);assert abs(z-16)<=1
   wide=min(width//2-hud.left-24,hud.right-width//2-24)
   z,_,_=gesture(20,wide);assert z==40;shot('zoom-max')
   z,_,_=gesture(wide,8);assert z==8;shot('zoom-min')
   for _ in range(2):hud.tools(2);hud.dock(3)
   hud.dock(1)
   point('DOWN',0,width//2-48,hud.cy+8);pump(.6);point('UP',0,width//2-48,hud.cy+8)
   wait(lambda:any('block copied' in x for x in messages));shot('copied-conveyor');hud.cancel()
   hud.choose(4,3,4);tap(width//2+48,hud.cy-24);shot('mender-range');hud.cancel()
   hud.dock(0);pump(.7);shot('construction-drawer')
   # Drawer is already open; select defense, then copper wall.
   dy=hud.bottom-8-3*hud.n-18
   tap(hud.left+4+int(2.5*(hud.width-8)/7),dy+hud.n//2)
   tap(hud.left+4+int(1.5*(hud.width-8)/3),dy+hud.n+(hud.n+18)//2);hud.mode='build'
   tap(width//2-32,hud.cy);pump(.5);shot('construction-preview');hud.cancel()
   hud.tools(0);pump(1);shot('research');hud.back()
   hud.dock(2);pump(.5);shot('map');hud.dock(2)
   # Inventory toggles by the right-hand pill in the top HUD.
   tap(hud.right-100,hud.top+4+hud.n//2);pump(.5);shot('inventory');tap(width//2,hud.cy)
   if a.production:
    def exact_tile(x,y):
     hud.jump(x,y)
     # At 305 DPI a minimap tap can quantize by half a tile. Read the real
     # camera with an unchanged two-finger gesture before placing/supplying.
     z,cx,cy=gesture(40,40)
     tap(width//2-int(cx*z)+int((x+.5)*z),hud.cy-int(cy*z)+int((y+.5)*z))
    def place(cat,index,count,x,y):
     hud.choose(cat,index,count);exact_tile(x,y);hud.confirm();hud.cancel()
    def supply(x,y,count=1):
     exact_tile(x,y);hud.mode='inspect'
     for _ in range(count):hud.supply()
     hud.cancel()
    if a.mining:
     exact_tile(21,33);pump(2.2);shot('alpha-mining')
     exact_tile(24,32);report['mining_scenario']=True
    place(3,1,3,27,31);supply(27,31)
    place(4,0,4,26,31);supply(26,31)
    exact_tile(27,31);hud.mode='inspect';shot('smelter-production');hud.cancel()
    place(6,0,2,27,35);supply(27,35,2)
    place(4,0,4,26,36);supply(26,36)
    exact_tile(27,35);hud.mode='inspect';shot('factory-partial-power');hud.cancel()
    pump(10);supply(26,36);pump(10)
    hud.jump(27,35);shot('factory-unit-produced')
    report['production_scenario']=True
   pump(4)
   req('PERF CLEAR');req('PERF START');recording=True;pump(5)
   probe=pxadb.info_properties(req('PERF STOP')[-1].payload);recording=False;report['probe']=probe
   assert not int(probe.get('surface_changed','0'))
   for kind in ('raster','present'):
    values=[]
    for offset in range(0,int(probe[kind]),16):
     props=pxadb.info_properties(req(f'PERF READ {kind} {offset}')[-1].payload)
     values.extend(int(v) for v in props['values'].split(',') if v)
    report[kind+'_us']=values
    if values:report[kind]={'count':len(values),'mean_us':statistics.mean(values),'median_us':statistics.median(values),'p95_us':sorted(values)[int((len(values)-1)*.95)]}
   assert report['present_us'],'No displayed game frames'
   report['display_fps']=1e6/report['present']['mean_us'];shot('measured-world')
   report['passed']=True
  finally:
   for cmd,needed in [('PERF STOP',recording),('PERF CLEAR',True),('PACKAGE stop pxa-mindustry',running)]:
    if needed:
     try:req(cmd);pump(.2)
     except pxadb.PxaDbError as error:report.setdefault('cleanup_errors',[]).append(str(error))
   try:
    if c.log_subscribed:c.unsubscribe_logs(timeout=3)
    if a.production and report['passed']:
     from device_storage import capture
     latest=capture(c,candidates[0]['relative'],a.output/'test-save')
     assert latest,'No checksum-validated on-device game save'
     assert latest['units']>=1,latest
     assert any(b['type']==10 and b['x']==26 and b['y']==36 for b in latest['details']),latest
     smelter=next(b for b in latest['details'] if b['type']==9)
     assert smelter['items'].get(5,0)>=5 and smelter['items'].get(3,0)==0,smelter
     report['production_save']=latest
     if a.audio:assert latest['music_mode']==1,latest
   except Exception as error:
    report['passed']=False;report['production_error']=str(error)
    raise
   finally:
    if a.restore_from:
     from device_storage import restore
     restore(c,candidates[0]['relative'],a.restore_from)
     report['original_save_restored']=True
     c.subscribe_logs();pump(.3);messages.clear()
     req('INPUT KEY HOME');pump(.2);req('INPUT SWIPE 240 420 240 120 400 8');pump(.8)
     req('PACKAGE run pxa-mindustry');wait(lambda:any('glyph cache ready' in x for x in messages))
     tap(width//2,max(4,(height-222)//2)+72);pump(.5);shot('restored-world')
    if a.audio:
     report['audio_events']=audio_events
     report['audio_decode_errors']=[x for x in audio_events if re.search(r'music event id=\d+ state=5',x)]
     if report['audio_decode_errors']:report['passed']=False
    (a.output/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({k:report[k] for k in ('passed','title_font_ready_ms','display_fps','raster','present')},indent=2))
if __name__=='__main__':main()
