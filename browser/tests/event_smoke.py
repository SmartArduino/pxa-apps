#!/usr/bin/env python3
"""Real PXA idle snapshots and dynamic hit registration/removal through input."""
import argparse
import json
import re
import time
from pathlib import Path
from PIL import Image
from event_fixture import Fixture
from harness import Browser


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
    rows=[]
    pattern=re.compile(r'BROWSER JS UPDATE revision=(\d+) text=\d+ links=(\d+) fields=\d+ markup_us=\d+ markup_turns=(\d+)')
    def updates(mark):
        return [dict(revision=int(m[1]),links=int(m[2]),batches=int(m[3])) for m in pattern.finditer(b.logs()[mark:])]
    def load(path):
        mark=len(b.logs());b.navigate(root+path)
        b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:] and updates(mark))
        return mark
    def wait_update(mark, links):
        b.wait(lambda:updates(mark))
        result=updates(mark)[-1];assert result['links']==links,result
        return result
    def point(name, color):
        b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content');pixels=[]
        for py in range(y,min(y+h,im.height)):
            for px in range(x,min(x+w,im.width)):
                if all(abs(v-c)<9 for v,c in zip(im.getpixel((px,py)),color)):pixels.append((px,py))
        assert len(pixels)>15,(name,color)
        return ((min(v[0] for v in pixels)+max(v[0] for v in pixels))//2,
                (min(v[1] for v in pixels)+max(v[1] for v in pixels))//2)
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        mark=load('/idle');b.wait(lambda:len(updates(mark))>=2)
        idle=updates(mark);assert len(idle)==2 and idle[1]['batches']==2*idle[0]['batches'],idle
        b.shot('idle-complete');rows.append({'page':'idle','callbacks':12,'snapshots':2,'trace':idle})
        for page,states in [('events',[2,1,2,1]),('inline-events',[1,2,1])]:
            mark=load('/'+page)
            if page=='inline-events':
                mark=len(b.logs());b.tap(*point(page+'-initial',(36,104,172)));wait_update(mark,2)
                rows.append({'page':page,'initial_inline_click':True})
            for phase,links in enumerate(states,1):
                control=point(page+'-control-'+str(phase),(208,96,32));mark=len(b.logs());b.tap(*control)
                update=wait_update(mark,links)
                target=point(page+'-phase-'+str(phase),(36,104,172));mark=len(b.logs());b.tap(*target)
                if links==2:wait_update(mark,2)
                else:
                    time.sleep(.35);assert not updates(mark),'removed handler remained clickable'
                rows.append({'page':page,'phase':phase,'links':links,'target_clicks':links==2,'trace':update})
        mark=load('/xhr-events');b.wait(lambda:len(updates(mark))>=2)
        xhr=updates(mark);assert xhr[0]['links']==0 and xhr[-1]['links']==1,xhr
        target=point('xhr-bound',(36,104,172));mark=len(b.logs());b.tap(*target);wait_update(mark,1)
        b.shot('xhr-clicked');rows.append({'page':'xhr-events','registered_without_dom_text_edit':True})
        assert f.requests['/bind-handler']==1 and all(f.requests[path]==1 for path in ('/idle','/events','/inline-events','/xhr-events'))
        assert 'BROWSER JS ERROR' not in b.logs() and 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        (a.output/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
        print('event simulator PASS:',len(rows),'flows; idle callbacks add zero snapshot batches',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
