#!/usr/bin/env python3
"""Exercise a user-supplied JAR in the actual signed PXA simulator package.
No ROM assets belong in source control. Keep output under local/artifacts.
"""
import argparse,hashlib,json,os,re,shutil,socket,struct,subprocess,time,zlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--package',type=Path,required=True);p.add_argument('--jar',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--seconds',type=float,default=12)
p.add_argument('--resolution',default='240x320');p.add_argument('--numeric-pad',action='store_true')
p.add_argument('--heap-kib',type=int,default=2048)
p.add_argument('--keys',default='A,Z,Z,DOWN,Z,RIGHT,Z')
a=p.parse_args();out=a.output.resolve();assert '/local/artifacts/' in str(out);out.mkdir(parents=True,exist_ok=True)
w,h=map(int,a.resolution.split('x'));state=out/'state';state.mkdir(exist_ok=True)
pub='17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171'
private=state/'private-files'/(pub+'-pxa-nano-j2me')/'data';private.mkdir(parents=True,exist_ok=True)
b=bytearray(36);b[:4]=b'NJP2';b[4]=2;b[6]=36;struct.pack_into('<HH',b,8,w,h);b[12]=2;b[13]=3;b[14]=1
b[16:28]=bytes([3,9,5,7,6,19,28,1,18,19,11,12] if a.numeric_pad else [13,14,15,16,17,19,28,1,18,19,11,12]);struct.pack_into('<H',b,28,a.heap_kib);struct.pack_into('<I',b,32,zlib.crc32(b[:32]));(private/'ui-settings.njp').write_bytes(b)
media=state/'media/f0e508596020f9f3d725c628f170454fb86794732f1635682fda18c9eb371355';media.mkdir(parents=True,exist_ok=True)
shutil.copyfile(a.jar,media/a.jar.name)
sock=f'/tmp/nano-game-{os.getpid()}.sock';logpath=out/'runtime.log';log=logpath.open('w')
proc=subprocess.Popen([str(Path('build/simulator/doit-gamekit-1/pxsys_product_simulator').resolve()),'--package',str(a.package.resolve()),'--publisher-key',str(Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der').resolve()),'--state-root',str(state),'--pxadb-control-socket',sock,'--width','800','--height','480','--density-dpi','216'],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='none'),stdout=log,stderr=subprocess.STDOUT)
result={'jar':a.jar.name,'jar_sha256':hashlib.sha256(a.jar.read_bytes()).hexdigest(),'resolution':[w,h],'heap_kib':a.heap_kib,'numeric_pad':a.numeric_pad,'screenshots':[]}
def cmd(t,png=None):
 with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as s:
  s.settimeout(20);s.connect(sock);s.sendall((t+'\n').encode());f=s.makefile('rb');header=f.readline()
  if png:
   assert header.startswith(b'PNG '),header;size=int(header.split()[1]);data=f.read(size);assert len(data)==size;(out/png).write_bytes(data);result['screenshots'].append(png)
  else:assert header.startswith(b'OK'),header

def key(k):
 cmd('KEY_DOWN '+k);time.sleep(.15);cmd('KEY_UP '+k);time.sleep(.25)
try:
 for _ in range(200):
  assert proc.poll() is None,'simulator exited'
  if 'J2ME library jars=1' in logpath.read_text():break
  time.sleep(.05)
 else:raise RuntimeError('library not ready')
 key('Z');time.sleep(a.seconds);cmd('SCREENSHOT AFTER_PRESENT','01-start.png')
 for n,k in enumerate(a.keys.split(',')):
  if k:key(k);time.sleep(.7)
  if n in (0,2,5):cmd('SCREENSHOT AFTER_PRESENT',f'02-key-{n}.png')
 time.sleep(a.seconds);cmd('SCREENSHOT AFTER_PRESENT','03-play.png')
 # Use the queued window Back request: a short controller Start pulse can
 # disappear between controller-state polls in a busy MIDlet.
 cmd('KEY BACK');time.sleep(.3)
 for _ in range(4):key('DOWN')
 key('Z')
 deadline=time.monotonic()+5
 while 'J2ME returned result=0' not in logpath.read_text() and time.monotonic()<deadline:
  time.sleep(.1)
 result['exited_normally']='J2ME returned result=0' in logpath.read_text()
except Exception as e:result['test_error']=str(e)
finally:
 if proc.poll() is None:
  proc.terminate()
  try:proc.wait(5)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
 log.close();Path(sock).unlink(missing_ok=True)
 text=logpath.read_text();result['stats']=re.findall(r'stats: .*',text)
 result['issues']=[s for s in text.splitlines() if re.search(r'Exception|Error|failed|not implemented|not a usable|cannot |unsupported|no such',s,re.I)]
 result['returned']=re.findall(r'J2ME returned result=(\d+) frames=(\d+)',text)
 from PIL import Image
 result['image_changes']=len({hashlib.sha256((out/f).read_bytes()).hexdigest() for f in result['screenshots']})
 for f in result['screenshots']:
  im=Image.open(out/f).convert('RGB');colors=len(im.getcolors(im.width*im.height) or [])
  result.setdefault('colors',{})[f]=colors
 (out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2));print(json.dumps({k:result[k] for k in ['jar','returned','image_changes','stats']},ensure_ascii=False))
