#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Drive an already signed release through real PXA product simulators.

Evidence is always outside the source tree. Store capture settings are explicit:
rectangular viewport, zero safe insets, no perf overlay and no bottom navigation.
"""
from pathlib import Path
import argparse,concurrent.futures,json,os,socket,subprocess,time
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument("--profiles",nargs="+");args=p.parse_args()
for name in ('simulator','package','publisher_key'):setattr(args,name,getattr(args,name).resolve())
args.output=args.output.resolve();args.output.mkdir(parents=True,exist_ok=False)
profiles={'pai-touch':(296,240),'doit-gamekit-2':(320,240),'esp-mosaico':(480,480),'espp4-a':(480,854)}
if args.profiles:profiles={name:profiles[name] for name in args.profiles}

def run(item):
 name,(pw,ph)=item;out=args.output/name;out.mkdir();state=out/'state';state.mkdir();sock=Path('/tmp')/f'iron-review-{os.getpid()}-{name}.sock';log=out/'run.log';proc=None;h=max(320,min(1060,ph*480//pw));bottom=h-113;mid=(43+bottom)//2;shots=[]
 def command(s):
  with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as c:
   c.settimeout(40);c.connect(str(sock));c.sendall((s+'\n').encode());f=c.makefile('rb');head=f.readline()
   if head.startswith(b'PNG '):return f.read(int(head.split()[1]))
   assert head==b'OK\n',(name,s,head)
 def launch():
  nonlocal proc
  if sock.exists():sock.unlink()
  old_ready=log.read_text().count("Iron Dawn ready") if log.exists() else 0
  proc=subprocess.Popen([str(args.simulator),'--package',str(args.package),'--publisher-key',str(args.publisher_key),'--state-root',str(state),'--locale','zh-CN','--width',str(pw),'--height',str(ph),'--safe-insets','0,0,0,0','--corner-radius','0','--pxadb-control-socket',str(sock)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar',PXA_SIMULATOR_PERF='0'),stdout=stream,stderr=subprocess.STDOUT)
  end=time.monotonic()+90
  while time.monotonic()<end:
   assert proc.poll() is None,(name,'simulator exited',log.read_text()[-1000:])
   if sock.exists() and log.read_text().count('Iron Dawn ready')>old_ready:break
   time.sleep(.2)
  else:raise TimeoutError(name+' readiness')
  time.sleep(.4)
 def tap(x,y):command(f'TAP {int(x*pw/480)} {int(y*ph/h)}');time.sleep(.25)
 def shot(label):
  path=out/(label+'.png');path.write_bytes(command('SCREENSHOT'));im=Image.open(path).convert('RGBA');assert im.size==(pw,ph);assert im.getextrema()[3]==(255,255);assert len(im.getcolors(pw*ph) or [])>40;shots.append(str(path));print(name,label,flush=True)
 def stop():
  nonlocal proc
  if proc:
   proc.terminate()
   try:proc.wait(10)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
   proc=None
 try:
  with log.open('w') as stream:
   launch();shot('home');tap(130,max(214,h-128)+20);shot('briefing');tap(180,h-44);time.sleep(.5);shot('battle')
   tap(28,bottom+50);tap(102,bottom+27);tap(360,mid);time.sleep(6);tap(295,bottom+27);tap(360,mid-40);time.sleep(11);shot('base-development')
   tap(27,bottom+20);tap(232,bottom+27);time.sleep(8);shot('reinforcements');tap(442,bottom+20);tap(436,97);tap(240,mid);time.sleep(3);shot('advance')
   tap(441,22);time.sleep(4);shot('paused');stop();launch();assert 'Iron Dawn restored' in log.read_text();shot('restored-home');tap(365,max(214,h-128)+20);time.sleep(.7);shot('restored-battle');stop()
  text=log.read_text();assert 'surface create error' not in text and 'save open failed' not in text and 'save publish failed' not in text and 'save write failed' not in text and 'PXA_UI_ERROR' not in text
  return {'profile':name,'passed':True,'screenshots':shots,'restored':True}
 except Exception as e:return {'profile':name,'passed':False,'error':str(e),'screenshots':shots}
 finally:stop();sock.unlink(missing_ok=True)

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(run,profiles.items()))
(args.output/'result.json').write_text(json.dumps({'passed':all(r['passed'] for r in results),'capture':'store-rectangular','profiles':results},ensure_ascii=False,indent=2)+'\n')
print(json.dumps(results,ensure_ascii=False),flush=True)
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
