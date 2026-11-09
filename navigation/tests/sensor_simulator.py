#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise production navigation through real PXA sensor, UI and KV services.

The isolated Host injects synthetic observations. Physical sensor accuracy is
verified separately on hardware. Original screenshots are retained unchanged.
"""
import argparse
from contextlib import contextmanager
import json
import math
import os
from pathlib import Path
import random
import re
import struct
import subprocess
import time
from types import SimpleNamespace
import zlib
from PIL import Image
from keys_simulator import Simulator


def calibration():
    data=bytearray(80);data[:4]=b'CMP2';data[5]=30
    for i in range(2):
        offset=8+i*28;data[offset]=1
        struct.pack_into('<fffff',data,offset+4,0,0,0,math.sqrt(24**2+38**2),0)
    checksum=2166136261
    for byte in data[:-4]:
        checksum=((checksum^byte)*16777619)&0xffffffff
    struct.pack_into('<I',data,76,checksum)
    return bytes(data)


class Sensors:
    def __init__(self,path):
        self.path=path
        self.pause=path.with_name('pause-absolute')
        self.revoke=path.with_name('revoke')
        self.values=[0,24000,-38000,0,0,9806650,0,0,0,0]
        self.write()

    def write(self):
        temporary=self.path.with_suffix('.tmp')
        temporary.write_text(' '.join(str(round(value))for value in self.values))
        temporary.replace(self.path)

    def bearing(self,heading):
        angle=math.radians(heading)
        self.values[:3]=[-24000*math.sin(angle),24000*math.cos(angle),-38000]
        self.values[9]=(heading%360)*1000000
        self.write()

    def angular(self,z):
        self.values[8]=z*1000000;self.write()

    def revoke_scope(self,scope):
        self.revoke.write_text(scope)


@contextmanager
def session(args,name,mode,*,calibrated=True,preserve=False,width=480,height=480,density=160):
    output=args.output/name;output.mkdir(parents=True,exist_ok=True)
    sensors=Sensors(output/'sensors.txt')
    original=dict(os.environ)
    os.environ.update(NAV_TEST_SENSORS=str(sensors.path),NAV_TEST_MODE=mode,
        NAV_TEST_PAUSE_ABSOLUTE=str(sensors.pause),NAV_TEST_REVOKE=str(sensors.revoke))
    case=SimpleNamespace(package=args.package,runner=args.runner,output=output,
        publisher_key=args.publisher_key,width=width,height=height,density_dpi=density)
    settings={'amap.key':b''}
    if calibrated:settings['compass.v2']=calibration()
    sim=Simulator(case,settings,preserve_state=preserve)
    try:
        sim.wait(lambda:Path(sim.socket).exists()and'Navigation: ready'in sim.logs())
        yield sim,sensors
    finally:
        sim.close()
        os.environ.clear();os.environ.update(original)


def value_crop(sim,name,*,dual=False):
    sim.shot(name)
    scale=sim.args.density_dpi/160
    safe_height=int(sim.args.height/scale)
    below=safe_height-46-(32 if dual else 0)-(60 if safe_height>=400 else 44)
    image=Image.open(sim.output/(name+'.png')).convert('RGB')
    return image.crop((round(14*scale),round(below*scale),image.width-round(14*scale),
                       round((below+(36 if safe_height>=400 else 24))*scale)))


def reading(sim,target,name,*,dual=False):
    deadline=time.monotonic()+5
    actual=None
    while time.monotonic()<deadline:
        image=value_crop(sim,name,dual=dual)
        path=sim.output/(name+'-ocr.png');image.save(path)
        result=subprocess.run(['tesseract',str(path),'stdout','-l','chi_sim+eng','--psm','7'],capture_output=True,text=True,check=True)
        numbers=re.findall(r'\d{1,3}',result.stdout)
        if numbers:
            actual=int(numbers[0])
            if abs((actual-target+180)%360-180)<=2:return actual
        time.sleep(.2)
    raise AssertionError(('heading display',target,actual,sim.logs()[-1000:]))


def saved_compass(state):
    snapshots=[]
    for path in state.rglob('.pxa-kv-*'):
        data=path.read_bytes()
        if data[:4]!=b'PXKV':continue
        _,_,generation,size,checksum=struct.unpack_from('<HHQII',data,4)
        body=data[24:];assert len(body)==size and zlib.crc32(body)==checksum
        count=struct.unpack_from('<H',body)[0];offset=2;values={}
        for _ in range(count):
            length=body[offset];offset+=1
            key=body[offset:offset+length].decode();offset+=length
            length=struct.unpack_from('<H',body,offset)[0];offset+=2
            values[key]=body[offset:offset+length];offset+=length
        snapshots.append((generation,values))
    return max(snapshots,key=lambda item:item[0])[1].get('compass.v2')


def rotate(vector,quaternion):
    x,y,z,w=quaternion
    def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
    u=(x,y,z);first=cross(u,vector);second=cross(u,first)
    return tuple(vector[i]+2*w*first[i]+2*second[i]for i in range(3))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('package','runner','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--publisher-key',type=Path,default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
    parser.add_argument('--case',choices=('all','absent','heading','raw','calibration','small'),default='all')
    args=parser.parse_args()
    for name in ('package','runner','output','publisher_key'):setattr(args,name,getattr(args,name).resolve())
    assert '/local/artifacts/'in str(args.output)
    args.output.mkdir(parents=True,exist_ok=True)
    report=[];empty=None
    if args.case in ('all','absent','raw'):
        modes=('none','gyro-only','no-gravity','bad-unit','deny-primary')if args.case!='raw'else('none',)
        for mode in modes:
            with session(args,'absent-'+mode,mode)as(sim,sensors):
                time.sleep(.6);sim.choose('偏好设置');sim.choose('指南针');time.sleep(.5)
                pixels=value_crop(sim,'unavailable').tobytes()
                if empty is None:empty=pixels
                assert pixels==empty,(mode,'unavailable source showed a heading')
                assert 'NAV_TEST_SUB'not in sim.logs(),mode
                sim.choose('重新检测');time.sleep(.4)
                assert value_crop(sim,'retried').tobytes()==empty
                sim.choose('返回');sim.choose('返回');sim.choose('退出地图')
                assert sim.process.wait(timeout=5)==0
            report.append({'case':mode,'result':'passed'})
            print('PASS',mode,flush=True)
    if args.case in ('all','heading'):
        for mode in ('heading','all'):
            with session(args,'absolute-'+mode,mode)as(sim,sensors):
                sim.wait(lambda:'system heading connected'in sim.logs());sim.choose('指南针')
                reading(sim,0,'north');sensors.bearing(90);reading(sim,90,'east')
                sensors.bearing(359);reading(sim,359,'wrap-before');sensors.bearing(1);reading(sim,1,'wrap-after')
                assert 'NAV_TEST_SUB 4'in sim.logs()
                assert not any('NAV_TEST_SUB '+str(i)in sim.logs()for i in (1,2,3,5))
                sensors.path.unlink();time.sleep(1.8)
                stale=value_crop(sim,'stale')
                if empty is not None:assert stale.tobytes()==empty
                sim.choose('返回');sim.choose('退出地图');sim.process.wait(timeout=5)
                assert 'NAV_TEST_UNSUB 4'in sim.logs()
            report.append({'case':mode,'result':'passed'})
            print('PASS',mode,flush=True)
    if args.case in ('all','raw'):
        with session(args,'raw','raw')as(sim,sensors):
            sim.wait(lambda:'angular streams connected'in sim.logs());sim.choose('指南针')
            reading(sim,0,'north');sensors.bearing(90);reading(sim,90,'east')
            sensors.pause.touch();sensors.angular(-math.pi/2);time.sleep(.35)
            predicted=value_crop(sim,'gyro-prediction')
            assert predicted.tobytes()!=value_crop(sim,'gyro-prediction-later').tobytes()
            time.sleep(1.6);assert value_crop(sim,'absolute-stale').tobytes()==empty
            sensors.angular(0);sensors.bearing(120);sensors.pause.unlink();reading(sim,120,'recovered')
            sensors.revoke_scope('motion.angular-velocity')
            sim.wait(lambda:'NAV_TEST_UNSUB 3'in sim.logs());reading(sim,120,'gyro-revoked')
            sensors.revoke_scope('magnetic-field.0')
            sim.wait(lambda:'NAV_TEST_UNSUB 1'in sim.logs());time.sleep(.3)
            assert value_crop(sim,'permission-revoked').tobytes()==empty
            assert 'NAV_TEST_UNSUB 2'in sim.logs()
        for mode in ('no-gyro','deny-gyro'):
            with session(args,mode,mode)as(sim,sensors):
                sim.wait(lambda:'magnetic and gravity streams connected'in sim.logs());sim.choose('指南针')
                sensors.bearing(45);reading(sim,45,'heading')
                sim.choose('六面校准');sim.choose('取消校准');reading(sim,45,'calibration-cancelled')
                sim.choose('返回');sim.choose('退出地图');sim.process.wait(timeout=5)
                for sensor in (1,2):assert 'NAV_TEST_UNSUB '+str(sensor)in sim.logs()
            print('PASS',mode,flush=True)
        report.append({'case':'raw/gyro/revocation/fallback/cancel','result':'passed'})
        print('PASS raw gyro, stale absolute reference, permission revocation and fallback',flush=True)
    if args.case in ('all','calibration'):
        with session(args,'calibration','raw',calibrated=False)as(sim,sensors):
            sim.wait(lambda:'angular streams connected'in sim.logs());sim.choose('指南针');sim.shot('calibration-needed')
            sensors.values[:3]=[65000,1000,-21000];sensors.write();time.sleep(.4)
            sim.choose('六面校准');rng=random.Random(314159)
            for _ in range(160):
                q=[rng.gauss(0,1)for _ in range(4)];length=math.sqrt(sum(v*v for v in q));q=[v/length for v in q]
                m=rotate((0,24000,-38000),q);g=rotate((0,0,9806650),q)
                sensors.values[:3]=[m[0]+65000,m[1]-23000,m[2]+17000]
                sensors.values[3:6]=g;sensors.write();time.sleep(.12)
            sim.choose('保存校准',False);sim.shot('calibration-ready');sim.key('ENTER');time.sleep(.8)
            blob=saved_compass(sim.state);assert blob and blob[:4]==b'CMP2'and blob[8]==1
            bias=struct.unpack_from('<fff',blob,12)
            assert all(abs(a-b)<.15 for a,b in zip(bias,(65,-23,17))),bias
        with session(args,'calibration','raw',calibrated=False,preserve=True)as(sim,sensors):
            sensors.values[:3]=[65000-24000,-23000,-38000+17000];sensors.write()
            sim.wait(lambda:'angular streams connected'in sim.logs());sim.choose('指南针')
            reading(sim,90,'restored-after-restart')
            sim.choose('返回');sim.choose('退出地图');sim.process.wait(timeout=5)
        report.append({'case':'six-face calibration and restart','result':'passed'})
        print('PASS six-face fit, calibration save and restart',flush=True)
    if args.case in ('all','small'):
        for width,height,density,mode in [(240,284,130,'dual'),(296,240,160,'dual'),(480,480,160,'dual')]:
            with session(args,f'layout-{width}-{height}',mode,width=width,height=height,density=density)as(sim,sensors):
                sim.wait(lambda:'angular streams connected'in sim.logs());sim.choose('指南针');sim.shot('compass')
                sim.choose('磁力计 2');sim.wait(lambda:'NAV_TEST_SUB 5'in sim.logs());time.sleep(.7);sim.shot('second-magnetometer')
                blob=saved_compass(sim.state);assert blob and blob[4]==1
                sim.choose('六面校准');sim.shot('calibration');sim.choose('取消校准');sim.choose('返回');sim.choose('退出地图');sim.process.wait(timeout=5)
            report.append({'case':'layout','size':[width,height],'density':density,'result':'passed'})
            print('PASS layout',width,height,density,flush=True)
    (args.output/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
