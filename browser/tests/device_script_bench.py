#!/usr/bin/env python3
"""Measure the /bench-menu fixture on an unlocked pai-touch (296x240).

Build with PXA_BROWSER_TRACE=1, PXA_BROWSER_PROFILE=1 and
PXA_BROWSER_START_URL=http://YOUR_PC:31925/bench-menu. Start script_fixture.py
on that address before running. This tool does not install packages or change
settings. Screenshots and sample downloads are outside measured heap windows.
"""
import argparse,sys,time,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--case',type=int,choices=range(7),default=0,help='0 static, 1 dynamic scroll, 2 DOM/network, 3 timeout, 4 OOM, 5 recursion, 6 parser nesting')
p.add_argument('--pxadb-dir',type=Path,default=Path('tools/pxadb'))
p.add_argument('--unlock',action='store_true',help='OCR-detect and unlock pai-touch; requires tesseract chi_sim+eng')
a=p.parse_args();sys.path.insert(0,str(a.pxadb_dir.resolve()));import pxadb
from geometry import geometry
out=a.output;out.mkdir(parents=True,exist_ok=True);case=a.case;logs=[];result={}
with pxadb.PxaDbClient(a.port,30) as c:
 c.log_callback=lambda f:logs.append(f.payload);c.raw_callback=lambda b:logs.append(b.decode(errors='replace'))
 result['hello']=c.hello();c.subscribe_logs()
 def cmd(t):return c.request(t,timeout=60)
 def pump(s):
  end=time.monotonic()+s
  while time.monotonic()<end:c.pump_logs();time.sleep(.005)
 def props(t):
  d={}
  for f in cmd(t):
   if f.kind in ('DATA','OK'):d.update(pxadb.info_properties(f.payload))
  return d
 def mem(t='MEMORY'):
  d={}
  for f in cmd(t):
   if f.kind=='DATA':
    v=pxadb.info_properties(f.payload);scope=v.pop('scope');name=v.pop('name','');d[scope+(':'+name if name else '')]={k:int(x) for k,x in v.items()}
  return d
 def tap(x,y):cmd(f'INPUT TAP {x} {y}');pump(.4)
 def shot(name):
  p=out/(name+'.png');pxadb.write_screenshot(pxadb.screenshot_capture(cmd('SCREENSHOT')),p);return p
 def wait_for(match,mark,timeout=25):
  end=time.monotonic()+timeout
  while not any(match in s for s in logs[mark:]):
   if time.monotonic()>end:raise RuntimeError('timeout '+match)
   pump(.05)
 try:
  try:cmd('PACKAGE stop pxa-browser');pump(.8)
  except pxadb.PxaDbRejectedError:pass
  cmd('INPUT CANCEL');before=shot('before');text=subprocess.check_output(['tesseract',str(before),'stdout','-l','chi_sim+eng','--psm','6'],stderr=subprocess.DEVNULL).decode(errors='replace')
  if '解锁' in text.replace(' ','') or re.search(r'20\d{2}[-/.]\d{2}[-/.]\d{2}',text):
   assert a.unlock, 'Unlock the device first, or pass --unlock'
   cmd('INPUT TAP 148 120');pump(.3);cmd('INPUT TOUCH DOWN 148 180 0');pump(.12)
   for y in range(170,44,-10):cmd(f'INPUT TOUCH MOVE 148 {y} 0');pump(.04)
   cmd('INPUT TOUCH UP 148 45 0');pump(.6)
  cmd('MEMORY START');result['before']=mem();mark=len(logs);cmd('PACKAGE run pxa-browser');wait_for('BROWSER DONE status=200',mark);pump(.4)
  layout=geometry('\n'.join(logs));result['layout']=layout;x,y,w,h=layout['content'];mark=len(logs);menu_scroll=0
  if case==6:
   cmd(f'INPUT SWIPE {x+w//2} {y+h-12} {x+w//2} {y+12} 400 24');pump(.5)
   offsets=re.findall(r'BROWSER PERF[^\n]* scroll=(\d+)','\n'.join(logs[mark:]));assert offsets, 'No menu scroll profile'
   menu_scroll=int(offsets[-1])
  tap(x+20,y+case*22+10-menu_scroll)
  wait_for('BROWSER DONE status=200',mark)
  if case in (1,2,5,6):wait_for('BROWSER JS READY',mark,35)
  if case==1:
   wait_for('BROWSER JS UPDATE revision=5',mark,35);pump(2)
  if case==2:
   wait_for('BROWSER JS UPDATE revision=2',mark,20)
   tap(x+w//8,y+42);wait_for('BROWSER JS UPDATE revision=3',mark,20)
  if case in (5,6):
   assert any(re.search(r'BROWSER JS READY[^\n]* errors=1',s) for s in logs[mark:])
   wait_for('BROWSER JS UPDATE revision=1',mark)
  if case in (3,4):
   wait_for('BROWSER JS ERROR',mark,20)
   assert any(('执行超时' if case==3 else '内存') in s for s in logs[mark:])
   pump(.2)
  pump(.6);result['loaded']=mem();result['startup_peak']=mem('MEMORY STOP');shot('loaded');pump(.3)
  cmd('MEMORY START');result['steady_before']=mem();cmd('PERF START display');drag_mark=len(logs);cmd(f'INPUT SWIPE {x+w//2} {y+h-12} {x+w//2} {y+12} 1600 96');pump(.1)
  perf={k:int(v) for k,v in props('PERF STOP').items()};result['steady_peak']=mem('MEMORY STOP');samples=[]
  for offset in range(0,perf['present'],16):samples += [int(s) for s in props(f'PERF READ present {offset}')['values'].split(',') if s]
  cmd('PERF CLEAR');shot('scrolled');result.update(perf=perf,present_us=samples,active_fps=len(samples)*1e6/sum(samples) if samples else None,case=case)
  assert perf['present_overflow']==0
  if case in (0,1):assert samples, 'No completed display intervals during scroll'
  held=False;updates=0
  for line in logs[drag_mark:]:
   if 'BROWSER POINTER phase=0' in line:held=True
   if 'BROWSER POINTER phase=2' in line:held=False
   if held and 'BROWSER JS UPDATE' in line:updates+=1
  result['script_updates_during_drag']=updates
  result['script']=re.findall(r'BROWSER JS[^\n\x1b]*','\n'.join(logs[mark:]))
  assert updates==0, 'Script repainted during touch drag'
  if case in (1,2):assert not any('BROWSER JS ERROR' in line for line in logs[mark:]), 'Normal page script stopped'
  assert not any('PXA Guest exception:' in line for line in logs[mark:]), 'Guest component trapped'
  cmd('PACKAGE stop pxa-browser');pump(.8);result['stopped']=mem();result['passed']=True
  print(json.dumps({'out':str(out),'fps':result['active_fps'],'perf':perf}),flush=True)
 finally:
  (out/'runtime.log').write_text('\n'.join(logs));(out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2))
  try:cmd('MEMORY STOP');cmd('PERF CLEAR');cmd('PACKAGE stop pxa-browser');pump(.5)
  except pxadb.PxaDbError:pass
