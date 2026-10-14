import os,sys,time,json
from pathlib import Path
sys.path.insert(0,'tools/pxadb');import pxadb
os.environ['PXADB_BAUD']='2000000';P=Path(__file__).resolve().parent;events=[]
with pxadb.PxaDbClient('/dev/serial/by-id/usb-1a86_USB_Dual_Serial_56D5054872-if02',30) as c:
 assert pxadb.info_properties(c.hello())['serial']=='8c:bf:ea:09:af:6c'
 c.log_callback=lambda f:events.append({'kind':f.kind,'payload':f.payload})
 def req(v):
  events.append({'command':v});r=c.request(v,timeout=30)
  for f in r:
   if f.kind!='DATA' or not v.startswith('SCREENSHOT'):events.append({'kind':f.kind,'payload':f.payload})
  return r
 def pump(t):
  end=time.monotonic()+t
  while time.monotonic()<end:c.pump_logs();time.sleep(.005)
 def pointer(phase,x,y):req(f'INPUT POINTER {phase} {x} {y} 0');req('INPUT SYNC')
 def tap(x,y):pointer('DOWN',x,y);pump(.2);pointer('UP',x,y);pump(.4)
 def shot(name):
  for attempt in range(3):
   try:
    capture=pxadb.screenshot_capture(req('SCREENSHOT JPEG'));assert capture.metadata['source']=='pxa-direct',capture.metadata;pxadb.write_screenshot(capture,P/(name+'.jpg'));print(name,flush=True);return
   except pxadb.PxaDbError:
    if attempt==2:raise
    pump(.3)
 req('INPUT KEY HOME');pump(.3)
 pointer('DOWN',206,317);pump(.15)
 for y in [280,239,198,157,115,90]:pointer('MOVE',206,y);pump(.12)
 pointer('UP',206,90);pump(.4)
 for f in req('PACKAGES'):
  if f.kind=='PKG' and 'pxa-voxel-craft' in f.payload and 'active=1' in f.payload:req('PACKAGE stop pxa-voxel-craft');pump(.8)
 req('PACKAGE run pxa-voxel-craft');pump(6);shot('final-voxel-title');tap(206,258);tap(206,165);pump(6)
 pointer('DOWN',270,150);pump(.2)
 for y in [180,210,240,270,300]:pointer('MOVE',270,y);pump(.2)
 pointer('UP',270,300);pump(.4)
 pointer('DOWN',270,150);pump(.2);pointer('MOVE',270,300);pump(.4);pointer('UP',270,300);pump(.8);shot('final-voxel-aim')
 pointer('DOWN',329,192);pump(.12);shot('final-voxel-mining');pump(3)
 pointer('UP',329,192);pump(.5);shot('final-voxel-edited')
 req('INPUT KEY HOME');pump(.6);req('PACKAGE run pxa-voxel-craft');pump(1);shot('final-voxel-resumed')
 req('PACKAGE stop pxa-voxel-craft');pump(.8);req('PACKAGE run pxa-voxel-craft');pump(6);tap(206,258);tap(206,165);pump(6);shot('final-voxel-reloaded')
 req('PACKAGE stop pxa-voxel-craft');pump(.8);req('PACKAGE run pxa-voxel-craft');pump(6);tap(206,296);shot('final-voxel-settings-before');tap(206,219);shot('final-voxel-settings');tap(206,301)
 req('PACKAGE stop pxa-voxel-craft');pump(.8)
 req('FSSHA '+pxadb.client_encode_path('pxa-state/data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171~pxa-voxel-craft/voxel-slot-0-b.bin'))
 req('PERF CLEAR');req('INPUT CANCEL');req('INPUT KEY HOME');req('MEMORY');req('PACKAGES')
(P/'functional-final-events.json').write_text(json.dumps(events,ensure_ascii=False,indent=2)+'\n')
