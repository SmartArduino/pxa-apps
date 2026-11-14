#!/usr/bin/env python3
"""Native PXA fragment staging, node identity, controls and script loads."""
import argparse,json
from pathlib import Path
from PIL import Image
from harness import Browser
from connection_smoke import point,COLORS as CONNECTION_COLORS
from dom_batch_fixture import Fixture
COLORS={**CONNECTION_COLORS,'mount':(112,32,128)}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    p.add_argument('--probe',action='store_true',help='Record baseline API availability without requiring support')
    a=p.parse_args();f=Fixture().start();b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0);rows=[]
    def load(case):
        mark=len(b.logs());before=len(f.events);b.navigate('http://127.0.0.1:'+str(f.server.server_port)+'/'+case);b.wait(lambda:'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:] and len(f.events)>before)
        event=f.events[-1];assert event['event']=='ready';assert a.probe or event['value']=='1',event;return event
    def action(name,case,value=None):
        before=len(f.events);mark=len(b.logs());at=point(b,a.output,case+'-'+name+'-'+str(before),COLORS[name]);assert at;b.tap(*at);b.wait(lambda:any(e['event']==('clear' if name=='remove' else name) for e in f.events[before:]))
        event=next(e for e in f.events[before:] if e['event']==('clear' if name=='remove' else name));assert value is None or event['value']==value,event
        rows.append(dict(flow=case+'-'+name,event=event));return mark
    def submit(case):
        before=len(f.submissions);mark=len(b.logs());at=point(b,a.output,case+'-submit',COLORS['finish']);assert at;b.tap(*at);b.wait(lambda:len(f.submissions)==before+1 and 'BROWSER DONE status=200' in b.logs()[mark:]);assert f.submissions[-1]['valid'];rows.append(dict(flow=case+'-submit',receipt=f.submissions[-1]))
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs());event=load('list');rows.append(dict(flow='availability',event=event))
        if not a.probe:
            action('stage','list','3');assert point(b,a.output,'list-staged',COLORS['staged']) is None
            mark=action('mount','list','0');b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);at=point(b,a.output,'list-mounted',COLORS['staged']);assert at;before=len(f.events);b.tap(*at);b.wait(lambda:len(f.events)>before);assert f.events[-1]['event']=='clicked' and f.events[-1]['value']=='B';rows.append(dict(flow='list-handler',event=f.events[-1]))
            mark=action('remove','list','1');b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);assert point(b,a.output,'list-cleared',COLORS['staged']) is None
            action('stage','list-again','3');mark=action('mount','list-again','0');b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);assert point(b,a.output,'list-remounted',COLORS['staged'])
            load('select');action('stage','select','2');mark=action('mount','select','0');b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);b.shot('select-mounted');im=Image.open(a.output/'select-mounted.png').convert('RGB');x,y,w,h=b.box('content');ys=[py for py in range(y,y+h) if im.getpixel((x+8,py))==(232,238,243) and im.getpixel((x+w-8,py))==(232,238,243)];assert ys;b.tap(x+w//3,min(ys)+7);b.shot('select-menu')
            before=len(f.events);b.tap(x+w//3,y+35*3+16);b.wait(lambda:len(f.events)>before);assert f.events[-1]['event']=='choice' and f.events[-1]['value']=='a';rows.append(dict(flow='select-user-choice',event=f.events[-1]));submit('select')
            load('radio');action('stage','radio','2');mark=action('mount','radio','0');b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);submit('radio')
            load('scripts');action('stage','scripts','3');assert not f.scripts;action('mount','scripts','0');b.wait(lambda:any(e['event']=='loaded' for e in f.events));assert f.scripts==['/one.js','/two.js'];assert next(e for e in f.events if e['event']=='loaded')['value']=='Script one two';b.shot('scripts-loaded');rows.append(dict(flow='scripts-after-complete-batch',requests=f.scripts))
        assert all(s not in b.logs() for s in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'));b.command('KEY HOME');b.wait(lambda:'PXA RESOURCE BUDGET' in b.logs());budget=[s for s in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in s][-1];assert 'remaining_internal=0 remaining_external=0' in budget
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows,budget=budget),indent=2)+'\n');print('DOM batch simulator PASS:',len(rows),'flows; availability, staged lists/handlers, options, radios, scripts and release',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
