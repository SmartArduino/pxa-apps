#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise empty collections, readable primary actions and final word cards."""
import argparse
import importlib.util
import os
from pathlib import Path
import sys

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE.parents[2] / 'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec = importlib.util.spec_from_file_location('sibo_pinyin', HERE.with_name('test_pinyin.py'))
py = importlib.util.module_from_spec(spec); spec.loader.exec_module(py)
f = py.f

def visible_control(sim, text):
    label, values = next((label, values) for label, values in py.labels(sim) if label == text)
    x1, y1, x2, y2, cx1, cy1, cx2, cy2, tw, th = values
    assert (x1,y1,x2,y2) == (cx1,cy1,cx2,cy2), (label, 'primary action clipped', values)
    assert tw <= x2-x1+2 and th <= y2-y1+2, (label, 'primary label clipped', values)

def exercise(sim):
    sim.expect('诗词小天地'); sim.screenshot('home')
    f.choose(sim, '我的收藏'); sim.expect('返回')
    assert any('喜欢的内容' in text for text, _ in py.labels(sim))
    sim.screenshot('empty-favorites'); f.returned(sim)
    f.choose(sim, '诗词小天地'); sim.key('ENTER'); sim.key('ENTER')
    sim.expect('开始阅读'); visible_control(sim, '开始阅读'); sim.screenshot('detail')
    sim.key('ENTER'); sim.expect('下一页','读完啦')
    visible_control(sim, sim.focus()); sim.screenshot('reader')
    for _ in range(4): f.returned(sim)
    f.seed(sim, f.ORIGIN+'/oss/vocab/catalog.json', {'packs':[{'id':'zk','name':'中考','count':2,'unitCount':1}]})
    f.choose(sim, '英语单词岛'); sim.key('ENTER')
    f.seed(sim, f.ORIGIN+'/oss/vocab/packs/zk/001.json', {'words':[
        {'id':'cat','word':'cat','phonetic':'kæt','translation':'猫'},
        {'id':'dog','word':'dog','phonetic':'dɒg','translation':'狗'}]})
    sim.key('ENTER'); sim.key('ENTER'); sim.expect('翻开词义')
    sim.key('ENTER'); sim.expect('下一个单词'); sim.key('ENTER'); sim.expect('翻开词义')
    sim.key('ENTER'); sim.expect('返回单词表'); visible_control(sim, '返回单词表')
    sim.screenshot('last-word'); sim.key('ENTER'); sim.expect('cat')
    print(f'{sim.stem}: empty collection, visible primary controls, final-card return and first-action focus passed', flush=True)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('workspace','package-root','executable','output'):
        p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--sizes', nargs='*', default=['240x284','160x160','800x480','480x800'])
    a=p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    os.environ['STUDY_TRACE_LAYOUT']='1'
    for size in a.sizes:
        w,h=map(int,size.split('x'))
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),
                      'sibo-learning',w,h,'experience',executable=a.executable.resolve()) as sim:
            exercise(sim)

if __name__=='__main__': main()
