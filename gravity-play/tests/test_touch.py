#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify pointer capture, touch actions and settings across a real app restart."""
import argparse,os,subprocess,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'common/study/tests'))
from test_simulator import Simulator

def key(s,k):s.command('KEY_DOWN '+k);time.sleep(.1);s.command('KEY_UP '+k);time.sleep(.2)
def wait(s,text,count=1):
 for _ in range(100):
  if s.log_path.read_text().count(text)>=count:return
  assert s.process.poll() is None,s.log_path.read_text()[-1000:]
  time.sleep(.05)
 raise AssertionError(text)
def restart(s):
 command=[v.decode() for v in Path(f'/proc/{s.process.pid}/cmdline').read_bytes().split(b'\0') if v]
 s.process.terminate();s.process.wait(timeout=3);Path(s.socket).unlink(missing_ok=True)
 s.process=subprocess.Popen(command,stdout=s.log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
 for _ in range(100):
  if Path(s.socket).exists():break
  assert s.process.poll() is None
  time.sleep(.05)
 wait(s,'Gravity ready',2);time.sleep(.4)
def exercise(s,width,height):
 scale=2 if min(width,height)>=400 else 1;w,h=width//scale,height//scale
 def tap(x,y):s.tap(x*scale,y*scale)
 def home_row(i):return 61+i*max(36,(h-86)//4)+max(32,(h-86)//4-5)//2
 wait(s,'Gravity ready');tap(w/2,home_row(0));wait(s,'Gravity sensors subscribed')
 tap(w/2,h/2);wait(s,'Gravity roll n=1');time.sleep(1.5);s.screenshot('touch-dice')
 tap(w-35,h-28);tap(w/2,home_row(1));wait(s,'Gravity calibrated');s.screenshot('touch-maze')
 x,y=int(width/2),int(height/2)
 s.command(f'TOUCH DOWN 0 {x} {y}');s.command(f'TOUCH MOVE 0 {x+40*scale} {y}');time.sleep(.6)
 # A different finger cannot pause or release the captured drag.
 s.command(f'TOUCH DOWN 1 {width-20} {10*scale}');s.command(f'TOUCH UP 1 {width-20} {10*scale}');time.sleep(.2)
 s.screenshot('touch-owner');s.command(f'TOUCH CANCEL 0 {x} {y}');time.sleep(.2)
 tap(w-20,15);s.screenshot('touch-pause')
 pause_step=max(29,(h-66-19)//5);tap(w/2,66+4*pause_step+max(26,pause_step-5)//2)
 tap(w/2,home_row(2))
 settings_step=max(29,(h-53-19)//5);tap(w/2,53+max(26,settings_step-5)//2);tap(w/2,53+settings_step+max(26,settings_step-5)//2)
 key(s,'UP');before=s.screenshot('settings-saved');restart(s);tap(w/2,home_row(2));after=s.screenshot('settings-restored')
 assert before.read_bytes()==after.read_bytes(),'Sensitivity or axis rotation did not survive restart'
 assert 'Gravity frame rejected' not in s.log_path.read_text()
 print(f'{width}x{height}: touch roll/menu, single drag owner, cancellation and settings restart passed',flush=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--sizes',nargs='*',default=['240x284','320x240','800x480','480x800']);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 path=a.output.resolve()/'motion.txt';path.write_text('0 0 9806650 0 0 0');os.environ['GRAVITY_TEST_MOTION']=str(path)
 for size in a.sizes:
  w,h=map(int,size.split('x'))
  with Simulator(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'gravity-play',w,h,'touch',executable=a.executable.resolve()) as s:exercise(s,w,h)
if __name__=='__main__':main()
