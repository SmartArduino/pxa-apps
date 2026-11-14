#!/usr/bin/env python3
"""Actual PXA staged DOM, connection APIs, reattachment and event targets."""
import argparse,json
from pathlib import Path
from PIL import Image,ImageChops
from harness import Browser
from connection_fixture import Fixture

COLORS={'stage':(208,96,32),'mount':(36,104,172),'remove':(103,58,183),'finish':(153,50,64),'staged':(24,122,66)}


def point(browser,output,name,color):
    browser.shot(name);im=Image.open(output/(name+'.png')).convert('RGB');x,y,w,h=browser.box('content');im=im.crop((x,y,x+w,y+h))
    d=ImageChops.difference(im,Image.new('RGB',im.size,color));r,g,b=d.split();mask=ImageChops.lighter(ImageChops.lighter(r,g),b).point(lambda v:255 if v<8 else 0);box=mask.getbbox()
    return None if box is None else (x+(box[0]+box[2])//2,y+(box[1]+box[3])//2)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480);a=p.parse_args();f=Fixture().start()
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0);rows=[]
    def perform(action,connected,n,visible=None):
        before=len(f.events);mark=len(b.logs());at=point(b,a.output,action+'-'+str(before),COLORS[action]);assert at,(action,'missing button');b.tap(*at)
        b.wait(lambda:len(f.events)==before+1);event=f.events[-1];assert event==dict(event='remove' if action=='remove' else action,n=str(n),connected=str(int(connected))),event
        if visible is not None:
            b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);assert (point(b,a.output,'visible-'+str(before),COLORS['staged']) is not None)==visible
        rows.append(dict(flow=action,**event))
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs());mark=len(b.logs());b.navigate('http://127.0.0.1:'+str(f.server.server_port)+'/connection')
        b.wait(lambda:'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:]);assert point(b,a.output,'initial',COLORS['staged']) is None
        for n in (1,2,3):perform('stage',False,n)
        perform('mount',True,3,True)
        before=len(f.events);mark=len(b.logs());b.tap(*point(b,a.output,'mounted-target',COLORS['staged']));b.wait(lambda:len(f.events)==before+1 and 'BROWSER JS UPDATE' in b.logs()[mark:]);assert f.events[-1]['event']=='clicked';rows.append(dict(flow='staged-handler',**f.events[-1]))
        perform('remove',False,3,False)
        for n in (4,5):perform('stage',False,n)
        perform('mount',True,5,True);perform('stage',True,6,True);perform('remove',False,6,False);perform('finish',False,6,False)
        assert all(s not in b.logs() for s in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
        b.command('KEY HOME');b.wait(lambda:'PXA RESOURCE BUDGET' in b.logs());budget=[s for s in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in s][-1];assert 'remaining_internal=0 remaining_external=0' in budget
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows,budget=budget),indent=2)+'\n');print('Connection simulator PASS:',len(rows),'flows; staging, connection APIs, handlers, attach/remove/reinsert and resource release',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
