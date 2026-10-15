#!/usr/bin/env python3
"""Verify a fixture-start GIF package on pai-touch with real LCD/heap evidence.

Install a TRACE-enabled package starting at <fixture-url>/gif first, with the
current Host. This test never flashes firmware or changes system settings.
It preserves preferences and stops the app on exit. Optional --restore-package
atomically installs a normal Home package after testing. No private accounts.
"""
import argparse
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from geometry import geometry, navigation


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--fixture-url', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pxadb-dir', type=Path, default=Path('tools/pxadb'))
    parser.add_argument('--restore-package', type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.pxadb_dir.resolve()))
    import pxadb
    from PIL import Image

    args.output.mkdir(parents=True, exist_ok=True)
    records, result = [], {}
    epoch_ms = 0
    with pxadb.PxaDbClient(args.port, 30) as client:
        client.hello()
        client.log_callback = lambda frame: records.append(dict(kind=frame.kind, payload=frame.payload))
        client.subscribe_logs()
        fs = pxadb.NormalFsClient(client)

        def command(text):
            frames = client.request(text, timeout=60)
            records.append(dict(command=text, frames=[dict(kind=f.kind, payload=f.payload)
                for f in frames if f.kind != 'LOG' and (text != 'SCREENSHOT' or f.kind != 'DATA')]))
            return frames

        def pump(seconds):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                client.pump_logs()
                time.sleep(.008)

        def logs(mark=0):
            values = []
            for record in records[mark:]:
                value = record.get('payload', '')
                stamp = re.match(r'^(\d+)\t', value)
                if not stamp or int(stamp[1]) >= epoch_ms:
                    values.append(value)
            return '\n'.join(values)

        def wait(needle, mark, timeout=25):
            end = time.monotonic() + timeout
            while needle not in logs(mark):
                assert time.monotonic() < end, (needle, logs(mark)[-2000:])
                pump(.05)
            pump(.4)  # Guest completion precedes its next LCD presentation.

        def memory(text):
            data = {}
            for frame in command(text):
                if frame.kind != 'DATA': continue
                fields = pxadb.info_properties(frame.payload)
                key = fields.pop('scope') + (':' + fields.pop('name') if 'name' in fields else '')
                data[key] = {k: int(v) for k, v in fields.items()}
            return data

        def properties(text):
            data = {}
            for frame in command(text):
                if frame.kind in ('DATA', 'OK'): data.update(pxadb.info_properties(frame.payload))
            return data

        def shot(name):
            path = args.output / (name + '.png')
            pxadb.write_screenshot(pxadb.screenshot_capture(command('SCREENSHOT')), path)
            return Image.open(path).convert('RGB')

        def ocr(image):
            data = io.BytesIO()
            image.resize((image.width * 3, image.height * 3)).save(data, format='PNG')
            return subprocess.check_output(['tesseract', 'stdin', 'stdout', '-l', 'chi_sim+eng',
                '--psm', '6'], input=data.getvalue(), stderr=subprocess.DEVNULL).decode(errors='replace').replace(' ', '')

        def tap(x, y):
            command(f'INPUT TOUCH DOWN {x} {y} 0'); pump(.1)
            command(f'INPUT TOUCH UP {x} {y} 0'); pump(.3)

        roots = [name for kind, size, name in fs.list('pxa-state/data') if name.endswith('~pxa-browser')]
        assert len(roots) == 1
        prefs = 'pxa-state/data/' + roots[0] + '/bookmarks.txt'
        fs.get(prefs, args.output / 'preferences-before.txt')
        original = (args.output / 'preferences-before.txt').read_bytes()
        lines = original.decode().splitlines()
        assert '#gif 0' not in lines and '#images 0' not in lines, 'Fixture test requires images/GIF enabled'
        active = [f.payload for f in command('PACKAGES') if f.kind == 'PKG' and 'active=1' in f.payload]
        assert all(':pxa-browser\t' in row for row in active), 'Another app is active'
        result['packages_before'] = [f.payload for f in command('PACKAGES') if f.kind == 'PKG']
        try:
            if active:
                command('PACKAGE stop pxa-browser'); pump(1)
            screen = shot('initial')
            if '解锁' in ocr(screen):
                command('INPUT SWIPE 148 220 148 80 500 20'); pump(1)
            command('INPUT KEY HOME'); pump(.5); command('INPUT KEY BACK'); pump(.4)
            epoch_ms = int(properties('INPUT SYNC').get('device_us', '0')) // 1000
            result['before_start'] = memory('MEMORY')
            command('MEMORY START')
            mark = len(records); command('PACKAGE run pxa-browser')
            wait('BROWSER NAV ' + args.fixture_url.rstrip('/') + '/gif', mark)
            wait('BROWSER IMAGE DONE index=0', mark)
            result['startup'] = memory('MEMORY STOP')
            layout = geometry(logs(mark)); result['layout'] = layout
            pump(1)
            command('MEMORY START'); pump(5)
            result['animation'] = memory('MEMORY STOP')
            # Keep screenshots and measurement instrumentation out of the heap window.
            command('PERF START display'); pump(6)
            perf = {k: int(v) for k, v in properties('PERF STOP').items()}
            assert perf['display'] == 1 and not perf['present_overflow'] and perf['completed'] > 0
            result['display_perf'] = perf
            result['lcd_completions_per_second'] = perf['completed'] * 1e6 / perf['window_us']
            command('PERF CLEAR')
            x, y, w, h = layout['content']
            colors = set()
            for index in range(9):
                rgb = shot('opaque-' + str(index)).getpixel((x + w // 2, y + 60))
                r, g, b = rgb
                if r > 150 and r > g * 1.4 and r > b * 1.4: colors.add('red')
                if b > 150 and b > r * 1.4 and b > g * 1.4: colors.add('blue')
                if g > 100 and g > r * 1.4 and g > b * 1.4: colors.add('green')
                pump(.13)
            assert colors == {'red', 'green', 'blue'}, colors
            result['real_frame_colors'] = sorted(colors)
            tap(*navigation(layout, 4)); pump(.5)
            paused = logs().count('BROWSER GIF FRAME'); pump(1.2)
            assert logs().count('BROWSER GIF FRAME') == paused
            shot('menu-paused'); tap(*navigation(layout, 4)); pump(.5)
            result['menu_pauses'] = True
            command('INPUT KEY HOME'); pump(1)
            result['background'] = memory('MEMORY')
            assert 'PXA' in ocr(shot('background')), 'Home did not reveal the launcher'
            result['launcher_visible_after_home'] = True
            assert result['background']['kind:temporary'] == result['before_start']['kind:temporary']
            paused = logs().count('BROWSER GIF FRAME'); pump(.8)
            assert logs().count('BROWSER GIF FRAME') == paused
            mark = len(records); command('PACKAGE run pxa-browser')
            wait('BROWSER IMAGE DONE index=0', mark)
            result['foreground_reloads'] = True
            x, y, w, h = geometry(logs())['content']
            mark = len(records); tap(x + 20, y + 10)
            wait('BROWSER NAV ' + args.fixture_url.rstrip('/') + '/gif-alpha', mark)
            wait('BROWSER IMAGE DONE index=0', mark)
            sprites = set()
            for index in range(5):
                image = shot('transparent-' + str(index))
                pixels = image.load()
                orange = frozenset((xx, yy) for yy in range(y, y+h) for xx in range(x, x+w)
                    if (color := pixels[xx, yy])[0] > 170 and 50 < color[1] < 160 and color[2] < 80)
                assert len(orange) > 100
                sprites.add(orange)
                hole_y = min(yy for xx, yy in orange) - 8
                hole, background = pixels[x+w//2, hole_y], pixels[x+2, hole_y]
                assert max(abs(a-b) for a, b in zip(hole, background)) <= 12
                pump(.15)
            assert len(sprites) >= 2
            result['transparent_sprite_moves_and_holes_match_background'] = True
            command('PACKAGE stop pxa-browser'); pump(1.3)
            result['closed'] = memory('MEMORY')
            for kind in ('kind:image', 'kind:temporary'):
                assert result['closed'][kind] == result['before_start'][kind], kind
            mark = len(records); command('PACKAGE run pxa-browser'); wait('BROWSER IMAGE DONE index=0', mark)
            command('PACKAGE stop pxa-browser'); pump(1.3)
            result['repeated_start_stop'] = True
            result['passed'] = True
        finally:
            try:
                for text in ('MEMORY STOP', 'PERF CLEAR', 'INPUT CANCEL', 'PACKAGE stop pxa-browser'):
                    try: command(text)
                    except pxadb.PxaDbError: pass
                pump(1)
                if args.restore_package:
                    pxadb.stage_package_files(fs, args.restore_package.resolve(), 'pxa-browser', 60, publish=False)
                    command('PACKAGE deploy pxa-browser'); pxadb.clear_staged_source(client, 'pxa-browser')
                    result['production_package_restored'] = True
                fs.get(prefs, args.output/'preferences-after.txt')
                result['preferences_preserved'] = original == (args.output/'preferences-after.txt').read_bytes()
                assert result['preferences_preserved']
                result['packages_after'] = [f.payload for f in command('PACKAGES') if f.kind == 'PKG']
            finally:
                result['passed'] = (result.get('passed', False) and result.get('preferences_preserved', False)
                    and (not args.restore_package or result.get('production_package_restored', False)))
                (args.output/'results.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
                (args.output/'records.json').write_text(json.dumps(records, ensure_ascii=False, indent=2)+'\n')
    print('pai-touch GIF pixels, LCD completion, heap windows, lifecycle and data preservation PASS')


if __name__ == '__main__': main()
