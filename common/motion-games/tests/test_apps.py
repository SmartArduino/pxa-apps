#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run both independent signed apps, using three keys and actual pointer input."""
from pathlib import Path
import argparse,os,re,struct,subprocess,sys,time,zlib
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'study/tests'))
from test_simulator import Simulator
ESC=re.compile(r'\x1b\[[0-9;]*m')
class AppSim(Simulator):
 def focus(self):
  lines=ESC.sub('',self.log_path.read_text()).splitlines()
  return next((line.split('Motion focus ',1)[1] for line in reversed(lines) if 'Motion focus ' in line),'')
 def key(self,name):
  assert name in ('UP','DOWN','ENTER');self.command('KEY_DOWN '+name);time.sleep(.10);self.command('KEY_UP '+name);time.sleep(.20)
 def expect(self,label):
  for _ in range(40):
   if self.focus().startswith(label):return
   assert self.process.poll() is None
   time.sleep(.05)
  raise AssertionError((label,self.focus(),self.log_path.read_text()[-1200:]))
 def choose(self,label):
  for _ in range(30):
   if self.focus().startswith(label):self.key('ENTER');return
   self.key('DOWN')
  raise AssertionError((label,self.focus()))
 def wait_log(self,label,count=1):
  for _ in range(120):
   if self.log_path.read_text().count(label)>=count:return
   assert self.process.poll() is None
   time.sleep(.05)
  raise AssertionError((label,self.log_path.read_text()[-1200:]))
def motion(path,values):
 temp=path.with_suffix('.tmp');temp.write_text(' '.join(map(str,values)));temp.replace(path)
def fnv(data):
 n=2166136261
 for b in data:n=((n^b)*16777619)&0xffffffff
 return n
def profile(sim,magic,size):
 for _ in range(60):
  found=[]
  for path in sim.state.rglob('.pxa-kv-*'):
   data=path.read_bytes();i=data.find(magic)
   if i>=0 and len(data)>=i+size:
    value=data[i:i+size]
    if fnv(value[:-4])==struct.unpack_from('<I',value,size-4)[0]:found.append((struct.unpack_from('<Q',data,8)[0],path,i,value))
  if found:return max(found,key=lambda item:item[0])
  time.sleep(.05)
 raise AssertionError(('No saved profile',magic))
def restart(sim,fixture=None):
 args=[x.decode() for x in Path(f'/proc/{sim.process.pid}/cmdline').read_bytes().split(b'\0') if x]
 sim.process.terminate();sim.process.wait(timeout=3);Path(sim.socket).unlink(missing_ok=True)
 if fixture:
  generation,path,index,value=profile(sim,b'1ZAM',112);data=bytearray(path.read_bytes());data[index:index+112]=fixture;struct.pack_into('<Q',data,8,generation+10);struct.pack_into('<I',data,20,zlib.crc32(data[24:]));path.write_bytes(data)
 sim.process=subprocess.Popen(args,stdout=sim.log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
 for _ in range(100):
  if Path(sim.socket).exists():time.sleep(.5);return
  assert sim.process.poll() is None
  time.sleep(.05)
 raise AssertionError('Restart timed out')
def assert_frames(sim):assert 'Motion frame rejected' not in sim.log_path.read_text(),sim.log_path.read_text()[-1500:]
def dice_flow(s,path,w,h):
 s.expect('摇一摇');s.screenshot('direct-launch')
 s.key('ENTER');s.wait_log('Dice complete n=1');s.expect('摇一摇')
 first=profile(s,b'1CID',96)[3];assert first[7]==1 and first[12]==2
 s.choose('2颗');s.expect('3颗')
 for _ in range(3):s.key('ENTER')
 s.expect('6颗');pending=profile(s,b'1CID',96)[3];assert pending[4]==6 and pending[6]==0 and pending[12:19]==first[12:19]
 s.screenshot('six-preview');s.key('UP');s.key('ENTER');time.sleep(.3);s.screenshot('six-rolling');s.wait_log('Dice complete n=2');s.expect('摇一摇')
 saved=profile(s,b'1CID',96)[3];assert saved[12]==6 and all(1<=v<=6 for v in saved[13:19])
 motion(path,[22000000,0,9806650,-2147483648,100000,0]);time.sleep(.2);motion(path,[-22000000,0,9806650,400000,0,100000]);s.wait_log('Dice complete n=3');motion(path,[0,0,9806650,0,0,0]);s.expect('摇一摇')
 s.key('ENTER');s.wait_log('Dice complete n=4');time.sleep(.6);s.screenshot('six-result')
 s.choose('更多');s.expect('继续摇一摇');s.choose('查看历史记录');s.screenshot('history-first')
 capacity=1 if h<200 else 2 if h<260 else 3
 if 4>capacity:s.choose('下一页');s.screenshot('history-next')
 s.choose('返回');s.expect('摇一摇');s.choose('更多');s.choose('摇动：已开启');s.expect('摇动：已关闭');s.choose('继续摇一摇');s.expect('摇一摇')
 stable=s.log_path.read_text().count('Dice begin');motion(path,[22000000,0,9806650,0,0,0]);time.sleep(.2);motion(path,[-22000000,0,9806650,0,0,0]);time.sleep(.4);assert s.log_path.read_text().count('Dice begin')==stable
 motion(path,[0,0,9806650,0,0,0]);before=s.screenshot('saved-result');data=profile(s,b'1CID',96)[3];assert data[5]==0 and data[7]==4
 restart(s);s.expect('摇一摇');after=s.screenshot('restored-result');assert before.read_bytes()==after.read_bytes(),'Dice result or selected count changed after restart'
 assert profile(s,b'1CID',96)[3]==data;assert_frames(s);print(f'Dice {w}x{h}: direct launch, six 3D dice, shake/cooldown, history paging, disabled motion and independent restart passed',flush=True)
def win_fixture(s,w,h,level):
 data=bytearray(profile(s,b'1ZAM',112)[3]);data[4]=level;data[5]=level;data[9]=1;data[12]=1;data[13]=level
 tier=min(2,level//4);rw=(9 if tier==0 else 13 if tier==1 else 15) if w>h else 7+2*tier;rh=(9 if tier==2 else 7) if w>h else 11+2*tier if h>w*3/2 else 7+2*tier
 data[14]=rw;data[15]=rh;data[16]=7;struct.pack_into('<I',data,20,(1<<level)-1);struct.pack_into('<I',data,24,6000);struct.pack_into('<ff',data,28,rw-1.5,rh-1.5)
 struct.pack_into('<I',data,108,fnv(data[:108]));return data
def maze_flow(s,path,w,h):
 s.expect('开始');s.screenshot('home');s.choose('选关');s.expect('01');s.screenshot('levels-locked');s.choose('返回')
 s.choose('设置');s.choose('控制：');s.expect('控制：按键转向');s.choose('灵敏度：');s.choose('方向旋转：');s.choose('左右镜像：');s.screenshot('settings');s.choose('返回首页');s.choose('开始');s.expect('暂停');time.sleep(.8);s.key('ENTER');s.expect('继续冒险');s.choose('返回首页');s.expect('继续')
 data=profile(s,b'1ZAM',112)[3];assert data[9]==1 and data[8]==1 and data[11]==1 and data[12]==1 and struct.unpack_from('<I',data,24)[0]>0
 before=s.screenshot('saved-home');restart(s);s.expect('继续');after=s.screenshot('restored-home');assert before.read_bytes()==after.read_bytes(),'Maze bookmark did not restore exactly'
 s.key('ENTER');s.key('ENTER');s.choose('重新校准体感');s.wait_log('Marble calibrated');s.screenshot('play');path.write_text('STALE');time.sleep(.6);a=s.screenshot('stale');time.sleep(.45);b=s.screenshot('stale-still');assert a.read_bytes()==b.read_bytes(),'Missing sensor data advanced physics or timer'
 motion(path,[0,0,9806650,0,0,0]);time.sleep(.25);s.key('ENTER');s.choose('路线提示：');s.expect('暂停');s.screenshot('hinted-route');s.key('ENTER');s.choose('返回首页')
 scale=2 if min(w,h)>=400 else 1;rw,rh=w//scale,h//scale
 fixture=win_fixture(s,rw,rh,0);restart(s,fixture);s.expect('继续');s.key('ENTER');s.wait_log('Marble won level=1');s.expect('下一关');s.screenshot('win-first');s.key('ENTER');s.wait_log('Marble start level=2');s.key('ENTER');s.choose('返回首页');s.choose('选关');s.expect('02');s.screenshot('levels-unlocked');s.choose('返回')
 fixture=win_fixture(s,rw,rh,11);restart(s,fixture);s.expect('继续');s.key('ENTER');s.wait_log('Marble won level=12');s.expect('再选一关');s.screenshot('win-final');s.key('ENTER');s.expect('12');s.choose('返回')
 assert_frames(s);print(f'Maze {w}x{h}: locks, three-key settings, exact bookmark, calibration/stale freeze, hints, first/final wins, best times and next-level navigation passed',flush=True)
def touch_flow(s,app,w,h):
 s.expect('摇一摇' if app=='shake-dice' else '开始');scale=2 if min(w,h)>=400 else 1;rw,rh=w//scale,h//scale
 if app=='shake-dice':
  s.tap(w/2,h/2);s.wait_log('Dice complete n=1');s.screenshot('touch-result');s.tap((rw-35)*scale,(rh-25)*scale);s.expect('继续摇一摇');s.screenshot('touch-menu')
 else:
  s.tap(35*scale,(rh-25)*scale);s.wait_log('Marble calibrated');x,y=w//2,h//2;s.command(f'TOUCH DOWN 0 {x} {y}');s.command(f'TOUCH MOVE 0 {x+30*scale} {y}');time.sleep(.4)
  s.command(f'TOUCH DOWN 1 {w-30} {15*scale}');s.command(f'TOUCH UP 1 {w-30} {15*scale}');time.sleep(.2);s.expect('暂停');s.screenshot('touch-owner');s.command(f'TOUCH CANCEL 0 {x} {y}');s.tap((rw-30)*scale,17*scale);s.expect('继续冒险');s.screenshot('touch-pause')
 assert_frames(s);print(f'{app} {w}x{h}: touch hit boxes and gesture ownership passed',flush=True)
def exit_flow(s,app,w,h):
 s.expect('摇一摇' if app=='shake-dice' else '开始');s.choose('更多' if app=='shake-dice' else '设置')
 for _ in range(24):
  if s.focus()=='退出游戏':break
  s.key('DOWN')
 else:raise AssertionError(('Exit focus unavailable',s.focus()))
 s.command('KEY_DOWN ENTER');s.wait_log('Motion exiting');assert_frames(s)
 print(f'{app} {w}x{h}: explicit three-key Exit reached system close',flush=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('workspace','package-root','executable','output'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--apps',nargs='*',default=['shake-dice','marble-maze']);p.add_argument('--sizes',nargs='*',default=['240x284','160x160','320x240','800x480','480x800']);p.add_argument('--touch',action='store_true');p.add_argument('--exit',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 for size in a.sizes:
  w,h=map(int,size.split('x'));path=a.output.resolve()/('motion-'+size+'.txt');motion(path,[0,0,9806650,0,0,0]);os.environ['GRAVITY_TEST_MOTION']=str(path)
  for app in a.apps:
   motion(path,[0,0,9806650,0,0,0])
   with AppSim(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),app,w,h,'exit' if a.exit else 'touch' if a.touch else 'keys',executable=a.executable.resolve()) as s:
    if a.exit:exit_flow(s,app,w,h)
    elif a.touch:touch_flow(s,app,w,h)
    elif app=='shake-dice':dice_flow(s,path,w,h)
    else:maze_flow(s,path,w,h)
if __name__=='__main__':main()
