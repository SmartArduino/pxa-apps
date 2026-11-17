"""Exercise actual Guest strip rendering, including held-finger loading.

Build the simulator test package with
  --define 'MANGA_API_BASE="http://127.0.0.1:18929"'
Then run with --sdk PATH --package PATH --publisher-key public.der --output PATH.
Output must be a new directory under the workspace's ignored local/artifacts.
Requires Pillow and the pinned native PXA DevKit; no device is modified.
"""
import argparse
from urllib.request import urlopen
import re
from PIL import Image
from pathlib import Path
import subprocess,json,os,time,sys,shutil,tempfile
parser=argparse.ArgumentParser(description=__doc__)
for option in ('sdk','package','publisher-key','output'):parser.add_argument('--'+option,type=Path,required=True)
args=parser.parse_args();sdk=args.sdk.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(sdk/'tools/pxadb'));import pxadb
from ui_query import expect,tap_target,decode_snapshot
class Sim:
 def __init__(self,name,w,h,dpi=160):self.name=name;self.w=w;self.h=h;self.dpi=dpi;self.state=out/name;self.state.mkdir(exist_ok=True);self.bridge=None;self.shell=None
 def __enter__(self):
  env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIMULATOR_FONT=str(sdk/'share/pxa/fonts/noto_sans_cjk_common.ttf'),PXA_SIM_NAV_MODE='gesture-no-bar',PXA_SIMULATOR_PERF='0');pub=self.state/'publisher.der';shutil.copyfile(args.publisher_key,pub)
  subprocess.run([str(sdk/'simulator/bin/pxsys_package_installer'),'--storage-root',str(self.state),'--publisher-key',str(pub),'--source',str(args.package.resolve()),'--expected-id','pxa-manga-reader'],env=env,stdout=(self.state/'install.log').open('w'),stderr=subprocess.STDOUT,check=True)
  self.ipc=tempfile.TemporaryDirectory(prefix='pxa-manga-');self.control=Path(self.ipc.name)/'control.sock';self.socket=Path(self.ipc.name)/'debug.sock'
  self.bridge=subprocess.Popen([sys.executable,str(sdk/'tools/simulator_pxadb.py'),'--state-root',str(self.state),'--socket',str(self.socket),'--control-socket',str(self.control),'--installer',str(sdk/'simulator/bin/pxsys_package_installer'),'--publisher-key',str(pub)],env=env,stdout=(self.state/'bridge.log').open('w'),stderr=subprocess.STDOUT)
  self.shell=subprocess.Popen([str(sdk/'simulator/bin/pxsys_desktop_simulator'),'--installed-packages-root',str(self.state/'packages'),'--state-root',str(self.state),'--publisher-key',str(pub),'--product-runner',str(sdk/'simulator/bin/pxsys_product_simulator'),'--launch','pxa-manga-reader','--width',str(self.w),'--height',str(self.h),'--density-dpi',str(self.dpi),'--safe-insets','0,0,0,0','--corner-radius','0','--no-navigation-bar','--pxadb-control-socket',str(self.control)],env=env,stdout=(self.state/'run.log').open('w'),stderr=subprocess.STDOUT)
  for _ in range(100):
   if self.socket.exists() and self.control.exists():break
   if self.shell.poll() is not None:
    error=str(self.shell.returncode)+': '+(self.state/'run.log').read_text()[-2000:];self.__exit__(None,None,None);raise RuntimeError(error)
   time.sleep(.1)
  self.c=pxadb.PxaDbClient('unix:'+str(self.socket),15);self.c.__enter__();self.c.hello();return self
 def __exit__(self,*args):
  if hasattr(self,'c'):self.c.__exit__(*args)
  for proc in [self.shell,self.bridge]:
   if proc:
    proc.terminate()
    try:proc.wait(timeout=8)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
  if hasattr(self,'ipc'):self.ipc.cleanup()
 def tree(self):return decode_snapshot(self.c.request('UI INSPECT'))
 def wait(self,text,kind='button',count=1,seconds=60):
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   try:return expect(self.tree(),{'type':kind,'text':text},{'count':count,**({'enabled':True} if kind=='button' and count else {})})
   except Exception:time.sleep(.2)
  raise RuntimeError('Missing '+text+': '+str([(x.get('text'),x.get('visible')) for x in self.tree()['nodes'] if x.get('text')]))
 def tap(self,text):
  r=tap_target(self.tree(),{'type':'button','text':text});self.pointer('DOWN',r['x'],r['y'],0);time.sleep(.12);self.pointer('UP',r['x'],r['y'],0);time.sleep(.25)
 def pointer(self,phase,x,y,id):self.c.request(f'INPUT POINTER {phase} {int(x)} {int(y)} {id}');self.c.request('INPUT SYNC')
 def shot(self,name):pxadb.write_screenshot(pxadb.screenshot_capture(self.c.request('SCREENSHOT AFTER_PRESENT')),self.state/(name+'.png'))

def wait_for(f,seconds=20):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  if f():return
  time.sleep(.1)
 raise AssertionError('condition timed out')
def text(d):return (d.state/'run.log').read_text()
def position(d):return json.loads(next((d.state/'private-files').glob('*pxa-manga-reader/manga-state.json')).read_text())['shelf'][0]['page']
def hold(d,delta):
 start=d.h*.88 if delta<0 else d.h*.12;end=start+delta
 d.pointer('DOWN',d.w*.55,start,0)
 for i in range(1,7):d.pointer('MOVE',d.w*.55,start+(end-start)*i/6,0);time.sleep(.025)
 return end
def release(d,end):d.pointer('UP',d.w*.55,end,0);time.sleep(.12)
def swipe(d,delta):release(d,hold(d,delta))
def enter(d,disk_off=False):
 d.wait('书架为空，去发现漫画','text');d.wait('设置')
 if disk_off:
  d.tap('设置');d.wait('阅读模式：自动');d.tap('阅读模式：自动');d.wait('设置已保存','text');d.tap('自动缓存：开启');d.wait('设置已保存','text');d.wait('自动缓存：关闭');d.tap('返回')
 d.tap('书源')
 if not any(n.get('visible') and n.get('text')=='MangaDex' for n in d.tree()['nodes']):d.tap('>')
 d.tap('MangaDex');d.wait('触摸测试漫画');d.tap('触摸测试漫画');d.wait('第 1 话 · 出发');d.tap('第 1 话 · 出发')
 wait_for(lambda:'MANGA page_ready_us=' in text(d))
def colours(path):
 values=list(Image.open(path).convert('RGB').getdata());return [sum(sum(abs(a-b) for a,b in zip(pixel,c))<15 for pixel in values) for c in [(204,221,194),(216,223,235),(237,205,185)]]
results={}
for name,w,h,dpi in [('continuous-small',320,240,125),('continuous-mosaico',480,480,305)]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','24','--height','64'],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
  with Sim(name,w,h,dpi) as d:
   enter(d,True);wait_for(lambda:f'MANGA prefetched={7 if w==480 else 5} ' in text(d));end=hold(d,-h*.85)
   wait_for(lambda:position(d)>=1);held=position(d);d.shot('stitched-while-held');assert sum(n>500 for n in colours(d.state/'stitched-while-held.png'))>=2
   release(d,end);assert position(d)==held;assert text(d).count('MANGA page_ready_us=')==1
   for i in range(4):swipe(d,-h*.65);time.sleep(.2)
   wait_for(lambda:position(d)>=5);forward=position(d)
   d.pointer('DOWN',w*.3,h*.5,0);d.pointer('DOWN',w*.7,h*.5,1);d.pointer('MOVE',w*.15,h*.5,0);d.pointer('MOVE',w*.85,h*.5,1);d.pointer('UP',w*.15,h*.5,0);d.pointer('UP',w*.85,h*.5,1)
   d.shot('whole-column-zoom');assert 'MANGA pinch_permille=' in text(d)
   for i in range(20):
    swipe(d,h*.65);time.sleep(.2)
    if position(d)==0:break
   wait_for(lambda:position(d)==0);d.shot('backward-continuous')
   private=next((d.state/'private-files').glob('*pxa-manga-reader'));assert not list(private.glob('reader-cache-*.png'));assert not (private/'reader-reserve.tmp').exists()
   assert text(d).count('MANGA page_ready_us=')==1
   results[name]={'passed':True,'disk_cache_off':True,'held_finger_advanced_to':held,'forward_page':forward,'returned_to_zero':True,'whole_column_zoom':True,'no_page_reload_during_scroll':True}
   print(name,'PASS',results[name],flush=True)
 finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','8','--delay-ms','2500'],stdout=(out/'slow-edge-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3)
 with Sim('continuous-slow-edge',320,240,125) as d:
  enter(d);swipe(d,-150);swipe(d,-150);end=hold(d,-190);d.shot('waiting-while-held')
  wait_for(lambda:'MANGA prefetched=1 ' in text(d));time.sleep(.2);d.shot('loaded-without-release')
  assert sum(n>500 for n in colours(d.state/'loaded-without-release.png'))>=2
  release(d,end);assert text(d).count('MANGA page_ready_us=')==1
  end=hold(d,-180);wait_for(lambda:position(d)>=1);d.shot('next-image-before-release');release(d,end)
  end=hold(d,180);wait_for(lambda:position(d)==0);d.shot('previous-image-before-release');release(d,end)
  assert text(d).count('MANGA page_ready_us=')==1
  results['slow-edge']={'passed':True,'delay_ms':2500,'kept_current_image':True,'joined_without_finger_release':True,'backward_without_page_reload':True}
  print('slow edge PASS',flush=True)
finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','12'],stdout=(out/'low-budget-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(2*1024*1024)
 with Sim('continuous-low-budget',320,240,125) as d:
  enter(d);wait_for(lambda:'MANGA prefetched=1 ' in text(d));wait_for(lambda:'MANGA buffer_capacity=' in text(d))
  for i in range(8):
   swipe(d,-200);time.sleep(.25)
   if position(d)>=2:break
  wait_for(lambda:position(d)>=2);d.shot('low-budget-forward')
  for i in range(20):
   swipe(d,180);time.sleep(.2)
   if position(d)==0:break
  wait_for(lambda:position(d)==0);d.shot('low-budget-backward');assert text(d).count('MANGA page_ready_us=')==1
  private=next((d.state/'private-files').glob('*pxa-manga-reader'));assert not (private/'reader-reserve.tmp').exists()
  results['low-budget']={'passed':True,'host_resource_budget_bytes':2*1024*1024,'kept_neighbors_on_denial':True,'forward_and_backward_continuous':True}
  print('low budget PASS',flush=True)
finally:server.terminate();server.wait(timeout=5)
for name,options in [('network-partial',['--noise']),('network-lossy',['--webp-lossy','--width','640','--height','1000'])]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','12',*options],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
  with Sim(name,480,480,305) as d:
   enter(d);wait_for(lambda:'MANGA prefetched=2 ' in text(d),40)
   counts=json.load(urlopen('http://127.0.0.1:18929/_metrics'))
   for i in range(1,4):assert counts.get('/data-saver/abc123/'+str(i)+'.png')==1,counts
   assert 'MANGA wire_prefetched=1 ' in text(d)
   if name=='network-partial':assert 'MANGA wire_prefetched=1 bytes=262144' in text(d)
   else:assert ' jpeg=1 ' in text(d) and 'MANGA wire_consumed_bytes=' in text(d)
   d.shot('network-ready');d.pointer('DOWN',240,240,0);time.sleep(.15);d.pointer('UP',240,240,0);d.wait('≡');d.tap('≡');d.wait('返回');d.tap('返回');d.wait('设置');before=text(d).count('MANGA page_ready_us=');d.tap('触摸测试漫画');wait_for(lambda:text(d).count('MANGA page_ready_us=')>before,40);d.shot('after-cancel-and-reopen')
   results[name]={'passed':True,'single_request_per_first_three_images':True,'wire_partial_response_reused':name=='network-partial','lossy_webp_jpeg_conversion':name=='network-lossy','cancel_and_resume':True}
   print(name,'PASS',flush=True)
 finally:server.terminate();server.wait(timeout=5)
(out/'continuous-result.json').write_text(json.dumps(results,indent=2));print('All continuous scrolling cases PASS',flush=True)
