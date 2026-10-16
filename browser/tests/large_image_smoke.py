#!/usr/bin/env python3
"""Native Host + real AOT UI/Wasm VM: large images and original documents."""
import argparse
import json
from pathlib import Path
import re
import time
from PIL import Image, ImageChops
from harness import Browser
from geometry import geometry,navigation
from large_image_fixture import Fixture

p=argparse.ArgumentParser(description=__doc__)
for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--width',type=int,default=296);p.add_argument('--height',type=int,default=240)
p.add_argument('--dpi',type=int,default=160);p.add_argument('--round',action='store_true');a=p.parse_args()
fixture=Fixture().start();b=None
try:
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,
              a.width,a.height,a.dpi,58,round_screen=a.round)
    b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
    for case in ('default','meta','header','none','element'):
        mark=len(b.logs());b.navigate(f'http://127.0.0.1:{fixture.page.server_port}/{case}?query=keep#private')
        b.wait(lambda:'BROWSER JS READY' in b.logs()[mark:],timeout=40)
        b.wait(lambda:'BROWSER IMAGE DONE' in b.logs()[mark:],timeout=30)
        b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:])
        assert len(re.findall('BROWSER JS STAGE EXECUTED',b.logs()[mark:]))>=2
        b.shot(case)
        image=Image.open(a.output/(case+'.png')).convert('RGB')
        x,y,w,h=b.box('content')
        crop=(x,y,x+w,y+h)
        assert sum(max(c)-min(c)>65 for c in image.crop(crop).getdata())>500,'decoded pixels missing'
        assert not re.search(r'BROWSER (?:JS|IMAGE|DRAW) ERROR|Guest exception',b.logs()[mark:])
        # The source DOM and script-owned label survive the image's layout update.
        assert 'BROWSER SITE ADAPTER' not in b.logs()[mark:]
    assert fixture.events and all(event['passed'] for event in fixture.events),fixture.events
    assert {event['case'] for event in fixture.events}=={'default','meta','header','none','element'}
    # The existing preview/zoom/pan path borrows the cached image without refetching.
    x,y,w,h=b.box('content');b.tap(x+w//2,y+h*2//3)
    b.wait(lambda:'BROWSER IMAGE VIEW' in b.logs());b.shot('fit');b.tap(*navigation(geometry(b.logs()),3,4));b.shot('zoom')
    assert ImageChops.difference(Image.open(a.output/'fit.png').convert('RGB'),Image.open(a.output/'zoom.png').convert('RGB')).getbbox()
    result={'passed':True,'source':[1199,2533],'encoded_bytes':len(fixture.image),
        'screen':[a.width,a.height,a.dpi],'round':a.round,'referrer_checks':fixture.events,
        'images':re.findall(r'BROWSER IMAGE DONE[^\n]*',b.logs()),'synthetic':True}
    (a.output/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2))
    print('Large image/original DOM/dynamic script/referrer/preview smoke passed')
finally:
    if b:b.close()
    fixture.close()
