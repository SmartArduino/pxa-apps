#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Actual signed simulator input and independent durable LGV1/KV assertions."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import time
import traceback
import zlib

sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[4]
spec=importlib.util.spec_from_file_location('pocket_sim',ROOT/'local/pxa-apps/common/pocket-game/tests/simulator.py')
backend=importlib.util.module_from_spec(spec);spec.loader.exec_module(backend)
ID='pxa-lantern-grove'
def require(ok,message):
    if not ok: raise AssertionError(message)
def payload(raw):
    require(len(raw)==128 and raw[:8]==b'LGV1\1\0\0\0','LGV1 size/header')
    h=2166136261
    for b in raw[:124]: h=((h^b)*16777619)&0xffffffff
    require(struct.unpack_from('<I',raw,124)[0]==h,'LGV1 checksum')
    require(not any(raw[80:124]),'LGV1 reserved')
    s={'resources':list(struct.unpack_from('<3I',raw,8)),'pending':list(struct.unpack_from('<3I',raw,20)),
       'fraction':list(struct.unpack_from('<3I',raw,32)),'levels':list(raw[44:48]),'goals':raw[48],'sound':raw[49],'view':raw[50],'selected':raw[51]}
    for name,at in [('collected',52),('taps',56),('cycle_ms',60)]: s[name]=struct.unpack_from('<I',raw,at)[0]
    s['produced_ms'],s['last_wall']=struct.unpack_from('<QQ',raw,64)
    require(all(v<=999999 for v in s['resources']),'inventory cap')
    return s

def host_kv(raw):
    if len(raw)<26:return None
    magic,version,_,generation,size,crc=struct.unpack_from('<4sHHQII',raw)
    body=raw[24:]
    if magic!=b'PXKV' or version!=1 or len(body)!=size or zlib.crc32(body)!=crc:return None
    at=2;values={}
    for _ in range(struct.unpack_from('<H',body)[0]):
        n=body[at];at+=1;key=body[at:at+n].decode();at+=n
        n=struct.unpack_from('<H',body,at)[0];at+=2;values[key]=body[at:at+n];at+=n
    require(at==len(body),'KV trailing data')
    return generation,values
class Session:
    def __init__(self,sim,report):self.b=sim;self.report=report;self.paused=False;self.index=0
    def check(self,condition,name,**evidence):require(condition,name);self.report['checks'].append({'check':name,**evidence})
    def saved(self):
        found=[]
        for path in self.b.state.glob('**/.pxa-kv-*'):
            try:
                kv=host_kv(path.read_bytes())
                if kv and ID+'.save.v1' in kv[1]:found.append(kv)
            except FileNotFoundError:pass
        require(found,'No durable Lantern KV')
        generation,values=max(found,key=lambda r:r[0]);raw=values[ID+'.save.v1'];state=payload(raw)
        return state
    def wait(self,predicate,label,timeout=12):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            try:
                state=self.saved()
                if predicate(state):return state
            except AssertionError:
                if 'Pocket: saved' in self.b.logs():raise
            time.sleep(.1)
        raise TimeoutError(label)
    def pause(self):
        if not self.paused:self.b.tap(276,10,settle=.3);self.paused=True
    def resume(self):
        if self.paused:self.b.tap(140,85,settle=.3);self.paused=False
    def flush(self,label):
        self.pause();time.sleep(.45);state=self.saved();self.index+=1
        (self.b.args.output/f'{self.b.label}-{self.index:03d}-{label}.json').write_text(json.dumps(state,indent=2)+'\n')
        return state
    def home(self):self.resume();self.b.tap(220,26,settle=.12)
    def exit(self):
        self.pause();self.b.pointer('DOWN',140,145)
        self.b.wait(lambda:self.b.proc.poll() is not None,'menu EXIT')
        require(self.b.proc.returncode==0,'exit flush failed');self.paused=False

def play(args,report):
    state=Path(tempfile.mkdtemp(prefix='state-',dir=args.output));report['state_root']=str(state)
    # UNIX sockets have a short OS path limit; this short directory is under the app artifact root.
    socket_dir=Path('sock');socket_dir.mkdir(exist_ok=True)
    with backend.Simulator(args,state,socket_dir/'one.sock','game',report) as sim:
        s=Session(sim,report);initial=s.wait(lambda v:True,'fresh KV');s.check(initial['levels']==[1,0,0,0] and initial['goals']==0,'fresh_grove')
        sim.shot('initial')
        # Touching the starter upgrade spends the exact displayed 8 L / 4 D.
        sim.tap(140,183,settle=.15);s.wait(lambda v:v['view']==1,'build page')
        sim.tap(140,61,settle=.25);upgraded=s.wait(lambda v:v['levels'][0]==2,'canopy upgrade')
        s.check(upgraded['resources']==[4,4,4],'touch_upgrade_real_debit',resources=upgraded['resources'])
        sim.shot('canopy-two');sim.tap(220,26,settle=.2)
        backed=s.wait(lambda v:v['view']==0,'touch build return');s.check(backed['resources']==upgraded['resources'] and backed['levels']==upgraded['levels'],'build_touch_return_retains_progress')
        time.sleep(3);before=s.flush('automatic');s.check(before['pending'][0]>=5 and before['pending'][1]>=1,'three_resource_automatic_production',pending=before['pending'])
        s.resume();sim.tap(48,183,settle=.2);collected=s.wait(lambda v:v['taps']>=1,'collect')
        s.check(collected['collected']>=sum(before['pending'])+1 and collected['resources'][0]>4,'active_collect_moves_basket_to_inventory')
        # Input cancellation must never activate an action.
        sim.pointer('DOWN',48,183);sim.pointer('CANCEL',48,183)
        sim.pointer('DOWN',48,183);sim.pointer('UP',48,214)
        after=s.flush('cancelled');s.check(after['taps']==collected['taps'],'cancel_and_outside_release_safe')
        # Visual/runtime pause does not consume tending cooldown; automatic income settles once on resume.
        before=after;time.sleep(3.2);s.resume();after=s.flush('pause-income')
        delta=after['produced_ms']-before['produced_ms'];s.check(3000<=delta<=5000,'menu_gap_produces_once_on_resume',delta_ms=delta)
        s.resume();sim.tap(245,201,settle=.2);s.wait(lambda v:v['view']==3,'guide');sim.shot('guide')
        sim.tap(210,198,settle=.2);s.wait(lambda v:v['sound']==0,'sound disabled');sim.tap(210,198,settle=.2);s.wait(lambda v:v['sound']==1,'sound enabled')
        s.check(True,'touch_guide_and_optional_sound_toggle');sim.tap(60,198,settle=.15)
        # Complete all eight milestones with touch alone, without save injection.
        started=time.monotonic();route=[]
        while time.monotonic()-started<540:
            s.home();sim.tap(48,183,settle=.15)
            current=s.saved();g=next((i for i in range(8) if not current['goals']&(1<<i)),8)
            if g==8:break
            sim.tap(230,183,settle=.12);sim.tap(140,64+(g%4)*32,settle=.2)
            current=s.saved();g=next((i for i in range(8) if not current['goals']&(1<<i)),8)
            level=current['levels'];target=None
            if g==1 and level[0]<2:target=0
            if g==2 and level[1]<1:target=1
            if g==4 and level[2]<1:target=2
            if g==5 and level[3]<1:target=3
            if g==6:target=0 if level[0]<4 else 1 if level[1]<2 else None
            if g==7:target=2 if level[2]<3 else 3 if level[3]<2 else None
            if target is not None:
                s.home();sim.tap(140,183,settle=.12);sim.tap(140,61+target*34,settle=.2)
            progress=(current['goals'],tuple(current['levels']))
            if not route or progress!=route[-1]['progress']:
                route.append({'seconds':round(time.monotonic()-started,2),'progress':progress});sim.shot(f'route-{len(route):02d}')
            time.sleep(.65)
        final=s.flush('festival');s.check(final['goals']==255,'all_eight_milestones_reached_by_touch_only',seconds=round(time.monotonic()-started,2),levels=final['levels'],collected=final['collected'],route=route)
        s.resume();s.home();sim.tap(230,183,settle=.2);sim.shot('festival-journal');sim.tap(249,199,settle=.1);sim.shot('journal-next-touch');s.home();sim.shot('grown-woodland')
        s.check(s.wait(lambda v:v['view']==0,'touch journal return')['goals']==255,'journal_touch_next_and_return_retains_progress')
        # The simulator keyboard maps Z/X/A/S to controller A/B/X/Y.
        sim.key('A',settle=.15);s.wait(lambda v:v['view']==1,'controller X build')
        sim.key('X',settle=.15);s.wait(lambda v:v['view']==0,'controller B home')
        sim.key('S',settle=.15);s.wait(lambda v:v['view']==2,'controller Y journal')
        sim.key('RIGHT',settle=.1);sim.key('X',settle=.15);sim.key('TAB',settle=.15);s.wait(lambda v:v['view']==3,'controller SELECT guide')
        sim.key('X',settle=.15);s.wait(lambda v:v['view']==0,'controller home');before_taps=s.saved()['taps'];sim.key('Z',settle=.3)
        s.wait(lambda v:v['taps']>before_taps,'controller A collect');s.check(True,'controller_ABXY_SELECT_arrows_work_through_real_keyboard_mapping')
        # Reset should keep the progress rather than wiping it.
        baseline=s.flush('before-reset');sim.tap(140,115,settle=.3);s.paused=False
        reset=s.flush('after-reset');s.check(all(reset[k]==baseline[k] for k in ('levels','resources','goals','collected')) and reset['view']==0,'reset_retains_every_upgrade_reward_and_inventory')
        s.exit();durable=s.saved();require(1704067200<=durable['last_wall']<4133980800,'Real simulator RTC invalid');report['exit_state']=durable
    # Actual process absence, with no writes or fake clock injection.
    stopped=time.monotonic();time.sleep(4.2)
    with backend.Simulator(args,state,socket_dir/'two.sock','reload',report) as sim:
        s=Session(sim,report);current=s.wait(lambda v:v['produced_ms']>durable['produced_ms'],'offline income')
        difference=current['produced_ms']-durable['produced_ms'];wall_gap=current['last_wall']-durable['last_wall']
        s.check('Pocket: load completed' in sim.logs() and all(current[k]==durable[k] for k in ('levels','resources','goals','collected')),'progress_restores_across_processes')
        s.check(4000<=difference<=10000 and 4<=wall_gap<=10,'real_wall_time_exit_and_relaunch_income_once',production_delta_ms=difference,wall_gap_seconds=wall_gap,actual_absence=round(time.monotonic()-stopped,2))
        sim.shot('restored');s.exit();report['cleanup']={'processes_closed':True,'normal_menu_exit':True}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','publisher-key','output'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args();args.output=args.output.resolve();require(args.output.is_relative_to(ROOT/'local/artifacts'),'Output outside local/artifacts')
    for name in ('simulator','package','publisher_key'):setattr(args,name,getattr(args,name).resolve())
    args.output.mkdir(parents=True,exist_ok=True);os.chdir(args.output)
    report={'passed':False,'game':'lantern-grove','backend':'simulator','checks':[],'processes':[]}
    try:play(args,report);report['passed']=True
    except BaseException:report['error']=traceback.format_exc()
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return 0 if report['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
