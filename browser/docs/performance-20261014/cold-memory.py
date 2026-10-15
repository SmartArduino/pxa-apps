import sys,time,json
from pathlib import Path
sys.path.insert(0,'tools/pxadb');import pxadb
base=Path('local/browser-validation/fps-20261014');name=sys.argv[1];logs=[]
port='/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_A4:CB:8F:D5:D4:F0-if00'
for attempt in range(15):
 try:
  client=pxadb.PxaDbClient(port,30);client.__enter__();client.hello();break
 except Exception:
  if attempt==14:raise
  time.sleep(1)
c=client
try:
 c.log_callback=lambda f:logs.append(f.payload);c.raw_callback=lambda b:logs.append(b.decode(errors='replace'));c.subscribe_logs()
 def cmd(s):return c.request(s,timeout=30)
 def pump(s):
  end=time.monotonic()+s
  while time.monotonic()<end:c.pump_logs();time.sleep(.005)
 def memory(s='MEMORY'):
  out={}
  for f in cmd(s):
   if f.kind=='DATA':
    d=pxadb.info_properties(f.payload);scope=d.pop('scope');kind=d.pop('name','');out[scope+(':'+kind if kind else '')]={k:int(v) for k,v in d.items()}
  return out
 pump(5)
 try:cmd('PACKAGE stop pxa-browser')
 except pxadb.PxaDbRejectedError:pass
 pump(1.5);cmd('MEMORY START');before=memory();cmd('PACKAGE run pxa-browser');pump(4);root=memory()
 cmd('INPUT TOUCH DOWN 50 165 0');pump(.12);cmd('INPUT TOUCH UP 50 165 0');pump(5);large=memory('MEMORY STOP')
 assert any('received=57344 text=49152' in s for s in logs),logs
 assert not any('BROWSER ERROR' in s or 'BROWSER DRAW ERROR' in s for s in logs),logs
 cmd('PACKAGE stop pxa-browser');pump(2);cmd('PACKAGE run pxa-browser');pump(2);cmd('PACKAGE stop pxa-browser');pump(1.5)
 (base/(name+'.json')).write_text(json.dumps(dict(before=before,root=root,large=large,logs=logs,passed=True),ensure_ascii=False,indent=2));(base/(name+'.log')).write_text('\n'.join(logs));print(name,'PASS',flush=True)
finally:c.__exit__(None,None,None)
