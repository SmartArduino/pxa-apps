#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise the Sibo PXA with the Gen4 native navigation bridge, without touch."""
import argparse
import json
import os
import subprocess
from pathlib import Path
import sys
import time

HERE=Path(__file__).resolve()
sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey

ORIGIN='https://wtoy.doit.am'
def seed(sim,address,value,body=''):
    private=next((sim.state/'private-files').iterdir())
    key=address+'?'+body;hashed=2166136261
    for byte in key.encode():hashed=((hashed^byte)*16777619)&0xffffffff
    data=json.dumps(value,ensure_ascii=False,separators=(',',':'))
    (private/f'cache-{hashed%4}.bin').write_text(key+'\n'+data)

def focus(sim,label):
    for _ in range(35):
        if sim.focus().startswith(label):return
        sim.key('DOWN')
    raise AssertionError((label,sim.focus()))

def choose(sim,label):
    focus(sim,label);sim.key('ENTER')

def returned(sim):choose(sim,'返回')

def personal(sim):
    private=next((sim.state/'private-files').iterdir())
    path=private/'personal.json'
    for _ in range(40):
        if path.exists():return json.loads(path.read_text())
        time.sleep(.025)
    raise AssertionError('personal snapshot was not saved')

def restart(sim):
    command=[part.decode() for part in Path(f'/proc/{sim.process.pid}/cmdline').read_bytes().split(b'\0') if part]
    sim.process.terminate();sim.process.wait(timeout=3);Path(sim.socket).unlink(missing_ok=True)
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy');env.pop('PXA_SIM_NAV_MODE',None)
    sim.process=subprocess.Popen(command,stdout=sim.log,stderr=subprocess.STDOUT,env=env)
    for _ in range(80):
        if sim.process.poll() is not None:raise AssertionError('restart exited')
        if Path(sim.socket).exists():time.sleep(.8);return
        time.sleep(.1)
    raise AssertionError('restart timed out')

def fixtures():
    stories={'categories':[{'id':'fable','name':'寓言故事'}],'items':[
        {'id':f'story-{i:04d}','title':'龟兔赛跑' if i==1 else f'小故事{i}',
         'category':'fable','ageMin':3,'ageMax':8,'minutes':2,'item':f'items/story-{i:04d}.json'}
        for i in range(1,9)]}
    story={'text':'森林里住着兔子和乌龟。\n'+('一步一步，乌龟坚持向前走。\n'*25),
           'questions':[{'type':'choice','prompt':'谁先到了？','options':['兔子','乌龟','小鸟'],'answer':1,'explanation':'乌龟坚持走到了终点。'}]}
    recite={'mengxue':[{'id':'test','title':'声律启蒙','tracks':2,'minutes':2,'item':'items/mengxue-test.json'}],
            'mathear':[{'id':'grade1','title':'一年级应用题','tracks':1,'minutes':1,'item':'items/mathear-grade1.json'}]}
    mengxue={'id':'test','tracks':[{'title':'上卷 · 一东','lines':['云对雨，雪对风，晚照对晴空。']},{'title':'上卷 · 二冬','lines':['春对夏，秋对冬。']}]}
    mathear={'id':'grade1','tracks':[{'title':'第1–10题','items':[{'index':1,'text':'有三颗糖，再拿两颗，一共几颗？','answer':'5'}]}]}
    meta={'pinyin':[{'items':[{'char':c,'pinyin':'jiāng' if c=='江' else 'nán'} for c in '江南可采莲莲叶何田田鱼戏莲叶间'*3]}],
          'translation':'江南到了采莲的时节。','explanation':'看看莲叶和游动的小鱼。'}
    index=[{'id':'p001','title':'江南','meta':'poems/p001/meta.json','listen':'poems/p001/listen.mp3'}]
    return stories,story,recite,mengxue,mathear,meta,index

def exercise(sim):
    stories,story,recite,mengxue,mathear,meta,index=fixtures()
    sim.expect('诗词小天地');sim.screenshot('home')
    sim.key('UP');sim.expect('退出');sim.key('DOWN');sim.expect('诗词小天地')
    sim.key('ENTER');sim.expect('课本古诗');sim.key('ENTER');sim.expect('江南')
    # Pagination changes the actual native control, restoring first-item focus.
    choose(sim,'下一页');assert sim.focus() not in ('下一页','返回','江南')
    choose(sim,'上一页');sim.expect('江南');sim.key('ENTER');sim.expect('开始阅读')
    sim.key('ENTER');sim.expect('下一页','读完啦');sim.screenshot('poem-reader')
    returned(sim);sim.expect('开始阅读')
    choose(sim,'来个小挑战');assert sim.focus()!='返回';sim.screenshot('quiz')
    sim.key('ENTER');sim.expect('下一题','看看收获');sim.screenshot('feedback')
    assert personal(sim)['answered']==1
    sim.key('ENTER');assert sim.focus() not in ('返回','下一题')
    returned(sim);sim.expect('开始阅读')
    seed(sim,ORIGIN+'/oss/shici/index.json',index)
    seed(sim,ORIGIN+'/oss/shici/poems/p001/meta.json',meta)
    choose(sim,'拼音伴读');sim.expect('下一页');sim.screenshot('pinyin')
    sim.key('ENTER');sim.expect('下一页','上一页');returned(sim);sim.expect('开始阅读')
    choose(sim,'诗意小课堂');sim.expect('下一页','读完啦');sim.screenshot('lesson');returned(sim)
    choose(sim,'收藏起来');assert len(personal(sim)['favorites'])==1
    returned(sim);returned(sim);returned(sim);sim.expect('继续读')
    seed(sim,ORIGIN+'/oss/stories/catalog.json',stories)
    choose(sim,'故事小书架');sim.expect('龟兔赛跑');sim.screenshot('stories')
    seed(sim,ORIGIN+'/oss/stories/items/story-0001.json',story)
    sim.key('ENTER');sim.expect('开始阅读');sim.key('ENTER');sim.expect('下一页')
    first=personal(sim);assert first['bookmark']['id']=='story-0001'
    sim.key('ENTER');assert personal(sim)['page']==1;sim.screenshot('story-page2')
    returned(sim);choose(sim,'来个小挑战');sim.expect('兔子');sim.key('DOWN');sim.expect('乌龟');sim.key('ENTER');sim.expect('看看收获')
    assert personal(sim)['correct']>=1
    returned(sim);returned(sim);choose(sim,'换个故事分类');sim.expect('全部故事');choose(sim,'寓言故事');sim.expect('龟兔赛跑')
    returned(sim);sim.expect('继续读')
    seed(sim,ORIGIN+'/oss/recite/catalog.json',recite)
    choose(sim,'读出声来');sim.expect('蒙学诵读');sim.key('ENTER');sim.expect('声律启蒙')
    seed(sim,ORIGIN+'/oss/recite/items/mengxue-test.json',mengxue)
    sim.key('ENTER');sim.expect('上卷');sim.key('ENTER');sim.expect('开始阅读');sim.key('ENTER');sim.screenshot('mengxue')
    returned(sim);returned(sim);returned(sim);returned(sim);sim.expect('蒙学诵读')
    seed(sim,ORIGIN+'/oss/recite/catalog.json',recite)
    choose(sim,'小学听题本');sim.expect('一年级应用题')
    seed(sim,ORIGIN+'/oss/recite/items/mathear-grade1.json',mathear)
    sim.key('ENTER');sim.expect('第1');sim.key('ENTER');sim.expect('开始阅读');sim.key('ENTER');sim.screenshot('mathear')
    returned(sim);returned(sim);returned(sim);returned(sim);returned(sim);sim.expect('继续读')
    choose(sim,'我的收藏');sim.expect('江南');sim.key('ENTER');sim.expect('开始阅读');returned(sim);returned(sim)
    choose(sim,'小书包与说明');sim.expect('阅读使用说明');sim.key('ENTER');sim.expect('下一页');sim.screenshot('help');returned(sim);returned(sim)
    saved=personal(sim);assert saved['answered']>=2
    restart(sim);sim.expect('继续读');sim.key('ENTER');sim.expect('下一页','读完啦');sim.screenshot('restored-reader')
    restored=personal(sim);assert restored['answered']==saved['answered'] and restored['favorites']==saved['favorites'] and restored['bookmark']==saved['bookmark']
    returned(sim);returned(sim)
    choose(sim,'小书包与说明');choose(sim,'清理在线缓存')
    private=next((sim.state/'private-files').iterdir());assert not list(private.glob('cache-*.bin'))
    assert personal(sim)==restored

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',type=Path,required=True)
    p.add_argument('--package-root',type=Path,required=True)
    p.add_argument('--executable',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    for w,h in [(240,284),(320,240),(160,160)]:
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',w,h,'three-key',executable=a.executable.resolve()) as sim:
            exercise(sim)
        print(f'{w}x{h}: poetry, pinyin, quiz, stories, recite, math, favorites and reading position passed',flush=True)
if __name__=='__main__':main()
