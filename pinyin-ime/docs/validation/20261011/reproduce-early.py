import sys,json,pathlib,time
sys.path.insert(0,'tools/pxadb');import pxadb
out=pathlib.Path('/tmp/pxa-ime-integration/device');rows=[];logs=[]
with pxadb.PxaDbClient('/dev/ttyACM1',timeout=15) as c:
 c.device_info=c.hello();c.log_callback=lambda f: logs.append(f.payload);c.subscribe_logs()
 def req(cmd):
  fs=c.request(cmd,timeout=30);rows.append({'command':cmd,'frames':[{'kind':f.kind,'payload':f.payload} for f in fs]});print(rows[-1],flush=True);return fs
 def pump(t):
  until=time.monotonic()+t
  while time.monotonic()<until:c.pump_logs();time.sleep(.01)
 def tap(x,y):
  req(f'INPUT POINTER DOWN {x} {y} 0');pump(.12);req(f'INPUT POINTER UP {x} {y} 0');req('INPUT SYNC');pump(.08)
 req('PACKAGE run pxa-cpp-system-input');pump(1)
 req('MEMORY');req('MEMORY START');tap(39,100);pump(.8);tap(51,203);pump(.1);tap(36,139)
 pump(4);req('IME');req('MEMORY STOP')
 c.unsubscribe_logs(timeout=3);pxadb.write_screenshot(pxadb.screenshot_capture(c.request('SCREENSHOT')),out/'early-input.png');c.subscribe_logs()
 tap(247,203);pump(.4);req('MEMORY');req('PACKAGE stop pxa-cpp-system-input');pump(.4)
 req('INPUT KEY HOME');pump(.3)
 c.unsubscribe_logs(timeout=3);pxadb.write_screenshot(pxadb.screenshot_capture(c.request('SCREENSHOT')),out/'home.png')
 (out/'early.json').write_text(json.dumps({'rows':rows,'logs':logs},ensure_ascii=False,indent=2))
 print('\n'.join(x for x in logs if any(w in x for w in ['PxaIme','timeout','exceeded','task_wdt'])),flush=True)
