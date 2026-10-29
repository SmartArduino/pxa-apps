#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the actual signed app: three keys, touch, shake and stale IMU samples."""
import argparse,importlib.util,os,re,sys,time
from pathlib import Path
here=Path(__file__).resolve()
sys.path.insert(0,str(here.parents[2]/'common/study/tests'))
from test_simulator import Simulator

def key(sim,name):
 sim.command('KEY_DOWN '+name);time.sleep(.10);sim.command('KEY_UP '+name);time.sleep(.20)
def logs(sim):return sim.log_path.read_text()
def wait_for(sim,text,timeout=5):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  if text in logs(sim):return
  assert sim.process.poll() is None,logs(sim)[-1500:]
  time.sleep(.05)
 raise AssertionError((text,logs(sim)[-1500:]))
def motion(path,values):
 tmp=path.with_suffix('.tmp');tmp.write_text(' '.join(map(str,values)));tmp.replace(path)
def exercise(sim,path,w,h):
 wait_for(sim,'Gravity ready');sim.screenshot('home')
 key(sim,'ENTER');wait_for(sim,'Gravity sensors subscribed');sim.screenshot('dice')
 key(sim,'ENTER');wait_for(sim,'Gravity roll n=1');sim.screenshot('rolling');time.sleep(1.4);sim.screenshot('dice-result')
 # Two distinct motion impulses trigger one roll, with a cooldown.
 motion(path,[22000000,0,9806650,400000,100000,0]);time.sleep(.2)
 motion(path,[-22000000,0,9806650,-400000,0,100000]);wait_for(sim,'Gravity roll n=2')
 motion(path,[0,0,9806650,0,0,0]);time.sleep(1.5)
 assert len(re.findall('Gravity roll n=',logs(sim)))==2
 key(sim,'DOWN');key(sim,'ENTER');key(sim,'UP');key(sim,'ENTER');wait_for(sim,'Gravity roll n=3 count=1');time.sleep(1.4)
 history=re.findall(r'Gravity roll n=(\d+) count=(\d+) result=(\d+),(\d+) previous=(\d+)',logs(sim))
 assert len(history)==3,history
 total=0
 for number,count,a,b,previous in history:
  assert int(previous)==total,('Dice history changed with die count',history)
  total=int(a)+(int(b) if int(count)==2 else 0)
 key(sim,'DOWN');key(sim,'DOWN');key(sim,'ENTER') # home
 key(sim,'DOWN');key(sim,'ENTER');wait_for(sim,'Gravity maze started');wait_for(sim,'Gravity calibrated');sim.screenshot('maze-flat')
 motion(path,[2500000,-3500000,9000000,0,0,0]);time.sleep(1.5);sim.screenshot('maze-tilted')
 # Missing samples freeze both physics and the timer, with a visible message.
 path.write_text('STALE');time.sleep(.65);first=sim.screenshot('maze-stale');time.sleep(.45);second=sim.screenshot('maze-stale-still')
 assert first.read_bytes()==second.read_bytes(),'Stale IMU continued to move the game'
 motion(path,[0,0,9806650,0,0,0]);key(sim,'ENTER');sim.screenshot('pause');key(sim,'ENTER');time.sleep(1.1);sim.screenshot('resumed')
 key(sim,'ENTER');key(sim,'UP');key(sim,'ENTER') # pause -> home
 key(sim,'DOWN');key(sim,'DOWN');key(sim,'ENTER');sim.screenshot('settings')
 key(sim,'DOWN');key(sim,'DOWN');key(sim,'ENTER') # manual mode
 key(sim,'DOWN');key(sim,'ENTER');time.sleep(.5);sim.screenshot('maze-manual') # first level
 key(sim,'UP');key(sim,'DOWN');key(sim,'ENTER');sim.screenshot('manual-pause')
 assert 'Gravity frame rejected' not in logs(sim),logs(sim)[-2500:]
 print(f'{w}x{h}: three keys, dice/shake, calibration, stale freeze, pause/resume and manual mode passed',flush=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('workspace','package-root','executable','output'):p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--sizes',nargs='*',default=['240x284','320x240','800x480','480x800'])
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 for size in a.sizes:
  w,h=map(int,size.split('x'));path=a.output.resolve()/f'motion-{size}.txt';motion(path,[0,0,9806650,0,0,0]);os.environ['GRAVITY_TEST_MOTION']=str(path)
  with Simulator(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'gravity-play',w,h,'motion',executable=a.executable.resolve()) as sim:exercise(sim,path,w,h)
if __name__=='__main__':main()
