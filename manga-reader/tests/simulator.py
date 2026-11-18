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
def position(d):
 shelf=json.loads(next((d.state/'private-files').glob('*pxa-manga-reader/manga-state.json')).read_text())['shelf']
 return shelf[0]['page'] if shelf else 0
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
  d.tap('设置');d.wait('阅读方式在每本漫画内单独设置','text');d.tap('自动缓存：开启');d.wait('设置已保存','text');d.wait('自动缓存：关闭');d.tap('返回')
 d.tap('书源')
 if not any(n.get('visible') and n.get('text')=='MangaDex' for n in d.tree()['nodes']):d.tap('>')
 d.tap('MangaDex');d.wait('触摸测试漫画');d.tap('触摸测试漫画')
 wait_for(lambda:'MANGA page_ready_us=' in text(d))
def colours(path):
 values=list(Image.open(path).convert('RGB').getdata());return [sum(sum(abs(a-b) for a,b in zip(pixel,c))<15 for pixel in values) for c in [(204,221,194),(216,223,235),(237,205,185)]]
def centered_notice(d,label):
 nodes=d.tree()['nodes'];text_node=next(n for n in nodes if n.get('visible') and n.get('text')==label and n['type']=='text');panel=next(n for n in nodes if n['id']==text_node['parent'] and n['surface']==text_node['surface']);a=text_node['bounds'];b=panel['bounds']
 for axis,size in [('x','width'),('y','height')]:
  assert abs((a[axis]+a[size]/2)-(b[axis]+b[size]/2))<=1,(label,a,b)
  assert b[axis]<=a[axis] and a[axis]+a[size]<=b[axis]+b[size],(label,a,b)
