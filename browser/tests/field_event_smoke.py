#!/usr/bin/env python3
"""Actual native edits trigger inline events; unchanged commits stay idle."""
import argparse
import json
from pathlib import Path
import time
from PIL import Image
from field_event_fixture import Fixture
from harness import Browser


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0);rows=[]
    def load(name):
        mark=len(b.logs());b.navigate(root+'/'+name)
        b.wait(lambda:'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:])
        assert 'BROWSER JS ERROR' not in b.logs()[mark:]
    def field(name):
        b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content')
        # Two narrow native-control background columns locate the first row
        # without an expensive Python scan of the entire screenshot.
        ys=[py for py in range(y,min(y+h,im.height)) if
            im.getpixel((x+b.dp(8),py))==(232,238,243) and im.getpixel((x+w-b.dp(8),py))==(232,238,243)]
        assert ys,(name,b.logs()[-2000:]);return x+w//4,min(ys)+b.dp(7)
    def edit(name,value=None,cancel=False):
        mark=len(b.logs());b.tap(*field(name+'-before'))
        b.wait(lambda:'BROWSER EDIT field=0 ' in b.logs()[mark:])
        if cancel:
            b.command('TEXT discarded');b.command('KEY BACK');time.sleep(.25)
        else:b.enter(value or '')
    def events(name,before,kinds,value,current='q'):
        b.wait(lambda:len(f.events)>=before+len(kinds))
        time.sleep(.2);found=sorted(f.events[before:],key=lambda e:int(e['seq']));assert len(found)==len(kinds),(name,found)
        assert [e['event'] for e in found]==kinds,(name,found)
        assert all(e['case']==name and e['value']==value and e['target']=='q' and e['current']==current for e in found),found
        b.shot(name+'-pass');rows.append(dict(flow=name,events=found))
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for name,kinds in [('input-only',['input','change']),('standalone',['input','change']),
                           ('change-only',['change']),('ancestor',['input','change']),
                           ('replace',['change']),('remove',['input']),('disables',['input','change'])]:
            load(name);before=len(f.events);edit(name,'编辑 A')
            # Native editors open with the caret at the end of the value.
            value=('old' if name=='change-only' else 'same')+'编辑 A'
            events(name,before,kinds,value,'parent' if name=='ancestor' else 'q')
        for name in ('noop-valid','noop-invalid'):
            load(name)
            for turn in range(3):
                mark=len(b.logs());before=len(f.events);edit(name+'-'+str(turn));time.sleep(.3)
                assert len(f.events)==before and 'BROWSER JS UPDATE' not in b.logs()[mark:] and 'BROWSER JS ERROR' not in b.logs()[mark:]
                rows.append(dict(flow=name+'-'+str(turn),callbacks=0,extra_snapshots=0))
            before=len(f.events);edit(name+'-cancel',cancel=True);assert len(f.events)==before
            rows.append(dict(flow=name+'-cancel',callbacks=0))
        load('radio');before=len(f.events);mark=len(b.logs());b.tap(*field('radio-before'));time.sleep(.3)
        assert len(f.events)==before and 'BROWSER JS UPDATE' not in b.logs()[mark:]
        rows.append(dict(flow='checked-radio',callbacks=0,extra_snapshots=0))
        load('checkbox')
        for checked in ('true','false'):
            before=len(f.events);b.tap(*field('checkbox-'+checked));events('checkbox',before,['input','change'],'on')
            assert all(e['checked']==checked for e in f.events[before:])
        b.command('KEY HOME');b.wait(lambda:'PXA RESOURCE BUDGET' in b.logs());budget=[line for line in b.logs().splitlines() if 'PXA RESOURCE BUDGET' in line][-1]
        assert 'remaining_internal=0 remaining_external=0' in budget
        assert all(error not in b.logs() for error in ('BROWSER JS ERROR','BROWSER DRAW ERROR','PXA Guest exception:'))
        (a.output/'results.json').write_text(json.dumps(dict(flows=rows,budget=budget,requests=dict(f.requests)),ensure_ascii=False,indent=2)+'\n')
        print('Field events simulator PASS:',len(rows),'flows; inline-only, bubbling, handler replacement, no-op edits and radio/checkbox',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
