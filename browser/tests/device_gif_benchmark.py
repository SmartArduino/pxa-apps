#!/usr/bin/env python3
"""Measure a trace-free /gif fixture package, then restore the normal package.

Use the same source, O3, dimensions and Host as the functional test. Pass its
measured layout JSON and keep the screen unlocked. This script separates startup/opaque/transparent heap
windows from screenshots and LCD instrumentation. No firmware/settings changes.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('output', 'package', 'restore-package', 'layout-file'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--fixture-url', required=True)
    parser.add_argument('--unlock', action='store_true', help='pai-touch: OCR-check and unlock after package transfer (requires tesseract chi_sim+eng)')
    parser.add_argument('--pxadb-dir', type=Path, default=Path('tools/pxadb'))
    args = parser.parse_args()
    sys.path.insert(0, str(args.pxadb_dir.resolve()))
    import pxadb
    from PIL import Image
    args.output.mkdir(parents=True, exist_ok=True)
    layout = json.loads(args.layout_file.read_text())['layout']
    records, result = [], dict(trace=False, runs=[], layout=layout)
    with pxadb.PxaDbClient(args.port, 30) as client:
        client.hello(); client.subscribe_logs()
        client.log_callback = lambda f: records.append(dict(kind=f.kind, payload=f.payload))
        fs = pxadb.NormalFsClient(client)

        def command(text):
            frames = client.request(text, timeout=90)
            records.append(dict(command=text, frames=[dict(kind=f.kind, payload=f.payload)
                for f in frames if f.kind != 'LOG' and (text != 'SCREENSHOT' or f.kind != 'DATA')]))
            return frames

        def pump(seconds):
            end = time.monotonic()+seconds
            while time.monotonic() < end:
                client.pump_logs(); time.sleep(.008)

        def properties(text):
            data = {}
            for frame in command(text):
                if frame.kind in ('DATA', 'OK'): data.update(pxadb.info_properties(frame.payload))
            return data

        def memory(text):
            data = {}
            for frame in command(text):
                if frame.kind != 'DATA': continue
                fields = pxadb.info_properties(frame.payload)
                key = fields.pop('scope')+(':'+fields.pop('name') if 'name' in fields else '')
                data[key] = {k: int(v) for k, v in fields.items()}
            return data

        def wait_image(mark, epoch, route='/gif'):
            end = time.monotonic()+25
            while True:
                fresh = [r.get('payload', '') for r in records[mark:]]
                fresh = [s for s in fresh if not (m := re.match(r'^(\d+)\t', s)) or int(m[1]) >= epoch]
                if (any('BROWSER IMAGE DONE index=0' in s for s in fresh) and
                    any('BROWSER NAV '+args.fixture_url.rstrip('/')+route in s for s in fresh)):
                    break
                assert time.monotonic() < end, fresh[-12:]
                pump(.05)
            pump(.5)

        def shot(name):
            path = args.output/(name+'.png')
            pxadb.write_screenshot(pxadb.screenshot_capture(command('SCREENSHOT')), path)
            return Image.open(path).convert('RGB')

        def install(path):
            pxadb.stage_package_files(fs, path.resolve(), 'pxa-browser', 60, publish=False)
            command('PACKAGE deploy pxa-browser'); pxadb.clear_staged_source(client, 'pxa-browser')

        def ensure_unlocked(run):
            if not args.unlock: return
            # Transfers can outlast the ordinary lock timeout. Inspect first;
            # an upward swipe on an unlocked screen would instead open Recents.
            import io, subprocess
            def text(name):
                picture = shot(name); buffer = io.BytesIO()
                picture.resize((picture.width*3, picture.height*3)).save(buffer, format='PNG')
                return subprocess.check_output(['tesseract', 'stdin', 'stdout', '-l', 'chi_sim+eng', '--psm', '6'],
                    input=buffer.getvalue(), stderr=subprocess.DEVNULL).decode(errors='replace').replace(' ', '')
            for attempt in range(3):
                if '解锁' not in text(f'before-run-{run}-{attempt}'): return
                # Wake first; then start clear of the system Home edge. Every
                # retry inspects the lock screen before injecting a gesture.
                command('INPUT TAP 148 120'); pump(.5)
                command('INPUT CANCEL'); command('INPUT TOUCH DOWN 148 180 0'); pump(.15)
                for y in range(170, 45, -10):
                    command(f'INPUT TOUCH MOVE 148 {y} 0'); pump(.04)
                command('INPUT TOUCH UP 148 45 0'); pump(1)
            assert '解锁' not in text('unlocked-'+str(run)), 'Panel remained locked'

        active = [f.payload for f in command('PACKAGES') if f.kind == 'PKG' and 'active=1' in f.payload]
        assert not active, 'Leave other running apps untouched'
        roots = [n for k, s, n in fs.list('pxa-state/data') if n.endswith('~pxa-browser')]
        assert len(roots) == 1
        remote = 'pxa-state/data/'+roots[0]+'/bookmarks.txt'
        fs.get(remote, args.output/'preferences-before.txt')
        try:
            install(args.package)
            for run in range(3):
                ensure_unlocked(run)
                epoch = int(properties('INPUT SYNC').get('device_us', '0'))//1000
                before = memory('MEMORY'); command('MEMORY START')
                mark = len(records); command('PACKAGE run pxa-browser'); wait_image(mark, epoch)
                startup = memory('MEMORY STOP')
                entry = dict(before=before, startup=startup)
                if run == 0:
                    command('MEMORY START'); pump(5); entry['opaque'] = memory('MEMORY STOP')
                    command('PERF START display'); pump(6)
                    perf = {k: int(v) for k, v in properties('PERF STOP').items()}
                    assert perf['display'] == 1 and not perf['present_overflow'] and perf['completed'] > 0
                    entry['display_perf'] = perf
                    entry['lcd_completion_rate'] = perf['completed']*1e6/perf['window_us']
                    command('PERF CLEAR')
                    x, y, w, h = layout['content']; colors = set()
                    for sample in range(9):
                        r, g, b = shot('opaque-'+str(sample)).getpixel((x+w//2, y+60))
                        if r > 150 and r > g*1.4 and r > b*1.4: colors.add('red')
                        if b > 150 and b > g*1.4 and b > r*1.4: colors.add('blue')
                        if g > 100 and g > r*1.4 and g > b*1.4: colors.add('green')
                        pump(.13)
                    assert colors == {'red', 'green', 'blue'}, colors
                    entry['actual_colors'] = sorted(colors)
                    command('MEMORY START'); mark = len(records)
                    command(f'INPUT TOUCH DOWN {x+20} {y+10} 0'); pump(.1)
                    command(f'INPUT TOUCH UP {x+20} {y+10} 0')
                    wait_image(mark, epoch, '/gif-alpha'); pump(5)
                    entry['transparent'] = memory('MEMORY STOP')
                    shot('transparent')
                command('PACKAGE stop pxa-browser'); pump(1.5)
                entry['closed'] = memory('MEMORY')
                for name in ('kind:image', 'kind:temporary'):
                    assert entry['closed'][name] == before[name]
                entry['logs'] = records[mark:]
                result['runs'].append(entry)
            result['passed'] = True
        finally:
            try:
                for text in ('MEMORY STOP', 'PERF CLEAR', 'INPUT CANCEL', 'PACKAGE stop pxa-browser'):
                    try: command(text)
                    except pxadb.PxaDbError: pass
                pump(.8); install(args.restore_package)
                result['production_restored'] = True
                fs.get(remote, args.output/'preferences-after.txt')
                result['preferences_preserved'] = ((args.output/'preferences-before.txt').read_bytes()
                    == (args.output/'preferences-after.txt').read_bytes())
                assert result['preferences_preserved']
            finally:
                result['passed'] = (result.get('passed', False) and result.get('production_restored', False)
                    and result.get('preferences_preserved', False))
                (args.output/'results.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
                (args.output/'records.json').write_text(json.dumps(records, ensure_ascii=False, indent=2)+'\n')
    print('Trace-free GIF startup/steady/transparent heap, LCD and repeat stability PASS')


if __name__ == '__main__': main()
