"""Production launches, no screenshot or PERF allocation in measured windows."""
import argparse,json,sys,time
from pathlib import Path
root=next((p for p in Path(__file__).resolve().parents if (p/'tools/pxadb/pxadb.py').is_file()),None)
if root is None:raise SystemExit('Run this harness from a pxa-projects workspace checkout.')
sys.path.insert(0,str(root/'tools/pxadb'));import pxadb
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--cold-first',action='store_true',help='Label the first launch cold only after an explicit device reboot');a=p.parse_args();out=a.output;out.mkdir(parents=True,exist_ok=True)
port=a.port;result={'cold_first':a.cold_first,'passed':False,'launches':[]}
with pxadb.PxaDbClient(port,30)as c,(out/'serial.log').open('w',buffering=1)as log:
 c.device_info=c.hello();c.subscribe_logs();c.raw_callback=lambda b:log.write(b.decode('utf-8','replace'));c.log_callback=lambda f:log.write(f.payload+'\n')
 def req(cmd):
  quiet=cmd.startswith(('MEMORY','PERF'))
  if quiet:c.unsubscribe_logs()
  frames=c.request(cmd,timeout=60)
  if quiet:c.subscribe_logs()
  log.write(cmd+' '+str([(f.kind,f.payload)for f in frames])+'\n');return frames
 def pump(t):
  end=time.monotonic()+t
  while time.monotonic()<end:c.pump_logs();time.sleep(.02)
 def mem(frames=None):
  d={}
  for f in frames if frames is not None else req('MEMORY'):
   if f.kind=='DATA':
    v=pxadb.info_properties(f.payload);scope=v.pop('scope');name=v.pop('name','');d[scope+(':'+name if name else '')]={k:int(x)for k,x in v.items()}
  return d
 def tap(x,y):
  req(f'INPUT POINTER DOWN {x} {y} 0');req('INPUT SYNC');pump(.18);req(f'INPUT POINTER UP {x} {y} 0');req('INPUT SYNC');pump(.5)
 req('INPUT KEY HOME');req('INPUT CANCEL');req('PERF CLEAR');pump(2)
 req('INPUT POINTER DOWN 148 216 0');req('INPUT SYNC');pump(.08)
 for y in [185,150,115,80,45]:req(f'INPUT POINTER MOVE 148 {y} 0');req('INPUT SYNC');pump(.06)
 req('INPUT POINTER UP 148 45 0');req('INPUT SYNC');pump(.8);req('INPUT KEY HOME')
 for app in ['pxa-voxel-craft-cpp','pxa-voxel-craft-cpp-ui2']:
  try:req('PACKAGE stop '+app);pump(2)
  except pxadb.PxaDbError as e:
   if str(e)!='package_not_running':raise
 for i in range(3):
  row={'kind':'cold'if i==0 and a.cold_first else'warm','before':mem()};result['launches'].append(row);req('MEMORY START');running=False;monitoring=True
  try:
   req('PACKAGE run pxa-voxel-craft-cpp');running=True;pump(12);row['title']=mem();tap(148,95);pump(10);row['game']=mem();row['local_peak']=mem(req('MEMORY STOP'));monitoring=False
  finally:
   if monitoring:req('MEMORY STOP')
   if running:req('PACKAGE stop pxa-voxel-craft-cpp');pump(4)
   req('INPUT CANCEL');row['after_stop']=mem()
   assert all(row['after_stop']['surface'][k]==0 for k in ['frame','scratch','mailbox','probe'])
   assert row['after_stop']['budget']['allocation_failures']==0
   (out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
  print('Production launch',i+1,'measured',flush=True)
 result['passed']=True;(out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
