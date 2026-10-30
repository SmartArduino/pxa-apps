#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Run real signed packages against a public file-driven IMU provider; no save injection."""
import argparse,copy,json,os,struct,sys,time
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'common/pocket-studio/tests'))
from simulator import Session,check,command,save_blob
APP='marble-islands'
FLAT=(0,0,9806650,0,0,0)
def motion(path,values):
    temp=path.with_suffix('.tmp');temp.write_text(' '.join(map(str,values)));temp.replace(path)
def wait_log(s,text,count=1):
    end=time.monotonic()+10
    while time.monotonic()<end:
        check(s.proc.poll() is None,s.logs())
        if s.logs().count(text)>=count:
            time.sleep(.12);return
        time.sleep(.05)
    raise AssertionError(s.logs()[-2500:])
def pos(blob):return tuple(v/65536 for v in struct.unpack_from('<ii',blob,56))
def pause(s):
    s.tap(s.cw-36,10,.35);return save_blob(s.state,APP)
def resume(s,expect_calibration=True):
    n=s.logs().count('Marble motion calibrated');s.tap(s.cw/2,(s.ch-180)//2+53 if s.ch>224 else 85,.1)
    if expect_calibration:wait_log(s,'Marble motion calibrated',n+1)
def close(s):
    s.tap(s.cw-36,10,.3);s.pointer('DOWN',s.cw/2,(s.ch-180)//2+113 if s.ch>224 else 145);s.proc.wait(timeout=4);check(s.proc.returncode==0,'clean exit')
def sensor_case(args,w,h,radius,report):
    motion(args.motion_file,FLAT)
    with Session(args,APP,w,h,radius) as s:
        s.key('Z');wait_log(s,'Marble motion connected acc=1 gyro=1');wait_log(s,'Marble motion calibrated');s.shot('01-gyro-ready')
        before=pause(s);resume(s)
        # Isolate angular velocity: acceleration stays level, so only the
        # gyroscope's between-frame attitude prediction can cause this move.
        motion(args.motion_file,(0,0,9806650,-1000000,0,0));time.sleep(.45)
        motion(args.motion_file,FLAT);command(s.sock,'KEY_DOWN Z');time.sleep(.2);command(s.sock,'KEY_UP Z')
        gyro=pause(s);check(pos(gyro)[0]>pos(before)[0]+.01,'gyroscope samples did not reach physics')
        before=gyro;resume(s)
        motion(args.motion_file,(0,-3500000,9150000,0,0,0));time.sleep(.8)
        motion(args.motion_file,FLAT);command(s.sock,'KEY_DOWN Z');time.sleep(.3);command(s.sock,'KEY_UP Z');s.shot('02-tilted')
        after=pause(s);check(pos(after)[0]>pos(before)[0]+.03,'accelerometer did not move ball toward screen right')
        resume(s);args.motion_file.unlink();time.sleep(.6);s.shot('03-signal-lost');stale=pause(s)
        resume(s,False);time.sleep(.65);frozen=pause(s)
        check(frozen[48:52]==stale[48:52] and pos(frozen)==pos(stale),'unavailable motion advanced physics/time')
        resume(s,False);command(s.sock,'KEY_DOWN RIGHT');time.sleep(.3);command(s.sock,'KEY_UP RIGHT');time.sleep(.1);manual=pause(s)
        check(struct.unpack_from('<I',manual,48)[0]>struct.unpack_from('<I',frozen,48)[0],'manual takeover did not work during signal loss')
        motion(args.motion_file,FLAT);resume(s);s.key('S')
        calibrations=s.logs().count('Marble motion calibrated');s.tap(120,51+(s.ch-70)//7*1+((s.ch-70)//7-2)//2,.1);wait_log(s,'Marble motion calibrated',calibrations+1);s.shot('04-controls-zh')
        settings_before=save_blob(s.state,APP)[93:97]
        s.pointer('DOWN',120,60);s.pointer('CANCEL',120,60);time.sleep(.25)
        check(save_blob(s.state,APP)[93:97]==settings_before,'cancel changed motion settings')
        s.tap(120,51+(s.ch-70)//7*2+((s.ch-70)//7-2)//2);s.tap(120,51+(s.ch-70)//7*3+((s.ch-70)//7-2)//2);s.tap(120,51+(s.ch-70)//7*4+((s.ch-70)//7-2)//2);time.sleep(.25)
        tuned=save_blob(s.state,APP);check(tuned[93:97]==bytes([1,2,1,1]),'sensitivity/direction/mirror not saved')
        s.tap(s.cw-36,10);s.tap(s.cw/2,(s.ch-180)//2+143 if s.ch>224 else 175);s.tap(s.cw/2,(s.ch-180)//2+53 if s.ch>224 else 85);s.shot('05-controls-en')
        s.tap(120,60,.3);s.tap(120,51+(s.ch-70)//7*6+((s.ch-70)//7-2)//2);time.sleep(.1);s.shot('06-touch-fallback');close(s)
        old=save_blob(s.state,APP);check(len(old)==104 and old[93:97]==bytes([0,2,1,1]),'motion options not durable')
    with Session(args,APP,w,h,radius) as s:
        wait_log(s,'Pocket: load completed');s.shot('07-restored');close(s)
        restored=save_blob(s.state,APP);check(restored[93:97]==old[93:97],'relaunch lost control settings')
        check('Marble motion connected' not in s.logs(),'disabled motion reopened sensors')
    row={'width':w,'height':h,'radius':radius,'gyro_and_acceleration':True,'calibration':True,'gyroscope_moves_ball':True,'tilt_moves_ball':True,'signal_loss_freezes':True,'manual_takeover':True,'options_restart':True,'passed':True}
    report.append(row);print(json.dumps(row),flush=True)
def upgrade(args):
    old=copy.copy(args);old.packages=args.previous_packages;old.simulator=args.standard_simulator;old.output=args.output/'upgrade'
    with Session(old,APP,480,320) as s:
        s.key('Z');s.key('UP');command(s.sock,'KEY_DOWN Z');time.sleep(.4);command(s.sock,'KEY_UP Z');close(s);before=save_blob(s.state,APP);check(len(before)==88,'expected old v1 save')
    new=copy.copy(old);new.packages=args.packages
    with Session(new,APP,480,320) as s:
        wait_log(s,'Pocket: load completed');s.shot('v1-restored');close(s);after=save_blob(s.state,APP)
    check(len(after)==104 and before[:48]==after[:48] and before[52:54]==after[52:54],'old island progress changed')
    check(all(abs(x-y)<.04 for x,y in zip(pos(before),pos(after))),'old ball position changed')
    check(after[93:97]==bytes([1,1,0,0]),'old save did not receive default motion options')
    return {'old_bytes':88,'new_bytes':104,'progress_preserved':True,'passed':True}
def no_sensor(a,w,h,r,report):
    with Session(a,APP,w,h,r) as s:
        s.key('Z');command(s.sock,'KEY_DOWN RIGHT');time.sleep(.25);command(s.sock,'KEY_UP RIGHT')
        s.shot('01-manual');before=pause(s);check(struct.unpack_from('<I',before,48)[0]>0,'manual clock')
        s.key('Z');s.pointer('DOWN',s.cw/2,100);s.pointer('MOVE',s.cw/2+10,100);time.sleep(.2);s.pointer('CANCEL',s.cw/2+10,100);s.shot('02-touch');close(s)
    report.append({'display':[w,h],'radius':r,'manual_and_touch':True,'passed':True})
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for flag in ['simulator','standard-simulator','packages','previous-packages','publisher-key','output']:p.add_argument('--'+flag,type=Path,required=True)
    a=p.parse_args()
    for k,v in vars(a).items():setattr(a,k,v.resolve())
    check('local/artifacts/' in str(a.output),'use an ignored artifact directory');a.output.mkdir(parents=True,exist_ok=True)
    a.motion_file=a.output/'imu-values.txt';os.environ['GRAVITY_TEST_MOTION']=str(a.motion_file)
    report={'motion_cases':[],'no_sensor_cases':[]}
    try:
        for w,h,r in [(480,320,0),(320,240,0),(296,240,58)]:sensor_case(a,w,h,r,report['motion_cases'])
        plain=copy.copy(a);plain.simulator=a.standard_simulator;plain.output=a.output/'no-sensor'
        for w,h,r in [(480,320,0),(320,240,0),(296,240,58)]:no_sensor(plain,w,h,r,report['no_sensor_cases'])
        report['upgrade']=upgrade(a)
    finally:(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
