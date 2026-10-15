#!/usr/bin/env python3
"""Test scrollbar capture/hiding and menu exit on pai-touch (/images build).

Preserves saved websites and settings. Screenshots run outside the local heap
window. Use a trace-enabled fixture-start package and an unlocked device.
"""
import argparse
import json
from pathlib import Path
import sys
import time
from PIL import Image, ImageChops
from geometry import geometry, navigation, content_crop


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--pxadb-dir',type=Path,default=Path('tools/pxadb'))
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    sys.path.insert(0,str(a.pxadb_dir.resolve()));import pxadb
    records=[]
    with pxadb.PxaDbClient(a.port,30) as c:
        c.hello();c.subscribe_logs()
        c.log_callback=lambda f:records.append(dict(kind=f.kind,payload=f.payload))
        def command(value):
            frames=c.request(value)
            records.append(dict(command=value,frames=[dict(kind=f.kind,payload=f.payload)
                for f in frames if value!='SCREENSHOT' or f.kind!='DATA']))
            return frames
        def pump(seconds):
            end=time.monotonic()+seconds
            while time.monotonic()<end:c.pump_logs();time.sleep(.005)
        def tap(x,y):
            command(f'INPUT TOUCH DOWN {x} {y} 0');pump(.12)
            command(f'INPUT TOUCH UP {x} {y} 0');pump(.25)
        def logs():return '\n'.join(r.get('payload','') for r in records)
        def shot(name):
            path=a.output/(name+'.png')
            pxadb.write_screenshot(pxadb.screenshot_capture(command('SCREENSHOT')),path)
            return Image.open(path).convert('RGB')
        def memory(value='MEMORY'):
            result={}
            for f in command(value):
                if f.kind!='DATA':continue
                row=pxadb.info_properties(f.payload)
                key=row.pop('scope')+(':'+row.pop('name') if 'name' in row else '')
                result[key]={k:int(v) for k,v in row.items()}
            return result
        def active():
            return any(f.kind=='PKG' and ':pxa-browser\t' in f.payload and
                       'active=1' in f.payload for f in command('PACKAGES'))
        def exit_menu(layout):
            tap(*navigation(layout,4))
            x,y,w,h=layout['content'];tap(x+w*3//4,y+16)
            end=time.monotonic()+10
            while active():
                assert time.monotonic()<end,'menu exit timeout'
                pump(.1)
            pump(.5)
        try:
            try:command('PACKAGE stop pxa-browser');pump(1.5)
            except pxadb.PxaDbRejectedError as error:
                if str(error)!='package_not_running':raise
            command('INPUT KEY BACK');pump(.3)
            mark=len(records);command('PACKAGE run pxa-browser')
            end=time.monotonic()+20
            while 'BROWSER IMAGE DONE index=0' not in '\n'.join(
                    r.get('payload','') for r in records[mark:]):
                assert time.monotonic()<end,'fixture image timeout'
                pump(.05)
            layout=geometry(logs());x,y,w,h=layout['content'];edge=x+w-4
            # Warm both image slots so expiry comparisons observe only the bar.
            for _ in range(3):command(f'INPUT SWIPE 150 {y+h-8} 150 {y+8} 400 24');pump(.1)
            pump(2);bottom=shot('page-bottom');before=memory()
            command('MEMORY START')
            command(f'INPUT TOUCH DOWN {edge} {y+h-8} 0');pump(.15)
            command(f'INPUT TOUCH MOVE {edge} {y} 0');pump(.2)
            top_memory=memory('MEMORY STOP');top=shot('drag-top')
            pump(1.1);held=shot('drag-held')
            crop=content_crop(layout,1)
            assert not ImageChops.difference(top.crop(crop),held.crop(crop)).getbbox()
            command(f'INPUT TOUCH UP {edge} {y} 0');pump(1.2)
            hidden=shot('bar-hidden')
            strip=(x+w-8,y,x+w,y+h);body=(x,y,x+w-10,y+h)
            assert ImageChops.difference(held.crop(strip),hidden.crop(strip)).getbbox()
            assert not ImageChops.difference(held.crop(body),hidden.crop(body)).getbbox()
            command('MEMORY START')
            command(f'INPUT TOUCH DOWN {edge} {y+4} 0');pump(.15)
            command(f'INPUT TOUCH MOVE {edge} {y+h+20} 0');pump(.15)
            command(f'INPUT TOUCH UP {edge} {y+h+20} 0');pump(1.2)
            after=memory('MEMORY STOP');returned=shot('returned-bottom')
            assert ImageChops.difference(hidden.crop(body),returned.crop(body)).getbbox()
            assert before['kind:image']==after['kind:image']
            assert not after['budget']['denied'] and not after['kind:temporary']['external']
            tap(*navigation(layout,2));shot('home')
            tap(*navigation(layout,4));shot('menu');tap(*navigation(layout,4))
            exit_menu(layout);closed=memory();shot('exited')
            assert not closed['kind:image']['external']
            command('PACKAGE run pxa-browser');pump(2.5)
            assert active(),'restart after menu exit failed'
            exit_menu(layout)
            report=dict(passed=True,layout=layout,before=before,top_memory=top_memory,
                after=after,closed=closed,bar_held_visible=True,bar_auto_hide=True,
                drag_outside_clamped=True,menu_exit=True,restart=True)
            (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print('device browser UI PASS',flush=True)
        finally:
            for value in ('INPUT CANCEL','MEMORY STOP'):
                try:command(value)
                except pxadb.PxaDbError:pass
            (a.output/'records.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
