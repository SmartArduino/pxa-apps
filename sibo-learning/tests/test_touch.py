#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise responsive layouts and actual pointer taps in the PXA simulator."""
import argparse
from collections import deque
import importlib.util
from pathlib import Path
import sys
from PIL import Image,ImageChops
HERE=Path(__file__).resolve();sys.path.insert(0,str(HERE.parents[2]/'common/study/tests'))
from test_three_key_simulator import ThreeKey
spec=importlib.util.spec_from_file_location('sibo_flows',HERE.with_name('test_simulator.py'))
f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
COLORS=[(32,134,107),(255,237,202),(224,241,229),(227,239,255),(238,231,255),(255,232,221),(232,242,223),(239,237,229)]
HOME_COLORS=[(215,238,231)]+COLORS[1:]
def regions(path,color):
    with Image.open(path) as im:
        im=im.convert('RGB');w,h=im.size
        parts=[c.point(lambda x,v=v:255 if abs(x-v)<=5 else 0) for c,v in zip(im.split(),color)]
        data=bytearray(ImageChops.multiply(ImageChops.multiply(parts[0],parts[1]),parts[2]).tobytes())
    out=[];start=0
    while True:
        start=data.find(255,start)
        if start<0:break
        data[start]=0;q=deque([start]);left=right=start%w;top=bottom=start//w;area=0
        while q:
            i=q.popleft();x=i%w;y=i//w;area+=1;left=min(left,x);right=max(right,x);top=min(top,y);bottom=max(bottom,y)
            for n in ((i-1 if x else -1),(i+1 if x+1<w else -1),i-w,i+w):
                if 0<=n<len(data) and data[n]:data[n]=0;q.append(n)
        if area>500:out.append((area,(left,top,right+1,bottom+1)))
    return [box for _,box in sorted(out,reverse=True)]
def tap(sim,box):sim.tap((box[0]+box[2])/2,(box[1]+box[3])/2)
def primary(sim,name):
    boxes=regions(sim.screenshot(name),COLORS[0]);assert boxes,(name,'no primary action');return boxes[0]
def exercise(sim,w,h):
    home=sim.screenshot('home');tiles=[]
    for color in HOME_COLORS:
        boxes=regions(home,color);assert boxes,(w,h,color,'missing home tile');tiles.append(boxes[0])
    for left,top,right,bottom in tiles:
        assert right-left>=100 and bottom-top>=56,(w,h,(left,top,right,bottom),'touch target too small')
        assert 8<=left<right<=w-8 and 40<=top<bottom<=h-8,(w,h,(left,top,right,bottom),'tile outside viewport')
    assert len({box[0] for box in tiles})>=2,'Home must use multiple columns'
    tap(sim,tiles[0]);sim.expect('课本古诗')
    tap(sim,primary(sim,'poetry'));sim.expect('江南')
    library=sim.screenshot('library');cards=regions(library,COLORS[2])
    cells=[b for b in cards if b[2]-b[0]>w*.18]
    top=min(b[1] for b in cells)
    first=min([b for b in cells if abs(b[1]-top)<5],key=lambda b:b[0])
    candidates=[b for b in cards if b[0]>first[2]-10 and abs(b[1]-first[1])<5]
    second=min(candidates,key=lambda b:b[0]) if candidates else None
    assert second,(w,h,'Library must place another title beside the first')
    tap(sim,second);sim.expect('开始阅读');tap(sim,primary(sim,'poem-detail'));sim.expect('下一页','读完啦')
    assert f.personal(sim)['bookmark']['id']=='p002','Pointer hit the wrong grid item'
    sim.screenshot('poem-reader');f.returned(sim);f.returned(sim);f.returned(sim);f.returned(sim)
    # A continuous paragraph checks that reading wraps to the actual screen,
    # rather than retaining the narrow small-device line length.
    f.seed(sim,f.ORIGIN+'/oss/stories/catalog.json',{'items':[{'id':'story-0002','title':'春天的小发现','item':'items/story-0002.json','ageMin':3,'ageMax':10,'minutes':2}]})
    tap(sim,tiles[1]);sim.expect('春天的小发现')
    f.seed(sim,f.ORIGIN+'/oss/stories/items/story-0002.json',{'text':'春天的风轻轻吹过森林，小熊发现了许多有趣的变化。'*30,'questions':[{'type':'choice','prompt':'谁在寻找春天？','options':['小兔','小熊','小鸟','小猫'],'answer':1,'explanation':'小熊发现了春天的变化。'}]})
    story_list=sim.screenshot('stories')
    story_cards=[b for b in regions(story_list,COLORS[2]) if b[2]-b[0]>w*.18]
    tap(sim,min(story_cards,key=lambda b:(b[1],b[0])));sim.expect('开始阅读');tap(sim,primary(sim,'story-detail'))
    picture=sim.screenshot('wide-reader')
    with Image.open(picture) as im:
        # Dark body text, excluding the fixed header and bottom controls.
        body=im.convert('RGB').crop((0,90,w,h-90));pixels=body.load();xs=[]
        for y in range(body.height):
            for x in range(w):
                r,g,b=pixels[x,y]
                if 20<r<65 and 45<g<90 and 40<b<85:xs.append(x)
        assert xs and max(xs)-min(xs)>w*.65,(w,h,'Reader still uses a narrow column')
    assert f.personal(sim)['bookmark']['id']=='story-0002'
    f.returned(sim)
    detail=sim.screenshot('story-with-quiz')
    controls=[b for b in regions(detail,COLORS[2]) if b[2]-b[0]>w*.25]
    tap(sim,min(controls,key=lambda b:b[1]))
    quiz=sim.screenshot('touch-quiz')
    options=sorted([b for b in regions(quiz,COLORS[2]) if b[2]-b[0]>w*.25],key=lambda b:(b[1],b[0]))
    assert len(options)==4,(w,h,options)
    rows=[]
    for box in options:
        if not rows or abs(box[1]-rows[-1][0][1])>8:rows.append([box])
        else:rows[-1].append(box)
    options=[box for row in rows for box in sorted(row,key=lambda b:b[0])]
    before=f.personal(sim)['correct'];tap(sim,options[1])
    assert f.personal(sim)['correct']==before+1,'Touch answer index did not match its tile'
    sim.screenshot('touch-correct')
    print(f'{w}x{h}: visible home tiles, multi-column catalog, pointer hit-testing and full-width reading passed',flush=True)
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','package-root','executable','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--sizes',nargs='*',default=['320x480','480x800','800x480','1024x768'])
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    for size in a.sizes:
        w,h=map(int,size.split('x'))
        with ThreeKey(a.workspace.resolve(),a.package_root.resolve(),a.output.resolve(),'sibo-learning',w,h,'touch',executable=a.executable.resolve()) as sim:exercise(sim,w,h)
if __name__=='__main__':main()
