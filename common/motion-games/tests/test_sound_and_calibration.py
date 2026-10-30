#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Capture real signed-app audio; verify muting, saves and calibration reuse."""
from pathlib import Path
import argparse,array,json,os,shlex,subprocess,time,wave
from test_apps import AppSim,motion,profile,assert_frames,restart as maze_restart,win_fixture
from test_extra_apps import restart

def select(s,label):
 for _ in range(30):
  if s.focus().startswith(label):return
  s.key('DOWN')
 raise AssertionError((label,s.focus()))

def audio_menu(s,app):
 if app=='fortune-sticks':s.choose('返回');s.choose('更多')
 elif app=='shake-dice':s.choose('更多')
 else:s.choose('设置')

def samples(path,offset=0):
 data=path.read_bytes()[offset:] if path.exists() else b''
 result=array.array('h');result.frombytes(data[:len(data)//2*2]);return result

def calibration_reuse(s,w,h):
 # One stable calibration serves pause/resume, restart, home and level choice.
 initial=s.log_path.read_text().count('Marble calibration begin');assert initial==1
 s.choose('继续冒险');s.expect('暂停');s.key('ENTER');s.choose('重玩本关');s.expect('暂停');s.key('ENTER');s.choose('返回首页');s.choose('选关');s.choose('01');s.expect('暂停')
 assert s.log_path.read_text().count('Marble calibration begin')==initial
 s.key('ENTER');s.choose('重新校准体感');s.wait_log('Marble calibration begin',initial+1);s.wait_log('Marble calibrated',2);s.key('ENTER')
 # Production has no win override: only this test instance's signed KV fixture
 # is changed. After its first calibration, advancing to level 2 reuses it.
 fixture=win_fixture(s,w,h,0);maze_restart(s,fixture);s.expect('继续');s.key('ENTER');s.wait_log('Marble won level=1')
 calibrated=s.log_path.read_text().count('Marble calibration begin');s.choose('下一关');s.wait_log('Marble start level=2');s.expect('暂停')
 assert s.log_path.read_text().count('Marble calibration begin')==calibrated
 s.key('ENTER');s.expect('继续冒险')
 print('Maze: calibration reused across pause, retry, home/level selection and next level; explicit recalibration still works',flush=True)

def run(a,app):
 pcm=a.output/(app+'.pcm');os.environ['PXA_TEST_AUDIO_PCM']=str(pcm.resolve());os.environ['LD_PRELOAD']=str(a.capture.resolve())
 path=a.output/(app+'-motion.txt');motion(path,[0,0,9806650,0,0,0]);os.environ['GRAVITY_TEST_MOTION']=str(path.resolve())
 magic,size={'fortune-sticks':(b'1TRF',64),'mini-racer':(b'1CAR',128),'shake-dice':(b'1CID',96),'marble-maze':(b'1ZAM',112)}[app]
 with AppSim(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),app,a.width,a.height,'sound',executable=a.executable.resolve()) as s:
  s.wait_log('Motion sound ready');start=pcm.stat().st_size;time.sleep(.15)
  if app=='fortune-sticks':s.key('ENTER');s.wait_log('Fortune complete n=1')
  elif app=='shake-dice':s.key('ENTER');s.wait_log('Dice complete n=1')
  elif app=='mini-racer':s.key('ENTER');s.wait_log('Racer go');time.sleep(.6);s.key('ENTER')
  else:s.key('ENTER');s.wait_log('Marble calibrated');time.sleep(.4);s.key('ENTER');calibration_reuse(s,a.width,a.height)
  sound=samples(pcm,start);assert sound and any(sound),(app,'Silent accepted gameplay sound');assert max(abs(x) for x in sound)<20000
  if app in ('fortune-sticks','shake-dice'):audio_menu(s,app)
  saved=profile(s,magic,size)[3];s.choose('音效');s.wait_log('Motion sound enabled=0');time.sleep(.9)
  assert profile(s,magic,size)[3]==saved,'Sound switch rewrote gameplay progress'
  mark=pcm.stat().st_size;s.screenshot('muted');s.choose('我的收藏' if app=='fortune-sticks' else '查看历史记录' if app=='shake-dice' else '返回首页');time.sleep(.5)
  assert not any(samples(pcm,mark)),(app,'Muted UI emitted samples')
  # Reopen without changing the app's KV: mute preference and game save survive.
  restart(s,magic,size);audio_menu(s,app);select(s,'音效');assert '关闭' in s.focus() or s.focus()=='音效关',s.focus();time.sleep(.3)
  assert profile(s,magic,size)[3]==saved,'Restart changed gameplay save'
  mark=pcm.stat().st_size;s.key('ENTER');s.wait_log('Motion sound enabled=1');time.sleep(.3);assert any(samples(pcm,mark)),(app,'Unmute did not restore audio')
  assert_frames(s);s.screenshot('sound-enabled')
 data=samples(pcm);fmt=json.loads(pcm.with_suffix('.pcm.json').read_text());assert fmt['format']==32784
 with wave.open(str(a.output/(app+'.wav')),'wb') as out:out.setnchannels(fmt['channels']);out.setsampwidth(2);out.setframerate(fmt['rate']);out.writeframes(data.tobytes())
 print(app+': actual mixer PCM, muted silence, independent/persistent preference, unchanged gameplay saves and unmute passed',flush=True)

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--apps',nargs='*',default=['fortune-sticks','mini-racer','shake-dice','marble-maze']);p.add_argument('--width',type=int,default=240);p.add_argument('--height',type=int,default=284);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 a.capture=a.output/'audio-capture.so';flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl2'],text=True));subprocess.run(['cc','-shared','-fPIC','-O2',*flags,str(Path(__file__).with_name('audio_capture.c')),'-ldl','-o',str(a.capture)],check=True)
 for app in a.apps:run(a,app)
if __name__=='__main__':main()
