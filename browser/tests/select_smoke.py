#!/usr/bin/env python3
"""Native select menus, duplicate indices, multiple picks and exact wire data."""
import argparse
import json
from pathlib import Path
import time
from PIL import Image,ImageChops
from harness import Browser
from select_fixture import Fixture


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0);rows=[]
    def load(name):
        mark=len(b.logs());b.navigate(root+'/'+name);b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:])
        if name.startswith('script-'):b.wait(lambda:'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:])
    def open_menu(name):
        b.shot(name+'-before');im=Image.open(a.output/(name+'-before.png')).convert('RGB');x,y,w,h=b.box('content')
        ys=[py for py in range(y,y+h) if im.getpixel((x+8,py))==(232,238,243) and im.getpixel((x+w-8,py))==(232,238,243)]
        assert ys,(name,b.logs()[-1500:]);b.tap(x+w//4,min(ys)+7);b.shot(name+'-menu')
    def pick(index,total,hidden=(),bottom=False):
        x,y,w,h=b.box('content');rank=index-sum(i<index for i in hidden);scroll=0
        if bottom:
            b.tap(x+w-2,y+h-1);scroll=max(0,(total-len(hidden)+1)*35-h)
        cy=y+35*(rank+1)-scroll+16;assert y<=cy<y+h,(index,cy,(x,y,w,h),scroll)
        b.tap(x+w//3,cy)
    def done():
        x,y,w,h=b.box('content');b.tap(x+w//3,y+16)
    def submit_point(name,scripted=False):
        b.shot(name+'-before-submit');im=Image.open(a.output/(name+'-before-submit.png')).convert('RGB');x,y,w,h=b.box('content')
        crop=im.crop((x,y,x+w,y+h));color=(208,96,32) if scripted else (21,108,168)
        delta=ImageChops.difference(crop,Image.new('RGB',crop.size,color));r,g,blue=delta.split()
        mask=ImageChops.lighter(ImageChops.lighter(r,g),blue).point(lambda v:255 if v<(9 if scripted else 1) else 0)
        box=mask.getbbox();assert box,(name,'submit control absent')
        return x+(box[0]+box[2])//2,y+(box[1]+box[3])//2
    def submit(name,scripted=False):
        before=len(f.submissions);mark=len(b.logs());b.tap(*submit_point(name,scripted))
        b.wait(lambda:len(f.submissions)==before+1 and 'BROWSER DONE status=200' in b.logs()[mark:])
        assert f.submissions[-1]['valid'],f.submissions;b.shot(name+'-pass');rows.append(dict(flow=name,request=f.submissions[-1]))
    def records(name,before,count,index=None):
        b.wait(lambda:len(f.events)>=before+count);time.sleep(.2)
        found=sorted(f.events[before:],key=lambda e:int(e['seq']));assert len(found)==count,found
        assert [row['event'] for row in found]==['input','change']*(count//2),found
        if index is not None:assert all(int(row['index'])==index for row in found),found
        rows.append(dict(flow=name,events=found))
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for name in ('single-post','single-get'):
            load(name);open_menu(name);pick(2,4);submit(name)
        load('multi');open_menu('multi');pick(0,4,hidden=(3,));pick(1,4,hidden=(3,));pick(2,4,hidden=(3,));done();submit('multi')
        load('script-duplicate');before=len(f.events);open_menu('script-duplicate');pick(2,3);records('duplicate-index',before,2,2)
        before=len(f.events);mark=len(b.logs());open_menu('duplicate-again');pick(2,3);time.sleep(.4)
        assert len(f.events)==before and 'BROWSER JS UPDATE' not in b.logs()[mark:];rows.append(dict(flow='same-choice',callbacks=0,extra_snapshots=0))
        submit('script-duplicate',True)
        load('script-multi');open_menu('script-multi');before=len(f.events);pick(0,4,hidden=(3,));records('multiple-first',before,2)
        before=len(f.events);pick(1,4,hidden=(3,));records('multiple-second',before,2);done();submit('script-multi',True)
        load('script-dynamic');before=len(f.events);open_menu('script-dynamic');pick(1,3);records('dynamic-selection',before,2,1);submit('script-dynamic',True)
        load('scroll');open_menu('scroll');pick(63,64,bottom=True);submit('scroll')
        for name in ('disabled','none'):
            load(name);submit(name)
        load('hidden');open_menu('hidden');pick(0,4);submit('hidden')
        load('overflow');before=len(f.submissions);b.shot('overflow');b.tap(*submit_point('overflow'));time.sleep(.4)
        assert len(f.submissions)==before;rows.append(dict(flow='overflow',submissions=0))
        load('early');b.wait(f.delay_started.is_set);open_menu('early');before=len(f.events);pick(1,3)
        records('early-selection-preserved',before,2,2);submit('early',True)
        b.command('KEY HOME');b.wait(lambda:'PXA RESOURCE BUDGET' in b.logs());budget=[line for line in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in line][-1]
        assert 'remaining_internal=0 remaining_external=0' in budget
        assert all(error not in b.logs() for error in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows,budget=budget),ensure_ascii=False,indent=2)+'\n')
        print('Select simulator PASS:',len(rows),'flows; single/multiple, disabled/hidden, duplicate indices, early edits, scrolling and exact wire data',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
