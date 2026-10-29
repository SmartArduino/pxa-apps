#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run static mono/stereo MP3 clips through a Host without open_pcm support."""
import argparse
import importlib.util
from pathlib import Path
import sys
import time
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'))
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
def wait(sim,text,count,seconds):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        if sim.process.poll() is not None:raise AssertionError('Simulator exited')
        if sim.log_path.read_text().count(text)>=count:return
        time.sleep(.1)
    raise AssertionError(sim.log_path.read_text()[-4000:])
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',240,284,'legacy-audio',executable=a.executable.resolve()) as sim:
        f.seed(sim,f.ORIGIN+'/oss/vocab/catalog.json',{'packs':[{'id':'zk','name':'中考','count':1,'unitCount':1}]})
        f.choose(sim,'英语单词岛');sim.key('ENTER')
        f.seed(sim,f.ORIGIN+'/oss/vocab/packs/zk/001.json',{'words':[{'id':'be','word':'be','phonetic':'bi:','translation':'是','audio':{'path':'packs/zk/001.mp3','startMs':569,'endMs':1291}},
            {'id':'stereo','word':'stereo','translation':'双声道样本','audio':{'path':f.ORIGIN+'/oss/shici/poems/p001/listen.mp3','startMs':0,'endMs':1500}}]})
        # The model intentionally restricts vocab media to its own static
        # base. For stereo use an ordinary story with an existing MP3.
        sim.key('ENTER');sim.key('ENTER');f.choose(sim,'听这个单词')
        wait(sim,'sibo PCM fallback 16000Hz mono',1,35);wait(sim,'sibo PCM started 16000Hz 1ch',1,10);wait(sim,'sibo audio: 这一段听完啦',1,10)
        sim.screenshot('mono-converted')
        for _ in range(4):f.returned(sim)
        f.seed(sim,f.ORIGIN+'/oss/shici/index.json',[{'id':'p001','title':'江南','meta':'poems/p001/meta.json','listen':'poems/p001/listen.mp3'}])
        f.seed(sim,f.ORIGIN+'/oss/shici/poems/p001/meta.json',{'translation':'江南到了采莲的时节。'})
        sim.key('ENTER');sim.key('ENTER');sim.key('ENTER');f.choose(sim,'听一听朗读')
        wait(sim,'sibo PCM fallback 16000Hz mono',2,35);wait(sim,'sibo PCM started 16000Hz 1ch',2,15)
        time.sleep(1);f.choose(sim,'暂停');sim.expect('继续听');sim.screenshot('stereo-paused');sim.key('ENTER');f.choose(sim,'停止朗读')
        wait(sim,'sibo audio cache complete bytes=623133',1,25)
        downloads=sim.log_path.read_text().count('sibo audio cache network')
        f.choose(sim,'听一听朗读');wait(sim,'sibo audio cache hit memory',1,10)
        wait(sim,'sibo PCM started 16000Hz 1ch',3,15)
        assert sim.log_path.read_text().count('sibo audio cache network')==downloads
        f.choose(sim,'停止朗读')
        assert 'sibo HTTP body error=' not in sim.log_path.read_text()
    print('Legacy public audio negotiation, 24 kHz mono/44.1 kHz stereo conversion, clipping, pause and stop passed',flush=True)
if __name__=='__main__':main()
