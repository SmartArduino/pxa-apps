import sys,time,pathlib,json
sys.path.insert(0,'tools/pxadb');import pxadb
out=pathlib.Path('/tmp/pxa-ime-integration/device');rows=[];logs=[]
with pxadb.PxaDbClient('/dev/ttyACM1',timeout=15) as c:
 c.device_info=c.hello();c.log_callback=lambda f:logs.append(f.payload);c.subscribe_logs()
 def req(cmd):
  f=c.request(cmd,timeout=30);rows.append({'command':cmd,'frames':[{'kind':x.kind,'payload':x.payload} for x in f]});print(rows[-1],flush=True)
 def pump(t):
  end=time.monotonic()+t
  while time.monotonic()<end:c.pump_logs();time.sleep(.01)
 req('MEMORY');req('MEMORY START');req('INPUT POINTER DOWN 260 55 0');pump(.12);req('INPUT POINTER UP 260 55 0');req('INPUT SYNC');pump(5)
 req('IME');req('MEMORY STOP')
 c.unsubscribe_logs(timeout=3);pxadb.write_screenshot(pxadb.screenshot_capture(c.request('SCREENSHOT')),out/'native-editor.png');c.subscribe_logs()
 (out/'native-editor.json').write_text(json.dumps({'rows':rows,'logs':logs},ensure_ascii=False,indent=2))
 print('\n'.join(x for x in logs if any(w in x for w in ['PxaIme','timeout','exceeded','Failed'])),flush=True)
