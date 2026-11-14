#!/usr/bin/env python3
"""Real native multiline editing, keyboard Enter and exact HTTP submissions."""
import argparse
from collections import Counter
import json
from pathlib import Path
import time
from PIL import Image
from harness import Browser
from textarea_fixture import Fixture, EDITED


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
    rows=[]
    def load(name):
        mark=len(b.logs());b.navigate(root+'/'+name);b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:])
        if name.startswith('script-'):b.wait(lambda:'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:])
    def pixels(name,color):
        b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content')
        points=[(px,py) for py in range(y,min(y+h,im.height)) for px in range(x,min(x+w,im.width))
                if all(abs(v-c)<9 for v,c in zip(im.getpixel((px,py)),color))]
        if color==(21,108,168):
            # A field preview uses a similar blue; only the submit button
            # paints a background across almost the entire content width.
            widths=Counter(py for _,py in points)
            points=[point for point in points if widths[point[1]]>=w*3//4]
        return points
    def center(points):
        assert len(points)>15
        return ((min(x for x,_ in points)+max(x for x,_ in points))//2,
                (min(y for _,y in points)+max(y for _,y in points))//2)
    def open_area(name,bare=False):
        x,y,w,h=b.box('content');mark=len(b.logs())
        b.tap(x+w//4,y+b.dp(15 if bare else 37))
        b.wait(lambda:'BROWSER EDIT ' in b.logs()[mark:] and 'multiline=1' in b.logs()[mark:])
        b.shot(name+'-editor')
    def done():
        x,y,w,h=b.box('address');b.tap(x+w-b.dp(15),y+h//2)
    key_number=0
    def keyboard_key(which):
        nonlocal key_number
        key_number+=1;name=f'keyboard-{key_number:02d}-{which}';b.shot(name)
        im=Image.open(a.output/(name+'.png')).convert('RGB')
        # Native IME keys have separate, rounded backgrounds. Locate the
        # wide right-hand keys, rather than sending SDL navigation keys to
        # an LVGL group that does not own this programmatically focused input.
        remaining={(x,y) for y in range(im.height//4,im.height) for x in range(im.width*7//10,im.width)
                   if im.getpixel((x,y))==(45,62,80)}
        boxes=[]
        while remaining:
            todo=[remaining.pop()];points=[]
            while todo:
                x,y=todo.pop();points.append((x,y))
                for p in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
                    if p in remaining:remaining.remove(p);todo.append(p)
            left=min(x for x,_ in points);right=max(x for x,_ in points)
            top=min(y for _,y in points);bottom=max(y for _,y in points)
            if right-left>=im.width*.12 and right>im.width*.92 and len(points)>100:
                boxes.append((left,top,right,bottom))
        boxes.sort(key=lambda box:box[1]);assert len(boxes)>=3,(which,boxes)
        left,top,right,bottom=boxes[-2 if which=='enter' else -3]
        b.tap((left+right)//2,(top+bottom)//2);time.sleep(.2)
    def type_text(text):
        for at in range(0,len(text),30):b.command('TEXT '+text[at:at+30]);time.sleep(.04)
    def type_lines(text):
        for index,line in enumerate(text.split('\n')):
            if index:keyboard_key('enter')
            if line:type_text(line);time.sleep(.15)
    def submit(name):
        before=len(f.submissions);mark=len(b.logs())
        color=(208,96,32) if name.startswith('script-') else (21,108,168)
        b.tap(*center(pixels(name+'-before-submit',color)))
        b.wait(lambda:len(f.submissions)>before and 'BROWSER DONE status=' in b.logs()[mark:])
        assert len(f.submissions)==before+1 and f.submissions[-1]['valid'],f.submissions
        assert 'BROWSER DONE status=200' in b.logs()[mark:],b.logs()[mark:]
        b.shot(name+'-pass');rows.append(dict(flow=name,request=f.submissions[-1]))
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        load('initial');mark=len(b.logs());b.tap(*center(pixels('readonly',(24,122,66))));time.sleep(.2)
        assert 'BROWSER EDIT ' not in b.logs()[mark:]
        rows.append(dict(flow='readonly-not-editable'));submit('initial')
        for name in ('post-edit','get-edit','script-edit'):
            load(name);open_area(name);type_lines(EDITED);b.shot(name+'-typed');mark=len(b.logs());done()
            if name.startswith('script-'):b.wait(lambda:'BROWSER JS UPDATE revision=2 ' in b.logs()[mark:])
            submit(name)
        load('script-preset');submit('script-preset')
        load('boundary');open_area('boundary');type_text('x'*510);time.sleep(.2)
        b.command('TEXT 中');b.command('TEXT ab');time.sleep(.2);done();submit('boundary')
        load('overflow');before=len(f.submissions);mark=len(b.logs())
        b.tap(*center(pixels('overflow',(21,108,168))));time.sleep(.3)
        assert len(f.submissions)==before and 'BROWSER NAV ' not in b.logs()[mark:]
        rows.append(dict(flow='overflow-has-no-partial-submission'))
        load('plain-limit');x,y,w,h=b.box('content');b.tap(x+w//4,y+b.dp(15));time.sleep(.25)
        type_text('x'*513);time.sleep(.2);done();time.sleep(.2)
        b.command('TEXT a');keyboard_key('backspace');keyboard_key('backspace')
        done();submit('plain-limit')
        load('switch');open_area('switch',True);type_lines(EDITED);done();x,y,w,h=b.box('content')
        mark=len(b.logs());b.tap(x+w//4,y+b.dp(45));b.wait(lambda:'multiline=0' in b.logs()[mark:])
        b.command('TEXT private');keyboard_key('enter');b.shot('switch-password-closed');submit('switch')
        b.command('KEY HOME');b.wait(lambda:'PXA RESOURCE BUDGET' in b.logs())
        budget=[s for s in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in s][-1]
        assert 'remaining_internal=0 remaining_external=0' in budget,budget
        assert all(error not in b.logs() for error in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
        assert len(rows)==10,len(rows)
        (a.output/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
        print('textarea simulator PASS:',len(rows),'flows; native Enter, Unicode, scripts, readonly/disabled/hidden, budgets and exact wire data',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
