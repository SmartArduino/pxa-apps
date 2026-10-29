#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Navigate learning packages using only Up, Down and Enter in isolated state."""
import argparse
import json
from pathlib import Path
import struct
import time
from PIL import Image
from test_simulator import Simulator

class ThreeKey(Simulator):
    def focus(self):
        lines=self.log_path.read_text().splitlines()
        return next((s.removeprefix('STUDY_FOCUS ') for s in reversed(lines) if s.startswith('STUDY_FOCUS ')), '')

    def key(self,name):
        assert name in ('UP','DOWN','ENTER')
        self.command('KEY '+name)
        time.sleep(.3)

    def expect(self,*names):
        deadline=time.monotonic()+1
        while True:
            actual=self.focus()
            if any(actual.startswith(name) for name in names):return
            if time.monotonic()>=deadline:break
            time.sleep(.025)
        assert False,(self.stem,names,actual)

    def all_stats(self,age):
        rows=[]
        for path in self.state.rglob('.pxa-kv-*'):
            data=path.read_bytes();offset=data.find(b'STDY')
            if offset>=0:rows.append((struct.unpack_from('<Q',data,8)[0],struct.unpack_from('<II',data,offset+12+12*age)))
        return max(rows)[1] if rows else (0,0)

def exercise(sim,subject,age):
    sim.expect('开始小挑战','开始练习')
    home_image=sim.screenshot('home-default')
    sim.key('UP');sim.expect('退出')
    sim.key('DOWN');sim.expect('开始小挑战','开始练习')
    sim.key('ENTER');assert sim.focus() not in ('退出','返回')
    first_answer=sim.focus()
    # All four answers are reachable using Down; Up restores the first.
    for _ in range(3):sim.key('DOWN')
    last_answer_image=sim.screenshot('last-answer')
    for _ in range(3):sim.key('UP')
    sim.expect(first_answer)
    question_image=sim.screenshot('question-default')
    # Inspect original captures: the fixed title/age region must stay in place
    # while focus scrolls long choices; Back/Exit text is intentionally excluded.
    for capture in (question_image,last_answer_image):
        with Image.open(home_image) as original,Image.open(capture) as current:
            band=(0,0,original.width-65,36)
            assert original.convert('RGB').crop(band).tobytes()==current.convert('RGB').crop(band).tobytes(),(sim.stem,'scroll covered the fixed header')
    for question in range(6 if subject=='poetry' else 10):
        assert sim.focus() not in ('退出','返回'),(sim.stem,question,sim.focus())
        sim.key('ENTER');sim.expect('继续探险','继续','看看收获','查看小结')
        assert sim.all_stats(age)[0]==question+1,(sim.stem,question,sim.all_stats(age))
        if question==0:
            sim.screenshot('feedback-default')
            sim.key('DOWN');sim.expect('分页讲解');sim.key('ENTER')
            sim.expect('下一页','返回')
            if sim.focus()=='下一页':
                sim.key('ENTER');sim.expect('下一页','上一页')
                # Return is always the final control; cycle down to it.
                for _ in range(3):
                    if sim.focus()=='返回':break
                    sim.key('DOWN')
            sim.expect('返回');sim.key('ENTER');sim.expect('继续探险','继续')
        sim.key('ENTER')
    sim.expect('返回首页');sim.screenshot('completion')
    sim.key('DOWN');sim.expect('再来一轮','再练一轮')
    sim.key('ENTER');assert sim.focus() not in ('退出','返回')
    sim.key('UP');sim.expect('返回');sim.key('ENTER');sim.expect('开始小挑战','开始练习')
    if subject!='math':
        sim.key('DOWN');sim.expect('翻开知识卡','翻卡学习');sim.key('ENTER');sim.expect('展开答案')
        before=sim.all_stats(age)
        sim.key('ENTER');sim.expect('还想再学','再学')
        assert sim.all_stats(age)==before
        sim.screenshot('card-revealed-default')
        sim.key('DOWN');sim.expect('我记住啦','记住');sim.key('ENTER');sim.expect('展开答案')
        assert sim.all_stats(age)==(before[0]+1,before[1]+1)
        # Repeated browsing keeps Next selected; rating returns to Reveal.
        for _ in range(6):
            if sim.focus()=='下一张 >':break
            sim.key('DOWN')
        sim.expect('下一张 >');sim.key('ENTER');sim.expect('下一张 >')
        for _ in range(8):
            if sim.focus()=='返回':break
            sim.key('DOWN')
        sim.expect('返回');sim.key('ENTER');sim.expect('开始小挑战','开始练习')
    # Instructions/settings and paged help remain reachable with three keys.
    for _ in range(5):
        if sim.focus() in ('给小向导的说明','使用说明','我的小背包','设置与资源'):break
        sim.key('DOWN')
    sim.expect('给小向导的说明','使用说明','我的小背包','设置与资源')
    sim.key('ENTER');sim.expect('阅读使用说明');sim.key('ENTER');sim.expect('下一页')
    sim.key('UP');sim.expect('返回');sim.key('ENTER');sim.expect('阅读使用说明')
    # Return from a page's first action is one Up, with no dedicated Back key.
    if subject=='math':sim.key('UP');sim.expect('返回')
    else:
        for _ in range(8):
            if sim.focus()=='返回':break
            sim.key('DOWN')
        sim.expect('返回')
    sim.key('ENTER');sim.expect('开始小挑战','开始练习')
    sim.key('UP');sim.expect('退出')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',type=Path,required=True)
    p.add_argument('--package-root',type=Path,required=True)
    p.add_argument('--simulator',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--apps',nargs='*')
    p.add_argument('--width',type=int,default=240);p.add_argument('--height',type=int,default=284)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    ages=('preschool','primary','teen','adult')
    apps=a.apps or [f'learn-{subject}-{age}' for age in ages for subject in ('math','poetry','words','hanzi','cards')]
    rows=[]
    for app in apps:
        _,subject,age=app.split('-')
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),app,a.width,a.height,'three-key',a.simulator.resolve()) as sim:
            exercise(sim,subject,ages.index(age))
        rows.append(dict(app=app,viewport=[a.width,a.height],result='passed'))
        print(json.dumps(rows[-1]),flush=True)
    (a.output/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
