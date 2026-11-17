#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Play every unit response through the actual game UI and SDK audio mixer.

Requires a native voice_fixture executable, a signed app and the official host.
All captures and fixture saves must be written to local/artifacts.
"""
from pathlib import Path
import argparse,json,math,os,re,socket,subprocess,time,wave
import numpy as np

p=argparse.ArgumentParser()
for name in ('simulator','package','publisher-key','fixture','output'):p.add_argument('--'+name,type=Path,required=True)
args=p.parse_args()
for name in vars(args):setattr(args,name,getattr(args,name).resolve())
args.output.mkdir(parents=True,exist_ok=False)
cast=json.loads((Path(__file__).resolve().parents[1]/'tools/voice-prompts.json').read_text())['cast'];results=[]
for kind,unit in enumerate(cast):
 out=args.output/unit['id'];out.mkdir();state=out/'state';state.mkdir();sock=Path('/tmp')/f'iron-voices-{os.getpid()}-{kind}.sock';raw=out/'mix.raw';log=out/'run.log';proc=None
 def command(value):
  with socket.socket(socket.AF_UNIX) as c:
   c.settimeout(25);c.connect(str(sock));c.sendall((value+'\n').encode());f=c.makefile('rb');head=f.readline()
   if head.startswith((b'PNG ',b'JSON ')):return f.read(int(head.split()[1]))
   assert head==b'OK\n',(value,head)
 def tap(x,y,pause=.15):command(f'TAP {round(x)} {round(y)}');time.sleep(pause)
 def played():return re.findall(r'Iron Dawn unit voice playing=([a-z0-9_-]+)',log.read_text())
 def cue(label,x,y):
  expected=unit['id']+'-'+label;n=len(played());tap(x,y);deadline=time.monotonic()+4
  while len(played())<=n and time.monotonic()<deadline:time.sleep(.05)
  assert played()[n:]==[expected],(expected,played()[n:],log.read_text()[-1200:]);time.sleep(2.6)
 def rms_tail(seconds=.25):
  a=np.frombuffer(raw.read_bytes()[-int(32000*seconds):],dtype='<i2').astype(float)/32768;return float(np.sqrt(np.mean(a*a)))
 try:
  # First launch obtains normal private-file layout and saves through the app.
  with log.open('w') as stream:
   def launch():
    global proc
    prior=log.read_text().count('Iron Dawn ready');proc=subprocess.Popen([str(args.simulator),'--package',str(args.package),'--publisher-key',str(args.publisher_key),'--state-root',str(state),'--width','854','--height','480','--density-dpi','160','--safe-insets','0,0,0,0','--corner-radius','0','--pxadb-control-socket',str(sock)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='disk',SDL_DISKAUDIOFILE=str(raw),SDL_DISKAUDIODELAY='32',PXA_SIM_NAV_MODE='gesture-no-bar'),stdout=stream,stderr=subprocess.STDOUT)
    deadline=time.monotonic()+40
    while log.read_text().count('Iron Dawn ready')<=prior:assert proc.poll() is None and time.monotonic()<deadline;time.sleep(.1)
   launch();command('KEY BACK');proc.wait(timeout=15);assert proc.returncode==0
   saves=list((state/'private-files').rglob('iron-dawn-*.save'));assert saves
   for save in saves:subprocess.run([str(args.fixture),str(kind),str(save)],check=True)
   launch();assert 'Iron Dawn restored' in log.read_text();tap((854-480)/2+365,max(214,480-128)+20)
   matches=re.findall(r'canvas=(\d+)x(\d+) wide=1 world=(\d+) top=(\d+) bottom=(\d+) scale=(\d+)',log.read_text());assert matches;w,h,ww,top,bottom,scale=map(int,matches[-1]);assert (w,h,scale)==(854,480,1);cx=ww/2;cy=(top+bottom)/2
   cue('select_0',cx,cy);cue('select_1',cx,cy)
   cue('move_0',cx-55,cy-30);cue('move_1',cx-65,cy-20)
   row=40;cw=(ww-46)//6
   if kind<4:
    cue('attack_0',cx+40,cy+20);cue('attack_1',cx+40,cy+20);cue('stop',8+2*(cw+6)+cw/2,bottom+5+row/2)
   # Request production using the real sidebar card, then await its spawn cue.
   side=w-ww;map_budget=h-top-2*row-56-3*36;ms=max(32,min(112,min(max(h//6,96),map_budget)));tabs=top+ms+20;grid=tabs+row+8;footer=h-row-8;ch=min((footer-8-grid-12)//3,76);cardw=(side-22)//2
   n=len(played());tap(ww+8+(kind%2)*(cardw+6)+cardw/2,grid+(kind//2)*(ch+6)+ch/2);deadline=time.monotonic()+15
   while len(played())<=n and time.monotonic()<deadline:time.sleep(.1)
   assert played()[n:]==[unit['id']+'-ready'],played()[n:];time.sleep(2.6)
   if kind==0:
    # Rapid reselection has one speaker; pause cancels that sentence and mute
    # prevents later selections from leaving an obsolete response queued.
    n=len(played())
    for i in range(12):tap(cx,cy,.025)
    time.sleep(.2);assert len(played())==n+1
    tap(w-42,24);time.sleep(.8);assert rms_tail()==0
    yy=(h-248)/2;tap(w*.75,yy+176);tap(w/2,yy+97);n=len(played());tap(cx,cy);time.sleep(.8);assert len(played())==n and rms_tail()==0
    tap(w-42,24);tap(w*.75,yy+176);tap(w/2,yy+97);time.sleep(.3);tap(cx,cy);time.sleep(.5);assert len(played())==n+1;time.sleep(2.6)
   (out/'runtime-info.json').write_bytes(command('RUNTIME INFO'));(out/'battle.png').write_bytes(command('SCREENSHOT'));assert rms_tail()>1e-4
   tap(w-42,24);yy=(h-248)/2;tap(w/2,yy+216);command('KEY BACK');proc.wait(timeout=15);assert proc.returncode==0
  text=log.read_text();assert not any(x in text for x in ('unit voice unavailable','double free','corruption','out of bounds','Exception:'));clips=played();assert set(unit['id']+'-'+key for key in unit['lines'])<=set(clips)
  with wave.open(str(out/'actual-host-mix.wav'),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(16000);f.writeframes(raw.read_bytes())
  results.append({'unit':unit['id'],'passed':True,'clips':clips});print(unit['id'],'passed',len(set(clips)),'responses',flush=True)
 except Exception as e:
  results.append({'unit':unit['id'],'passed':False,'error':str(e)});print(unit['id'],'failed',str(e),flush=True)
 finally:
  if proc and proc.poll() is None:proc.terminate();proc.wait(timeout=10)
  sock.unlink(missing_ok=True)
(args.output/'result.json').write_text(json.dumps({'passed':all(x['passed'] for x in results),'units':results,'capture':'signed production app, controlled V6 saves, real UI input and SDK SDL disk audio'},indent=2)+'\n')
raise SystemExit(0 if all(x['passed'] for x in results) else 1)
