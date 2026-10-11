"""Verify the redesigned reader and native TextInput in an isolated full system.

Build with tests/build_playtest.py first. No network source is contacted and no
real shelf/settings are changed. Each profile has a separate private state root.
"""
import argparse
import io
import json
import os
from pathlib import Path
import re
import socket
import shutil
import hashlib
import subprocess
import tempfile
import time
from urllib.request import urlopen
from PIL import Image, ImageChops, ImageFont
from fontTools.ttLib import TTFont

root = next(p for p in Path(__file__).resolve().parents if (p / 'tools/app.sh').exists())
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--binary', type=Path, default=root / 'build/simulator/esp-mosaico/pxsys_desktop_simulator')
p.add_argument("--baseline", action="store_true", help="Compare old Canvas UI without native-label assertions")
p.add_argument('--failure-cases', action='store_true', help='After progressive caching, verify broken/changed/invalid downloads and chapters')
p.add_argument('--fixture', action='store_true', help='QA package has a local fixture source; verify real file reads and download')
p.add_argument('--export-state', type=Path, help='Save the private QA state for a later offline run')
p.add_argument('--cached-state', type=Path, help='Clone private QA state and read the completed cached book')
p.add_argument('--profile', help='Run only the named screen profile')
p.add_argument('--real-book',type=Path,help='Use unmodified real Gutenberg/GITenberg 西游记 bytes in the private partial cache; requires --cached-state')
p.add_argument('--live-theme', action='store_true', help='Toggle the full system quick-settings theme and restore it')
p.add_argument('--font-boundaries', action='store_true', help='Exercise 12 and 28 point reading preferences')
p.add_argument('--spacing-matrix', action='store_true', help='Check 12/16/20/24/28 dp at 0/6/12 dp leading on the same real chapter')
a = p.parse_args()
if a.spacing_matrix and not a.real_book: p.error('--spacing-matrix requires --real-book and --cached-state')
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=True)
binary = a.binary.resolve()
key = root / 'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'
profiles = [(480, 480, 305, 58, False, 'dark'), (296, 240, 160, 58, False, 'light'),
            (360, 480, 320, 40, False, 'light'), (480, 480, 320, 0, True, 'dark'),
            (800, 480, 160, 0, False, 'light')]
if a.fixture: profiles = profiles[:1]
if a.profile:
    profiles = [v for v in profiles if f'{v[0]}x{v[1]}-{v[2]}dpi' + ('-circle' if v[4] else '') == a.profile]
    assert profiles, a.profile
def expand_hits(text):
 return re.sub(r'READER-HITS ([0-9,;]+)', lambda m: '\n'.join(
  'READER-HIT action=%s arg=%s x=%s y=%s' % tuple(item.split(','))
  for item in m[1].split(';') if item), text)
records = []
font_path = root/'factory/base/system/fonts/noto_sans_cjk_common.ttf'
font_cmap = TTFont(font_path).getBestCmap()
for width, height, dpi, radius, circle, theme in profiles:
    name = f'{width}x{height}-{dpi}dpi' + ('-circle' if circle else '')
    out = a.output / name
    out.mkdir(exist_ok=True)
    state_context = tempfile.TemporaryDirectory(dir=out, prefix='state-')
    state = Path(state_context.name)
    if a.cached_state: shutil.copytree(a.cached_state, state, dirs_exist_ok=True)
    subprocess.run([str(binary.parent / 'pxsys_package_installer'), '--storage-root', str(state),
                    '--publisher-key', str(key), '--source', str(a.package.resolve()),
                    '--expected-id', 'pxa-novel-reader-qa'], check=True, capture_output=True)
    if a.real_book:
        assert a.cached_state, 'Use a private state template for the real-book test'
        book_url = 'https://raw.githubusercontent.com/GITenberg/---_23962/master/23962-0.txt'
        folder = next(state.glob('private-files/*pxa-novel-reader-qa'))
        for path in folder.glob('b*.txt'): path.unlink()
        raw = a.real_book.read_bytes(); partial = raw[:131072]
        value = 14695981039346656037
        for byte in book_url.encode(): value = ((value ^ byte)*1099511628211)&((1<<64)-1)
        (folder/f'b{value:x}.part').write_bytes(partial)
        state_file = folder/'reader-state.json'; saved = json.loads(state_file.read_text())
        saved['settings'].update(font=16,gap=6,theme=3)
        saved['sources'] = [v for v in saved['sources'] if v['kind']!=1]
        saved['shelf'][1] = dict(title='西游记',author='吴承恩',url=book_url,toc='',validator='',position=0,total=len(raw),complete=False)
        state_file.write_text(json.dumps(saved,ensure_ascii=False))
    control = Path('/tmp') / f'pxa-reader-sdk-{os.getpid()}.sock'
    log_path = out / 'run.log'
    def logs():
        raw = log_path.read_text(errors='replace')
        # A live printf can be observed between writes; never parse a partial
        # hit tuple or treat an unfinished frame trace as a ready control tree.
        return expand_hits(re.sub(r'\x1b\[[0-9;]*m', '', raw[:raw.rfind('\n')+1]))
    def wait(predicate, timeout=15):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            text = logs()
            assert 'READER-ERROR' not in text, text[-2000:]
            if predicate(text):
                return text
            time.sleep(.05)
        raise AssertionError(str(process.poll()) + ' ' + logs()[-2000:])
    def send(command):
        with socket.socket(socket.AF_UNIX) as s:
            s.settimeout(15)
            s.connect(str(control))
            s.sendall((command + '\n').encode())
            data = b''
            while b'\n' not in data:
                data += s.recv(65536)
            header, data = data.split(b'\n', 1)
            if header.startswith(b'PNG '):
                size = int(header[4:])
                while len(data) < size:
                    data += s.recv(65536)
                return data
            assert header == b'OK', header
    def tap(action, arg=0, native=False):
        before = logs()
        tail = before[before.rfind('READER-UI '):]
        if action in (5, 6, 7) and not a.baseline:
            assert 'screen=3 ' in tail, ('Reading tap outside body', action, tail)
            assert 'READER-HIT ' not in tail, 'Native controls overlap the reading body'
            x = width // 6 if action == 6 else width * 5 // 6 if action == 5 else width // 2
            send(f'TAP {x} {height // 2}')
            wait(lambda s: ('READER-UI screen=4 ' if action == 7 else 'READER-PAGE ') in s[len(before):])
            time.sleep(.35)
            return
        hit = re.search(rf'READER-HIT action={action} arg={arg} x=(\d+) y=(\d+)', tail)
        assert hit, (action, tail)
        x, y = map(int, hit.groups())
        assert 0 <= x < width and 0 <= y < height
        send(f'TAP {x} {y}')
        if not native:
            wait(lambda s: 'READER-UI ' in s[len(before):])
        time.sleep(.35)
    def hold_book(index=0):
        before = logs(); tail = before[before.rfind('READER-UI '):]
        hit = re.search(rf'READER-HIT action=4 arg={index} x=(\d+) y=(\d+)', tail)
        assert hit, tail
        x, y = map(int, hit.groups())
        send(f'POINTER DOWN {x} {y} 0'); time.sleep(.7); send(f'POINTER UP {x} {y} 0')
        wait(lambda s: 'READER-UI screen=8 ' in s[len(before):]); time.sleep(.2)
    os_memory = []
    def capture(label):
        status = (Path('/proc') / str(process.pid) / 'status').read_text()
        os_memory.append({'frame': label, **{key: int(re.search(rf'^{key}:\s+(\d+)', status, re.M)[1]) * 1024
                        for key in ('VmRSS', 'VmHWM')}})
        data = send('SCREENSHOT')
        (out / f'{label}.png').write_bytes(data)
        return Image.open(io.BytesIO(data)).convert('RGB')
    def assert_button_surface(frame, action):
        text = logs(); tail = text[text.rfind('READER-UI '):]
        hit = re.search(rf'READER-HIT action={action} arg=0 x=(\d+) y=(\d+)', tail)
        box = re.findall(rf'READER-BUTTON action={action} width=(\d+) height=(\d+)', tail)
        assert hit and box, ('Missing contrast probe', action)
        x, y = map(int, hit.groups()); w, h = map(int, box[-1])
        fill = frame.getpixel((x - w // 2 + 5, y))
        background = frame.getpixel((width // 2, height - 2))
        assert max(abs(a-b) for a,b in zip(fill,background)) >= 8, ('Button blends into page', action, fill, background)
        # At least one fully covered glyph pixel must visibly contrast with
        # the fill. Exclude the outline and rounded corners from the sample.
        pixel_bytes = frame.crop((x-w//2+4,y-h//2+4,x+w//2-4,y+h//2-4)).tobytes()
        pixels = (pixel_bytes[i:i+3] for i in range(0,len(pixel_bytes),3))
        assert max(sum(abs(a-b) for a,b in zip(pixel,fill)) for pixel in pixels) >= 90, ('Unreadable button label', action, fill)
    preview_results = []
    reading_space = []
    def assert_preview(label):
        frame = capture(label)
        matches = re.findall(r'READER-PREVIEW pixels=(\d+) step=(\d+) y=(\d+) x=(\d+) width=(\d+) height=(\d+) x2=(\d+) width2=(\d+)', logs())
        assert matches, 'Missing preview geometry'
        size, step, y, x, w, h, x2, w2 = map(int,matches[-1])
        assert y >= 0 and y+step+h <= height and min(w,w2) >= 5*size
        # Read actual ink rows separately for each sample; no reliance solely
        # on Guest font-size logs. Font ascenders must not be clipped.
        ink_heights = []
        for yy,x,w in ((y,x,w),(y+step,x2,w2)):
            background = frame.getpixel((x+w-3,yy+h//2))
            rows = [row for row in range(yy,yy+h) if any(
                sum(abs(a-b) for a,b in zip(frame.getpixel((col,row)),background))>90
                for col in range(x,min(x+5*size,x+w)))]
            assert rows and min(rows)>yy and max(rows)<yy+h-1, (name,label,yy,h,rows)
            ink_heights.append(max(rows)-min(rows)+1)
        value = {'label':label,'pixels':size,'step':step,'ink_heights':ink_heights}
        preview_results.append(value); return value
    def assert_page_number(label):
        frame=capture(label)
        values=re.findall(r'READER-NUMBER current=(\d+) total=(\d+) x=(\d+) y=(\d+) width=(\d+) height=(\d+) pixels=(\d+) text=([^\n]*)',logs())
        assert values,'Missing page number'
        current,total,x,y,w,h,pixels=map(int,values[-1][:7]);text=values[-1][7].rstrip()
        assert x>=0 and y>=0 and x+w<=width and y+h<=height,(name,values[-1])
        font=ImageFont.truetype(str(root/'factory/base/system/fonts/noto_sans_cjk_common.ttf'),pixels)
        assert font.getlength(text)<=w and font.getbbox(text)[3]-font.getbbox(text)[1]<h
        background=frame.getpixel((width//2,min(height-1,y+h-1)))
        ink=[yy for yy in range(y,y+h) if any(sum(abs(a-b) for a,b in zip(frame.getpixel((xx,yy)),background))>90
               for xx in range(x,x+w))]
        assert ink and min(ink)>y and max(ink)<y+h-1,(name,text,ink)
        return dict(current=current,total=total,text=text,pixels=pixels,y=y,width=w,height=h)
    def assert_reading_glyphs(label):
        frame=capture(label)
        text=logs();footer=text.rfind('READER-NUMBER ')
        assert footer>=0,'Missing current body footer'
        previous=text.rfind('READER-NUMBER ',0,footer)
        tail=text[max(0,previous):footer]
        lines=re.findall(r'READER-LINE x=(\d+) y=(\d+) width=(\d+) height=(\d+) size=(\d+) text=([^\n]*)',tail)
        assert lines,'Missing body glyph geometry'
        headers=re.findall(r'READER-HEADER x=(\d+) y=(\d+) width=(\d+) height=(\d+) pixels=(\d+) text=([^\n]*)',text)
        if headers:
            x,y,w,h,pixels=map(int,headers[-1][:5]);title=headers[-1][5].rstrip()
            font=ImageFont.truetype(str(font_path),pixels)
            assert font.getlength(title)<=w,(name,'Book title overflows',title,w,pixels)
            assert y+h<=int(lines[0][1]),(name,'Book title overlaps body',headers[-1],lines[0])
            background=frame.getpixel((x+w-1,y+h-1))
            ink=[yy for yy in range(y,y+h) if any(sum(abs(a-b) for a,b in zip(frame.getpixel((xx,yy)),background))>90
                 for xx in range(x,x+w))]
            assert ink and min(ink)>y and max(ink)<y+h-1,(name,'Book title clipped',ink,headers[-1])
        checked=0
        for xx,yy,ww,hh,size,value in lines:
            x,y,w,h,pixels=map(int,(xx,yy,ww,hh,size))
            font=ImageFont.truetype(str(font_path),pixels)
            for i,ch in enumerate(value.rstrip()):
                if ch.isspace() or ord(ch)==0xfeff:continue
                assert ord(ch) in font_cmap,(label,'Missing glyph',ch,hex(ord(ch)))
                if not ('\u3400'<=ch<='\u9fff'):continue
                # Host positions by the font's advances, not the Guest's
                # conservative wrapping estimate. Inspect each Han character.
                left=x+int(font.getlength(value[:i]));right=x+int(font.getlength(value[:i+1]))
                assert x<=left<right<=x+w,(label,ch,left,right,x,w)
                background=frame.getpixel((x+w-1,y+h-1))
                ink=sum(sum(abs(a-b) for a,b in zip(frame.getpixel((col,row)),background))>90
                        for row in range(y,y+h) for col in range(left,right))
                assert ink>2,(label,'Blank screen glyph',ch,left,right,y,h)
                checked+=1
        assert checked>0
        value={'label':label,'visible_han_characters':checked,'body_lines':len(lines),
               'body_top':int(lines[0][1]),'body_bottom':int(lines[-1][1])+int(lines[-1][3]),
               'body_pixels':int(lines[0][4]),
               'file_offset':int(re.findall(r'READER-PAGE offset=(\d+)',text)[-1]),
               'text_sha256':hashlib.sha256(''.join(v[-1].rstrip() for v in lines).encode()).hexdigest()}
        if a.spacing_matrix:
            number=re.findall(r'READER-NUMBER current=\d+ total=\d+ x=\d+ y=(\d+)',text)[-1]
            value['footer_top']=int(number)
            value['bottom_space_to_footer']=value['footer_top']-value['body_bottom']
        if headers: value['header_pixels']=int(headers[-1][4])
        reading_space.append(value)
        return value
    args = [str(binary), '--width', str(width), '--height', str(height), '--density-dpi', str(dpi),
            '--safe-insets', '12,12,12,12', '--locale', 'zh-CN', '--' + theme, '--gestures',
            '--installed-packages-root', str(state / 'packages'), '--product-runner', str(binary.parent / 'pxsys_product_simulator'),
            '--publisher-key', str(key), '--state-root', str(state), '--pxadb-control-socket', str(control),
            '--launch', 'pxa-novel-reader-qa']
    args += ['--round'] if circle else ['--corner-radius', str(radius)]
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with log_path.open('w') as log:
        process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT,
                                   env=env)
        try:
            wait(lambda s: 'READER-HIT action=4 arg=0 ' in s)
            initial = capture('shelf')
            center = initial.getpixel((width // 2, height * 3 // 4))
            assert (max(center) < 70) == (theme == 'dark'), (name, center, theme)
            if a.live_theme:
                def shade():
                    send(f'POINTER DOWN {width // 2} 1 0')
                    for step in range(1, 17):
                        send(f'POINTER MOVE {width // 2} {height * step // 20} 0'); time.sleep(.03)
                    send(f'POINTER UP {width // 2} {height * 4 // 5} 0'); time.sleep(.4)
                shade(); capture('system-theme-shade')
                assert (width, height, dpi) == (800, 480, 160), 'Verified system quick-settings geometry'
                def dismiss_shade():
                    send(f'POINTER DOWN {width // 2} {height - 3} 0')
                    for step in range(1, 13):
                        send(f'POINTER MOVE {width // 2} {height - 3 - step * 24} 0'); time.sleep(.03)
                    send(f'POINTER UP {width // 2} {height - 291} 0'); time.sleep(.5)
                send('TAP 588 320'); time.sleep(.4); capture('system-dark-shade')
                dismiss_shade()
                changed = capture('system-dark-reader')
                assert max(changed.getpixel((width // 2, height * 3 // 4))) < 70
                assert 'READER-HIT action=4 ' in logs()[logs().rfind('READER-UI '):]
                shade(); send('TAP 588 320'); time.sleep(.4); dismiss_shade()
                restored = capture('system-light-restored')
                assert min(restored.getpixel((width // 2, height * 3 // 4))) > 200
                shade(); send('TAP 758 40'); time.sleep(.5); capture('system-settings')
                send('TAP 400 288'); time.sleep(.5); capture('system-appearance')
                def return_from_settings():
                    send('TAP 35 64'); time.sleep(.3)
                    send('POINTER DOWN 1 240 0')
                    for step in range(1, 13):
                        send(f'POINTER MOVE {1 + step * 20} 240 0'); time.sleep(.03)
                    send('POINTER UP 241 240 0'); time.sleep(.6)
                send('TAP 104 395'); time.sleep(.4); capture('system-coral-settings')
                return_from_settings(); coral = capture('system-coral-reader')
                r, g, b = coral.getpixel((30, height - 34))
                assert r > b + 30, ('Reader did not adopt system coral', r, g, b)
                shade(); send('TAP 758 40'); time.sleep(.4); send('TAP 400 288'); time.sleep(.4)
                send('TAP 104 310'); time.sleep(.4); return_from_settings()
                blue = capture('system-blue-restored')
                assert blue.getpixel((30, height - 34)) == initial.getpixel((30, height - 34))

            hold_book(); capture('book-management'); tap(27); capture('remove-confirmation'); tap(16)
            assert 'READER-HIT action=4 arg=0 ' in logs()[logs().rfind('READER-UI '):]

            tap(3); settings_frame = capture('settings'); assert_button_surface(settings_frame, 11)
            tap(11); tap(12); tap(13); capture('tinted-settings')
            tap(13); capture('night-settings'); tap(13); tap(13)
            if a.font_boundaries:
                for _ in range(3): tap(10)
                small_preview = assert_preview('smallest-font-settings')
                tap(16); tap(4, 1 if a.cached_state else 0)
                if a.real_book: assert_reading_glyphs('smallest-reading')
                else: capture('smallest-reading')
                tap(7); tap(3)
                for _ in range(8): tap(11)
                large_preview = assert_preview('largest-font-settings')
                assert large_preview['pixels'] > small_preview['pixels'] and min(large_preview['ink_heights']) > max(small_preview['ink_heights'])
                tap(12); gap_preview = assert_preview('largest-font-larger-gap')
                assert gap_preview['step'] > large_preview['step'] and gap_preview['pixels'] == large_preview['pixels']
                tap(16)
                if a.real_book: assert_reading_glyphs('largest-reading')
                else: capture('largest-reading')
                tap(7); tap(3)
                for _ in range(6): tap(10)
                # Restore the seeded 6 dp gap after the boundary checks, so
                # layout comparisons use ordinary reading preferences.
                for _ in range(5): tap(12)
                tap(16); tap(7); tap(0); tap(3)
            tap(16); tap(4, 1 if a.cached_state else 0); wait(lambda s: 'READER-PAGE offset=' in s); capture('reading')
            if not a.baseline:
                wait(lambda s:re.search(r'READER-NUMBER current=\d+ total=[1-9]',s[s.rfind('READER-PAGE '):]),30)
                numbered=assert_page_number('page-number')
                assert numbered['total']>0 and numbered['text'].startswith('1 / ')
            if not a.baseline:
                # A drag and a stray release must leave this page
                # unchanged. The former footer must now also accept body taps.
                before = logs()
                x, y = width * 5 // 6, height // 2
                send(f'POINTER DOWN {x} {y} 0'); time.sleep(.08)
                send(f'POINTER MOVE {x - 24} {y} 0'); time.sleep(.08)
                send(f'POINTER UP {x - 24} {y} 0')
                time.sleep(.4)
                assert 'READER-PAGE ' not in logs()[len(before):]
                assert 'READER-UI screen=4 ' not in logs()[len(before):]
                send(f'POINTER UP {x} {y} 0')
                time.sleep(.4)
                assert 'READER-PAGE ' not in logs()[len(before):]
                first = int(re.findall(r'READER-PAGE offset=(\d+)', logs())[-1])
                tap(5); tap(6)
                assert int(re.findall(r'READER-PAGE offset=(\d+)', logs())[-1]) == first
                last_y, last_h = map(int,re.findall(r'READER-LINE x=\d+ y=(\d+) width=\d+ height=(\d+)',logs())[-1])
                before = logs(); send(f'TAP {width//2} {last_y+last_h//2}')
                wait(lambda s:'READER-UI screen=4 ' in s[len(before):]); time.sleep(.35)
                tap(16)
                (out/'reading-controls.json').write_text(json.dumps({'visible_navigation':False,'tap_next_previous_roundtrip':True,'bottom_body_menu_tap':True,'drag_ignored':True,'stray_release_ignored':True},indent=2)+'\n')
                numbered_after=assert_page_number('page-number-return')
                assert numbered_after['current']==1 and numbered_after['total']==numbered['total']
                (out/'page-number.json').write_text(json.dumps(numbered_after,ensure_ascii=False,indent=2)+'\n')
            if a.real_book:
                assert 'READER-DOWNLOAD' not in logs()
                tap(7); tap(8); capture('real-partial-contents')
                assert 'READER-HIT action=25 arg=0 ' in logs()[logs().rfind('READER-UI '):]
                tap(25); glyph_checks=[assert_reading_glyphs('real-first-chapter')]
                for page_index in range(6): tap(5); capture(f'real-prose-{page_index}')
                glyph_checks.append(assert_reading_glyphs('real-prose-glyphs'))
                # Compare the same original byte position even if the layout
                # now fits more text and ordinary page boundaries have moved.
                tap(7); tap(8); tap(25,1)
                glyph_checks.append(assert_reading_glyphs('fixed-second-chapter'))
                (out/'glyph-checks.json').write_text(json.dumps(glyph_checks,ensure_ascii=False,indent=2)+'\n')
                if a.spacing_matrix:
                    spacing=[]
                    def preferences(font,gap):
                        tap(7);tap(3)
                        current=json.loads(state_file.read_text())['settings']
                        for _ in range(abs(current['font']-font)//2):tap(10 if current['font']>font else 11)
                        for _ in range(((gap-current['gap'])%14)//2):tap(12)
                        tap(16)
                    for font in (12,16,20,24,28):
                        for gap in (0,6,12):
                            preferences(font,gap)
                            value=assert_reading_glyphs(f'spacing-{font}-{gap}')
                            value.update(font_dp=font,gap_dp=gap)
                            assert value['file_offset']==22819
                            spacing.append(value)
                            tap(5);tap(6)
                            assert int(re.findall(r'READER-PAGE offset=(\d+)',logs())[-1])==22819
                    preferences(16,6)
                    (out/'spacing-matrix.json').write_text(json.dumps(spacing,ensure_ascii=False,indent=2)+'\n')
                tap(7); tap(0); tap(1); tap(18); tap(29)
                wait(lambda s:'READER-SEARCH results=1 instant=1' in s)
                capture('real-instant-search'); tap(4); capture('real-search-open')
                assert next(folder.glob('*.part')).read_bytes()==partial
                (out/'real-book.json').write_text(json.dumps({'source':book_url,'original_bytes':len(raw),'original_sha256':hashlib.sha256(raw).hexdigest(),'partial_bytes':len(partial),'partial_sha256':hashlib.sha256(partial).hexdigest(),'partial_directory':True,'searched_and_opened':True},ensure_ascii=False,indent=2)+'\n')
            tap(7); capture('reading-menu'); assert 'action=27 ' not in logs()[logs().rfind('READER-UI '):]; tap(8); capture('contents'); tap(16)
            tap(7); tap(0); tap(1); base_input_bytes = int(re.findall(r'input_bytes=(\d+)',logs())[-1]); before = capture('search'); assert_button_surface(before, 17); tap(30, native=True)
            time.sleep(.3); after = capture('system-ime')
            pixels = ImageChops.difference(before, after).tobytes()
            changed = sum(any(pixels[i:i+3]) for i in range(0, len(pixels), 3))
            assert changed > width * height // 15, 'Missing system input method'
            if width == 480 and height == 480 and dpi == 305:
                # K9 pinyin: 64 -> choose ni -> first candidate 你 -> submit.
                for x, y in [(329, 335), (150, 335), (240, 430), (100, 232)]:
                    send(f'TAP {x} {y}'); time.sleep(.2)
                capture('chinese-input')
                position = len(logs())
                send('TAP 420 382')
                wait(lambda s: re.search(rf'READER-UI .*input_bytes={base_input_bytes+3}\b', s[position:]))
                submitted = capture('chinese-submitted')
                pixels = ImageChops.difference(submitted, after).tobytes()
                assert sum(any(pixels[i:i+3]) for i in range(0, len(pixels), 3)) > width * height // 15
                # Switching page and restoring search must preserve query; deletion then submit works.
                tap(0); tap(1); capture('query-restored')
                assert re.search(rf'input_bytes={base_input_bytes+3}\b',logs()[logs().rfind('READER-UI '):])
                tap(30, native=True)
                reopened = capture('query-edit-ime')
                difference = ImageChops.difference(reopened, submitted).tobytes()
                assert sum(any(difference[i:i+3]) for i in range(0, len(difference), 3)) > width * height // 15
                send('TAP 420 287'); time.sleep(.2); position = len(logs()); send('TAP 420 382')
                wait(lambda s: re.search(rf'READER-UI .*input_bytes={base_input_bytes}\b', s[position:]))
                capture('empty-submitted')
            if not (width == 480 and height == 480 and dpi == 305):
                x, y = {(296, 240): (250, 185), (360, 480): (309, 382),
                        (480, 480): (365, 350), (800, 480): (725, 431)}[(width, height)]
                position = len(logs()); send(f'TAP {x} {y}')
                wait(lambda s: 'READER-UI ' in s[position:])
            time.sleep(.5)
            if not a.fixture and not a.cached_state:
                tap(17); wait(lambda s: 'READER-SEARCH results=4 instant=1' in s)
                capture('instant-search-results')
                assert 'READER-HIT action=4 arg=0 ' in logs()[logs().rfind('READER-UI '):], 'Search succeeded but no visible result can be opened'
                tap(18); tap(29, 1); wait(lambda s: 'READER-SEARCH results=1 instant=1' in s)
                capture('single-search-result'); assert 'READER-HIT action=4 arg=0 ' in logs()[logs().rfind('READER-UI '):]
            tap(2); capture('sources'); tap(21); capture('disabled-source'); tap(21)
            if not a.fixture:
                tap(19); capture('add-source'); tap(2)
            tap(0)
            if a.fixture:
                tap(1); tap(17)
                wait(lambda s: 'READER-SEARCH results=6' in s)
                urlopen('http://127.0.0.1:18766/control/cache-delay?seconds=1.2', timeout=5).close()
                begin = time.monotonic(); position = len(logs()); tap(4)
                wait(lambda s: re.search(r'READER-PAGE .*lines=[1-9]', s[position:]))
                assert 'READER-DOWNLOAD complete=1' not in logs()[position:]
                first_read_ms = round((time.monotonic() - begin) * 1000, 1)
                capture('partial-reading'); first_offset = int(re.findall(r'READER-PAGE offset=(\d+)', logs())[-1])
                tail = logs()[logs().rfind('READER-UI '):]
                assert 'screen=3 busy=1 ' in tail
                assert 'READER-HIT ' not in tail
                assert not re.search(r'READER-TEXT .*text=正在缓存正文',logs()), 'Loading label remains above cached body'
                tap(5); wait(lambda s: int(re.findall(r'READER-PAGE offset=(\d+)', s)[-1]) > first_offset)
                capture('partial-next-page'); tap(7); tap(8)
                capture('partial-contents')
                tail = logs()[logs().rfind('READER-UI '):]
                assert 'READER-HIT action=25 arg=0 ' in tail and 'busy=1 ' in tail
                assert 'READER-DOWNLOAD complete=1' not in logs()[position:]
                tap(25); tap(7); tap(28)
                assert '缓存已暂停' in logs()
                partial = list(state.glob('private-files/**/*.part'))
                assert len(partial) == 1 and 8192 <= partial[0].stat().st_size < 258777
                paused_bytes = partial[0].stat().st_size
                tap(16); tap(7); tap(0); tap(4, 1); capture('reopened-partial')
                wait(lambda s: re.search(r'READER-NUMBER .*text=\d+ / 已缓存 [1-9]', s[s.rfind('READER-PAGE '):]),30)
                partial_number=assert_page_number('partial-page-number')
                assert '已缓存' in partial_number['text'] and '+' in partial_number['text']
                tap(7); tap(9)
                wait(lambda s: 'READER-DOWNLOAD complete=1 bytes=258777' in s, 40)
                ranges = json.load(urlopen('http://127.0.0.1:18766/stats', timeout=5))
                assert sum(row['start'] == 0 for row in ranges) == 1, ranges
                assert any(row['start'] == paused_bytes for row in ranges), ranges
                (out / 'progressive-cache.json').write_text(json.dumps({'first_read_ms':first_read_ms,'paused_bytes':paused_bytes,'ranges':ranges}, indent=2) + '\n')
                wait(lambda s: 'screen=4 busy=0' in s[s.rfind('READER-UI '):]); time.sleep(.2); tap(16)

                wait(lambda s: 'screen=3 busy=0' in s[s.rfind('READER-UI '):])
                wait(lambda s: re.search(r'READER-NUMBER .*text=\d+ / [1-9]\d* 页', s[s.rfind('READER-PAGE '):]),30)
                complete_number=assert_page_number('downloaded-reading')
                assert '已缓存' not in complete_number['text'] and complete_number['total']>partial_number['total']
                tap(5); tap(6)
                tap(7); tap(8); tap(25, 1)
                # Clears recent history, exercising the sparse canonical-page lookup.
                before = logs(); offset = int(re.findall(r'READER-PAGE offset=(\d+)', before)[-1])
                tap(6); wait(lambda s: 'READER-PAGE offset=' in s[len(before):])
                previous_offset = int(re.findall(r'READER-PAGE offset=(\d+)', logs())[-1])
                assert 0 <= previous_offset < offset
                capture('previous-from-file')
                previous_number=assert_page_number('previous-page-number')
                assert previous_number['current']>0 and previous_number['total']==complete_number['total']
                (out/'cached-page-numbers.json').write_text(json.dumps({'partial':partial_number,'complete':complete_number,'previous_lookup':previous_number},ensure_ascii=False,indent=2)+'\n')
                files = list(state.glob('private-files/**/*.txt'))
                assert len(files) == 1 and files[0].stat().st_size == 258777
                assert hashlib.sha256(files[0].read_bytes()).hexdigest() == 'f1401c49bf6a399717c05f9dbcd86ec63019f66e3e030f2a9a505078a574d7d9'
                if a.export_state:
                    if a.export_state.exists(): shutil.rmtree(a.export_state)
                    shutil.copytree(state, a.export_state)
            if a.failure_cases:
                assert a.fixture
                outcomes = []
                def select_fixture(index):
                    tap(7); tap(0); tap(1); tap(17)
                    wait(lambda s: 'READER-SEARCH results=6' in s)
                    for _ in range(6):
                        tail = logs()[logs().rfind('READER-UI '):]
                        if f'READER-HIT action=4 arg={index} ' in tail: break
                        tap(24)
                    else: raise AssertionError('Fixture book not visible')
                    tap(4, index)
                for index, expected, label in [(2, '书源暂时不可用', 'broken'),
                        (3, '正文已变化', 'changed'), (4, '书源目录为空', 'empty'),
                        (5, '正文必须是有效 UTF-8', 'invalid-utf8')]:
                    at = len(logs()); select_fixture(index)
                    wait(lambda s: expected in s[at:] and 'busy=0 ' in s[s.rfind('READER-UI '):], 30)
                    capture('rejected-' + label)
                    assert len(list(state.glob('private-files/**/*.txt'))) == 1, 'Invalid download was published'
                    outcomes.append({'case': label, 'rejected': True, 'complete_cache_preserved': True})
                urlopen('http://127.0.0.1:18766/control/cache-delay?seconds=3',timeout=5).close()
                select_fixture(1); tap(7); tap(8); capture('progressive-chapter-contents')
                tail=logs()[logs().rfind('READER-UI '):]
                assert 'busy=1 ' in tail and '待缓存' in tail and 'READER-HIT action=25 arg=2 ' in tail
                before=len(logs());tap(25,2)
                assert 'screen=6 ' in logs()[logs().rfind('READER-UI '):] and '这一章还未缓存' in logs()[before:]
                capture('pending-chapter-guard')
                tail=logs()[logs().rfind('READER-UI '):]
                hit=re.search(r'READER-HIT action=25 arg=0 x=(\d+) y=(\d+)',tail);assert hit
                x,y=map(int,hit.groups());cached_before=int(re.findall(r'READER-INDEX chapters=\d+ cached=(\d+)',logs())[-1])
                send(f'POINTER DOWN {x} {y} 0');time.sleep(.1);position=len(logs())
                wait(lambda s:any(int(n)>cached_before for n in re.findall(r'READER-INDEX chapters=\d+ cached=(\d+)',s[position:])),10)
                assert 'READER-UI screen=6 ' not in logs()[position:], 'Directory tree replaced during press'
                send(f'POINTER UP {x} {y} 0')
                wait(lambda s:'READER-UI screen=3 ' in s[position:]);time.sleep(.4)
                outcomes.append({'case':'directory-held-during-cache','pointer_release_opened_chapter':True})
                wait(lambda s: '分章小说' in s and len(list(state.glob('private-files/**/*.txt'))) == 2, 30)
                expected = ''.join(f'第{i}章 旅途\n\n' + '山间的书屋打开了门。\n' * 60 + '\n\n' for i in range(1, 4)).encode()
                chapter_file = next(path for path in state.glob('private-files/**/*.txt') if path.stat().st_size != 258777)
                assert chapter_file.read_bytes() == expected, 'Chapter concatenation differs'
                capture('chapter-cache'); outcomes.append({'case': 'chapters', 'sha256': hashlib.sha256(expected).hexdigest()})
                (out / 'failure-cases.json').write_text(json.dumps(outcomes, ensure_ascii=False, indent=2) + '\n')
            if not a.baseline:
                font_path = root / 'factory/base/system/fonts/noto_sans_cjk_common.ttf'
                checked = 0
                for w, h, size, label in re.findall(r'READER-BUTTON action=\d+ width=(\d+) height=(\d+) size=(\d+) text=([^\n]*)', logs()):
                    w, h, size = map(int, (w, h, size)); font = ImageFont.truetype(str(font_path), size)
                    bbox = font.getbbox(label)
                    assert font.getlength(label) <= w - 2, (name, label, w, size)
                    assert bbox[3] - bbox[1] <= h - 2, (name, label, h, size)
                    checked += 1
                for w,h,size,label in re.findall(r'READER-LINE x=\d+ y=\d+ width=(\d+) height=(\d+) size=(\d+) text=([^\n]*)',logs()):
                    w,h,size=map(int,(w,h,size));font=ImageFont.truetype(str(font_path),size)
                    assert font.getlength(label) <= w+1,(name,label,w,size,font.getlength(label))
                    assert font.getbbox(label)[3]-font.getbbox(label)[1] <= h,(name,label,h,size)
                assert checked > 50 and 'native_ui=1' in logs()
                for line in re.findall(r'READER-UI [^\n]*', logs()):
                    if 'screen=3 ' not in line and 'screen=5 ' not in line: assert 'draw_bytes=0' in line, line
            if not a.fixture and not a.cached_state:
                if 'screen=0 ' not in logs()[logs().rfind('READER-UI '):]: tap(0)
                hold_book(); tap(27); tap(32); capture('empty-shelf')
                wait(lambda s: '已移除书籍及本地缓存' in s)
                assert 'READER-HIT action=4 ' not in logs()[logs().rfind('READER-UI '):]
            (out / 'preview.json').write_text(json.dumps(preview_results,indent=2)+'\n')
            (out / 'reading-space.json').write_text(json.dumps(reading_space,indent=2)+'\n')
            (out / 'os-memory.json').write_text(json.dumps(os_memory, indent=2) + '\n')
            draw_sizes = [int(v) for v in re.findall(r'draw_bytes=(\d+)', logs())]
            assert draw_sizes and max(draw_sizes) <= 3600
            if not a.fixture:
                assert 'READER-DOWNLOAD' not in logs() and 'READER-SEARCH failed=' not in logs()
            records.append({'profile': name, 'native_ime': True, 'local_reading': True,
                            'menu_settings_contents': True, 'max_canvas_bytes': max(draw_sizes)})
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait(timeout=5)
            control.unlink(missing_ok=True)
            state_context.cleanup()
(a.output / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
print('Reader SDK/UI full-system verification passed:', len(records), 'profiles')
