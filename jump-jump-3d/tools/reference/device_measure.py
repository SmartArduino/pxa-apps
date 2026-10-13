# Run from the PXA workspace root; argument: output directory.
# Korvo: /dev/ttyUSB0, 2000000 baud, 800 x 480; data is preserved.
import sys,os,time,json
from pathlib import Path
sys.path.insert(0,'tools/pxadb');import pxadb
os.environ['PXADB_BAUD']='2000000'
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def pump(c,seconds):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  c.pump_logs();time.sleep(.01)
with pxadb.open_client('/dev/ttyUSB0',15) as c:
 logs=[]
 def log(frame):logs.append(frame.payload);pxadb.print_log_frame(frame)
 c.log_callback=log
 c.hello();c.subscribe_logs()
 frames=c.request('PACKAGES');(out/'packages.txt').write_text('\n'.join(f.payload for f in frames))
 c.request('INPUT KEY HOME');c.request('INPUT SWIPE 400 430 400 100 400 10');pump(c,1)
 try:c.request('PACKAGE stop pxa-jump-jump-3d')
 except pxadb.PxaDbRejectedError as e:
  if 'package_not_running' not in str(e):raise
 pump(c,2);logs.clear();c.request('PACKAGE run pxa-jump-jump-3d');pump(c,5)
 if not any('J3CPP ready' in line for line in logs):raise RuntimeError('game did not launch')
 def snap(name):
  for attempt in range(5):
   try:
    cap=pxadb.screenshot_capture(c.request('SCREENSHOT JPEG AFTER_PRESENT',timeout=20))
    pxadb.write_screenshot(cap,out/(name+'.jpg'));return
   except Exception: pump(c,.2)
  raise RuntimeError('no presented screenshot')
 mem=c.request('MEMORY');(out/'initial-memory.txt').write_text('\n'.join(f.payload for f in mem))
 snap('ready')
 start=time.monotonic()
 while time.monotonic()-start<16:pump(c,.2)
 mem=c.request('MEMORY');(out/'ready-memory.json').write_text('\n'.join(f.payload for f in mem))
 c.request('INPUT POINTER DOWN 400 280 0');c.request('INPUT SYNC');pump(c,.7)
 c.request('INPUT POINTER UP 400 280 0');c.request('INPUT SYNC');pump(c,.15)
 snap('jump');pump(c,1.5);snap('landed')
 for i in range(7):
  c.request('INPUT TAP 400 280');c.request('INPUT SYNC');pump(c,.7)
 mem=c.request('MEMORY');(out/'end-memory.json').write_text('\n'.join(f.payload for f in mem))
 while time.monotonic()-start<34:pump(c,.2)

 (out/'logs.txt').write_text('\n'.join(logs)+'\n')
 print('captured',out)
