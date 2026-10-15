#!/usr/bin/env python3
"""Exercise /images preview on pai-touch after installing a fixture-start build.

The device must be unlocked. No persistent settings are changed. Screenshots
and PERF run outside the local heap measurement window.
"""
import argparse
import json
from pathlib import Path
import sys
import time
from PIL import Image, ImageChops
from geometry import geometry, navigation, content_crop


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--pxadb-dir', type=Path, default=Path('tools/pxadb'))
    p.add_argument('--layout-file', type=Path,
                   help='Use measured geometry with a production package (no pointer trace)')
    a = p.parse_args(); a.output.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(a.pxadb_dir.resolve()))
    import pxadb
    records = []
    with pxadb.PxaDbClient(a.port, 30) as c:
        c.hello(); c.subscribe_logs()
        c.log_callback = lambda f: records.append(dict(kind=f.kind, payload=f.payload))

        def command(text):
            frames = c.request(text)
            records.append(dict(command=text, frames=[dict(kind=f.kind, payload=f.payload)
                            for f in frames if text != 'SCREENSHOT' or f.kind != 'DATA']))
            return frames

        def pump(seconds):
            deadline = time.monotonic()+seconds
            while time.monotonic()<deadline:c.pump_logs();time.sleep(.005)

        def tap(x, y):
            command(f'INPUT TOUCH DOWN {x} {y} 0');pump(.12)
            command(f'INPUT TOUCH UP {x} {y} 0');pump(.25)

        def memory(text='MEMORY'):
            result = {}
            for f in command(text):
                if f.kind != 'DATA':continue
                fields = pxadb.info_properties(f.payload)
                key = fields.pop('scope')+(':'+fields.pop('name') if 'name' in fields else '')
                result[key] = {key:int(value) for key,value in fields.items()}
            return result

        def shot(name):
            path = a.output/(name+'.png')
            pxadb.write_screenshot(pxadb.screenshot_capture(command('SCREENSHOT')),path)
            return Image.open(path).convert('RGB')

        def crop(image):return image.crop(content_crop(layout,4))

        def nav(index,count=4):tap(*navigation(layout,index,count))

        try:
            try:command('PACKAGE stop pxa-browser');pump(1.5)
            except pxadb.PxaDbError:pass
            # Close a launcher task-switcher/overlay left by earlier manual
            # input. Running an app alone does not dismiss that system page.
            command('INPUT KEY BACK');pump(.5)
            command('PACKAGE run pxa-browser')
            deadline = time.monotonic()+20
            while not any('BROWSER IMAGE DONE index=0' in r.get('payload','') for r in records):
                assert time.monotonic()<deadline,'image timeout (check lock screen and fixture server)'
                pump(.05)
            pump(.5);original = shot('page');before = memory()
            layout=(json.loads(a.layout_file.read_text())['layout'] if a.layout_file else
                    geometry('\n'.join(r.get('payload','') for r in records)))
            tap(150,130);fitted = shot('fit')
            # Per-touch tracing can exhaust the device's log quota. Verify the
            # rendered state instead of requiring an informational log line.
            assert ImageChops.difference(crop(original),crop(fitted)).getbbox()
            nav(3);zoomed = shot('zoom-2')
            assert ImageChops.difference(crop(fitted),crop(zoomed)).getbbox()
            command('INPUT SWIPE 160 155 130 125 500 30');pump(.3)
            panned = shot('pan')
            assert ImageChops.difference(crop(zoomed),crop(panned)).getbbox()
            nav(2);refit = shot('refit')
            assert not ImageChops.difference(crop(fitted),crop(refit)).getbbox()
            command('INPUT KEY HOME');pump(.7)
            command('PACKAGE run pxa-browser');pump(.7)
            resumed = shot('resumed')
            assert not ImageChops.difference(crop(fitted),crop(resumed)).getbbox()
            # Stress repeated interaction without allocating another image.
            command('MEMORY START')
            for _ in range(12):
                nav(3);nav(3);nav(3)
                nav(1);nav(1);nav(2)
            after = memory('MEMORY STOP')
            assert before['kind:image']==after['kind:image']
            assert not after['kind:temporary']['external'] and not after['budget']['denied']
            nav(3);nav(3);nav(3)
            command('PERF START display')
            command('INPUT SWIPE 150 165 130 85 1600 96');pump(.1)
            perf = {}
            for f in command('PERF STOP'):
                if f.kind in ('DATA','OK'):perf.update(pxadb.info_properties(f.payload))
            perf = {key:int(value) for key,value in perf.items()}
            assert perf['completed']>10 and not perf['present_overflow']
            command('PERF CLEAR')
            shot('zoom-8-pan')
            command('INPUT KEY BACK');pump(.4);returned = shot('returned')
            assert not ImageChops.difference(crop(original),crop(returned)).getbbox()
            assert sum('BROWSER IMAGE DONE index=0' in r.get('payload','') for r in records)<=1
            nav(2,5);released = memory()
            assert not released['kind:image']['external']
            report = dict(passed=True, before=before, after=after, after_home=released,
                          perf=perf, display_fps=perf['completed']*1e6/perf['window_us'],
                          repeated_zoom_cycles=12, page_restored=True, preview_downloads=0)
            (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
            print('device image preview PASS; LCD FPS',report['display_fps'],flush=True)
        finally:
            for text in ('INPUT CANCEL','PERF CLEAR','MEMORY STOP'):
                try:command(text)
                except pxadb.PxaDbError:pass
            (a.output/'records.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')


if __name__ == '__main__':main()
