#!/usr/bin/env python3
"""Real PXA text containers: no phantom scripts/images, safe DOM repaint."""
import argparse
import json
from pathlib import Path
from PIL import Image
from text_mode_fixture import Fixture
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
    def proof(name):
        count=len(points(name,(24,122,66)));assert count>15,(name,count)
        rows.append({'flow':name,'green_proof_pixels':count})
    def tap(points_):
        assert len(points_)>15
        b.tap((min(v[0] for v in points_)+max(v[0] for v in points_))//2,
              (min(v[1] for v in points_)+max(v[1] for v in points_))//2)
    def load(name,scripted=False):
        mark=len(b.logs());b.navigate(root+'/'+name);b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:])
        if scripted:b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:])
        else:assert 'BROWSER JS OPEN' not in b.logs()[mark:],name
        return mark
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for tag in ('title','textarea','xmp','iframe','noembed','noframes','plaintext'):
            load('static-'+tag);proof('static-'+tag)
            if tag=='iframe':
                assert not f.counts['/frame-target'];mark=len(b.logs());tap(points('iframe-link',(20,105,172)))
                b.wait(lambda:f.counts['/frame-target']==1 and 'BROWSER DONE status=200' in b.logs()[mark:]);proof('frame-explicit-open')
        load('update',True);mark=len(b.logs());tap(points('update-before',(208,96,32)))
        b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);proof('safe-update')
        load('script-plaintext',True);proof('script-plaintext')
        assert not {k:v for k,v in f.counts.items() if k.startswith('/trap')},f.counts
        assert 'BROWSER JS ERROR' not in b.logs() and 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        (a.output/'results.json').write_text(json.dumps({'flows':rows,'requests':dict(f.counts)},ensure_ascii=False,indent=2)+'\n')
        print('HTML text-mode simulator PASS:',len(rows),'flows; no phantom script/image requests',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
