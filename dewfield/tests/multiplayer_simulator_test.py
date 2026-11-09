#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Four real signed PXA instances: discovery/direct IP, tools, save isolation."""
import argparse
import json
import os
from pathlib import Path
import re
import socket
import struct
import subprocess
import time
import zlib

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runner',type=Path,required=True)
parser.add_argument('--package',type=Path,required=True)
parser.add_argument('--publisher-key',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();out=args.output.resolve()
assert '/local/artifacts/' in str(out)
out.mkdir(parents=True,exist_ok=True);instances={}

def log(name):return (out/(name+'.log')).read_text(errors='replace')
def until(predicate,label,timeout=20):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        for name,instance in instances.items():assert instance['process'].poll() is None,log(name)[-2000:]
        if predicate():return
        time.sleep(.1)
    raise AssertionError(label+'\n'+'\n'.join(log(n)[-1200:] for n in instances))
def control(name,value,png=None):
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as client:
        client.settimeout(12);client.connect(instances[name]['socket']);client.sendall((value+'\n').encode())
        stream=client.makefile('rb');header=stream.readline()
        if png:
            assert header.startswith(b'PNG '),header
            count=int(header.split()[1]);data=stream.read(count);assert len(data)==count
            (out/png).write_bytes(data)
        else:assert header.startswith(b'OK'),(value,header)
def tap(name,x,y):
    control(name,f'TOUCH DOWN 0 {x*2} {y*2}');time.sleep(.06)
    control(name,f'TOUCH UP 0 {x*2} {y*2}');time.sleep(.2)
def key(name,value,duration=.07):
    control(name,'KEY_DOWN '+value);time.sleep(duration);control(name,'KEY_UP '+value);time.sleep(.2)
def start(name):
    state=out/(name+'-state');assert not state.exists(),'Use a fresh output directory'
    state.mkdir();stream=(out/(name+'.log')).open('w');sock=f'/tmp/dewfield-room-{os.getpid()}-{name}.sock'
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='none')
    process=subprocess.Popen([str(args.runner.resolve()),'--package',str(args.package.resolve()),'--publisher-key',str(args.publisher_key.resolve()),'--state-root',str(state),'--pxadb-control-socket',sock,'--width','480','--height','480','--density-dpi','160','--corner-radius','0','--safe-insets','0,0,0,0','--locale','zh-CN'],stdout=stream,stderr=subprocess.STDOUT,env=env)
    instances[name]={'process':process,'stream':stream,'socket':sock,'state':state}
    until(lambda:'Pocket: ready' in log(name),name+' startup');key(name,'S')
def save(name):
    candidates=[]
    for path in instances[name]['state'].glob('**/.pxa-kv-*'):
        raw=path.read_bytes()
        if len(raw)<26:continue
        magic,version,_,generation,size,crc=struct.unpack_from('<4sHHQII',raw);body=raw[24:]
        if magic!=b'PXKV' or version!=1 or len(body)!=size or zlib.crc32(body)!=crc:continue
        at=2
        for _ in range(struct.unpack_from('<H',body)[0]):
            n=body[at];at+=1;item=body[at:at+n];at+=n;n=struct.unpack_from('<H',body,at)[0];at+=2;value=body[at:at+n];at+=n
            if item==b'dewfield.save.v1':candidates.append((generation,value))
    return max(candidates,key=lambda item:item[0])[1] if candidates else b''
def plots():
    raw=save('host');return raw[98:178] if len(raw)==960 else b''
def shot(name,label):control(name,'SCREENSHOT AFTER_PRESENT',label+'.png')
try:
    start('host');tap('host',67,113)
    until(lambda:'Dewfield NET HOST' in log('host'),'host listener')
    ip,port=re.search(r'Dewfield NET HOST ([0-9.]+):(\d+)',log('host')).groups()
    shot('host','host-room')
    for name in ['guest1','guest2']:
        start(name);tap(name,95,144)
        until(lambda:'Dewfield NET FOUND' in log(name),'mDNS discovery')
        shot(name,name+'-discovery');tap(name,114,174)
        until(lambda:'Dewfield NET SYNC' in log(name),'guest snapshot')
    start('guest3');tap('guest3',82,83);key('guest3','A') # X clears the keypad.
    keys='123456789.0<'
    for c in ip:
        i=keys.index(c);tap('guest3',50+(i%3)*69,96+(i//3)*32)
    tap('guest3',197,64);tap('guest3',172,113)
    until(lambda:'Dewfield NET SYNC' in log('guest3'),'direct IP join')
    assert log('host').count('Dewfield NET JOIN')==3
    tap('host',67,206);shot('host','four-farmers')
    # Use the actual delayed tool commit, then seed and water the same cell.
    key('guest1','Z')
    until(lambda:len(plots())==80 and bool(plots()[63]&1),'guest tool reaches durable host state')
    tilled=plots();time.sleep(.7);key('guest1','W');key('guest1','Z')
    until(lambda:len(plots())==80 and bool((plots()[63]>>1)&7),'guest seed reaches host')
    planted=plots();time.sleep(.7);key('guest1','W');key('guest1','Z');shot('guest1','watering-input')
    until(lambda:len(plots())==80 and bool(plots()[63]&128),'guest watering reaches host')
    shot('host','shared-cultivation');shot('guest1','guest-cultivation')
    # Guest storage remains its own untouched world throughout the visit.
    for name in ['guest1','guest2','guest3']:
        raw=save(name);assert not raw or not raw[98+63]
    # Closing the host forces guests out safely and permits normal saving again.
    instances['host']['process'].terminate();instances['host']['process'].wait(timeout=10)
    # Exclude the intentionally stopped host from liveness checks.
    stopped=instances.pop('host');stopped['stream'].close()
    time.sleep(1)
    for name in instances:
        key(name,'ENTER');shot(name,name+'-restored')
        raw=save(name);assert not raw or not raw[98+63],name+' persisted the host farm'
    (out/'result.json').write_text(json.dumps({'players':4,'discovery':True,'direct_ip':ip,'port':int(port),'guest_till_seed_water':True,'host_persistence':True,'guest_save_isolation':True},indent=2)+'\n')
    print('PASS: four signed instances, mDNS and direct IP, authoritative delayed tools/seed/water, durable host and guest save isolation')
finally:
    for instance in instances.values():
        process=instance['process']
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=8)
            except subprocess.TimeoutExpired:process.kill();process.wait()
        instance['stream'].close()
