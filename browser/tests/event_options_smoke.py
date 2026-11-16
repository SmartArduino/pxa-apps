#!/usr/bin/env python3
"""Exercise event listener phases and once hit removal on a real PXA UI."""
import argparse
import json
from pathlib import Path
import re
import time
from PIL import Image
from event_fixture import Fixture
from image_response_smoke import StableBrowser


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=296);p.add_argument('--height',type=int,default=240)
    a=p.parse_args();fixture=Fixture().start();b=None
    try:
        b=StableBrowser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
        b.wait(lambda:'BROWSER VIEWPORT'in b.logs());mark=len(b.logs());b.navigate('http://127.0.0.1:'+str(fixture.server.server_port)+'/listener-options')
        updates=re.compile(r'BROWSER JS UPDATE revision=(\d+) text=\d+ links=(\d+)')
        b.wait(lambda:updates.search(b.logs()[mark:]));rows=[]
        def point(name,color):
            b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content')
            pixels=[(px,py)for py in range(y,y+h)for px in range(x,x+w)
                    if all(abs(c-v)<9 for c,v in zip(im.getpixel((px,py)),color))]
            assert len(pixels)>10,(name,color)
            return ((min(x for x,y in pixels)+max(x for x,y in pixels))//2,(min(y for x,y in pixels)+max(y for x,y in pixels))//2)
        for run in (1,2):
            xy=point('before-run-'+str(run),(208,96,32));mark=len(b.logs());b.tap(*xy)
            b.wait(lambda:updates.search(b.logs()[mark:]));b.shot('run-'+str(run))
            rows.append({'run':run,'links':int(list(updates.finditer(b.logs()[mark:]))[-1][2])})
        xy=point('before-once',(36,104,172));mark=len(b.logs());b.tap(*xy)
        b.wait(lambda:updates.search(b.logs()[mark:]));after=int(list(updates.finditer(b.logs()[mark:]))[-1][2]);assert after==rows[-1]['links']-1
        b.shot('once-released');mark=len(b.logs());b.tap(*xy);time.sleep(.5)
        assert not updates.search(b.logs()[mark:]),'Consumed one-time target was still clickable'
        mark=len(b.logs());b.navigate('http://127.0.0.1:'+str(fixture.server.server_port)+'/event-intrinsics')
        b.wait(lambda:updates.search(b.logs()[mark:]));xy=point('intrinsics-before',(208,96,32));mark=len(b.logs());b.tap(*xy)
        b.wait(lambda:updates.search(b.logs()[mark:]));b.shot('intrinsics-passed')
        assert all(s not in b.logs()for s in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
        b.command('KEY HOME');b.wait(lambda:'PXA RESOURCE BUDGET'in b.logs());budget=next(s for s in reversed(b.logs().splitlines())if 'PXA RESOURCE BUDGET'in s)
        assert 'remaining_internal=0 remaining_external=0'in budget
        (a.output/'result.json').write_text(json.dumps({'passed':True,'runs':rows,'once_links_after':after,'resource_release':budget},indent=2)+'\n')
        print('Event options simulator PASS',flush=True)
    finally:
        if b:b.close()
        fixture.close()


if __name__=='__main__':main()
