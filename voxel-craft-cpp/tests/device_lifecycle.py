import argparse,json,re,sys,time
from pathlib import Path
root=next((p for p in Path(__file__).resolve().parents if (p/'tools/pxadb/pxadb.py').is_file()),None)
if root is None:raise SystemExit('Run this harness from a pxa-projects workspace checkout.')
sys.path.insert(0,str(root/'tools/pxadb'));import pxadb
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--app',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--cycles',type=int,default=1);p.add_argument('--stop-pending',action='store_true');p.add_argument('--deny-audio',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
port=a.port;rows=[];epoch=0;lines=[];dropped=False
with pxadb.PxaDbClient(port,30)as c,(a.output/'serial.jsonl').open('w',buffering=1)as log:
 c.device_info=c.hello();c.subscribe_logs()
 def received(f):
  global dropped
  if f.kind=='DROP':dropped=True
  log.write(json.dumps({'kind':f.kind,'payload':f.payload})+'\n')
  m=re.match(r'^(\d+)\t',f.payload)
  if not m or int(m[1])>=epoch:lines.append(f.payload)
 c.log_callback=received;c.raw_callback=lambda b:log.write(json.dumps({'kind':'raw','payload':b.decode('utf8','replace')})+'\n')
 def req(cmd):
  log.write(json.dumps({'command':cmd})+'\n')
  # Large MEMORY responses share the bounded firmware TX queue with live
  # logs. Keep the measurement itself free of log traffic; then replay the
  # retained ring so a launch burst cannot hide its permission prompt.
  quiet=cmd.startswith('MEMORY')
  if quiet:c.unsubscribe_logs()
  r=c.request(cmd,timeout=60)
  if quiet:c.subscribe_logs()
  log.write(json.dumps({'response':[(f.kind,f.payload)for f in r]})+'\n');return r
 def pump(t):
  global dropped
  until=time.monotonic()+t
  while time.monotonic()<until:
   c.pump_logs()
   if dropped:
    dropped=False;c.unsubscribe_logs();c.subscribe_logs()
   time.sleep(.02)
 def memory(frames):
  values={}
  for f in frames:
   if f.kind=='DATA':
    v=pxadb.info_properties(f.payload);scope=v.pop('scope');name=v.pop('name','');values[scope+(':'+name if name else '')]={k:int(x)for k,x in v.items()}
  return values
 req('INPUT KEY HOME');req('INPUT CANCEL');pump(.5)
 req('INPUT POINTER DOWN 148 216 0');req('INPUT SYNC');pump(.08)
 for y in [185,150,115,80,45]:req(f'INPUT POINTER MOVE 148 {y} 0');req('INPUT SYNC');pump(.06)
 req('INPUT POINTER UP 148 45 0');req('INPUT SYNC');pump(.8);req('INPUT KEY HOME')
 for i in range(a.cycles):
  lines.clear();epoch=int(pxadb.info_properties(req('INPUT SYNC')[-1].payload)['device_us'])//1000
  row={'cycle':i};rows.append(row);req('MEMORY START');running=False;monitor=True
  try:
   req('PACKAGE run '+a.app);running=True;end=time.monotonic()+20
   while time.monotonic()<end and not any('Voxel audio ready'in x or ('Runtime permission prompt queued:'in x and ':'+a.app+' permission=音频播放'in x)for x in lines):pump(.1)
   row['prompt']=any('Runtime permission prompt queued:'in x and ':'+a.app+' permission=音频播放'in x for x in lines)
   if a.stop_pending:assert row['prompt'],'Permission prompt did not appear'
   elif row['prompt']:
    pump(1) # Wait for the queued modal to replace the launch animation.
    x=93 if a.deny_audio else 199;req(f'INPUT POINTER DOWN {x} 183 0');req('INPUT SYNC');pump(.15);req(f'INPUT POINTER UP {x} 183 0');req('INPUT SYNC');pump(1)
   if not a.stop_pending:
    expected='Voxel audio permission unavailable'if a.deny_audio else'Voxel audio ready'
    assert any(expected in x for x in lines),'Audio grant/denial did not complete'
    end=time.monotonic()+20
    while time.monotonic()<end and not any('Voxel Craft C++ ready'in x for x in lines):pump(.1)
    assert any('Voxel Craft C++ ready'in x for x in lines),'Game did not initialize after audio decision'
   pump(1);row['steady']=memory(req('MEMORY'));row['local_peak']=memory(req('MEMORY STOP'));monitor=False
  finally:
   if monitor:req('MEMORY STOP')
   if running:req('PACKAGE stop '+a.app);pump(3)
   row['after_stop']=memory(req('MEMORY'));assert all(row['after_stop']['surface'][k]==0 for k in ['frame','scratch','mailbox','probe']);assert row['after_stop']['budget']['allocation_failures']==0
  assert not any('Guru Meditation'in x or 'Stack canary'in x for x in lines)
  (a.output/'report.json').write_text(json.dumps({'passed':True,'stop_pending':a.stop_pending,'denied_audio':a.deny_audio,'rows':rows},indent=2)+'\n')
  print('Lifecycle cycle',i+1,'passed',flush=True)
