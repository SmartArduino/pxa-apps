#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Real cross-version save migration: play old signed packages, then open new ones."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import time
from simulator import APPS,Session,check,save_blob

def close(s):
    s.tap(252,10,.3);s.pointer('DOWN',140,145);s.proc.wait(timeout=4)
    check(s.proc.returncode==0,'normal save/exit');return save_blob(s.state,s.name)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['simulator','packages','previous-packages','publisher-key','output']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    for key in ['simulator','packages','previous_packages','publisher_key','output']:setattr(a,key,getattr(a,key).resolve())
    check('local/artifacts/' in str(a.output),'use an ignored local artifact destination')
    a.output.mkdir(parents=True,exist_ok=True);report=[];old=copy.copy(a);old.packages=a.previous_packages
    try:
        for name in APPS:
            with Session(old,name,480,320) as s:
                if name=='game-2048':
                    for k in ['LEFT','DOWN','LEFT']:s.key(k)
                elif name=='pocket-sokoban':s.key('RIGHT');s.key('RIGHT');s.key('Z')
                elif name=='marble-islands':s.key('Z');s.key('UP')
                elif name=='lantern-grove':s.tap(48,183);s.tap(140,183);s.tap(140,61);s.tap(220,26)
                else:
                    time.sleep(.7);s.tap(150,117);s.tap(142,180);s.tap(142,180);time.sleep(2.7);s.tap(142,180)
                before=close(s)
            with Session(a,name,480,320) as s:
                check('Pocket: load completed' in s.logs(),'updated package did not load prior save')
                s.shot('upgrade-restored');after=close(s)
            if name in ('game-2048','pocket-sokoban'):check(before==after,'board, history, score or level changed during upgrade')
            elif name=='marble-islands':check(before[:48]==after[:48] and before[52:54]==after[52:54],'island progress, medals or gems changed')
            elif name=='lantern-grove':check(before[8:20]==after[8:20] and before[44:60]==after[44:60],'grove inventory, levels, milestones or lifetime counters changed')
            else:check(before[8:40]==after[8:40] and before[58:74]==after[58:74],'cafe cash, stock, upgrades or sales changed')
            record={'app':name,'passed':True,'previous_sha256':hashlib.sha256(before).hexdigest(),'new_sha256':hashlib.sha256(after).hexdigest()};report.append(record);print(json.dumps(record),flush=True)
    finally:(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
