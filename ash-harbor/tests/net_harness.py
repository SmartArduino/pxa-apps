"""Shared real-PXA-simulator controls for the LAN integration scenarios."""
import os,re,socket,struct,subprocess,time,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]
def configure(package, output):
 global _package, _output
 _package=Path(package).resolve();_output=Path(output).resolve();_output.mkdir(parents=True,exist_ok=True)
class Game:
 def __init__(self,name,width,height,nav="gesture-no-bar",safe="0,0,0,0"):
  self.name=name;self.width=width;self.height=height;self.out=_output/name;self.out.mkdir(exist_ok=True);self.log=self.out/'runtime.log';self.sock=Path(f'/tmp/ash-lan-{os.getpid()}-{name}.sock')
  (self.out/'tested-artifacts.json').write_text(json.dumps({name:hashlib.sha256((_package/name).read_bytes()).hexdigest() for name in ['manifest.pxm','artifacts/main.linux-x86_64.aot']},indent=2))
  (self.out/'state').mkdir(exist_ok=True);self.stream=self.log.open('w');self.proc=subprocess.Popen([str(ROOT/'build/simulator/pai-touch/pxsys_product_simulator'),'--package',str(_package.resolve()),'--publisher-key',str(ROOT/'local/pxa-apps/.dev-signing/publisher-public.der'),'--state-root',str(self.out/'state'),'--width',str(width),'--height',str(height),'--corner-radius','0','--safe-insets',safe,'--pxadb-control-socket',str(self.sock)],stdout=self.stream,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE=nav,PXA_SIMULATOR_PERF='0'))
 def text(self):return self.log.read_text()
 def wait(self,s,timeout=30,offset=0):
  until=time.monotonic()+timeout
  while time.monotonic()<until:
   assert self.proc.poll() is None,self.text()[-2500:]
   if s in self.text()[offset:]:return
   time.sleep(.05)
  raise AssertionError((self.name,s,self.text()[-3500:]))
 def cmd(self,text):
  with socket.socket(socket.AF_UNIX) as c:
   c.settimeout(15);c.connect(str(self.sock));c.sendall((text+'\n').encode());f=c.makefile('rb');h=f.readline()
   if h.startswith((b'PNG ',b'JSON ')):
    n=int(h.split()[1]);b=b''
    while len(b)<n:
     part=f.read(n-len(b));assert part;b+=part
    return b
   assert h==b'OK\n',(text,h)
 def key(self,k,hold=.08):self.cmd('KEY_DOWN '+k);time.sleep(hold);self.cmd('KEY_UP '+k);time.sleep(.08)
 def geometry(self):return tuple(float(v) if i==2 else int(v) for i,v in enumerate(re.findall(r'Ash Harbor layout width=(\d+) height=(\d+) scale=([\d.]+) x=(-?\d+) y=(-?\d+) lift=(\d+) menu_lift=(\d+) dpad_shift=(\d+)',self.text())[-1]))
 def tap(self,x,y):
  rw,rh,scale,ox,oy,*_=self.geometry();x=round((ox+x*scale)*self.width/rw);y=round((oy+y*scale)*self.height/rh)
  self.cmd(f'TOUCH DOWN 0 {x} {y}');time.sleep(.065);self.cmd(f'TOUCH UP 0 {x} {y}');time.sleep(.2)
 def shot(self,name):
  b=self.cmd('SCREENSHOT');assert struct.unpack('>II',b[16:24])==(self.width,self.height);(self.out/(name+'.png')).write_bytes(b)
 def start(self):
  self.wait('Ash Harbor ready');time.sleep(.3)
  if 'launch view=5' in self.text():self.key('X');self.wait('story-finished')
  self.tap(136,116);self.wait('online-open');self.shot('lobby')
 def pointer(self,phase,id,x,y):
  rw,rh,scale,ox,oy,*_=self.geometry();x=round((ox+x*scale)*self.width/rw);y=round((oy+y*scale)*self.height/rh)
  offset=len(self.text());self.cmd(f'TOUCH {phase} {id} {x} {y}')
  self.wait('Ash Harbor touch-'+('down' if phase=='DOWN' else 'up'),timeout=3,offset=offset)
 def qcf(self,left=False):
  # Acknowledge actual Guest callbacks instead of relying on SDL key sampling
  # while the renderer/compiler is busy. Neutral releases are valid in motions.
  geometry=self.geometry();lift,shift=geometry[5],geometry[7];previous=None
  for x,y in [(42,229),(24 if left else 60,223),(18 if left else 66,205)]:
   if previous is not None:self.pointer('UP',0,*previous)
   point=(x+shift,y-lift);self.pointer('DOWN',0,*point);previous=point
  self.pointer('DOWN',1,228,220-lift);self.pointer('UP',1,228,220-lift);self.pointer('UP',0,*previous)
  time.sleep(.1)
 def stop(self):
  if self.proc.poll() is None:
   self.proc.terminate()
   try:self.proc.wait(timeout=5)
   except subprocess.TimeoutExpired:self.proc.kill();self.proc.wait(timeout=5)
  self.stream.close();self.sock.unlink(missing_ok=True)
