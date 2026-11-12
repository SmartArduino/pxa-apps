#!/usr/bin/env python3
"""Measure a reversible pan workload on the current 480x480 Mosaico map."""
from pathlib import Path
import argparse,datetime,importlib.util,json,re,sys,time
p=argparse.ArgumentParser(__doc__)
p.add_argument('--pxadb',required=True,type=Path,help='path to the current pxadb.py')
p.add_argument('--port',required=True)
p.add_argument('--baud',type=int,default=2000000)
p.add_argument('--output',type=Path)
a=p.parse_args()
out=a.output or Path(__file__).resolve().parents[3]/'artifacts/unciv'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
out.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(a.pxadb.resolve().parent));spec=importlib.util.spec_from_file_location('unciv_pxadb',a.pxadb);pxadb=importlib.util.module_from_spec(spec);sys.modules[spec.name]=pxadb;spec.loader.exec_module(pxadb)
logs=[]
with pxadb.open_client(a.port,20,baudrate=a.baud) as client:
 client.log_callback=lambda f:logs.append(f'[{f.kind}] {f.payload}')
 info='\n'.join(f.payload for f in client.request('INFO') if f.kind=='OK');fields=pxadb.info_properties(info)
 if fields.get('board')!='esp-mosaico' or (fields.get('width'),fields.get('height'))!=('480','480'):raise SystemExit('This workload requires the 480x480 ESP-Mosaico. No input sent.')
 def require_foreground():
  if not any(f.kind=='PKG' and f.payload.split('\t')[0].endswith(':pxa-unciv') and 'active=1' in f.payload for f in client.request('PACKAGES')):
   raise SystemExit('Unciv is not in the foreground. Open its map and retry; no swipe sent.')
 require_foreground()
 client.subscribe_logs();client.request('INPUT SYNC')
 shot=pxadb.screenshot_capture(client.request('SCREENSHOT',timeout=60));pxadb.write_screenshot(shot,out/'before.png');logs.clear()
 try:
  for i in range(4):
   require_foreground()
   x1,x2=(310,160) if i%2==0 else (160,310)
   client.request(f'INPUT SWIPE {x1} 220 {x2} 220 1500 30',timeout=10)
   time.sleep(.5)
  client.request('INPUT SYNC')
  require_foreground()
  shot=pxadb.screenshot_capture(client.request('SCREENSHOT',timeout=60));pxadb.write_screenshot(shot,out/'after.png')
 finally:client.request('INPUT CANCEL')
text='\n'.join(logs);(out/'device.log').write_text(text)
rows=[dict(frames=int(f),wall_us=int(w),render_us=int(r)) for f,w,r in re.findall(r'Unciv: pan frames=(\d+) wall_us=(\d+) render_us=(\d+)',text)][-4:]
if len(rows)!=4:raise SystemExit('Expected four completed map drags. Open the map, wait for idle, and retry with a new output directory.')
frames=sum(r['frames'] for r in rows);wall=sum(r['wall_us'] for r in rows)
result=dict(info=info,strokes=rows,submission_fps=frames*1e6/wall,mean_draw_ms=sum(r['render_us'] for r in rows)/max(1,frames)/1000,note='Guest Surface submissions, not an independent LCD scan-rate measurement; inspect the raw before/after images.')
(out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
