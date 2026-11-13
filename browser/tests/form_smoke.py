#!/usr/bin/env python3
"""Real PXA form hits, exact HTTP encoding and cancellable submit events."""
import argparse
import json
import time
from pathlib import Path
from PIL import Image
from form_fixture import Fixture
from harness import Browser


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('simulator','package','publisher-key','ime-package','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--width',type=int,default=480);p.add_argument('--height',type=int,default=480)
    a=p.parse_args();f=Fixture().start();root='http://127.0.0.1:'+str(f.server.server_port)
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,width=a.width,height=a.height,corner=0)
    rows=[]
    def load(path, scripted):
        mark=len(b.logs());b.navigate(root+path)
        b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:])
        if scripted:b.wait(lambda:'BROWSER JS UPDATE revision=1 ' in b.logs()[mark:])
        else:assert 'BROWSER JS READY' not in b.logs()[mark:]
    def point(name, color, index=0):
        b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');x,y,w,h=b.box('content');points=[]
        for py in range(y,min(y+h,im.height)):
            for px in range(x,min(x+w,im.width)):
                if all(abs(v-c)<9 for v,c in zip(im.getpixel((px,py)),color)):points.append((px,py))
        assert len(points)>15,(name,color)
        if color==(21,108,168):
            # Adjacent native buttons share a blue background edge. Locate
            # their white labels so each actual control remains a distinct hit.
            left,right=min(v[0] for v in points),max(v[0] for v in points)
            top,bottom=min(v[1] for v in points),max(v[1] for v in points)
            points=[(px,py) for py in range(top,bottom+1) for px in range(left,right+1)
                    if all(v>248 for v in im.getpixel((px,py)))]
            assert len(points)>15,name
        groups=[]
        for py in sorted({v[1] for v in points}):
            if not groups or py-groups[-1][-1]>5:groups.append([])
            groups[-1].append(py)
        group=groups[index];selected=[v for v in points if group[0]<=v[1]<=group[-1]]
        return ((min(v[0] for v in selected)+max(v[0] for v in selected))//2,(group[0]+group[-1])//2)
    def submitted(before, mark):
        b.wait(lambda:len(f.submissions)>before and 'BROWSER DONE status=' in b.logs()[mark:])
        assert len(f.submissions)==before+1 and f.submissions[-1]['valid'],f.submissions
        assert 'BROWSER DONE status=200' in b.logs()[mark:],b.logs()[mark:]
    try:
        b.wait(lambda:'BROWSER VIEWPORT' in b.logs())
        for mode in ('static','script'):
            for method,choice in (('get','first'),('get','other'),('post','first')):
                name=mode+'-'+method+'-'+choice;load('/'+name,mode=='script')
                color=(21,108,168) if mode=='static' else ((24,122,66) if choice=='other' else (208,96,32))
                at=point(name,color,1 if mode=='static' and choice=='other' else 0)
                before=len(f.submissions);mark=len(b.logs());b.tap(*at);submitted(before,mark);b.shot(name+'-pass')
                rows.append({'flow':name,'request':f.submissions[-1]})
        load('/controls',True)
        for name,color in [('ordinary',(208,96,32)),('disabled',(128,48,160)),('reset-handler',(24,122,66))]:
            at=point(name,color);mark=len(b.logs());b.tap(*at)
            if name!='disabled':b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:])
            time.sleep(.2);assert not f.requests['/unexpected'];rows.append({'flow':name,'no_form_navigation':True})
        load('/request',True);before=len(f.submissions);at=point('request-before',(208,96,32));mark=len(b.logs());b.tap(*at)
        b.wait(lambda:'BROWSER JS UPDATE revision=2 ' in b.logs()[mark:]);assert len(f.submissions)==before
        b.shot('request-cancelled');rows.append({'flow':'request-cancel','no_navigation':True})
        at=point('request-again',(208,96,32));mark=len(b.logs());b.tap(*at);submitted(before,mark)
        rows.append({'flow':'request-submit','request':f.submissions[-1]})
        load('/direct',True);at=point('direct-before',(208,96,32));before=len(f.submissions);mark=len(b.logs());b.tap(*at);submitted(before,mark)
        rows.append({'flow':'direct-submit-from-handler','request':f.submissions[-1]});b.shot('direct-pass')
        assert 'BROWSER JS ERROR' not in b.logs() and 'BROWSER DRAW ERROR' not in b.logs() and 'PXA Guest exception:' not in b.logs()
        (a.output/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
        print('form simulator PASS:',len(rows),'flows; exact GET/POST data, submitter and event behavior',flush=True)
    finally:b.close();f.close()


if __name__=='__main__':main()
