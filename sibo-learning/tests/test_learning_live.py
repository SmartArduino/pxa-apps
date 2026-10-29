#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in public static English/science checks; never requests AI generation."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
import time
import urllib.request
HERE=Path(__file__).resolve()
sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'))
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
VOCAB=f.ORIGIN+'/oss/vocab';SCIENCE=f.ORIGIN+'/oss/vistep'
def wait(sim,predicate,seconds=30):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        if sim.process.poll() is not None:raise AssertionError('Simulator exited')
        if predicate():return
        time.sleep(.1)
    raise AssertionError(sim.log_path.read_text()[-5000:])
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',240,284,'learning-live',executable=a.executable.resolve()) as sim:
        f.choose(sim,'英语单词岛');wait(sim,lambda:sim.focus().startswith('中考'))
        sim.key('ENTER');sim.expect('第 1 单元');sim.key('ENTER');wait(sim,lambda:sim.focus()=='the')
        sim.key('ENTER');sim.expect('翻开词义');f.choose(sim,'听这个单词')
        # A full unit lasts ~41 seconds. Both the first and last word must
        # finish promptly, exercising clipping and decode/discard seeking.
        wait(sim,lambda:'sibo PCM started 24000Hz 1ch' in sim.log_path.read_text())
        wait(sim,lambda:'sibo audio: 这一段听完啦' in sim.log_path.read_text(),8)
        sim.screenshot('first-word-audio');f.returned(sim)
        for _ in range(8):f.choose(sim,'下一页')
        sim.expect('there');f.choose(sim,'year');sim.expect('翻开词义')
        f.choose(sim,'听这个单词')
        wait(sim,lambda:sim.log_path.read_text().count('sibo PCM started 24000Hz 1ch')>=2,25)
        wait(sim,lambda:sim.log_path.read_text().count('sibo audio: 这一段听完啦')>=2,8)
        sim.screenshot('last-word-audio');assert 'sibo HTTP body error=' not in sim.log_path.read_text()
        for _ in range(4):f.returned(sim)
        f.choose(sim,'科学发现馆');wait(sim,lambda:sim.focus().startswith('空气没有约定'))
        f.choose(sim,'背景虚化');wait(sim,lambda:sim.focus()=='一张照片里的距离')
        sim.key('ENTER');sim.expect('开始阅读');f.choose(sim,'看看插图')
        wait(sim,lambda:sim.focus()=='放大',35);sim.screenshot('science-image')
    print('Live vocabulary catalogs, first/last word audio clips and science chapters/images passed',flush=True)
if __name__=='__main__':main()
