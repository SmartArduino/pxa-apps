#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Use real PXA controls to exercise offline maths and cross-process state."""
import argparse,importlib.util,json,os,sys,time
from pathlib import Path
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'));f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
spec=importlib.util.spec_from_file_location('sibo_layout',HERE.with_name('test_pinyin.py'));layout=importlib.util.module_from_spec(spec);spec.loader.exec_module(layout)
def exact(sim,text):
    for _ in range(70):
        if sim.focus()==text:sim.key('ENTER');return
        previous=sim.log_path.read_text().count('STUDY_FOCUS ');sim.key('DOWN')
        end=time.monotonic()+3
        while previous==sim.log_path.read_text().count('STUDY_FOCUS ') and time.monotonic()<end:time.sleep(.025)
    raise AssertionError((text,sim.focus()))
def saved(sim,check=lambda x:True):
    result=f.personal(sim,lambda p:'thinking' in p and check(list(map(int,p['thinking'].split()))))
    return list(map(int,result['thinking'].split()))
def all_text(sim):return '\n'.join(text for text,_ in layout.labels(sim))
def wait_text(sim,text):
    end=time.monotonic()+3
    while time.monotonic()<end:
        if text in all_text(sim):return
        time.sleep(.05)
    raise AssertionError((text,all_text(sim)))
def tap_label(sim,text):
    _,v=next((t,v) for t,v in layout.labels(sim) if t==text)
    x1,y1,x2,y2,cx1,cy1,cx2,cy2,*_=v
    assert cx1<=cx2 and cy1<=cy2,(text,v)
    sim.tap((max(x1,cx1)+min(x2,cx2))/2,(max(y1,cy1)+min(y2,cy2))/2)
def exercise(sim,w,h):
    # Existing learning data must survive the new, separately named snapshot field.
    sim.key('ENTER');sim.key('ENTER');sim.key('ENTER');sim.key('ENTER')
    f.returned(sim);f.choose(sim,'收藏起来')
    old=f.personal(sim,lambda p:bool(p['favorites']));bookmark=old['bookmark'];favorites=old['favorites']
    for _ in range(3):f.returned(sim)
    f.choose(sim,'数学动手玩');sim.expect('4×4 数独');sim.screenshot('math-menu')
    sim.key('ENTER')
    if h>=220:wait_text(sim,'每行、每列')
    state=saved(sim);givens=state[2:18];cells=state[18:34];chosen=next(i for i,n in enumerate(givens) if not n)
    sim.screenshot('sudoku-board')
    f.choose(sim,'提示这一格');state=saved(sim,lambda s:s[34]==1);assert state[18+chosen] in (1,2,3,4)
    f.choose(sim,'清除这一格');saved(sim,lambda s:s[18+chosen]==0)
    f.choose(sim,'换一道数独');sim.expect('继续原题');sim.key('ENTER')
    assert saved(sim)[2:18]==givens
    # Navigation and persistent hint tracking continue after a real process restart.
    snapshot=f.personal(sim)['thinking'];f.restart(sim);f.choose(sim,'数学动手玩');f.choose(sim,'4×4 数独')
    assert f.personal(sim)['thinking']==snapshot
    f.returned(sim);f.choose(sim,'24点挑战');state=saved(sim,lambda s:s[36]!=0)
    exact(sim,'+');wait_text(sim,'先选');f.returned(sim)
    assert saved(sim)[40]==0
    f.choose(sim,'提示下一步');wait_text(sim,'试试');sim.screenshot('24-hint-reader');f.returned(sim)
    s=saved(sim);depth=s[40];assert s[41+3*depth]==1
    f.choose(sim,'本题重来');s=saved(sim);assert s[40]==0 and s[41]==1
    sim.screenshot('24-hint')
    # Seed only a deterministic original question, then solve it through native controls.
    # All operators, including a fractional intermediate, execute inside the guest.
    f.returned(sim);f.returned(sim)
    value=f.personal(sim);tokens=list(map(int,value['thinking'].split()));tokens[36:40]=[3,3,8,8]
    value['thinking']=' '.join(map(str,tokens))
    path=next((sim.state/'private-files').iterdir())/'personal.json'
    path.write_text(json.dumps(value,ensure_ascii=False));f.restart(sim)
    f.choose(sim,'数学动手玩');f.choose(sim,'24点挑战')
    exact(sim,'8');exact(sim,'3');exact(sim,'÷') # [3, 8, 8/3]
    saved(sim,lambda s:s[40]==1)
    exact(sim,'3');exact(sim,'8/3');exact(sim,'−') # [8, 1/3]
    saved(sim,lambda s:s[40]==2)
    exact(sim,'8');exact(sim,'1/3');exact(sim,'÷')
    wait_text(sim,'得到24');s=saved(sim,lambda s:s[40]==3);assert s[-1]==1 # hinted completion
    sim.screenshot('24-complete');f.returned(sim);f.choose(sim,'撤销一步')
    exact(sim,'8');exact(sim,'1/3');exact(sim,'÷');saved(sim,lambda s:s[40]==3)
    assert saved(sim)[-1]==1,'Replay counted the same question twice'
    f.returned(sim)
    f.returned(sim);f.choose(sim,'分数实验室');wait_text(sim,'1 / 2')
    sim.screenshot('fraction-question')
    # On the large viewport also exercise the actual touch hit target.
    if h>=480:tap_label(sim,'左边更少 <')
    else:f.choose(sim,'左边更少 <')
    wait_text(sim,'答对了');sim.screenshot('fraction-explanation');f.returned(sim);s=saved(sim);assert s[-7:-4]==[1,1,1]
    f.choose(sim,'再看解析');wait_text(sim,'左边小于');f.returned(sim)
    assert saved(sim)[-7:-4]==[1,1,1]
    f.choose(sim,'两边一样');assert saved(sim)[-7:-4]==[1,1,1]
    for number in range(2,6):
        f.choose(sim,'下一题');s=saved(sim,lambda s:s[-5]==0)
        n,d,rn,rd=s[-11:-7];relation=n*rd-rn*d
        f.choose(sim,'左边更少' if relation<0 else '左边更多' if relation>0 else '两边一样')
        saved(sim,lambda s:s[-7]==number);f.returned(sim)
    assert saved(sim)[-7:-4]==[5,5,1];sim.screenshot('fraction-round')
    final=f.personal(sim);assert final['favorites']==favorites and final['bookmark']==bookmark
    f.restart(sim);f.choose(sim,'数学动手玩');f.choose(sim,'分数实验室')
    assert f.personal(sim)==final;wait_text(sim,'答对 5')
    f.choose(sim,'再看解析');wait_text(sim,'分数比较');f.returned(sim)
    assert f.personal(sim)==final,'Review changed the saved score'
    assert not list(sim.state.rglob('cache-*.bin')),'Offline maths downloaded content'
    # A corrupt optional math field must never discard the original learning data.
    damaged=dict(final,thinking='broken progress')
    path.write_text(json.dumps(damaged,ensure_ascii=False));f.restart(sim)
    f.choose(sim,'数学动手玩');f.choose(sim,'4×4 数独')
    recovered=f.personal(sim,lambda p:p.get('thinking')!='broken progress')
    assert {k:v for k,v in recovered.items() if k!='thinking'}=={k:v for k,v in final.items() if k!='thinking'}
    assert saved(sim)[-4:]==[0,0,0,0]
    print(f'{w}x{h}: Sudoku hint/erase/cancel/restart; 24 exact fractional solution/undo/no duplicate score; five fraction answers and old records preserved: PASS',flush=True)
def main():
    p=argparse.ArgumentParser()
    for key in ('workspace','package-root','executable','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--sizes',nargs='*',default=['240x284','160x160','800x480'])
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);os.environ['STUDY_TRACE_LAYOUT']='1'
    for size in a.sizes:
        w,h=map(int,size.split('x'))
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',w,h,'thinking',executable=a.executable.resolve()) as sim:exercise(sim,w,h)
if __name__=='__main__':main()
