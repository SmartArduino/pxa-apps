#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Three-key learning flows, independent self-rating and a real 64 KiB Host."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys
import time
HERE=Path(__file__).resolve()
sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'))
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
VOCAB=f.ORIGIN+'/oss/vocab'
SCHOOL='https://pindou.doit.am/study/data'
SCIENCE=f.ORIGIN+'/oss/vistep'
def until(predicate):
    for _ in range(80):
        if predicate():return
        time.sleep(.05)
    raise AssertionError('Expected persisted change was not observed')
def exercise(sim):
    private=next((sim.state/'private-files').iterdir())
    f.seed(sim,VOCAB+'/catalog.json',{'packs':[{'id':'zk','name':'中考','count':50,'unitCount':1}]})
    f.choose(sim,'英语单词岛');sim.expect('中考');sim.key('ENTER');sim.expect('第 1 单元')
    f.seed(sim,VOCAB+'/packs/zk/001.json',{'words':[
        {'id':str(i),'word':word,'phonetic':phonetic,'translation':meaning}
        for i,(word,phonetic,meaning) in enumerate([('cat','kæt','猫'),('dog','dɒg','狗'),('fish','fɪʃ','鱼'),('bird','bɜːd','鸟')])]})
    sim.key('ENTER');sim.expect('cat');sim.key('ENTER');sim.expect('翻开词义');sim.screenshot('word-hidden')
    sim.key('ENTER');sim.expect('下一个单词');sim.screenshot('word-revealed')
    sim.key('ENTER');sim.expect('翻开词义');sim.screenshot('next-word-default')
    f.choose(sim,'上一个单词');sim.expect('翻开词义')
    f.choose(sim,'单词小挑战');sim.expect('猫');sim.key('ENTER');sim.expect('下一题')
    sim.key('ENTER');assert sim.focus() not in ('返回','下一题','我答对了');sim.screenshot('word-quiz-next-default')
    f.returned(sim);sim.expect('翻开词义');f.returned(sim);sim.expect('cat')
    for _ in range(3):f.returned(sim)
    sim.expect('诗词小天地')
    f.choose(sim,'同步小课堂');sim.expect('语文')
    f.seed(sim,SCHOOL+'/index.json',{'books':[{'id':'g1up','subject':'math','subjectName':'数学','gradeName':'一年级','semesterName':'上册','lessonsCount':2}]})
    f.choose(sim,'数学');sim.expect('一年级上册')
    f.seed(sim,SCHOOL+'/books/g1up/outline.json',{'lessons':[{'id':'g1up-test','title':'数一数','unitTitle':'准备课','questionCount':2}]})
    sim.key('ENTER');sim.expect('数一数')
    f.seed(sim,SCHOOL+'/books/g1up/lessons/g1up-test.json',{'id':'g1up-test','knowledge':{'core_concept':'有顺序地数一数。'},'questions':[
        {'type':'true_false','question':'一只手有五根手指。','answer':'对','explanation':'1、2、3、4、5。'},
        {'type':'fill_blank','question':'两颗糖再拿三颗，一共几颗？','answer':'5','explanation':'2+3=5。'}]})
    sim.key('ENTER');sim.expect('开始阅读');sim.screenshot('school-lesson')
    f.choose(sim,'来个小挑战');sim.expect('对');sim.key('ENTER');sim.expect('下一题')
    sim.key('ENTER');sim.expect('想好啦');before=f.personal(sim)['answered']
    sim.key('ENTER');sim.expect('我答对了');assert f.personal(sim)['answered']==before
    sim.screenshot('self-check-answer');f.choose(sim,'还要再练练');sim.expect('看看收获')
    until(lambda:f.personal(sim)['answered']==before+1)
    assert f.personal(sim)['correct']<f.personal(sim)['answered']
    f.returned(sim);f.returned(sim);f.returned(sim);f.returned(sim)
    sim.expect('语文');f.returned(sim)
    f.seed(sim,SCIENCE+'/catalog.json',{'items':[{'id':'light','title':'光的小秘密','categoryName':'光与声音','minAge':8,'item':'items/light.json'}]})
    f.choose(sim,'科学发现馆');sim.expect('光的小秘密')
    f.seed(sim,SCIENCE+'/items/light.json',{'id':'light','sections':[{'title':'第一步','text':'先看看光从哪里来。'},{'title':'第二步','text':'再看看影子在哪里。'}]})
    sim.key('ENTER');sim.expect('第一步');sim.key('DOWN');sim.expect('第二步');sim.key('ENTER');sim.expect('开始阅读');sim.key('ENTER');sim.expect('下一页','读完啦');sim.screenshot('science-reader')
    f.returned(sim);f.returned(sim);f.returned(sim);f.returned(sim)
    # Simulate interrupted writes from the old release. Host initializes with
    # the full 64 KiB quota, then the app must reclaim only its own temp files.
    snapshot=f.personal(sim)
    (private/'cache.tmp').write_bytes(b'x'*45000)
    (private/'personal.tmp').write_bytes(b'x'*4000)
    f.restart(sim);sim.expect('继续读')
    assert not (private/'cache.tmp').exists() and not (private/'personal.tmp').exists()
    assert f.personal(sim)==snapshot
    assert sum(p.stat().st_size for p in private.glob('cache-*.bin'))<=62*1024
    sim.screenshot('records-after-restart')
    # Initialize the actual Host with a small personal record and ~48 KiB
    # of legacy caches. Growing the bookmark to 18 KiB cannot stage its
    # replacement within 64 KiB, so saving must discard caches and retry.
    data={'id':'g1up-long','knowledge':{'core_concept':'学'*2000,'key_formula':'记'*600,'tips':'想'*600,'common_mistakes':['错'*266]*6},'questions':[{'type':'true_false','question':'问'*500,'answer':'对','explanation':'理'*700}]}
    fixtures=[
        (SCHOOL+'/index.json',{'books':[{'id':'g1up','subject':'math','subjectName':'数学','gradeName':'一年级','semesterName':'上册','lessonsCount':1}]}),
        (SCHOOL+'/books/g1up/outline.json',{'lessons':[{'id':'g1up-long','title':'长篇小课堂','unitTitle':'准备课','questionCount':1}]}),
        (SCHOOL+'/books/g1up/lessons/g1up-long.json',data),
        (f.ORIGIN+'/old-cache.json',{'unused':'x'*29000})]
    for i,(url,value) in enumerate(fixtures):
        (private/f'cache-{i}.bin').write_text(url+'?\n'+json.dumps(value,ensure_ascii=False,separators=(',',':')))
    f.restart(sim);sim.expect('继续读')
    assert len(list(private.glob('cache-*.bin')))==4
    f.choose(sim,'同步小课堂');f.choose(sim,'数学');sim.key('ENTER');sim.expect('长篇小课堂')
    sim.key('ENTER');sim.expect('开始阅读');sim.key('ENTER')
    until(lambda:f.personal(sim)['bookmark']['id']=='g1up-long')
    assert not list(private.glob('cache-*.bin')),'Quota retry did not release the competing caches'
    f.returned(sim)
    before=f.personal(sim)['answered'];f.choose(sim,'来个小挑战');sim.key('ENTER')
    until(lambda:f.personal(sim)['answered']==before+1)
    assert f.personal(sim)['bookmark']['id']=='g1up-long'
    assert not list(private.glob('*.tmp'))
    assert sum(p.stat().st_size for p in private.iterdir() if p.is_file())<=65536
    f.restart(sim);sim.expect('继续读');assert f.personal(sim)['answered']==before+1
    sim.screenshot('quota-records-restored')
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    for w,h in ((240,284),(160,160)):
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',w,h,'extensions',executable=a.executable.resolve()) as sim:exercise(sim)
        print(f'{w}x{h}: word cards, next-question focus, textbook self-check, science and 64 KiB persistence passed',flush=True)
if __name__=='__main__':main()
