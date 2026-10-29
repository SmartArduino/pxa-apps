#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual signed fortune/racer apps: keys, touch, motion, saved state and results."""
from pathlib import Path
import argparse,os,struct,subprocess,time,zlib
from test_apps import AppSim,motion,profile,fnv,assert_frames

def restart(s,magic,size,fixture=None):
 args=[x.decode() for x in Path(f'/proc/{s.process.pid}/cmdline').read_bytes().split(b'\0') if x]
 s.process.terminate();s.process.wait(timeout=3);Path(s.socket).unlink(missing_ok=True)
 if fixture is not None:
  generation,path,index,_=profile(s,magic,size);data=bytearray(path.read_bytes());data[index:index+size]=fixture;struct.pack_into('<Q',data,8,generation+10);struct.pack_into('<I',data,20,zlib.crc32(data[24:]));path.write_bytes(data)
 s.process=subprocess.Popen(args,stdout=s.log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
 for _ in range(100):
  if Path(s.socket).exists():time.sleep(.6);return
  assert s.process.poll() is None
  time.sleep(.05)
 raise AssertionError('Restart timed out')

def hold(s,key,seconds):s.command('KEY_DOWN '+key);time.sleep(seconds);s.command('KEY_UP '+key);time.sleep(.15)
def exit_game(s):
 for _ in range(30):
  if s.focus()=='退出游戏':break
  s.key('DOWN')
 else:raise AssertionError(s.focus())
 s.command('KEY_DOWN ENTER');s.wait_log('Motion exiting')

def fortune_flow(s,path,w,h):
 s.expect('求一签');s.screenshot('home');s.choose('随心');s.expect('学业');s.choose('求一签');time.sleep(.5);s.screenshot('shaking');s.wait_log('Fortune complete n=1');s.expect('再求一签')
 data=profile(s,b'1TRF',64)[3];assert data[4]==1 and data[6]==1 and data[16]<4;s.screenshot('verse');s.choose('解签');s.expect('小行动');s.screenshot('interpretation');s.key('ENTER');s.expect('看签文');s.screenshot('action');s.key('ENTER');s.expect('解签')
 sign=data[16];s.choose('收藏');s.expect('已收藏');assert struct.unpack_from('<I',profile(s,b'1TRF',64)[3],12)[0]&(1<<sign)
 s.choose('返回');s.choose('更多');s.choose('我的收藏');s.expect(f'{sign+1:02d} ·');s.screenshot('favorites');s.key('ENTER');s.expect('再求一签');assert struct.unpack_from('<I',profile(s,b'1TRF',64)[3],8)[0]==1;s.choose('返回');s.choose('返回');s.expect('回到签筒');s.choose('回到签筒');time.sleep(.4)
 motion(path,[22000000,0,9806650,-2147483648,100000,0]);time.sleep(.2);motion(path,[-22000000,0,9806650,400000,0,100000]);s.wait_log('Fortune complete n=2');motion(path,[0,0,9806650,0,0,0]);data=profile(s,b'1TRF',64)[3];assert data[16]!=sign and data[16]<4
 for n in (3,4):s.choose('再求一签');s.wait_log('Fortune complete n='+str(n))
 s.choose('返回');s.choose('更多');s.choose('最近求过的签');s.screenshot('history');s.choose('下一页');s.screenshot('history-next');s.choose('返回');s.choose('摇动：已开启');s.expect('摇动：已关闭');s.choose('回到签筒')
 before=s.log_path.read_text().count('Fortune draw begin');motion(path,[22000000,0,9806650,0,0,0]);time.sleep(.2);motion(path,[-22000000,0,9806650,0,0,0]);time.sleep(.5);assert s.log_path.read_text().count('Fortune draw begin')==before;motion(path,[0,0,9806650,0,0,0]);s.choose('求一签');s.wait_log('Fortune complete n=5');time.sleep(.8)
 saved=profile(s,b'1TRF',64)[3];before=s.screenshot('saved-result');restart(s,b'1TRF',64);s.expect('再求一签');after=s.screenshot('restored-result');assert before.read_bytes()==after.read_bytes();assert profile(s,b'1TRF',64)[3]==saved
 assert 'Fortune text overflow' not in s.log_path.read_text();assert_frames(s);s.choose('返回');s.choose('更多');exit_game(s)
 print(f'Fortune {w}x{h}: themes, three readable pages, shake, favorites, history paging, motion switch, exact restart and exit passed',flush=True)

def racer_fixture(s,track):
 data=bytearray(profile(s,b'1CAR',128)[3]);data[4]=track;data[5]=1;data[31]=1;data[32]=track;distance=(1800+track*400)*2-.3
 for offset,value in zip(range(36,68,4),(0,distance,70,60+track*10,0,0,0,0)):struct.pack_into('<f',data,offset,value)
 struct.pack_into('<III',data,68,63,0,3)
 for i in range(3):struct.pack_into('<f',data,80+4*i,distance-80-i*20)
 struct.pack_into('<I',data,124,fnv(data[:124]));return data

def racer_flow(s,path,w,h):
 s.expect('开始比赛');s.screenshot('home');s.choose('设置');s.choose('控制：');s.expect('控制：按键转向');s.choose('灵敏度：');s.choose('方向：');s.choose('车身：');s.screenshot('settings');s.choose('返回首页');s.choose('开始比赛');s.wait_log('Racer go');hold(s,'DOWN',.6);s.screenshot('road-buttons');s.key('ENTER');s.expect('继续比赛');s.choose('返回首页');s.expect('继续比赛')
 saved=profile(s,b'1CAR',128)[3];assert saved[5]==1 and saved[31]==1 and struct.unpack_from('<f',saved,36)[0]>.1;before=s.screenshot('saved-home');restart(s,b'1CAR',128);s.expect('继续比赛');after=s.screenshot('restored-home');assert before.read_bytes()==after.read_bytes(),'Racer checkpoint preview changed after restart'
 s.choose('设置');s.choose('控制：');s.choose('控制：');s.choose('返回首页');s.choose('继续比赛');s.wait_log('Racer calibrated');s.screenshot('road-motion');motion(path,[2451662,0,9498000,0,0,0]);time.sleep(.8);path.write_text('STALE');s.expect('继续比赛');a=s.screenshot('stale-paused');time.sleep(.3);b=s.screenshot('stale-still');assert a.read_bytes()==b.read_bytes();motion(path,[0,0,9806650,0,0,0]);s.choose('控制：');s.expect('暂停');hold(s,'UP',.5);s.key('ENTER');s.choose('返回首页');s.expect('继续比赛')
 for track in range(3):
  fixture=racer_fixture(s,track);restart(s,b'1CAR',128,fixture);s.expect('继续比赛');s.key('ENTER');s.wait_log(f'Racer finished track={track+1}');s.expect('再跑一次');s.screenshot(f'finish-{track+1}');s.choose('返回首页')
 record=profile(s,b'1CAR',128)[3];assert record[10]==7 and all(struct.unpack_from('<I',record,16+4*i)[0]>0 for i in range(3)) and record[28:31]==b'\1\1\1'
 restart(s,b'1CAR',128);s.expect('开始比赛');s.choose('赛道');s.expect('星光夜城');s.screenshot('three-track-records');s.choose('返回首页');s.choose('设置');assert_frames(s);exit_game(s)
 print(f'Racer {w}x{h}: keys, calibration, stale pause, exact checkpoint, all three finish/result/best-time paths and exit passed',flush=True)

def touch_flow(s,app,w,h):
 scale=2 if min(w,h)>=400 else 1;rw,rh=w//scale,h//scale
 if app=='fortune-sticks':
  s.expect('求一签');s.tap(w/2,h/2);s.wait_log('Fortune complete n=1');s.screenshot('touch-result');s.tap((rw/2)*scale,(rh-20)*scale);s.expect('小行动');s.screenshot('touch-meaning')
 else:
  s.expect('开始比赛');s.tap(36*scale,(rh-20)*scale);s.wait_log('Racer go');x,y=rw*.20*scale,rh*.65*scale;s.command(f'TOUCH DOWN 0 {int(x)} {int(y)}');time.sleep(.6);s.command(f'TOUCH DOWN 1 {int(w-25*scale)} {int(16*scale)}');s.command(f'TOUCH UP 1 {int(w-25*scale)} {int(16*scale)}');time.sleep(.1);s.expect('暂停');s.screenshot('touch-owner');s.command(f'TOUCH CANCEL 0 {int(x)} {int(y)}');s.tap((rw-25)*scale,16*scale);s.expect('继续比赛');s.screenshot('touch-pause')
 assert_frames(s);print(f'{app} {w}x{h}: touch hit boxes, held steering and pointer ownership passed',flush=True)

def all_signs(s):
 s.expect('求一签');s.key('ENTER');s.wait_log('Fortune complete n=1');base=bytearray(profile(s,b'1TRF',64)[3]);base[5]=0
 for sign in range(24):
  data=base.copy();data[16]=sign;struct.pack_into('<I',data,60,fnv(data[:60]));restart(s,b'1TRF',64,data);s.expect('再求一签');s.choose('解签');s.expect('小行动');s.key('ENTER');s.expect('看签文')
  if sign in (0,16,23):s.screenshot(f'action-sign-{sign+1}')
 assert 'Fortune text overflow' not in s.log_path.read_text();assert_frames(s);print('Fortune: all 24 interpretations and actions fit the smallest viewport without hidden text',flush=True)

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--apps',nargs='*',default=['fortune-sticks','mini-racer']);p.add_argument('--sizes',nargs='*',default=['240x284','160x160','320x240','800x480','480x800']);p.add_argument('--touch',action='store_true');p.add_argument('--all-signs',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 for size in a.sizes:
  w,h=map(int,size.split('x'));path=a.output.resolve()/('motion-'+size+'.txt');motion(path,[0,0,9806650,0,0,0]);os.environ['GRAVITY_TEST_MOTION']=str(path)
  for app in a.apps:
   motion(path,[0,0,9806650,0,0,0])
   with AppSim(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),app,w,h,'touch' if a.touch else 'all-signs' if a.all_signs else 'keys',executable=a.executable.resolve()) as s:
    if a.all_signs:assert app=='fortune-sticks';all_signs(s)
    elif a.touch:touch_flow(s,app,w,h)
    elif app=='fortune-sticks':fortune_flow(s,path,w,h)
    else:racer_flow(s,path,w,h)
if __name__=='__main__':main()
