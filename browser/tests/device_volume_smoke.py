#!/usr/bin/env python3
"""Check volume paging and persisted toggle on pai-touch; restore the toggle.

Use a trace-enabled /images fixture package and updated injected-key routing.
Only application settings are read; the original volume-key preference and all
saved sites are preserved. Hold timing is covered by the simulator UI test.
"""
import argparse
import json
from pathlib import Path
import sys
import time
from PIL import Image, ImageChops
from geometry import geometry, navigation


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--pxadb-dir',type=Path,default=Path('tools/pxadb'))
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    sys.path.insert(0,str(a.pxadb_dir.resolve()));import pxadb
    records=[];original=None;enabled=None;menu_open=False
    with pxadb.PxaDbClient(a.port,30) as c:
        c.hello();c.subscribe_logs()
        c.log_callback=lambda f:records.append(dict(kind=f.kind,payload=f.payload))
        fs=pxadb.NormalFsClient(c)
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
            command(f'INPUT TOUCH UP {x} {y} 0');pump(.3)
        def shot(name):
            path=a.output/(name+'.png')
            pxadb.write_screenshot(pxadb.screenshot_capture(command('SCREENSHOT')),path)
            return Image.open(path).convert('RGB')
        def prefs():
            target=a.output/'preferences-current.txt'
            fs.get(remote,target);return target.read_text()
        def go_top():
            x,y,w,h=layout['content'];edge=x+w-4
            command(f'INPUT TOUCH DOWN {edge} {y+h-4} 0');pump(.12)
            command(f'INPUT TOUCH MOVE {edge} {y} 0');pump(.15)
            command(f'INPUT TOUCH UP {edge} {y} 0');pump(1.2)
        def toggle():
            nonlocal enabled,menu_open
            tap(*navigation(layout,4));menu_open=True
            x,y,w,h=layout['content']
            command(f'INPUT SWIPE {x+w//2} {y+h-8} {x+w//2} {y+8} 400 24');pump(.2)
            offset=min(max(0,(websites+5)*35-h),h-16)
            tap(x+w//2,y+4*35-offset+16)
            enabled=not enabled;pump(.4);shot('volume-on' if enabled else 'volume-off')
            assert ('#volume-scroll 1\n' in prefs())==enabled,'toggle was not saved'
            tap(*navigation(layout,4));menu_open=False
        try:
            try:command('PACKAGE stop pxa-browser');pump(1.5)
            except pxadb.PxaDbRejectedError as error:
                if str(error)!='package_not_running':raise
            command('INPUT KEY BACK');pump(.3)
            mark=len(records);command('PACKAGE run pxa-browser')
            deadline=time.monotonic()+20
            while not any('BROWSER IMAGE DONE index=0' in r.get('payload','') for r in records[mark:]):
                assert time.monotonic()<deadline,'image load timeout';pump(.05)
            layout=geometry('\n'.join(r.get('payload','') for r in records))
            roots=[name for kind,size,name in fs.list('pxa-state/data') if name.endswith('~pxa-browser')]
            assert len(roots)==1,roots
            remote='pxa-state/data/'+roots[0]+'/bookmarks.txt'
            original=prefs();enabled='#volume-scroll 1\n' in original
            initial_enabled=enabled
            websites=sum(bool(line) and not line.startswith('#') for line in original.splitlines())
            if not enabled:toggle()
            x,y,w,h=layout['content'];body=(x+1,y+1,x+w-10,y+h-1)
            # Warm both caches before pixel comparisons.
            for _ in range(3):command(f'INPUT SWIPE 150 {y+h-8} 150 {y+8} 400 24');pump(.1)
            pump(1);go_top();top=shot('top')
            command('INPUT KEY VOLUME-DOWN');pump(1.2);down=shot('down')
            assert ImageChops.difference(top.crop(body),down.crop(body)).getbbox()
            pump(.7);stopped=shot('release-stopped')
            assert not ImageChops.difference(down.crop(body),stopped.crop(body)).getbbox()
            command('INPUT KEY VOLUME-UP');pump(1.2);up=shot('up')
            assert not ImageChops.difference(top.crop(body),up.crop(body)).getbbox()
            # Restart reloads the preference from the real application filesystem.
            command('PACKAGE stop pxa-browser');pump(1.6)
            command('PACKAGE run pxa-browser');pump(3)
            go_top();command('INPUT KEY VOLUME-DOWN');pump(1.2)
            assert ImageChops.difference(top.crop(body),shot('restart-down').crop(body)).getbbox()
            if enabled!=initial_enabled:toggle()
            restored=prefs()
            assert restored==original,'saved sites or other settings changed'
            (a.output/'report.json').write_text(json.dumps(dict(passed=True,layout=layout,
                up_down=True,release_stops=True,persistent_restart=True,
                preference_restored=True,sites_preserved=True),indent=2)+'\n')
            print('device volume paging PASS',flush=True)
        finally:
            if original is not None and enabled!=('#volume-scroll 1\n' in original):
                if menu_open:tap(*navigation(layout,4));menu_open=False
                toggle()
            try:command('INPUT CANCEL')
            except pxadb.PxaDbError:pass
            (a.output/'records.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
