#!/usr/bin/env python3
"""Real PXA downloads, decoder limits, picture links and comic detail mode."""
import argparse
import json
from pathlib import Path
import re
import time
from PIL import Image
from harness import Browser
from responsive_fixture import Fixture

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    args=p.parse_args();fixture=Fixture().start();root='http://127.0.0.1:'+str(fixture.server.server_port)
    b=Browser(args.simulator,args.package,args.publisher_key,args.ime_package,args.output,
              width=args.width,height=args.height,corner=0)
    rows=[]
    def decoded(mark):
        b.wait(lambda:'BROWSER IMAGE DONE index=0' in b.logs()[mark:])
        match=re.findall(r'BROWSER IMAGE DONE index=0[^\n]*pixels=(\d+)x(\d+) resident=(\d+)',b.logs()[mark:])[-1]
        return {'width':int(match[0]),'height':int(match[1]),'resident':int(match[2])}
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for name,path in [('picture','/page1.jpg'),('srcset','/page2.png'),('lazy','/page1.jpg'),
                          ('lazyset','/cdn,page2.png'),('script','/page2.png'),
                          ('script-picture','/page3.jpg'),('linked','/page2.png'),('limits','/page3.jpg')]:
            before=fixture.counts[path];mark=len(b.logs());b.navigate(root+'/'+name)
            b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:])
            if name.startswith('script'):b.wait(lambda:'BROWSER JS READY' in b.logs()[mark:])
            normal=decoded(mark);assert normal['width']*normal['height']<=32768,normal
            assert fixture.counts[path]>before
            b.shot(name)
            im=Image.open(args.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content')
            colored=sum(max(im.getpixel((px,py)))-min(im.getpixel((px,py)))>70
                        for py in range(y,min(y+h,im.height)) for px in range(x,min(x+w,im.width)))
            assert colored>100, 'decoded image was not painted'
            if name=='picture':
                b.nav(4);time.sleep(.3);x,y,w,h=b.box('content');b.tap(x+w-2,y+h-1)
                b.tap(x+w//2,y+h-b.dp(18))
                b.wait(lambda:'BROWSER COMIC index=0 page=1 count=1' in b.logs()[mark:])
                mark2=len(b.logs());b.nav(3);detail=decoded(mark2)
                assert 32768<detail['width']*detail['height']<=262144,detail
                b.shot('picture-comic-detail');navs=b.logs().count('BROWSER NAV ');b.nav(4)
                assert b.logs().count('BROWSER NAV ')==navs
                rows.append({'flow':'picture comic detail','decode':detail})
            if name=='linked':
                x,y,w,h=b.box('content');mark2=len(b.logs());b.tap(x+w//2,y+80)
                b.wait(lambda:'BROWSER NAV '+root+'/target' in b.logs()[mark2:])
                b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark2:]);b.shot('linked-target')
            rows.append({'flow':name,'requested':path,'normal':normal,'colored_pixels':colored})
        for path in ('/blocked-type','/blocked-fallback.svg','/invalid.jpg','/blocked.webp','/blocked.avif',
                     '/loading.gif','/unused.jpg','/unused-large.jpg','/late.jpg'):
            assert fixture.counts[path]==0,(path,fixture.counts[path])
        assert 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        (args.output/'results.json').write_text(json.dumps({'flows':rows,'requests':fixture.counts},indent=2)+'\n')
        print('responsive image simulator PASS:',len(rows),'flows; unsupported and unused sources not requested')
    finally:b.close();fixture.close()

if __name__=='__main__':main()
