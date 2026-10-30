#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Signed packages: edge-to-edge surfaces, reflow, actual touch mapping and keys."""
import argparse,json,os,re,sys,time
from pathlib import Path
sys.dont_write_bytecode=True
from simulator import Session,check,command,save_blob
from three_keys import ThreeKeys

def focus(k,screen):check(k.focus()[1]==screen,('wrong page',k.focus(),screen))
def wait(s,text,count=1):
    deadline=time.monotonic()+12
    while time.monotonic()<deadline:
        check(s.proc.poll() is None,s.logs()[-1500:])
        if s.logs().count(text)>=count:time.sleep(.15);return
        time.sleep(.05)
    raise AssertionError(text+'\n'+s.logs()[-2000:])
def menu(s):s.tap(s.cw-36,12,.3)
def case(a,name,w,h,r,report):
    with Session(a,name,w,h,r) as s:
        level=name=='spirit-level';k=ThreeKeys(s,'Level' if level else 'Marble',6 if level else 8)
        full=re.findall(r'fullscreen origin=(\d+),(\d+) size=(\d+)x(\d+) display=(\d+)x(\d+)',s.logs())
        check(full and tuple(map(int,full[-1]))==(0,0,w,h,w,h),'surface left a letterbox')
        if level:
            wait(s,'Level gravity ready');time.sleep(.5);s.shot('01-main')
            # Coordinates are logical control centers, mapped by the same Session.
            s.tap(s.cw-44,s.ch-15);time.sleep(.25);check(save_blob(s.state,name)[5]==1,'edge touch mapping');s.shot('02-edge')
            s.tap(s.cw-44,s.ch-15);s.tap(s.cw-78,13);focus(k,1);s.shot('03-settings');k.choose(25)
            third=(s.cw-48)/3;s.tap(12+third/2,s.ch-15);wait(s,'Level: hold');s.shot('04-held');k.choose(0)
        else:
            s.key('Z');wait(s,'Marble motion calibrated');s.shot('01-main')
            s.tap(round(s.cw*225/288),s.ch-21);focus(k,3);s.shot('02-settings');k.choose(26)
            s.tap(round(s.cw*43/288),s.ch-21);focus(k,2);s.shot('03-islands')
            s.tap(8+(s.cw-24)/4,43+(s.ch-84)//8);focus(k,1)
            # A held/cancelled brake touch must release without becoming a menu action.
            s.pointer('DOWN',s.cw-35,s.ch-54);time.sleep(.1);s.pointer('CANCEL',s.cw-35,s.ch-54)
        menu(s);wait(s,'Pocket: menu opened');s.shot('05-menu');k.key('UP');k.key('CONFIRM');k.key('DOWN');k.key('CONFIRM');wait(s,'Pocket: language en');time.sleep(.8);s.shot('06-english')
        menu(s);k.key('DOWN');k.key('DOWN');command(s.sock,'KEY_DOWN Z');s.proc.wait(timeout=5);check(s.proc.returncode==0,'save/exit')
        check(len(save_blob(s.state,name))==(64 if level else 104),'durable preferences/game')
        report.append({'app':name,'display':[w,h],'corner_radius':r,'canvas':[s.cw,s.ch],'safe_content':[s.vx,s.vy,s.vw,s.vh],'fullscreen_surface':[0,0,w,h],'touch_and_keys':True,'bilingual':True,'clean_exit':True,'passed':True});print(json.dumps(report[-1]),flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['simulator','packages','publisher-key','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument("--apps",nargs="+",choices=["spirit-level","marble-islands"],default=["spirit-level","marble-islands"])
    a=p.parse_args()
    for key,value in vars(a).items():
        if isinstance(value,Path):setattr(a,key,value.resolve())
    check('local/artifacts/' in str(a.output),'use ignored local artifacts');a.output.mkdir(parents=True,exist_ok=True)
    imu=a.output/'imu.txt';imu.write_text('0 0 9806650 0 0 0');os.environ['GRAVITY_TEST_MOTION']=str(imu)
    report=[]
    try:
        for name in a.apps:
            for w,h,r in [(240,284,0),(480,320,0),(240,320,0),(320,480,0),(240,240,0),(296,240,58)]:case(a,name,w,h,r,report)
    finally:(a.output/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
