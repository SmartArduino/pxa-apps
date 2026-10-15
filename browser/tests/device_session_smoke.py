#!/usr/bin/env python3
"""pai-touch cookie/password/history smoke; install a /session/login?prefilled=1
fixture build. Only public fixture credentials are used. Preserve preferences.
"""
import argparse,io,json,re,subprocess,sys,time
from PIL import Image
from pathlib import Path
from geometry import geometry,navigation

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--pxadb-dir',type=Path,default=Path('tools/pxadb'));a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 sys.path.insert(0,str(a.pxadb_dir.resolve()));import pxadb
 records=[];mem={};result={}
 with pxadb.PxaDbClient(a.port,30) as c:
  c.hello();c.subscribe_logs();c.log_callback=lambda f:records.append({'kind':f.kind,'payload':f.payload})
  fs=pxadb.NormalFsClient(c)
  def command(value):
   frames=c.request(value);records.append({'command':value,'frames':[{'kind':f.kind,'payload':f.payload} for f in frames if value!='SCREENSHOT' or f.kind!='DATA']});return frames
  epoch_ms=0
  def logs(mark=0):
   fresh=[]
   for r in records[mark:]:
    value=r.get('payload','');match=re.match(r'^(\d+)\t',value)
    if not match or int(match.group(1))>=epoch_ms:fresh.append(value)
   return '\n'.join(fresh)
  def pump(n):
   end=time.monotonic()+n
   while time.monotonic()<end:c.pump_logs();time.sleep(.005)
  def wait(needle,mark,timeout=20):
   end=time.monotonic()+timeout
   while needle not in logs(mark):
    assert time.monotonic()<end,(needle,logs(mark)[-2000:]);pump(.05)
   pump(1.0) # Input acknowledgements precede Guest handling and LCD presentation.
  def tap(x,y):command(f'INPUT TOUCH DOWN {x} {y} 0');pump(.12);command(f'INPUT TOUCH UP {x} {y} 0');pump(.3)
  def shot(name):
   pump(1.0)
   pxadb.write_screenshot(pxadb.screenshot_capture(command('SCREENSHOT')),a.output/(name+'.png'))
  def expect_screen(name,predicate):
   end=time.monotonic()+12
   while True:
    shot(name);path=a.output/(name+'.png')
    if predicate(Image.open(path).convert('RGB'),path):return
    assert time.monotonic()<end,'screen did not reach '+name
  def ocr(path):
   image=Image.open(path).convert('RGB');buffer=io.BytesIO()
   image.resize((image.width*3,image.height*3)).save(buffer,format='PNG')
   return subprocess.check_output(['tesseract','stdin','stdout','-l','chi_sim+eng','--psm','6'],input=buffer.getvalue(),stderr=subprocess.DEVNULL).decode('utf8','replace').replace(' ','')
  def memory(value):return [{'kind':f.kind,'payload':f.payload} for f in command(value)]
  roots=[name for kind,size,name in fs.list('pxa-state/data') if name.endswith('~pxa-browser')];assert len(roots)==1
  remote='pxa-state/data/'+roots[0]+'/bookmarks.txt';fs.get(remote,a.output/'preferences-before.txt')
  original=(a.output/'preferences-before.txt').read_bytes()
  try:
   if any(f.kind=='PKG' and ':pxa-browser\t' in f.payload and 'active=1' in f.payload for f in command('PACKAGES')):command('PACKAGE stop pxa-browser');pump(1.6)
   # Require an unlocked panel. Blind upward swipes on an unlocked device
   # open Recents and can hide an otherwise running app.
   command('INPUT KEY HOME');pump(1.5)
   ack=command('INPUT KEY BACK');pump(1.5)
   epoch_ms=max(int(pxadb.info_properties(f.payload).get('device_us','0'))//1000 for f in ack if f.kind=='OK')
   command('MEMORY START');mem['before-start']=memory('MEMORY');mark=len(records);command('PACKAGE run pxa-browser');wait('BROWSER DONE status=200',mark)
   mem['startup']=memory('MEMORY STOP');layout=geometry(logs(mark));shot('prefilled-form')
   x,y,w,h=layout['content'];mark=len(records);tap(x+w//2,y+45)
   # Trace INFO is quota-limited; a missing HIT log is not a missing click.
   expect_screen('password-sensitive-ime',lambda image,path:
     sum(max(image.getpixel((xx,yy)))<120 for yy in range(150,190) for xx in range(30,266))>4000)
   command('INPUT KEY BACK');pump(1.0)
   layout=geometry(logs());x,y,w,h=layout['content'];command('MEMORY START');mark=len(records);tap(x+w//2,y+75);wait('BROWSER DONE status=200',mark)
   mem['login']=memory('MEMORY STOP');expect_screen('logged-in',lambda image,path:'登录成功' in ocr(path));result['login_redirect']=True
   mark=len(records);tap(*navigation(layout,3));wait('BROWSER DONE status=200',mark);shot('session-reload');result['cookie_reload']=True
   tap(*navigation(layout,4));x,y,w,h=layout['content'];edge=x+w-3
   command(f'INPUT TOUCH DOWN {x+w//2} {y+h-12} 0');pump(.12);command(f'INPUT TOUCH MOVE {x+w//2} {y+12} 0');pump(.12);command(f'INPUT TOUCH UP {x+w//2} {y+12} 0');pump(.2)
   offset=min(h-24,max(0,(9+sum(bool(l) and not l.startswith('#') for l in original.decode().splitlines()))*35-h))
   mark=len(records);tap(x+w//2,y+5*35-offset+16)
   expect_screen('history',lambda image,path:'清空浏览' in ocr(path));result['history_list']=True
   command('INPUT KEY BACK');pump(.3);tap(*navigation(layout,4))
   command('INPUT KEY HOME');pump(.6);mark=len(records);command('PACKAGE run pxa-browser');pump(.8);shot('foreground');result['foreground_resume']=True
   tap(*navigation(layout,2));shot('home');tap(*navigation(layout,4));tap(x+w*3//4,y+16);pump(2)
   assert not any(f.kind=='PKG' and ':pxa-browser\t' in f.payload and 'active=1' in f.payload for f in command('PACKAGES'))
   result['menu_exit']=True;mem['closed']=memory('MEMORY');fs.get(remote,a.output/'preferences-after.txt');assert (a.output/'preferences-after.txt').read_bytes()==original
   result['preferences_preserved']=True
   (a.output/'results.json').write_text(json.dumps({'passed':result,'memory':mem,'records':records},ensure_ascii=False,indent=2));print('device session smoke PASS',flush=True)
  finally:
   (a.output/'records.json').write_text(json.dumps(records,ensure_ascii=False,indent=2))
   command('INPUT CANCEL')
   try:command('MEMORY STOP')
   except pxadb.PxaDbRejectedError:pass
if __name__=='__main__':main()
