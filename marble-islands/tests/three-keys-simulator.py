#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Exercise signed Marble Islands with Gen4 Up/Down/Confirm and public IMU only."""
import argparse,copy,json,os,struct,sys,time
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'common/pocket-studio/tests'))
from simulator import Session,command,save_blob,check
from three_keys import ThreeKeys
APP='marble-islands'
def motion(path,values=(0,0,9806650,0,0,0)):
    temp=path.with_suffix('.tmp');temp.write_text(' '.join(map(str,values)));temp.replace(path)
def wait_log(s,text,count=1):
    end=time.monotonic()+12
    while time.monotonic()<end:
        check(s.proc.poll() is None,s.logs()[-2000:])
        if s.logs().count(text)>=count:time.sleep(.12);return
        time.sleep(.05)
    raise AssertionError(text+'\n'+s.logs()[-2000:])
def position(data):return tuple(v/65536 for v in struct.unpack_from('<ii',data,56))
def snap(s):time.sleep(.3);return save_blob(s.state,APP)
def resume(k):
    n=k.s.logs().count('Marble motion calibrated');k.key('CONFIRM');wait_log(k.s,'Marble motion calibrated',n+1)
def pause(k):k.menu();return snap(k.s)
def sensor_case(a,w,h,report):
    motion(a.motion_file)
    with Session(a,APP,w,h) as s:
        k=ThreeKeys(s,'Marble',8);s.shot('01-intro');k.choose(0);wait_log(s,'Marble motion calibrated');s.shot('02-ready')
        before=pause(k);resume(k)
        # Holding volume keys traverses the UI but must never steer the ball.
        k.key('DOWN',.6);k.key('UP',.6);after=pause(k);check(position(after)==position(before),'Gen4 focus keys steered the ball');resume(k)
        # A held confirm is one edge, not a stream of repeated brake toggles.
        on=s.logs().count('Marble: slow brake on');k.choose(7);k.key('CONFIRM',.65);check(s.logs().count('Marble: slow brake on')==on+1,'held confirm repeated brake');k.choose(7);s.shot('03-slow-brake')
        # Tilt still drives the actual physics while the brake remains latched.
        before=pause(k);resume(k);motion(a.motion_file,(0,-3500000,9150000,0,0,0));time.sleep(.65);motion(a.motion_file);time.sleep(.25);moved=pause(k)
        check(position(moved)[0]>position(before)[0]+.025,'three-key mode lost IMU steering');resume(k)
        k.choose(3);s.shot('04-islands');first=snap(s)
        for card in range(11,18):k.choose(card);check(k.focus()[1]==2,'locked card started a run')
        check(position(snap(s))==position(first),'selector advanced physics');s.shot('05-locked-last-island')
        k.choose(1);check(k.focus()[1]==1,'three-key back failed');k.choose(4);check(struct.unpack_from('<I',snap(s),48)[0]<1000,'retry did not reset the clock')
        k.choose(6);wait_log(s,'Marble motion calibrated');k.choose(21);wait_log(s,'Marble motion calibrated',s.logs().count('Marble motion calibrated')+1)
        k.choose(22);k.choose(23);k.choose(24);k.choose(25);tuned=snap(s);check(tuned[92:98]==bytes([2,1,2,1,1,1]),'three-key settings were not saved');s.shot('06-settings')
        k.choose(26);k.choose(3);k.choose(2);s.shot('07-help');k.choose(0)
        # Reach root menu and language without a Start/Back key or any touch.
        k.menu();s.shot('08-menu');k.key('UP');k.key('CONFIRM');k.key('DOWN');k.key('CONFIRM');wait_log(s,'Pocket: language en');wait_log(s,'Marble motion calibrated',s.logs().count('Marble motion calibrated')+1);s.shot('09-english');k.choose(6);s.shot('10-settings-english');k.choose(26)
        a.motion_file.unlink();time.sleep(.7);stale=pause(k);k.key('CONFIRM');time.sleep(.7);frozen=pause(k)
        check(stale[48:68]==frozen[48:68],'lost IMU continued physics/time');k.key('CONFIRM');motion(a.motion_file);wait_log(s,'Marble motion calibrated',s.logs().count('Marble motion calibrated')+1);k.close();saved=save_blob(s.state,APP)
    with Session(a,APP,w,h) as s:
        k=ThreeKeys(s,'Marble',8);wait_log(s,'Marble motion calibrated');check(snap(s)[93:98]==saved[93:98],'restart lost navigation or IMU settings');s.shot('11-restored');k.close()
    report.append({'width':w,'height':h,'only_three_physical_keys':True,'no_accidental_steering':True,'held_confirm_single_action':True,'tilt_moves_physics':True,'all_eight_cards':True,'settings_back_help_language_exit':True,'disconnect_freezes':True,'restart':True,'passed':True});print(json.dumps(report[-1]),flush=True)
def upgrade(a):
    old=copy.copy(a);old.packages=a.previous_packages;old.output=a.output/'upgrade';motion(a.motion_file)
    # The legacy 1.5 package requires its original gamepad shortcuts.
    # Every interaction with the new package below uses only the Gen4 keys.
    with Session(old,APP,240,284) as s:
        s.key('Z');wait_log(s,'Marble motion calibrated');s.key('S');s.key('DOWN');s.key('DOWN');s.key('Z');s.key('X');s.key('ENTER');s.key('DOWN');s.key('DOWN');command(s.sock,'KEY_DOWN Z');s.proc.wait(timeout=5);before=save_blob(s.state,APP)
        check(before[92]==1 and before[94]==2,'expected real 1.5 options')
    new=copy.copy(old);new.packages=a.packages
    with Session(new,APP,240,284) as s:
        k=ThreeKeys(s,'Marble',8);wait_log(s,'Marble motion calibrated');s.shot('12-upgraded');k.close();after=save_blob(s.state,APP)
    check(after[92]==2 and after[93:97]==before[93:97] and after[97]==0,'1.5 settings did not migrate to Auto navigation');check(before[:48]==after[:48] and before[52:54]==after[52:54],'upgrade lost island progress');check(all(abs(x-y)<.01 for x,y in zip(position(before),position(after))),'upgrade moved the ball')
    return {'from':'1.5.0','to':'1.7.0','progress_and_options_preserved':True,'three_key_auto':True,'passed':True}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['simulator','packages','previous-packages','publisher-key','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args()
    for key,value in vars(a).items():setattr(a,key,value.resolve())
    check('local/artifacts/' in str(a.output),'use ignored artifacts');a.output.mkdir(parents=True,exist_ok=True);a.motion_file=a.output/'imu.txt';os.environ['GRAVITY_TEST_MOTION']=str(a.motion_file)
    report={'three_key_cases':[]}
    try:
        for w,h in [(240,284),(240,320)]:sensor_case(a,w,h,report['three_key_cases'])
        report['upgrade']=upgrade(a)
    finally:(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
