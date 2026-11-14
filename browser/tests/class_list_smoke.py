#!/usr/bin/env python3
"""Actual PXA class changes repaint CSS, retain state and gate image loading."""
import argparse,json,time
from pathlib import Path
from PIL import Image
from class_list_fixture import Fixture
from harness import Browser


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
    colors={'action':(208,96,32),'blue':(36,104,172),'green':(24,122,66)};rows=[];button=None
    def screen(name,expected):
        nonlocal button
        b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content');points={key:[] for key in colors}
        for py in range(y,min(y+h,im.height)):
            for px in range(x,min(x+w,im.width)):
                pixel=im.getpixel((px,py))
                for key,color in colors.items():
                    if all(abs(v-c)<9 for v,c in zip(pixel,color)):points[key].append((px,py))
        found=points['action'];assert len(found)>15,(name,'missing action')
        button=((min(v[0] for v in found)+max(v[0] for v in found))//2,(min(v[1] for v in found)+max(v[1] for v in found))//2)
        for key in ('blue','green'):assert (len(points[key])>15)==(key==expected),(name,key,len(points[key]))
        rows.append({'flow':name,'expected':expected,'color_pixels':{key:len(v) for key,v in points.items()}})
    def load(path):
        mark=len(b.logs());b.navigate(root+path)
        b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:] and 'BROWSER JS UPDATE' in b.logs()[mark:])
    def advance():
        mark=len(b.logs());b.tap(*button);b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);return mark
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs());load('/classes')
        for phase,expected in enumerate(('blue','green',None,'green','blue','green','blue','green',None,'blue','green','green')):
            if phase:advance()
            screen('classes-'+str(phase),expected)
        mark=len(b.logs());b.tap(*button);time.sleep(.5)
        assert 'BROWSER JS UPDATE' not in b.logs()[mark:],'class no-op produced a snapshot';rows.append({'flow':'class-no-op-stays-idle'})
        load('/quota');screen('quota-before','green')
        for phase,expected in enumerate(('green',None,'green'),1):advance();screen('quota-'+str(phase),expected)
        load('/image');screen('image-hidden',None);assert not f.counts['/class-image.png'],f.counts
        mark=advance();b.wait(lambda:'BROWSER IMAGE DONE' in b.logs()[mark:]);screen('image-shown','green');assert f.counts['/class-image.png']==1,f.counts
        advance();screen('image-hidden-again',None);assert f.counts['/class-image.png']==1,f.counts
        assert all(error not in b.logs() for error in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
        assert all(f.counts[path]==1 for path in ('/classes','/quota','/image')),f.counts
        (a.output/'results.json').write_text(json.dumps({'flows':rows,'requests':dict(f.counts)},ensure_ascii=False,indent=2)+'\n')
        print('classList simulator PASS:',len(rows),'flows; CSS, limits, idle and image visibility',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
