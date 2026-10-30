#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Signed level-meter and Gen4 Up/Down/Confirm validation; never injects saves."""
import argparse,copy,json,math,os,re,struct,sys,time
from pathlib import Path
from PIL import ImageChops
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'common/pocket-studio/tests'))
from simulator import Session,command,save_blob,check
from three_keys import ThreeKeys
APP='spirit-level'
def pose(path,x=0,y=0):
    sx,sy=math.sin(math.radians(x)),math.sin(math.radians(y));v=(sy,-sx,math.sqrt(max(0,1-sx*sx-sy*sy)))
    temp=path.with_suffix('.tmp');temp.write_text(' '.join(map(str,[*(round(t*9806650) for t in v),0,0,0])));temp.replace(path)
def wait_log(s,text,count=1):
    end=time.monotonic()+12
    while time.monotonic()<end:
        check(s.proc.poll() is None,s.logs()[-1600:])
        if s.logs().count(text)>=count:time.sleep(.12);return
        time.sleep(.05)
    raise AssertionError(text+'\n'+s.logs()[-2000:])
def bubble(s,im):
    cx=s.cw/2 if s.ch>=s.cw else (s.cw-124)/2;cy=(s.ch-52)/2 if s.ch>=s.cw else 116;points=[]
    for y in range(max(0,int(s.vy+(cy-43)*s.vh/s.ch)),min(s.h,int(s.vy+(cy+43)*s.vh/s.ch))):
        for x in range(max(0,int(s.vx+(cx-43)*s.vw/s.cw)),min(s.w,int(s.vx+(cx+43)*s.vw/s.cw))):
            r,g,b=im.getpixel((x,y));lx=(x-s.vx)*s.cw/s.vw;ly=(y-s.vy)*s.ch/s.vh
            if (lx-cx)**2+(ly-cy)**2<43**2 and g>205 and b>135:points.append((lx,ly))
    check(len(points)>20,'no bright measurement bubble');return tuple(sum(p[i] for p in points)/len(points) for i in (0,1))
def snapshot(s):time.sleep(.28);return save_blob(s.state,APP)
def sensor_case(a,w,h,radius,report):
    pose(a.motion_file)
    with Session(a,APP,w,h,radius) as s:
        k=ThreeKeys(s);wait_log(s,'Level gravity ready');time.sleep(.6);home=s.shot('01-level');origin=bubble(s,home)
        pose(a.motion_file,2.4,-1.2);time.sleep(1.6);tilted=s.shot('02-tilted');centroid=bubble(s,tilted);check(centroid[0]>origin[0]+12,'positive X tilt did not move the bubble')
        check(centroid[1]>origin[1]+5,'negative measured Y must project downward')
        pose(a.motion_file,0,2.4);time.sleep(1.6);up=s.shot('02-positive-y');point=bubble(s,up)
        check(abs(point[0]-origin[0])<5 and point[1]<origin[1]-12,'positive measured Y must project upward')
        holds=s.logs().count('Level: hold');k.choose(0);time.sleep(.4);locked=s.shot('03-held');pose(a.motion_file,8,4);time.sleep(1.1);held=s.shot('04-held-after-move')
        check(ImageChops.difference(locked,held).getbbox() is None,'held measurement changed with motion');check(s.logs().count('Level: hold')==holds+1,'confirm repeated an action')
        k.choose(0);k.choose(1);time.sleep(.5);relative=s.shot('05-relative');center=bubble(s,relative);check(abs(center[0]-origin[0])<5 and abs(center[1]-origin[1])<5,'relative zero did not center the bubble');k.choose(1)
        k.choose(2);pose(a.motion_file,89.9);time.sleep(2.4);s.shot('06-vertical');k.choose(9);check(snapshot(s)[9]==1,'edge axis unreachable');k.choose(9);k.choose(2)
        pose(a.motion_file,2);time.sleep(1.3);k.choose(3);s.shot('07-settings');k.choose(20);k.choose(21);k.choose(22);check(snapshot(s)[6:9]==bytes([1,2,1]),'three-key preferences failed')
        k.choose(23);s.shot('08-calibration');done=s.logs().count('Level: alignment captured');k.choose(30);wait_log(s,'Level: alignment captured',done+1);s.shot('09-calibration-ready');k.choose(31);check(snapshot(s)[10]==0,'cancel committed an alignment')
        k.choose(23);done=s.logs().count('Level: alignment captured');k.choose(30);wait_log(s,'Level: alignment captured',done+1);k.choose(30);saved=snapshot(s);check(saved[10]==1,'alignment was not saved')
        raw=struct.unpack_from('<iii',saved,12);check(abs(raw[1]/1073741824+math.sin(math.radians(2)))<.001,'saved the wrong physical reference')
        k.choose(25);time.sleep(.5);calibrated=s.shot('10-calibrated');center=bubble(s,calibrated);check(abs(center[0]-origin[0])<5 and abs(center[1]-origin[1])<5,'saved alignment did not level the reference plane')
        k.choose(3);k.choose(24);s.shot('11-guide-first');k.choose(52);s.shot('12-guide-second');k.choose(50);k.choose(25)
        # The shared menu is reached and operated without Start, Back or touch.
        k.menu();s.shot('13-menu');k.key('UP');k.key('CONFIRM');k.key('DOWN');k.key('CONFIRM');wait_log(s,'Pocket: language en');time.sleep(.8);s.shot('14-english')
        a.motion_file.unlink();time.sleep(.6);s.shot('15-signal-lost');k.choose(0);check(s.logs().count('Level: hold')==holds+1,'stale data was lockable')
        pose(a.motion_file,2);time.sleep(.9);k.close();durable=save_blob(s.state,APP)
        check('Pocket: audio' not in s.logs(),'silent utility requested audio')
    with Session(a,APP,w,h,radius) as s:
        k=ThreeKeys(s);wait_log(s,'Level gravity ready');time.sleep(.6);s.shot('16-restored');after=snapshot(s);check(after==durable,'restart lost measurement preferences or alignment')
        check('Level: hold' not in s.logs() and 'Level: relative' not in s.logs(),'restart restored transient measurement states')
        k.choose(3);k.choose(23);k.choose(32);s.shot('17-clear-confirm');k.key('CONFIRM');check(snapshot(s)[10]==1,'safe default cleared alignment')
        k.choose(32);k.choose(41);cleared=snapshot(s);check(cleared[10]==0 and struct.unpack_from('<iii',cleared,12)==(0,0,1073741824),'clear alignment failed');k.choose(25);k.close()
    record={'width':w,'height':h,'radius':radius,'logical':[s.cw,s.ch],'three_keys_only':True,'absolute_tilt':True,'hold':True,'relative':True,'vertical':True,'alignment_save_cancel_clear':True,'restart':True,'disconnect':True,'bilingual':True,'passed':True};report.append(record);print(json.dumps(record),flush=True)
def no_sensor_case(a,report):
    plain=copy.copy(a);plain.simulator=a.standard_simulator;plain.output=a.output/'no-sensor'
    with Session(plain,APP,240,284) as s:
        k=ThreeKeys(s);time.sleep(.4);s.shot('01-no-sensor');k.choose(0);k.choose(1);check('Level: hold' not in s.logs() and 'Level: relative' not in s.logs(),'invented measurement without a sensor');k.choose(3);k.choose(23);k.choose(30);check('Level: alignment capture' not in s.logs(),'calibration started without a sensor');k.choose(31);k.choose(25);k.close()
    report.append({'width':240,'height':284,'three_keys_only':True,'no_fabricated_readings':True,'passed':True})
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for flag in ['simulator','standard-simulator','packages','publisher-key','output']:p.add_argument('--'+flag,type=Path,required=True)
    a=p.parse_args()
    for key,value in vars(a).items():setattr(a,key,value.resolve())
    check('local/artifacts/' in str(a.output),'use ignored local artifacts');a.output.mkdir(parents=True,exist_ok=True);a.motion_file=a.output/'imu.txt';os.environ['GRAVITY_TEST_MOTION']=str(a.motion_file)
    report={'measurement_cases':[],'no_sensor_cases':[]}
    try:
        for w,h,r in [(480,320,0),(240,284,0),(296,240,58)]:sensor_case(a,w,h,r,report['measurement_cases'])
        no_sensor_case(a,report['no_sensor_cases'])
    finally:(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
