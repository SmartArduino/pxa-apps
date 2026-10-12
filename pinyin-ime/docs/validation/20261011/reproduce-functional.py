import sys,json,pathlib,time
sys.path.insert(0,'tools/pxadb');import pxadb
out=pathlib.Path('/tmp/pxa-ime-integration/device');rows=[];logs=[]
with pxadb.PxaDbClient('/dev/ttyACM1',timeout=15) as c:
 c.device_info=c.hello();c.log_callback=lambda f:logs.append(f.payload);c.subscribe_logs()
 def req(cmd):
  fs=c.request(cmd,timeout=30);rows.append({'command':cmd,'frames':[{'kind':f.kind,'payload':f.payload} for f in fs]});print(rows[-1],flush=True);return fs
 def pump(t):
  until=time.monotonic()+t
  while time.monotonic()<until:c.pump_logs();time.sleep(.01)
 def tap(x,y):req(f'INPUT TAP {x} {y}');req('INPUT SYNC');pump(.15)
 def shot(name):
  c.unsubscribe_logs(timeout=3);pxadb.write_screenshot(pxadb.screenshot_capture(c.request('SCREENSHOT')),out/(name+'.png'));c.subscribe_logs()
 # Cancel the incorrectly aimed physical-coordinate trial, then type ni hao.
 req('MEMORY START')
 req('INPUT POINTER DOWN 255 159 0');pump(.8);req('INPUT POINTER UP 255 159 0');pump(.2)
 for x,y in [(204,159),(210,119),(184,139),(38,139),(236,119)]:tap(x,y)
 req('IME');req('MEMORY STOP');shot('final-candidates')
 tap(136,96);shot('final-committed')
 tap(51,203)
 for x in [35,60,86,111,137,163,189,215,241,265]:tap(x,119)
 req('INPUT POINTER DOWN 255 159 0');pump(1.1);req('INPUT POINTER UP 255 159 0');pump(.2)
 shot('final-held-delete');req('IME')
 tap(230,179);pump(.4)
 # Repeat on-demand start/release with no screenshot allocations in peak window.
 req('MEMORY START')
 for i in range(3):
  tap(39,100);pump(3.7);tap(247,203);pump(.4);req('MEMORY')
 req('MEMORY STOP');req('PACKAGE stop pxa-cpp-system-input');pump(.5);req('MEMORY')
 (out/'functional.json').write_text(json.dumps({'rows':rows,'logs':logs},ensure_ascii=False,indent=2))
