#!/usr/bin/env python3
"""Real PXA selector-driven images, first-link navigation and bound events."""
import argparse
import json
import re
from pathlib import Path
from PIL import Image
from selector_fixture import Fixture
from harness import Browser


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
    rows=[]
    def load(name):
        mark=len(b.logs());b.navigate(root+'/'+name)
        b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:] and 'BROWSER JS UPDATE' in b.logs()[mark:])
        return mark
    def pixels(name,color):
        b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content')
        return [(px,py) for py in range(y,min(y+h,im.height)) for px in range(x,min(x+w,im.width))
                if all(abs(v-c)<9 for v,c in zip(im.getpixel((px,py)),color))]
    def passed(name):
        points=pixels(name,(24,122,66));assert len(points)>15,(name,len(points))
        rows.append({'flow':name,'green_proof_pixels':len(points)})
    def tap_button(name):
        points=pixels(name,(208,96,32));assert len(points)>15,name
        b.tap((min(v[0] for v in points)+max(v[0] for v in points))//2,
              (min(v[1] for v in points)+max(v[1] for v in points))//2)
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        mark=load('gallery');b.wait(lambda:'BROWSER IMAGE DONE index=0' in b.logs()[mark:]);passed('gallery')
        decoded=re.findall(r'BROWSER IMAGE DONE index=0[^\n]*pixels=(\d+)x(\d+)',b.logs()[mark:])[-1]
        assert all(int(v)>32 for v in decoded),decoded
        assert f.counts['/page1.jpg']==1 and not f.counts['/unused.jpg'],f.counts
        rows[-1].update(image_pixels=list(map(int,decoded)))
        load('order');passed('order');mark=len(b.logs());tap_button('order-before-click')
        b.wait(lambda:f.counts['/chosen-first']==1 and 'BROWSER DONE status=200' in b.logs()[mark:]);passed('first-link-navigation')
        assert not f.counts['/wrong-second'],f.counts
        load('events');mark=len(b.logs());tap_button('event-before-click')
        b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);passed('bound-event')
        for name in ('scope','errors','static'):load(name);passed(name)
        assert 'BROWSER JS ERROR' not in b.logs() and 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        (a.output/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
        print('selector simulator PASS:',len(rows),'flows; real image requests, DOM order and event activation',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
