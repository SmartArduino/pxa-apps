#!/usr/bin/env python3
"""Real PXA fragment rejection preserves UI, old handlers and later updates."""
import argparse
import json
from pathlib import Path
from PIL import Image
from fragment_fixture import CASES,Fixture
from harness import Browser


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
    rows=[]
    def points(name,color):
        b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content')
        return [(px,py) for py in range(y,min(y+h,im.height)) for px in range(x,min(x+w,im.width))
                if all(abs(v-c)<9 for v,c in zip(im.getpixel((px,py)),color))]
    def click(name,color):
        found=points(name,color);assert len(found)>15,(name,len(found))
        b.tap((min(v[0] for v in found)+max(v[0] for v in found))//2,
              (min(v[1] for v in found)+max(v[1] for v in found))//2)
    def proof(name):
        count=len(points(name,(24,122,66)));assert count>15,(name,count)
        rows.append({'flow':name,'green_proof_pixels':count})
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for case in CASES:
            mark=len(b.logs());b.navigate(root+'/'+case)
            b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:] and 'BROWSER JS UPDATE' in b.logs()[mark:])
            for phase,color in (('reject',(208,96,32)),('old-click',(20,105,172)),
                                ('recover',(208,96,32)),('new-click',(20,105,172))):
                name=case+'-'+phase;mark=len(b.logs());click(name+'-before',color)
                b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);proof(name)
        mark=len(b.logs());b.navigate(root+'/tags')
        b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:] and 'BROWSER JS UPDATE' in b.logs()[mark:]);proof('tags-initial')
        for phase in range(1,4):
            mark=len(b.logs());click('tags-'+str(phase)+'-before',(208,96,32))
            b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);proof('tags-'+str(phase))
            if phase==2:assert 'fields=1' in b.logs()[mark:],'cached tag changed the form field count'
        assert not {k:v for k,v in f.counts.items() if k.startswith('/trap')},f.counts
        assert all(error not in b.logs() for error in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
        (a.output/'results.json').write_text(json.dumps({'flows':rows,'requests':dict(f.counts)},ensure_ascii=False,indent=2)+'\n')
        print('Fragment simulator PASS:',len(rows),'flows; failure recovery and old/new handlers',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
