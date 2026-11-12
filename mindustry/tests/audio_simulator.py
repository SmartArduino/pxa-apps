#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run actual Ogg decoders, capture the Host mix, and exercise audio settings."""
import argparse,array,json,math,os,re,signal,subprocess,time,wave
from pathlib import Path
from simulator import command,read_saves
from ui_controls import Hud

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.package=a.package.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
 sock=Path(f'/tmp/md-audio-{os.getpid()}.sock');state=a.output/'state';state.mkdir(parents=True,exist_ok=True);logpath=a.output/'simulator.log';raw=a.output/'host-mix.s16';result={}
 env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='disk',SDL_DISKAUDIOFILE=str(raw),SDL_DISKAUDIODELAY='32',PXA_SIM_NAV_MODE='gesture-no-bar')
 argv=[str(a.simulator),'--package',str(a.package),'--publisher-key',str(a.publisher_key),'--state-root',str(state),'--width','480','--height','480','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=env,start_new_session=True)
  def text():return logpath.read_text(errors='replace')
  def wait(predicate,timeout=35):
   end=time.monotonic()+timeout
   while time.monotonic()<end:
    assert proc.poll() is None,text()[-3000:]
    if predicate():return
    time.sleep(.05)
   raise TimeoutError(text()[-4000:])
  def tap(x,y):
   command(sock,f'TOUCH DOWN 0 {int(x)} {int(y)}');time.sleep(.05);command(sock,f'TOUCH UP 0 {int(x)} {int(y)}');time.sleep(.25)
  def rms():
   samples=array.array('h',raw.read_bytes()[-32000:]);assert len(samples)>10000
   return math.sqrt(sum(v*v for v in samples)/len(samples))
  def ready_count():return len(re.findall(r'music event id=\d+ state=1 status=0',text()))
  def shot(name):
   head,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and head==f'PNG {len(png)}'.encode();(a.output/f'{name}.png').write_bytes(png)
  try:
   wait(lambda:sock.exists() and 'glyph cache ready' in text() and 'music event id=0 state=1 status=0' in text())
   time.sleep(3);result['menu_rms']=rms();assert result['menu_rms']>50
   hud=Hud(480,480,tap);top=12+hud.n
   tap(240,129+61+4*28+10) # Title -> settings.
   shot('settings')
   music=lambda:tap(240,top+2*(hud.n+12)+hud.n//2)
   effects=lambda:tap(240,top+hud.n+12+hud.n//2)
   music();music() # Original -> continuous -> off.
   time.sleep(2.5);result['muted_rms']=rms();assert result['muted_rms']<1,result
   effects() # Effects off as well.
   count=ready_count();music() # Original mode, menu music enabled independently.
   wait(lambda:ready_count()>count);time.sleep(3)
   result['music_with_effects_off_rms']=rms();assert result['music_with_effects_off_rms']>50
   music() # Continuous gameplay mode.
   shot('continuous-music');hud.back();tap(240,129+61+2*28+10) # New sandbox.
   wait(lambda:'new sandbox' in text())
   wait(lambda:re.search(r'music event id=[1-6] state=1 status=0',text()))
   time.sleep(2.5);result['game_rms']=rms();assert result['game_rms']>30
   hud.tools(5);hud.dock(3) # Effects back on, close tools.
   hud.jump(19,33);tap(248,hud.cy+8)
   wait(lambda:'recording play id=13 track=3' in text())
   shot('mining')
   # Construct a wall through Alpha and observe original build and completion sounds.
   hud.choose(2,1,3);hud.jump(22,34);tap(248,hud.cy+8);hud.confirm();hud.cancel()
   wait(lambda:'recording play id=12 track=3' in text())
   wait(lambda:'recording play id=4 track=2' in text())
   hud.wave();wait(lambda:'recording play id=1 track=2' in text())
   before=len(re.findall('music accepted',text()));hud.pause();time.sleep(.8)
   assert len(re.findall('music accepted',text()))==before
   hud.menu(3);wait(lambda:ready_count()>=4 and text().rfind('music event id=0 state=1')>text().rfind('music event id=1 state=1'))
   shot('returned-menu');command(sock,'KEY HOME');proc.wait(timeout=10);assert proc.returncode==0
   result['music_events']=re.findall(r'Mindustry: music (?:accepted|event)[^\n]*',text())
   match=re.search(r'PXA MUSIC BUFFER underruns=(\d+).*?consumed=(\d+).*?ready_max_us=(\d+)',text());assert match,text()[-2000:]
   result['underruns'],result['consumed_samples'],result['ready_max_us']=map(int,match.groups());assert result['consumed_samples']>10000
   assert not re.search(r'music event id=\d+ state=5|Guest exception|out of bounds memory|recording preparation deferred',text()),text()[-3000:]
   latest=max(read_saves(state),key=lambda s:s['ticks']);result['save']=latest
   # Small export of the actual mixed output, ready to listen to locally.
   with wave.open(str(a.output/'host-mix.wav'),'wb') as wav:
    wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(16000);wav.writeframes(raw.read_bytes())
   result['passed']=True;(a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
   print('Real audio passed: original Ogg READY + nonzero Host mix, independent mute, mining/build loops, completion/wave effects, menu/game transitions')
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)
if __name__=='__main__':main()
