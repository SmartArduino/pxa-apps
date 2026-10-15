#!/usr/bin/env python3
"""Test a /images fixture-start browser on pai-touch; preserve device settings.

Install the test package first. Uses the workspace tools/pxadb Python module;
pass --pxadb-dir when running outside pxa-workspace. Screenshots and PERF are
outside the MEMORY START/STOP windows so they do not inflate decoder peaks.
"""
import argparse
import json
import math
from pathlib import Path
import statistics
import sys
import time
from geometry import geometry, navigation, content_crop


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--pxadb-dir', type=Path, default=Path('tools/pxadb'))
    p.add_argument('--repeat', type=int, default=2)
    p.add_argument('--layout-file', type=Path,
                   help='Use a measured layout JSON with a production package (no pointer trace)')
    a = p.parse_args();assert a.repeat>0
    sys.path.insert(0, str(a.pxadb_dir.resolve()))
    import pxadb
    from PIL import Image, ImageChops
    a.output.mkdir(parents=True, exist_ok=True)
    records, runs = [], []
    with pxadb.PxaDbClient(a.port, 30) as c:
        c.log_callback = lambda f: records.append(dict(kind=f.kind, payload=f.payload))
        c.raw_callback = lambda b: None
        c.hello();c.subscribe_logs()
        def command(text):
            frames = c.request(text)
            records.append(dict(command=text, frames=[dict(kind=f.kind,payload=f.payload)
                            for f in frames if text!='SCREENSHOT' or f.kind!='DATA']))
            return frames
        def pump(seconds):
            deadline = time.monotonic()+seconds
            while time.monotonic()<deadline: c.pump_logs();time.sleep(.005)
        def memory(text):
            result = {}
            for f in command(text):
                if f.kind == 'DATA':
                    fields = pxadb.info_properties(f.payload)
                    key = fields.pop('scope')+(':'+fields.pop('name') if 'name' in fields else '')
                    result[key] = {key:int(value) for key,value in fields.items()}
            return result
        def wait_image(index, mark):
            deadline = time.monotonic()+15
            while not any(f'BROWSER IMAGE DONE index={index}' in r.get('payload','') for r in records[mark:]):
                assert time.monotonic()<deadline, 'image timeout: see records.json'
                pump(.05)
            pump(.4) # Completion log precedes the next Canvas/LCD repaint.
        def tap(x,y):
            command(f'INPUT TOUCH DOWN {x} {y} 0');pump(.12)
            command(f'INPUT TOUCH UP {x} {y} 0');pump(.2)
        def shot(name):
            path = a.output/(name+'.png')
            pxadb.write_screenshot(pxadb.screenshot_capture(command('SCREENSHOT')), path)
            return Image.open(path).convert('RGB')
        def properties(text):
            result = {}
            for f in command(text):
                if f.kind in ('DATA','OK'): result.update(pxadb.info_properties(f.payload))
            return result
        pump(.3)
        try:
            for repeat in range(a.repeat):
                packages = command('PACKAGES')
                for f in packages:
                    if f.kind=='PKG' and 'active=1' in f.payload:
                        assert ':pxa-browser\t' in f.payload, 'another app is active'
                        command('PACKAGE stop pxa-browser');pump(1.6)
                command('INPUT KEY BACK');pump(.3)
                command('MEMORY START');before = memory('MEMORY')
                mark = len(records);command('PACKAGE run pxa-browser');wait_image(0,mark)
                png = memory('MEMORY STOP');shot(f'{repeat}-png')
                layout=(json.loads(a.layout_file.read_text())['layout'] if a.layout_file else
                        geometry('\n'.join(r.get('payload','') for r in records[mark:])))
                command('MEMORY START');mark = len(records)
                for _ in range(3):
                    command('INPUT SWIPE 150 170 150 80 500 30');pump(.15)
                wait_image(1,mark)
                jpeg = memory('MEMORY STOP');shot(f'{repeat}-jpeg')
                for _ in range(5):
                    command('INPUT SWIPE 150 80 150 170 300 18');pump(.1)
                command('PERF START display')
                command('INPUT SWIPE 150 170 150 80 1600 96');pump(.1)
                perf = {k:int(v) for k,v in properties('PERF STOP').items()}
                intervals = []
                for offset in range(0,perf['present'],16):
                    block = properties(f'PERF READ present {offset}')
                    intervals.extend(int(x) for x in block['values'].split(',') if x)
                command('PERF CLEAR')
                assert not perf['present_overflow'] and perf['display']==1
                pump(1.1);partial = shot(f'{repeat}-partial');tap(*navigation(layout,4));tap(*navigation(layout,4))
                full = shot(f'{repeat}-full')
                assert not ImageChops.difference(partial.crop(content_crop(layout,4)),
                                                 full.crop(content_crop(layout,4))).getbbox()
                tap(*navigation(layout,2));pump(.4);released = memory('MEMORY')
                assert released['kind:image']['external']==0
                assert released['kind:temporary']['external']==0
                assert released['budget']['denied']==0
                ordered = sorted(intervals)
                runs.append(dict(before=before,png=png,jpeg=jpeg,after_home=released,perf=perf,
                                 present_intervals_us=intervals,
                                 median_us=statistics.median(intervals),
                                 p95_us=ordered[math.ceil(len(ordered)*.95)-1],
                                 window_fps=perf['completed']*1e6/perf['window_us'],
                                 partial_matches_full=True))
                (a.output/'report.json').write_text(json.dumps(dict(passed=True,runs=runs),indent=2)+'\n')
                print(repeat,'PNG/JPEG, scrolling, repaint, release PASS',flush=True)
                command('PACKAGE stop pxa-browser');pump(1.6)
        finally:
            for text in ('INPUT CANCEL','PERF CLEAR','MEMORY STOP'):
                try: command(text)
                except pxadb.PxaDbError: pass
            (a.output/'records.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')


if __name__ == '__main__': main()
