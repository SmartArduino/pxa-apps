#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read full long questions/choices/feedback/word cards using only three keys."""
import argparse,importlib.util,sys
from pathlib import Path
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('flows',HERE.with_name('test_simulator.py'));f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
def read_all(sim,name):
 sim.expect('下一页','读完啦');sim.screenshot(name+'-first');count=1
 while sim.focus()!='读完啦':
  assert count<180,'Reader did not reach its final page'
  sim.key('ENTER');count+=1
 sim.screenshot(name+'-last');f.returned(sim);return count
def exercise(sim,w,h):
 f.seed(sim,f.ORIGIN+'/oss/stories/catalog.json',{'items':[{'id':'story-0002','title':'认真读一读','item':'items/story-0002.json'}]})
 f.choose(sim,'故事小书架')
 question='题干起点。'+('请先观察故事里的变化，再仔细想一想。'*8)+'题干终点星星。'
 explanation='讲解起点。'+('每一条线索都很重要，认真阅读就会找到答案。'*8)+'讲解终点星星。'
 f.seed(sim,f.ORIGIN+'/oss/stories/items/story-0002.json',{'text':'一起认真看书。','questions':[{'type':'choice','prompt':question,'options':['甲选项。'+('这是一段比较长的选项说明。'*8)+'选项终点星星。','乙选项。'],'answer':0,'explanation':explanation}]})
 sim.key('ENTER');f.choose(sim,'来个小挑战');sim.expect('甲选项')
 f.choose(sim,'完整题目');pages=read_all(sim,'question');sim.expect('甲选项')
 sim.key('ENTER');assert f.personal(sim)['answered']==1
 f.choose(sim,'分页看讲解');read_all(sim,'explanation');assert f.personal(sim)['answered']==1
 f.returned(sim);f.returned(sim);f.returned(sim)
 f.seed(sim,f.ORIGIN+'/oss/vocab/catalog.json',{'packs':[{'id':'zk','name':'中考','count':1,'unitCount':1}]})
 f.choose(sim,'英语单词岛');sim.key('ENTER')
 f.seed(sim,f.ORIGIN+'/oss/vocab/packs/zk/001.json',{'words':[{'id':'read','word':'read','phonetic':'ri:d','translation':'词义起点。'+('阅读可以帮助我们认识更大的世界。'*15)+'词义终点星星。'}]})
 sim.key('ENTER');sim.key('ENTER');sim.expect('翻开词义');sim.key('ENTER');sim.expect('下一个单词')
 f.choose(sim,'完整词卡');read_all(sim,'word');sim.expect('下一个单词')
 assert f.personal(sim)['answered']==1
 print(f'{w}x{h}: complete question/choices ({pages} pages), feedback and word meaning accessible with three keys; answer and next-word focus preserved',flush=True)
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('workspace','package-root','executable','output'):p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--sizes',nargs='*',default=['240x284','160x160','800x480','480x800'])
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 for size in a.sizes:
  w,h=map(int,size.split('x'))
  with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',w,h,'long-text',executable=a.executable.resolve()) as sim:exercise(sim,w,h)
if __name__=='__main__':main()
