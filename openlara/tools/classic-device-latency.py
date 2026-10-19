#!/usr/bin/env python3
"""Measure a stationary native City scene via the GameKit controller path."""
import argparse,hashlib,json,os,re,statistics,sys,time
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',required=True);p.add_argument('--serial',required=True)
p.add_argument('--version',required=True);p.add_argument('--package',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--samples',type=int,default=30)
p.add_argument('--install',action='store_true',help='replace only OpenLara with this signed package before testing')
p.add_argument('--reboot-check',action='store_true',help='also restore the checkpoint after a physical device reboot')
p.add_argument('--baud',default='2000000');p.add_argument('--wait-seconds',type=int,default=900)
p.add_argument('--pxadb-dir',type=Path,default=Path(__file__).resolve().parents[4]/'tools/pxadb')
a=p.parse_args();sys.path.insert(0,str(a.pxadb_dir));import pxadb
os.environ['PXADB_BAUD']=a.baud;a.output.mkdir(parents=True,exist_ok=True)
report={'passed':False,'mode':'S31 native AOT; stationary public PC demo City; controller-0 injection','package_sha256':hashlib.sha256(a.package.read_bytes()).hexdigest(),'samples':[]};c=None;messages=[];unresponsive=False;input_owned=False;timing=False;last_stats_at=0
log=(a.output/'serial.jsonl').open('w',buffering=1)
def record(kind,payload):
 global unresponsive,last_stats_at
 log.write(json.dumps({'t':time.monotonic(),'kind':kind,'payload':payload})+'\n')
 if kind=='LOG':messages.append(payload)
 if kind=='LOG' and 'OpenLara frames=' in payload:last_stats_at=time.monotonic()
 if 'awaiting user decision' in payload:unresponsive=True
 if 'OpenLara frames=' in payload or 'OpenLara input ' in payload or 'OpenLara frame clock:' in payload:print(payload,flush=True)
def request(cmd,timeout=30):
 record('COMMAND',cmd);rows=c.request(cmd,timeout=timeout)
 for row in rows:
  if row.kind!='DATA' or not cmd.startswith('SCREENSHOT'):record(row.kind,row.payload)
 return rows
def pump(seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:c.pump_logs();time.sleep(.005)
def packages():return [r.payload for r in request('PACKAGES') if r.kind=='PKG']
def require_target():
 rows=packages();active=[r for r in rows if 'active=1' in r]
 if unresponsive:raise RuntimeError('Guest timeout; samples invalid')
 if len(active)!=1 or ':pxa-openlara\t' not in active[0]:raise RuntimeError('OpenLara lost foreground; samples invalid')
 if timing and time.monotonic()-last_stats_at>8:raise RuntimeError('Game telemetry stopped; foreground/FPS samples invalid')
def state(buttons):
 rows=request(f'INPUT CONTROLLER 0 1 {buttons}')
 if any('status=rejected' in r.payload for r in rows):raise RuntimeError('Physical input took over; controller test cancelled')
def press(buttons):
 require_target();state(buttons);pump(.25);state(0);pump(.25)
def shot(name):
 for attempt in range(10):
  try:
   pxadb.write_screenshot(pxadb.screenshot_capture(request('SCREENSHOT JPEG')),a.output/(name+'.jpg'));return
  except pxadb.PxaDbRejectedError as error:
   if str(error)!='screenshot_rate_limited':raise
   pump(.3)
 raise RuntimeError('screenshot remained rate limited')
def black(name):
 with Image.open(a.output/(name+'.jpg')) as im:
  rgb=list(im.convert('RGB').get_flattened_data());return sum(max(c)<12 for c in rgb)/len(rgb)
def menu(index):
 shot('menu-check')
 if black('menu-check')<.8:
  started=time.monotonic();press(128)
  while time.monotonic()-started<8:
   shot('menu-check')
   if black('menu-check')>.8:break
   pump(.15)
  assert black('menu-check')>.8,'PXA menu did not open within8seconds'
  report.setdefault('menu_response_upper_bounds_ms',[]).append(round((time.monotonic()-started)*1000))
 for _ in range(index):press(2)
 press(16)
def wait(text,start,timeout=120):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  if unresponsive:raise RuntimeError('Guest timeout: '+text)
  if any(text in s for s in messages[start:]):return
  pump(.1);request('PING')
 raise RuntimeError('timeout: '+text)
def position():
 for message in reversed(messages):
  match=re.search(r'pos=(-?\d+),(-?\d+),(-?\d+) yaw=(-?\d+)',message)
  if match:return list(map(int,match.groups()))
 raise RuntimeError('no actual player telemetry')
def same(left,right):return left[:3]==right[:3] and abs((left[3]-right[3]+32768)%65536-32768)<=1
try:
 deadline=time.monotonic()+a.wait_seconds
 while time.monotonic()<deadline:
  try:
   candidate=pxadb.PxaDbClient(a.port,15);hello=candidate.hello()
   assert pxadb.info_properties(hello)['serial']==a.serial,'wrong physical device'
   rows=[r.payload for r in candidate.request('PACKAGES') if r.kind=='PKG']
   if any('active=1' in r and ':pxa-openlara\t' not in r for r in rows):candidate.close();time.sleep(2);continue
   c=candidate;break
  except (OSError,pxadb.PxaDbError) as error:
   if 'temporarily unavailable' not in str(error):raise
   time.sleep(2)
 if c is None:raise RuntimeError('device remains in use')
 report['hello']=hello;report['packages_before']=rows
 if a.install:
  source=a.package.with_suffix('');assert source.is_dir(),'signed package directory missing'
  if any('active=1' in r for r in rows):
   require_target();request('INPUT KEY HOME');pump(.5)
  if any(':pxa-openlara\t' in r for r in rows):
   try:request('PACKAGE stop pxa-openlara')
   except pxadb.PxaDbRejectedError as error:
    if str(error)!='package_not_running':raise
   request('PACKAGE archive pxa-openlara')
  try:
   pxadb.stage_package_files(pxadb.NormalFsClient(c),source,'pxa-openlara',60,publish=False)
   request('PACKAGE deploy pxa-openlara',60)
  finally:pxadb.clear_staged_source(c,'pxa-openlara')
  request('REBOOT');time.sleep(3)
  c._hello_completed=False;c.log_subscribed=False;c.serial.buffer.clear();c.serial.reset_input_buffer();c.pending_frames.clear()
  end=time.monotonic()+45
  while True:
   try:c.hello();break
   except pxadb.PxaDbError:
    if time.monotonic()>=end:raise
    time.sleep(1)
  assert pxadb.info_properties(c.device_info)['serial']==a.serial
  rows=packages()
 assert any(':pxa-openlara\t' in r and '\t'+a.version+'\t' in r for r in rows),'installed version differs'
 c.log_callback=lambda r:record(r.kind,r.payload);c.raw_callback=lambda b:record('RAW',b.decode('utf-8','replace'))
 c.subscribe_logs();pump(.2);unresponsive=False
 firmware=Path('local/storage-device/firmware-current.json')
 if firmware.exists():report['firmware']=json.loads(firmware.read_text())
 if not any('active=1' in r for r in rows):
  start=len(messages);request('PACKAGE run pxa-openlara');wait('OpenLara ready level=0',start);pump(2)
 require_target();request('INPUT CANCEL');state(0);input_owned=True;pump(1)
 start=len(messages);menu(6);wait('OpenLara ready level=0',start);pump(1)
 start=len(messages);menu(4);wait('OpenLara ready level=3',start);pump(5.5);shot('idle')
 stationary=position();assert stationary[:3]==[78336,-1024,12800],('unexpected City start',stationary)
 report['stationary_position']=stationary;request('MEMORY START');timing_start=len(messages);timing=True
 for i in range(a.samples):
  state(0) # Require neutral controller ownership for every stationary sample.
  pump(1);require_target();props={}
  for row in request('PERF FPS'):
   if row.kind in ('DATA','OK'):props.update(pxadb.info_properties(row.payload))
  assert same(position(),stationary),'Lara moved during stationary timing'
  report['samples'].append(int(props['presentation_fps_x10'])/10)
 timing=False;report['memory']=[r.payload for r in request('MEMORY STOP') if r.kind=='DATA']
 fps=report['samples'];assert min(fps)>0;report['fps']={'median':statistics.median(fps),'mean':statistics.mean(fps),'min':min(fps),'max':max(fps)}
 report['stationary_throughout_timing']=True;print(json.dumps(report['fps']),flush=True)
 # Functional controller and checkpoint tests are outside timing. Wait for
 # a settled telemetry sample before saving, rather than a stale moving pose.
 state(8);pump(.8);state(0);pump(5.5);turned=position();assert turned[3]!=stationary[3],'controller right press was lost';shot('turned')
 state(1);pump(.8);state(0);pump(5.5);saved=position();assert saved[:3]!=turned[:3],'controller forward press was lost';shot('walked')
 start=len(messages);menu(1);wait('OpenLara save committed',start);pump(1)
 state(8);pump(.8);state(0);pump(5.5);assert not same(position(),saved)
 start=len(messages);menu(2);wait('OpenLara save restored',start);wait('OpenLara ready level=3',start);pump(5.5)
 restored=position();assert same(saved,restored),(saved,restored);shot('restored')
 press(64);pump(1);shot('inventory');press(64);pump(1);shot('inventory-return')
 report.update(controller_turn=True,controller_walk=True,saved_position=saved,restored_position=restored,save_restore=True)
 pump(5.5) # Flush inventory/menu input from the preceding five-second window.
 response_start=len(messages);record('NOTE','Twenty120ms right pulses;500ms neutral gaps; gameplay response timing')
 for _ in range(20):state(8);pump(.12);state(0);pump(.5)
 pump(5.5);report['pulse_input_reports']=[m for m in messages[response_start:] if 'OpenLara input ' in m]
 assert sum(int(re.search(r'samples=(\d+)',m)[1]) for m in report['pulse_input_reports'])>=20,'brief controller pulses were lost or response reports missing'
 report['twenty_brief_controller_pulses']=True
 if a.reboot_check:
  request('INPUT CANCEL');request('REBOOT');time.sleep(3)
  c._hello_completed=False;c.log_subscribed=False;c.serial.buffer.clear();c.serial.reset_input_buffer();c.pending_frames.clear()
  end=time.monotonic()+45
  while True:
   try:c.hello();break
   except pxadb.PxaDbError:
    if time.monotonic()>=end:raise
    time.sleep(1)
  assert pxadb.info_properties(c.device_info)['serial']==a.serial
  c.subscribe_logs();pump(2);start=len(messages);request('PACKAGE run pxa-openlara');wait('OpenLara ready level=0',start);pump(2)
  state(0);start=len(messages);menu(2);wait('OpenLara save restored',start);wait('OpenLara ready level=3',start);pump(5.5)
  assert same(position(),saved),('checkpoint differs after hardware reboot',saved,position())
  report['physical_reboot_restore']=True;shot('reboot-restored')
 report['input_reports']=[m for m in messages if 'OpenLara input ' in m]
 report['frame_clock_reports']=[m for m in messages if 'OpenLara frame clock:' in m]
 report['packages_after']=packages();require_target();report['passed']=True
except BaseException as error:
 report['error']=str(error);raise
finally:
 if c:
  if input_owned:
   try:request('INPUT CANCEL');request('MEMORY STOP')
   except Exception:pass
  c.close()
 (a.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n');log.close()
