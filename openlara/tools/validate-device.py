#!/usr/bin/env python3
"""Hold one identified device throughout same-firmware A/B, controls and reboot tests."""
import argparse,hashlib,json,os,re,statistics,sys,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',required=True);p.add_argument('--serial',required=True)
p.add_argument('--baud',default='2000000');p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--candidate',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--allow-audio',action='store_true')
p.add_argument('--resume-functional',action='store_true',help='resume post-benchmark checks from this output; never reinstall or resample')
p.add_argument('--baseline-evidence',type=Path,help='reuse a passed baseline from the same firmware; record its provenance')
p.add_argument('--candidate-samples',type=int,default=60)
p.add_argument('--classic',action='store_true',help='complete engine: benchmark PC demo City, use the 0.3.x menu')
p.add_argument('--baseline-only',action='store_true',help='record the baseline and save; leave candidate for a later build')
p.add_argument('--pxadb-dir',type=Path,default=Path(__file__).resolve().parents[4]/'tools/pxadb')
a=p.parse_args();sys.path.insert(0,str(a.pxadb_dir));import pxadb
os.environ['PXADB_BAUD']=a.baud;os.environ['PXADB_UART_UPLOAD_CHUNK']='1024'
a.output.mkdir(parents=True,exist_ok=True)
report={'passed':False,'runs':{}};messages=[];permission_pending=False;phase='acquire';unresponsive=False;memory_started=False
if a.resume_functional:
 report=json.loads((a.output/'metrics.json').read_text())
 for name,source in [('baseline',a.baseline),('candidate',a.candidate)]:
  assert report['runs'][name]['package_sha256']==hashlib.sha256(source.with_suffix('.pxa').read_bytes()).hexdigest()
 report['passed']=False
log=(a.output/'serial.jsonl').open('a' if a.resume_functional else 'w',buffering=1)
def record(kind,payload):
 global permission_pending,unresponsive
 log.write(json.dumps({'t':time.monotonic(),'phase':phase,'kind':kind,'payload':payload})+'\n')
 if 'OpenLara' in payload:print(payload,flush=True)
 if 'Runtime permission prompt queued:' in payload and 'pxa-openlara' in payload:permission_pending=True
 if 'awaiting user decision' in payload:unresponsive=True
 if kind=='LOG':messages.append(payload)
def request(cmd,timeout=30):
 global memory_started
 record('COMMAND',cmd);rows=c.request(cmd,timeout=timeout)
 for f in rows:
  if f.kind!='DATA' or not cmd.startswith('SCREENSHOT'):record(f.kind,f.payload)
 if cmd=='MEMORY START':memory_started=True
 if cmd=='MEMORY STOP':memory_started=False
 return rows
def require_target():
 rows=request('PACKAGES')
 active=[f.payload for f in rows if f.kind=='PKG' and 'active=1' in f.payload]
 if unresponsive:raise RuntimeError('OpenLara Guest timeout; FPS samples invalid, dismiss its dialog on the device')
 if len(active)!=1 or ':pxa-openlara\t' not in active[0]:raise RuntimeError('OpenLara is no longer the sole active app; FPS samples invalid')
def pump(seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:c.pump_logs();time.sleep(.01)
def tap(x,y):
 require_target()
 request(f'INPUT POINTER DOWN {x} {y} 0');request('INPUT SYNC');pump(.12)
 request(f'INPUT POINTER UP {x} {y} 0');request('INPUT SYNC');pump(.3)
def capture(name):
 shot=pxadb.screenshot_capture(request('SCREENSHOT JPEG'));pxadb.write_screenshot(shot,a.output/(name+'.jpg'))
def wait_message(text,after=0,timeout=100):
 global permission_pending
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  pump(.1)
  if unresponsive:raise RuntimeError('OpenLara Guest timeout; validation aborted')
  if any(text in s for s in messages[after:]):return
  if permission_pending:
   if not a.allow_audio:raise RuntimeError('Audio permission needs approval on the device')
   capture(phase+'-permission');tap(211,183);permission_pending=False
  request('PING')
 raise RuntimeError('Timed out waiting for '+text)
def menu(selection):
 tap(180,220);tap(160,44+selection*18 if a.classic else 48+selection*20)
def hold(x,y,seconds):
 require_target()
 request(f'INPUT POINTER DOWN {x} {y} 0');request('INPUT SYNC');pump(seconds)
 request(f'INPUT POINTER UP {x} {y} 0');request('INPUT SYNC');pump(1.5)
def position():
 for s in reversed(messages):
  m=re.search(r'pos=(-?\d+),(-?\d+),(-?\d+) yaw=(-?\d+)',s)
  if m:return tuple(map(int,m.groups()))
 raise RuntimeError('No engine position sample')
def same_pose(left,right):
 return tuple(left[:3])==tuple(right[:3]) and abs((left[3]-right[3]+32768)%65536-32768)<=(1 if a.classic else 0)
def stop():
 try:request('PACKAGE stop pxa-openlara');pump(.5)
 except pxadb.PxaDbRejectedError as exc:
  if str(exc)!='package_not_running':raise
  record('NOTE','Firmware did not resolve short stop identity; reboot will end this instance')
def reboot():
 global permission_pending
 request('REBOOT');time.sleep(3)
 c._hello_completed=False;c.log_subscribed=False;c.serial.buffer.clear();c.serial.reset_input_buffer();c.pending_frames.clear()
 # UART can become ready after the normal HELLO budget on a warm reboot.
 deadline=time.monotonic()+45
 while True:
  try:c.hello();break
  except pxadb.PxaDbError:
   if time.monotonic()>=deadline:raise
   time.sleep(1)
 assert pxadb.info_properties(c.device_info)['serial']==a.serial
 c.subscribe_logs()
 permission_pending=False
 for _ in range(30):
  try:request('PACKAGES');break
  except pxadb.PxaDbRejectedError:pump(.5)
 pump(2)
def install(source):
 stop()
 rows=request('PACKAGES')
 if any(':pxa-openlara\t' in f.payload for f in rows if f.kind=='PKG'):request('PACKAGE archive pxa-openlara')
 try:
  pxadb.stage_package_files(pxadb.NormalFsClient(c),source,'pxa-openlara',60,publish=False)
  request('PACKAGE deploy pxa-openlara',60)
 finally:pxadb.clear_staged_source(c,'pxa-openlara')
 reboot()
def launch():
 global permission_pending
 request('INPUT SWIPE 160 205 160 30 300 12');pump(.5)
 request('INPUT KEY HOME');pump(.3);permission_pending=False;start=len(messages)
 request('PACKAGE run pxa-openlara');wait_message('OpenLara ready level=0',start);pump(1)
def level(selection,level_id):
 start=len(messages);menu(selection);capture(phase+'-loading-level-'+str(level_id));wait_message(f'OpenLara ready level={level_id}',start);pump(1)
def benchmark(name,source,samples):
 global phase
 phase=name;install(source)
 request('MEMORY START');launch();capture(name+'-title')
 if name=='candidate':
  start=len(messages);menu(2);wait_message('OpenLara save restored',start);pump(5.2)
  report['upgrade_restored_position']=position()
  assert same_pose(position(),report['runs']['baseline']['saved_position'])
  capture('candidate-upgrade-restored')
 level(4 if a.classic else 3,3 if a.classic else 2);pump(3)
 memory_start=[f.payload for f in request('MEMORY STOP') if f.kind=='DATA'];capture(name+'-idle')
 request('MEMORY START');pump(1);fps=[]
 for i in range(samples):
  if i%5==0:tap(70,170)
  pump(1);require_target()
  properties={}
  for f in request('PERF FPS'):
   if f.kind in ('DATA','OK'):properties.update(pxadb.info_properties(f.payload))
  require_target()
  fps.append(int(properties['presentation_fps_x10'])/10)
 memory_steady=[f.payload for f in request('MEMORY STOP') if f.kind=='DATA']
 result={'package_sha256':hashlib.sha256(source.with_suffix('.pxa').read_bytes()).hexdigest(),
  'samples':fps,'fps':{'min':min(fps),'median':statistics.median(fps),'max':max(fps),'mean':statistics.mean(fps)},
  'startup_memory':memory_start,'steady_memory':memory_steady}
 assert min(fps)>0;print(name,result['fps'],flush=True)
 hold(120,170,1.2);capture(name+'-turned');hold(70,115,1.2);capture(name+'-walked');pump(5.2)
 before=position();start=len(messages);menu(1);wait_message('OpenLara save committed',start);capture(name+'-saved')
 result['saved_position']=before
 hold(120,170,.8);hold(70,115,.8);pump(5.2);assert position()!=before
 start=len(messages);menu(2);wait_message('OpenLara save restored',start);pump(5.2)
 result['restored_position']=position();assert same_pose(position(),before);capture(name+'-restored')
 # The original inventory is separate from the PXA pause menu.
 tap(200,20);pump(1);capture(name+'-inventory');tap(200,20);pump(1);capture(name+'-inventory-return')
 report['runs'][name]=result
 (a.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
 return result
c=None
try:
 end=time.monotonic()+900
 while time.monotonic()<end:
  try:
   candidate=pxadb.PxaDbClient(a.port,15);hello=candidate.hello()
   if pxadb.info_properties(hello)['serial']!=a.serial:raise RuntimeError('Wrong physical device')
   rows=candidate.request('PACKAGES')
   if any('active=1' in f.payload and ':pxa-openlara\t' not in f.payload for f in rows if f.kind=='PKG'):
    candidate.close();time.sleep(2);continue
   c=candidate;break
  except Exception as exc:
   if 'temporarily unavailable' not in str(exc):raise
   time.sleep(2)
 if c is None:raise RuntimeError('Device remains in use by another task')
 c.log_callback=lambda f:record(f.kind,f.payload);c.raw_callback=lambda b:record('RAW',b.decode('utf-8','replace'))
 c.subscribe_logs();report['resume_hello' if a.resume_functional else 'hello']=c.device_info
 report['packages_before_resume' if a.resume_functional else 'packages_before']=[f.payload for f in request('PACKAGES') if f.kind=='PKG']
 firmware=Path('local/storage-device/firmware-current.json')
 if firmware.exists() and not a.resume_functional:report['firmware_record']=json.loads(firmware.read_text())
 if a.resume_functional:
  assert report['firmware_record']==json.loads(firmware.read_text())
  base=report['runs']['baseline'];final=report['runs']['candidate']
 elif a.baseline_evidence:
  inherited=json.loads(a.baseline_evidence.read_text())
  assert inherited['passed'] and inherited['firmware_record']==report['firmware_record']
  base=inherited['runs']['baseline']
  assert base['package_sha256']==hashlib.sha256(a.baseline.with_suffix('.pxa').read_bytes()).hexdigest()
  report['runs']['baseline']=base
  report['baseline_evidence']={'path':str(a.baseline_evidence),'sha256':hashlib.sha256(a.baseline_evidence.read_bytes()).hexdigest()}
  capture('before');final=benchmark('candidate',a.candidate,a.candidate_samples)
 else:
  capture('before');base=benchmark('baseline',a.baseline,20)
  if a.baseline_only:
   report['passed']=True
   raise SystemExit(0)
  final=benchmark('candidate',a.candidate,a.candidate_samples)
 phase='final-functional'
 # Retain an OFF setting over a real reboot, then restore the initial ON value.
 if not a.resume_functional:
  menu(7);start=len(messages);tap(160,77 if a.classic else 75);wait_message('OpenLara settings committed',start);capture('candidate-music-off')
 reboot();launch();menu(7);capture('candidate-settings-after-reboot')
 start=len(messages);tap(160,77 if a.classic else 75);wait_message('OpenLara settings committed',start);tap(160,177 if a.classic else 175)
 start=len(messages);tap(160,80 if a.classic else 88);wait_message('OpenLara save restored',start);pump(5.2)
 assert same_pose(position(),final['saved_position']);report['reboot_restored_position']=position();capture('candidate-reboot-restored')
 level(4,3);capture('candidate-city')
 if not a.classic:
  level(5,1);capture('candidate-gym');level(3,2)
 # Verify the lifecycle pause/resume route; leave the app ready on screen.
 request('INPUT KEY HOME');pump(2);request('PACKAGE run pxa-openlara');pump(3)
 capture('candidate-final');report['packages_after']=[f.payload for f in request('PACKAGES') if f.kind=='PKG']
 report['gain_percent']=(final['fps']['median']/base['fps']['median']-1)*100
 report['passed']=True
finally:
 if c:
  try:
   active=[f.payload for f in request('PACKAGES') if f.kind=='PKG' and 'active=1' in f.payload]
   if len(active)==1 and ':pxa-openlara\t' in active[0]:request('INPUT CANCEL')
   if memory_started:request('MEMORY STOP')
  except Exception:pass
  c.close()
 (a.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n');log.close()
if not report['passed']:raise SystemExit(1)
