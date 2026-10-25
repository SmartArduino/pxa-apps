#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Real signed-product simulator inputs and independently decoded durable KV.

No storage writes, save injection or game debug commands. Touch coordinates are
288x224 logical canvas coordinates; the shared simulator transport adds (16,8).
Use --output under local/artifacts and --oracle built from tests/resume.cpp.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import traceback
import zlib
sys.dont_write_bytecode=True
APP=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(APP.parent/'common/pocket-game/tests'))
import simulator as common
require=common.require
SAVE_KEY='pxa-tiny-elements.save.v1'
NAMES=('ERASE','SAND','WATER','STONE','WOOD','FIRE','STEAM','PLANT','SEED','LAVA')


def decode(raw):
    require(len(raw)==1780 and raw[:8]==b'TEL1\x01\x48\x30\x00','Tiny header/version/geometry')
    h=2166136261
    for byte in raw[:-4]:h=((h^byte)*16777619)&0xffffffff
    require(h==struct.unpack_from('<I',raw,1776)[0],'Tiny checksum')
    require(raw[43]<2 and raw[44:48]==bytes(4),'Tiny audio flag/reserved bytes')
    rng,steps,start,burned,cooled=struct.unpack_from('<5I',raw,8)
    accum=struct.unpack_from('<H',raw,28)[0]
    keys=('scene','badges','material','brush','paused','ui','pending','target','cx','cy','selected','page','confirm_yes')
    result=dict(zip(keys,raw[30:43]));result['muted']=raw[43]
    require(rng and accum<50 and result['scene']<5 and result['badges']<16 and result['material']<10 and result['brush']<3 and result['paused']<2 and result['ui']<4 and result['pending']<3 and result['target']<5 and result['cx']<72 and result['cy']<48 and result['selected']<5 and result['page']<2 and result['confirm_yes']<2,'Tiny field ranges')
    require((result['ui']==3)==(result['pending']!=0),'Tiny confirmation invariant')
    cells=[]
    for byte in raw[48:1776]:cells.extend((byte&15,byte>>4))
    require(max(cells)<10 and len(cells)==3456,'Tiny material nibbles')
    result.update(rng=rng,steps=steps,scene_start=start,burned=burned,cooled=cooled,accumulator=accum,cells=cells,
                  counts={name:cells.count(i) for i,name in enumerate(NAMES)},sha256=hashlib.sha256(raw).hexdigest(),world_sha256=hashlib.sha256(bytes(cells)).hexdigest())
    return result


def host_kv(raw):
    if len(raw)<26:return None
    magic,version,_,generation,size,crc=struct.unpack_from('<4sHHQII',raw)
    body=raw[24:]
    if magic!=b'PXKV' or version!=1 or len(body)!=size or zlib.crc32(body)!=crc:return None
    at,values=2,{}
    try:
        for _ in range(struct.unpack_from('<H',body)[0]):
            length=body[at];at+=1;key=body[at:at+length].decode();at+=length
            length=struct.unpack_from('<H',body,at)[0];at+=2
            require(at+length<=len(body),'KV value bounds');values[key]=body[at:at+length];at+=length
        require(at==len(body),'KV trailing bytes')
    except (IndexError,struct.error,UnicodeDecodeError):return None
    if SAVE_KEY not in values:return None
    head=values[SAVE_KEY]
    require(len(head)==24,'PPC1 head length')
    magic,version,bank,total,fnv,commit_generation,reserved=struct.unpack('<4sBBHIQI',head)
    require(magic==b'PPC1' and version==1 and bank<2 and 0<total<=1792 and commit_generation>0 and reserved==0,'PPC1 strict head fields')
    require(all(len(value)<=960 for value in values.values()),'Every actual storage value fits firmware 960B')
    parts=[]
    for index in range((total+895)//896):
        key=SAVE_KEY+f'.b{bank}.c{index}';require(key in values,'PPC1 committed chunk present')
        part=values[key];require(len(part)==min(896,total-index*896),'PPC1 exact chunk length');parts.append(part)
    payload=b''.join(parts);actual=2166136261
    for byte in payload:actual=((actual^byte)*16777619)&0xffffffff
    require(len(payload)==total and actual==fnv,'PPC1 complete payload integrity')
    metadata=dict(commit_bank=bank,commit_generation=commit_generation,head_bytes=len(head),chunk_bytes=[len(part) for part in parts],max_value_bytes=max(map(len,values.values())),kv_value_bytes={key:len(value) for key,value in values.items()})
    return generation,payload,metadata


class Session:
    def __init__(self,backend,report):self.b=backend;self.report=report;self.menu=False;self.index=0
    def check(self,ok,name,**data):require(ok,name);self.report['checks'].append(dict(name=name,**data))
    def pause(self):
        if not self.menu:
            before=self.b.logs().count('Pocket: saved');self.b.tap(276,10,.1);self.menu=True
            self.b.wait(lambda:self.b.logs().count('Pocket: saved')>before,'durable pause save')
    def resume(self):
        if self.menu:self.b.tap(140,85,.08);self.menu=False
    def read(self,label,flush=True):
        if flush:self.pause()
        values=[]
        for path in self.b.state.rglob('.pxa-kv-*'):
            try:
                row=host_kv(path.read_bytes())
                if row:values.append(row)
            except FileNotFoundError:pass
        require(values,'No actual durable Tiny save')
        generation,raw,transport=max(values,key=lambda row:row[0]);state=decode(raw);self.index+=1
        name=f'{self.b.label}-{self.index:03d}-{label}';path=self.b.args.output/(name+'.save');path.write_bytes(raw)
        self.report.setdefault('saves',{})[name]={key:value for key,value in state.items() if key!='cells'}
        self.report['saves'][name].update(generation=generation,path=str(path),transport=transport)
        return state,path
    def shot(self,name):return self.b.shot(name)
    def brush(self,target):
        state,_=self.read('brush-before');self.resume()
        for _ in range((target-state['brush'])%3):self.b.tap(267,76,.03)
    def scene(self,n):
        self.resume();self.b.tap(267,123,.03);self.b.tap(140,55+29*n,.03);self.b.tap(80,180,.06)
        state,_=self.read('scene-'+str(n));self.check(state['scene']==n and state['paused']==(n!=0),'touch_scene_confirmation',scene=n);self.resume();return state
    def stroke(self,x0,y0,x1,y1,hold=0):
        self.b.pointer('DOWN',31+3*x0,41+3*y0);time.sleep(.04)
        if (x0,y0)!=(x1,y1):self.b.pointer('MOVE',31+3*x1,41+3*y1)
        if hold:time.sleep(hold)
        self.b.pointer('UP',31+3*x1,41+3*y1);time.sleep(.04)
    def start(self):self.resume();self.b.tap(267,51,.03)
    def wait_badge(self,bit,timeout=35):
        end=time.monotonic()+timeout;last=None
        while time.monotonic()<end:
            self.resume();time.sleep(2);last,_=self.read('badge-poll')
            if last['badges']&bit:return last
        raise AssertionError(f'Challenge badge {bit} unreachable: {last}')
    def exit(self):
        self.pause();self.b.pointer('DOWN',140,145);self.b.wait(lambda:self.b.proc.poll() is not None,'menu EXIT');require(self.b.proc.returncode==0,'Normal exit')


def exercise(args,report):
    state_root=args.output/'state'
    with common.Simulator(args,state_root,Path('te-one.sock'),'game',report) as b:
        s=Session(b,report);s.shot('01-help');require('Pocket: load completed' not in b.logs(),'Fresh launch storage')
        b.tap(144,195,.06);s.shot('02-garden');b.tap(137,199,.06)
        s.check('Elements: material selected' in b.logs(),'touch_material_emits_confirmation_tone')
        # Real touch clear requires explicit replacement; all following particles are drawn.
        b.tap(267,171,.03);before,_=s.read('clear-confirm');s.check(before['ui']==3,'clear_requires_confirmation');s.resume();b.tap(200,180,.03)
        s.check(s.read('clear-cancel')[0]['counts']==before['counts'],'cancel_clear_preserves_world');s.resume()
        b.tap(267,171,.03);b.tap(80,180,.03);b.tap(94,199,.03);s.brush(0)
        s.stroke(2,2,69,25);a,_=s.read('interpolation');s.check(a['counts']['SAND']==68,'MOVE_interpolates_every_grid_column',sand=a['counts']['SAND']);s.resume();s.start()
        initial=a['counts']['SAND'];s.stroke(36,4,36,4,hold=1.2);a,_=s.read('held-touch');s.check(a['counts']['SAND']>initial+8,'held_touch_continuously_adds_falling_particles',sand=a['counts']['SAND']);s.resume()
        b.pointer('DOWN',139,53);time.sleep(.25);b.pointer('UP',-8,0);s.pause();a,_=s.read('outside-up');s.resume();time.sleep(.8);a2,_=s.read('after-outside-up');s.check(a['counts']['SAND']==a2['counts']['SAND'],'outside_UP_releases_brush');s.resume()
        b.pointer('DOWN',139,53);time.sleep(.25);b.pointer('CANCEL',-8,0);s.pause();a,_=s.read('cancel');s.resume();time.sleep(.8);a2,_=s.read('after-cancel');s.check(a['counts']['SAND']==a2['counts']['SAND'],'CANCEL_releases_brush');s.resume()
        # The desktop maps Z to controller A, A to controller X, arrows to cursor.
        b.key('RIGHT',.03);b.key('UP',.03);initial=a2['counts']['SAND'];common.command(b.sock,'KEY_DOWN Z');time.sleep(1.1);common.command(b.sock,'KEY_UP Z');time.sleep(.08)
        a,_=s.read('held-controller');s.check(a['counts']['SAND']>initial+5,'controller_held_A_paints_without_repeat_edges',sand=a['counts']['SAND']);s.resume()
        b.key('A',.03);a,_=s.read('physics-pause');s.resume();time.sleep(.7);a2,_=s.read('physics-still');s.check(a['steps']==a2['steps'] and a['rng']==a2['rng'] and a['cells']==a2['cells'],'controller_X_freezes_world_and_rng');s.shot('03-tools')
        # Four genuine playable challenge solutions, all through touch input.
        s.scene(1);s.brush(2);s.stroke(25,28,46,28);s.start();a=s.wait_badge(1,18);s.check(a['badges']&1,'water_fills_jar_and_earns_badge');s.resume();s.shot('04-jar')
        s.scene(2);s.brush(2);s.stroke(11,32,61,32);s.start();a=s.wait_badge(2,35);s.check(a['counts']['PLANT']>=16,'watering_seeds_grows_real_forest_and_badge',plants=a['counts']['PLANT']);s.resume();s.shot('05-forest')
        s.scene(3);s.brush(1);s.stroke(38,12,38,42);s.start();a=s.wait_badge(4,35);s.check(a['burned']>=12 and a['steps']-a['scene_start']>=400,'stone_firebreak_protects_real_grove_for_20_seconds',burned=a['burned']);s.resume();s.shot('06-firebreak')
        s.scene(4);s.brush(0)
        for y in (38,40,42):s.stroke(24,y,48,y)
        s.start();a=s.wait_badge(8,15);s.check(a['cooled']>=60 and a['badges']==15,'water_lava_reacts_to_stone_steam_all_four_badges',cooled=a['cooled']);s.resume();s.shot('07-cooling')
        # Freeze using the game's own pause; its UI bit and fractional step persist.
        b.tap(267,51,.03);baseline,base_path=s.read('exit-baseline');require(baseline['paused']==1,'Own pause before exact reload');s.resume();image=s.shot('08-save-baseline').crop((16,24,304,232));s.exit()
        final,final_path=s.read('exit-final',False);s.check(final_path.read_bytes()==base_path.read_bytes(),'EXIT_commits_exact_1780_byte_world_rng_step_ui',sha256=final['sha256'])
    time.sleep(2)
    with common.Simulator(args,state_root,Path('te-two.sock'),'reload',report) as b:
        s=Session(b,report);require('Elements: restored exact world rng step ui' in b.logs(),'Game accepted actual save')
        loaded,load_path=s.read('loaded',False);s.check(load_path.read_bytes()==final_path.read_bytes(),'process_relaunch_restores_every_saved_byte_excluding_wall_time')
        current=s.shot('09-restored').crop((16,24,304,232));s.check(common.difference(image,current)<.001,'restored_world_pixels_match',difference=common.difference(image,current))
        # Resume actual physics and verify every cell/RNG against native continuation.
        b.tap(267,51,.03);time.sleep(1.35);resumed,resumed_path=s.read('resumed');steps=resumed['steps']-loaded['steps'];require(steps>=10,'Actual resumed fixed steps')
        expected=args.output/'native-expected.save';subprocess.run([str(args.oracle),str(load_path),str(steps),str(expected)],check=True);oracle=decode(expected.read_bytes())
        fields=('rng','steps','scene_start','burned','cooled','badges','cells')
        s.check(all(resumed[k]==oracle[k] for k in fields),'actual_resumed_world_matches_native_physics_at_exact_saved_step',steps=steps,world_sha256=resumed['world_sha256'])
        s.exit()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','publisher-key','output','oracle'):p.add_argument('--'+name,required=True,type=Path)
    args=p.parse_args()
    for name in ('simulator','package','publisher_key','output','oracle'):setattr(args,name,getattr(args,name).resolve())
    args.output.mkdir(parents=True,exist_ok=True);os.chdir(args.output)
    report={'passed':False,'game':'tiny-elements','checks':[],'processes':[]}
    try:exercise(args,report);report['passed']=True
    except BaseException as error:report['error']=repr(error);report['traceback']=traceback.format_exc();raise
    finally:(args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS Tiny Elements real simulator touch, four challenges, controller and actual KV resume')
if __name__=='__main__':main()
