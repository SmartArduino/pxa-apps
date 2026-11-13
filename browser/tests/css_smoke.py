#!/usr/bin/env python3
"""Check real Canvas colors and right-aligned link hits before/after JS paint."""
import argparse
import json
from PIL import Image
from css_fixture import Fixture
from harness import Browser

def main():
    from pathlib import Path
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    p.add_argument('--dpi',type=int,default=160)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,dpi=a.dpi,corner=0)
    rows=[]
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for name in ('static','script'):
            mark=len(b.logs());b.navigate(root+'/'+name)
            b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:])
            if name=='script': b.wait(lambda:'BROWSER JS UPDATE revision=2 ' in b.logs()[mark:])
            b.shot(name)
            im=Image.open(a.output/(name+'.png')).convert('RGB')
            x,y,w,h=b.box('content'); points={key:[] for key in ('red','blue','green')}
            for py in range(y,min(y+h,im.height)):
                for px in range(x,min(x+w,im.width)):
                    r,g,blue=im.getpixel((px,py))
                    if abs(r-156)<9 and abs(g-36)<9 and abs(blue-54)<9: points['red'].append((px,py))
                    if abs(r-36)<9 and abs(g-104)<9 and abs(blue-172)<9: points['blue'].append((px,py))
                    if abs(r-24)<9 and abs(g-122)<9 and abs(blue-66)<9: points['green'].append((px,py))
            assert all(len(v)>15 for v in points.values()),{k:len(v) for k,v in points.items()}
            green=points['green'];left=min(v[0] for v in green);right=max(v[0] for v in green)
            assert right>x+w*0.8 and left>x+w*0.25,(left,right,x,w)
            mark=len(b.logs());b.tap((left+right)//2,(min(v[1] for v in green)+max(v[1] for v in green))//2)
            b.wait(lambda:'BROWSER NAV '+root+'/target' in b.logs()[mark:] and 'BROWSER DONE status=200' in b.logs()[mark:])
            b.shot(name+'-link-target')
            assert f.requests['/hidden-image.png']==0 and f.requests['/hidden-link']==0
            rows.append({'page':name,'color_pixels':{k:len(v) for k,v in points.items()},'aligned_link_hit':True,'hidden_requests':0})
        assert 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        (a.output/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
        print('CSS simulator PASS:',len(rows),'static/script flows')
    finally: b.close();f.close()

if __name__=='__main__': main()
