#!/usr/bin/env python3
"""Exercise the signed app through the real PXA product simulator.

Uses its debug socket to inject real pointer and keyboard events, captures
unaltered PNGs, and verifies the private save after crafting and restarting.
Run from the workspace root; --output must be under local/artifacts/.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import time

parser=argparse.ArgumentParser()
parser.add_argument('--package',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--runner',type=Path,default=Path('build/simulator/doit-gamekit-1/pxsys_product_simulator'))
parser.add_argument('--key',type=Path,default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
parser.add_argument('--fixture-tool',type=Path)
parser.add_argument('--width',type=int,default=480)
parser.add_argument('--height',type=int,default=480)
parser.add_argument('--density',type=int,default=305)
parser.add_argument('--headed',action='store_true',help='Show a desktop window; headless is the default')
parser.add_argument('--weapons-only',action='store_true',help='Fast focused regression for mining feedback and held weapons')
parser.add_argument('--terrain-only',action='store_true',help='Focused regression for terrain seams and forest/snow/jungle trees')
parser.add_argument('--appearance-only',action='store_true',help='Focused natural tree and character wardrobe captures at one display profile')
parser.add_argument('--fixtures',nargs='+',help='Limit a focused run to named save fixtures')
parser.add_argument('--motion-only',action='store_true',help='Record walking and planted tool swings; verify idle preview remains still')
parser.add_argument('--worlds-only',action='store_true',help='World catalogue, legacy saves, seed keypad, all sizes, switching and deletion at one display profile')
parser.add_argument('--legacy-fixture-tool',type=Path,help='Optional original v0.8 fixture binary for backwards compatibility QA')
parser.add_argument('--input-hints-only',action='store_true',help='Check touch/controller hint transitions in both languages at one display profile')
args=parser.parse_args()
out=args.output.resolve()
if '/local/artifacts/' not in str(out):raise SystemExit('QA output must be in local/artifacts/')
out.mkdir(parents=True,exist_ok=True)
state=out/f'state-{os.getpid()}'
state.mkdir(exist_ok=True)
sockpath=f'/tmp/pxa-starsoil-qa-{os.getpid()}.sock'
process=None
log=None
history=[]

def command(text,png=None):
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as c:
        c.settimeout(15);c.connect(sockpath);c.sendall((text+'\n').encode())
        f=c.makefile('rb');header=f.readline()
        if png:
            assert header.startswith(b'PNG '),header
            n=int(header.split()[1]);data=f.read(n);assert len(data)==n
            (out/png).write_bytes(data)
        else:assert header.startswith(b'OK'),(text,header)

def key(name,duration=.12):
    command('KEY_DOWN '+name);time.sleep(duration);command('KEY_UP '+name);time.sleep(.15)

def display_point(x,y,layout='content'):
    # These are expected UI anchors, not the app's input conversion code.
    # The display is filled; extra world area preserves square sprite pixels.
    if args.width*224>=args.height*288:
        ch=224;cw=(args.width*ch+args.height//2)//args.height
    else:
        cw=288;ch=(args.height*cw+args.width//2)//args.width
    ox,oy=(cw-288)//2,(ch-224)//2
    if layout=='content':x+=ox;y+=oy
    elif layout=='footer':x+=ox;y+=ch-224
    elif layout=='header':x+=cw-288
    elif layout=='tab':x=5+(x-5)*(cw-8)/280
    elif layout=='world':x+=ox;y+=ch-224
    return int(x*args.width/cw),int(y*args.height/ch)

def tap(x,y,layout='content'):
    px,py=display_point(x,y,layout)
    command(f'TOUCH DOWN 0 {px} {py}');time.sleep(.08)
    command(f'TOUCH UP 0 {px} {py}');time.sleep(.25)

def capture(name):
    command('SCREENSHOT AFTER_PRESENT',name+'.png')
    # Inspect original captures for full black rows/columns (letterboxing).
    from PIL import Image
    im=Image.open(out/(name+'.png')).convert('RGB')
    assert im.size==(args.width,args.height),im.size
    pixels=list(im.getdata())
    assert all(any(pixels[y*args.width+x]!=(0,0,0) for x in range(args.width)) for y in range(args.height)), 'Black horizontal border'
    assert all(any(pixels[y*args.width+x]!=(0,0,0) for y in range(args.height)) for x in range(args.width)), 'Black vertical border'

def start(number):
    global process,log
    log=(out/f'simulator-{number}.log').open('w')
    env=os.environ.copy();env['PXA_SIM_NAV_MODE']='none'
    if not args.headed:env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    process=subprocess.Popen([str(args.runner.resolve()),'--package',str(args.package.resolve()),'--publisher-key',str(args.key.resolve()),'--state-root',str(state),'--pxadb-control-socket',sockpath,'--width',str(args.width),'--height',str(args.height),'--density-dpi',str(args.density),'--locale','zh-CN'],stdout=log,stderr=subprocess.STDOUT,env=env)
    for _ in range(100):
        if process.poll() is not None:raise RuntimeError('simulator exited; inspect log')
        if Path(sockpath).exists() and 'Pocket: ready' in (out/f'simulator-{number}.log').read_text(errors='replace'):break
        time.sleep(.1)
    else:raise RuntimeError('simulator did not become ready')
    time.sleep(.3)

def stop():
    global process,log
    if process and process.poll() is None:
        process.terminate()
        try:process.wait(10)
        except subprocess.TimeoutExpired:process.kill();process.wait()
    if log:log.close()
    process=None

def saved():
    files=list((state/'private-files').glob('*/starsoil-world-v1.bin'))
    assert len(files)==1,files
    data=files[0].read_bytes()
    assert len(data)==87160,len(data)
    assert data[:4]==b'SSR1'
    return files[0],data

def enter_world():
    files=list((state/'private-files').glob('*/starsoil-world*.bin'))
    if files:
        tap(202,208)
    else:
        tap(144,208);tap(202,194)
    time.sleep(.7)

try:
    if args.worlds_only:
        assert args.fixture_tool, '--worlds-only requires --fixture-tool'
        def metadata(path):
            data=path.read_bytes()
            assert data[:4]==b'SSR1'
            w,h=struct.unpack_from('<HH',data,6)
            assert (w,h)in((256,96),(384,112),(512,144))
            assert len(data)==760+w+w*h*2
            checksum=2166136261
            for b in data[:-4]:checksum=((checksum^b)*16777619)&0xffffffff
            assert struct.unpack_from('<I',data,len(data)-4)[0]==checksum
            return {'width':w,'height':h,'seed':struct.unpack_from('<I',data,10)[0],
                    'tiles':data[228+w:228+w+w*h]}
        def go_home():
            key('ENTER');tap(144,116);time.sleep(.6)
        def type_seed(value):
            tap(72,173) # Clear, then use the actual in-game number pad.
            for digit in value:
                i=10 if digit=='0'else int(digit)-1
                tap(72+(i%3)*72,95+(i//3)*26)
        def create(size,seed,overflow=False):
            tap(72,208);tap(64+size*80,101);tap(170,148)
            type_seed(seed)
            if overflow:
                capture('seed-overflow')
                tap(198,202) # Invalid value must keep the editor open.
                capture('seed-overflow-rejected')
                from PIL import Image,ImageChops
                box=(*display_point(36,51),*display_point(252,78))
                assert ImageChops.difference(Image.open(out/'seed-overflow.png').crop(box),Image.open(out/'seed-overflow-rejected.png').crop(box)).getbbox()is None
                tap(216,173) # Backspace 6, then replace with 5.
                tap(144,121)
            capture('seed-'+str(size))
            tap(198,202);capture('create-size-'+str(size))
            tap(202,194);time.sleep(1.0);capture('created-'+str(size))
        start(100);capture('empty-title');enter_world();key('ENTER');time.sleep(.5)
        original_path,_=saved();stop()
        legacy=args.legacy_fixture_tool or args.fixture_tool
        subprocess.run([str(legacy.resolve()),'reference',str(original_path)],check=True)
        legacy_bytes=original_path.read_bytes()
        assert len(legacy_bytes)==87160
        root=original_path.parent
        start(101);capture('legacy-catalogue')
        assert original_path.read_bytes()==legacy_bytes, 'Catalogue rewrote legacy world'
        create(0,'1234567890');go_home()
        small=root/'starsoil-world-2.bin'
        first_small=metadata(small)
        assert (first_small['width'],first_small['height'],first_small['seed'])==(256,96,1234567890)
        create(2,'4294967296',True);go_home()
        large=root/'starsoil-world-3.bin'
        assert metadata(large)['seed']==4294967295 and metadata(large)['width']==512
        create(0,'1234567890');go_home()
        twin=root/'starsoil-world-4.bin'
        assert metadata(twin)['tiles']==first_small['tiles'], 'Same seed and size generated different terrain'
        assert original_path.read_bytes()==legacy_bytes, 'Creating another world changed legacy save'
        capture('four-worlds');stop();start(102);capture('restarted-catalogue')
        tap(225,108);time.sleep(1.3);capture('legacy-loaded');go_home()
        # Compare the old v1 terrain bytes after loading and saving through the new runtime.
        assert metadata(original_path)['tiles']==legacy_bytes[612:612+384*112]
        tap(225,150);time.sleep(1.3);capture('small-loaded');go_home()
        assert metadata(small)['seed']==1234567890
        tap(250,183);tap(225,108);time.sleep(1.3);capture('large-loaded');go_home()
        tap(253,108);capture('delete-confirmation');tap(90,153)
        assert large.exists(), 'Cancel deleted a world'
        kept={f.name:f.read_bytes()for f in(root/'starsoil-world-v1.bin',small,twin)}
        tap(253,108);tap(198,153);time.sleep(.7);capture('after-delete')
        assert not large.exists()
        assert all((root/n).read_bytes()==b for n,b in kept.items()), 'Delete changed another world'
        stop()
        bad=root/'starsoil-world-8.bin';bad.write_bytes(b'corrupt-world-preserve-me')
        start(103);capture('catalogue-after-delete')
        assert not large.exists(), 'Deleted active world was recreated after restart'
        assert all((root/n).read_bytes()==b for n,b in kept.items())
        tap(250,183);capture('corrupt-world-card');tap(225,150);capture('corrupt-world-error')
        tap(144,159)
        create(0,'0');go_home()
        assert metadata(large)['seed']==0, 'Explicit zero seed did not reuse the empty slot'
        assert bad.read_bytes()==b'corrupt-world-preserve-me', 'Create overwrote a corrupt world'
        tap(250,183);tap(253,108);tap(198,153);time.sleep(.6)
        assert not bad.exists(), 'Explicit deletion of a corrupt world failed'
        capture('finished-catalogue');stop()
        logs=''.join(p.read_text(errors='replace')for p in out.glob('simulator-*.log'))
        assert 'Guest exception'not in logs and 'save failed'not in logs.lower()
        print('PASS real PXA world menu: legacy v1 preserved and loaded; numeric keypad and overflow rejection; actual small/medium/large saves; identical seed terrain; four-world pagination; switch/restart; cancel and confirm active-world deletion; no resurrection; corrupt save isolation and explicit deletion; one 480px profile')
        raise SystemExit(0)
    if args.input_hints_only:
        from PIL import Image
        def verify_native_font(name,value,x,y):
            import runpy
            font_tool=runpy.run_path(str(Path(__file__).resolve().parents[1]/'tools/build_font.py'))
            glyphs={cp:(advance,rows)for cp,advance,rows in font_tool['collect'](Path(__file__).resolve().parents[1]/'art/fonts/ZLabsPixel_12px_M_CN.kbitx')}
            # One actual 480px display profile: 288 layout units, two screen
            # pixels per font bit. Origin follows the production edge mapping.
            assert (args.width,args.height)==(480,480)
            ox,oy=(x*480+287)//288,(y*480+287)//288
            width=sum(glyphs[ord(c)][0]for c in value)*2
            expected=Image.new('1',(width,24));at=0
            for c in value:
                advance,rows=glyphs[ord(c)]
                for yy in range(12):
                    for xx in range(advance):
                        if rows[yy]&(1<<(11-xx)):
                            for sy in range(2):
                                for sx in range(2):expected.putpixel((at+xx*2+sx,yy*2+sy),1)
                at+=advance*2
            actual=Image.open(out/(name+'.png')).convert('RGB').crop((ox,oy,ox+width,oy+24))
            point=next((xx,yy)for yy in range(24)for xx in range(width)if expected.getpixel((xx,yy)))
            ink=actual.getpixel(point)
            assert all((actual.getpixel((xx,yy))==ink)==bool(expected.getpixel((xx,yy))) for yy in range(24)for xx in range(width)), 'font pixels were resampled instead of exact 2x2 blocks'
        def hint_area(name,region,layout):
            a,b=region
            return Image.open(out/(name+'.png')).crop((*display_point(*a,layout),*display_point(*b,layout))).tobytes()
        def probe(name,keys,region,layout='footer',touch=(100,9,'fixed')):
            capture(name+'-touch')
            for k in keys:key(k)
            capture(name+'-controller')
            # Key release keeps the hints until another input method is used.
            time.sleep(.2);capture(name+'-released')
            tap(*touch);capture(name+'-touch-restored')
            a=hint_area(name+'-touch',region,layout)
            b=hint_area(name+'-controller',region,layout)
            assert a!=b,(name,'controller press did not show hints')
            assert hint_area(name+'-released',region,layout)==b,(name,'release hid hints')
            assert hint_area(name+'-touch-restored',region,layout)==a,(name,'touch did not restore default hints')
        start(90)
        probe('title-cn',['RIGHT'],((18,198),(270,219)),'content',(144,72,'content'))
        enter_world();time.sleep(10.1) # Let the welcome toast stop covering bag hints.
        tap(215,9,'header')
        probe('bag-cn',['UP','DOWN'],((6,174),(282,219)))
        tap(85,35,'tab')
        probe('craft-cn',['UP','DOWN'],((111,199),(234,222)))
        tap(145,35,'tab')
        probe('gear-cn',['RIGHT'],((7,188),(281,222)))
        verify_native_font('gear-cn-touch','旅人的衣橱',8,6)
        tap(258,35,'tab')
        probe('guide-cn',['UP'],((8,49),(278,194)),'content')
        key('X');tap(276,9,'header')
        probe('pause-cn',['UP','DOWN'],((68,198),(222,211)),'content',(144,43,'content'))
        tap(144,176) # Language row in the real pause menu.
        probe('pause-en',['UP','DOWN'],((68,198),(222,211)),'content',(144,43,'content'))
        tap(144,85) # Continue, then open the field guide using touch.
        tap(215,9,'header');tap(258,35,'tab')
        probe('guide-en',['UP'],((8,49),(278,194)),'content')
        tap(145,35,'tab')
        probe('gear-en',['RIGHT'],((7,188),(281,222)))
        key('X');key('ENTER');time.sleep(.5);stop()
        # The last input was a controller. A fresh process still starts hidden.
        start(91)
        probe('restarted-title-en',['RIGHT'],((18,198),(270,219)),'content',(144,72,'content'))
        stop()
        logs=''.join(p.read_text(errors='replace') for p in out.glob('simulator-*.log'))
        assert 'Guest exception' not in logs and 'save failed' not in logs.lower()
        print('PASS real PXA: exact 2x2 native Chinese font pixels; input hints hidden by default and after restart, controller press shows, release retains, touch hides; Chinese/English menus and guide; one display profile')
        raise SystemExit(0)
    if args.motion_only:
        assert args.fixture_tool,'--motion-only requires --fixture-tool'
        from PIL import Image
        def area(name,a,b):
            return Image.open(out/(name+'.png')).crop((*display_point(*a),*display_point(*b))).tobytes()
        def clip(prefix,count,delay):
            times=[]
            for f in range(count):
                time.sleep(delay);capture(f'{prefix}-{f:02d}')
                times.append(time.monotonic())
            frames=[Image.open(out/f'{prefix}-{f:02d}.png').convert('RGB')for f in range(count)]
            durations=[max(10,round((b-a)*100)*10) for a,b in zip(times,times[1:])]
            durations.append(durations[-1])
            frames[0].save(out/(prefix+'.gif'),save_all=True,append_images=frames[1:],duration=durations,loop=0)
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'motion',str(path)],check=True)
        start(80);enter_world();time.sleep(1.3)
        tap(215,9,'header');tap(145,35,'tab');capture('idle-preview-a');time.sleep(.9);capture('idle-preview-b')
        assert area('idle-preview-a',(8,49),(121,175))==area('idle-preview-b',(8,49),(121,175)), 'standing preview keeps walking'
        key('X');command('KEY_DOWN RIGHT');clip('walk-right',40,0);command('KEY_UP RIGHT');time.sleep(.3)
        command('KEY_DOWN LEFT');clip('walk-left',40,0);command('KEY_UP LEFT');stop()
        subprocess.run([str(args.fixture_tool.resolve()),'motion-tool',str(path)],check=True)
        start(81);enter_world();time.sleep(1.3);command('KEY_DOWN X');clip('tool-right',40,.015);command('KEY_UP X');time.sleep(.55)
        # Both opaque boot soles; exclude the grass that sways beside them.
        baseline=area('tool-right-00',(141,164),(150,166))
        assert all(area(f'tool-right-{i:02d}',(141,164),(150,166))==baseline for i in range(40)), 'feet change while standing and swinging'
        key('LEFT',.06);command('KEY_DOWN X');clip('tool-left',40,.015);command('KEY_UP X');stop()
        baseline=area('tool-left-00',(138,164),(147,166))
        assert all(area(f'tool-left-{i:02d}',(138,164),(147,166))==baseline for i in range(40)), 'mirrored feet change while swinging'
        print('PASS real PXA motion: static idle preview, right/left gait and planted feet during tool swings; one display profile')
        raise SystemExit(0)
    if args.terrain_only or args.appearance_only:
        assert args.fixture_tool, 'Focused appearance/terrain QA requires --fixture-tool'
        start(0);enter_world();key('ENTER');time.sleep(.6)
        path,_=saved();stop()
        fixtures=('forest','snow','jungle','reference','plain','cloth','copper','iron','wardrobe','robe') if args.appearance_only else ('joins','forest','snow','jungle','cave')
        if args.fixtures:
            assert all(name in fixtures for name in args.fixtures), 'Choose fixtures from the focused run'
            fixtures=args.fixtures
        for number,fixture in enumerate(fixtures,50):
            subprocess.run([str(args.fixture_tool.resolve()),fixture,str(path)],check=True)
            start(number);enter_world();time.sleep(1.2);capture(fixture+'-fixed')
            if args.appearance_only and fixture not in ('forest','snow','jungle'):
                tap(215,9,'header');tap(145,35,'tab');capture(fixture+'-portrait-right')
                tap(62,115);capture(fixture+'-portrait-left')
            stop()
        print('PASS headless focused appearance/terrain regression; one display profile; no black borders')
        raise SystemExit(0)
    if args.weapons_only:
        assert args.fixture_tool, '--weapons-only requires --fixture-tool'
        start(0);enter_world()
        px,py=display_point(152,146,'world')
        command(f'TOUCH DOWN 0 {px} {py}');time.sleep(.35);capture('mining-feedback')
        command(f'TOUCH UP 0 {px} {py}');key('ENTER');time.sleep(.6)
        path,_=saved();stop()
        for number,fixture in enumerate(('bow','wand'),40):
            subprocess.run([str(args.fixture_tool.resolve()),fixture,str(path)],check=True)
            start(number);enter_world();time.sleep(1.2)
            # Load protection blinks for 1 second; capture the visible actor.
            command('KEY_DOWN X');time.sleep(.14);capture(f'{fixture}-using-right');command('KEY_UP X')
            key('LEFT',.15);command('KEY_DOWN X');time.sleep(.14);capture(f'{fixture}-using-left');command('KEY_UP X')
            stop()
        print('PASS headless focused mining/weapon captures; one display profile; no black borders')
        raise SystemExit(0)
    start(1);capture('01-title');enter_world();time.sleep(.5);capture('02-world')
    px,py=display_point(152,146,"world")
    command(f'TOUCH DOWN 0 {px} {py}')
    time.sleep(.35);capture('02b-mining-feedback')
    time.sleep(.45);command(f'TOUCH UP 0 {px} {py}');time.sleep(.4)
    tap(135,38,"fixed");tap(136,98,"world");tap(15,38,"fixed")
    # Header tap opens bag at 125 DPI; a missing conversion opens pause instead.
    tap(215,9,"header");capture('03-bag')
    tap(145,35,"tab");capture('04-equipment')
    tap(85,35,"tab");capture('05-recipes');tap(168,208,"footer");time.sleep(.4)
    tap(200,35,"tab");capture('06-map');tap(258,35,"tab");capture('07-guide')
    for _ in range(4):tap(254,208,'footer')
    capture('07c-creature-guide')
    key('X');key('ENTER');time.sleep(.7);capture('07b-pause')
    path,before=saved()
    seed=struct.unpack_from('<I',before,10)[0]
    played=struct.unpack_from('<I',before,26)[0]
    crafted=struct.unpack_from('<I',before,38)[0]
    mined=struct.unpack_from('<I',before,34)[0]
    assert played>0,'Touch did not enter the world'
    assert crafted>=1,'Touch did not craft planks'
    assert mined>=1,'Holding a world tile did not mine'
    # Mining one of the two blocks below the player's body leaves the other
    # supporting the feet, so the camera remains at its initial Y.
    assert before[612+28*384+47]==9,'Plank placement did not persist'
    history.append(f'PASS real pointer input at {args.density} DPI; crafted={crafted}; bytes={len(before)}')
    if args.fixture_tool:
        inspect=subprocess.check_output([str(args.fixture_tool.resolve()),'inspect',str(path)],text=True)
        history.append(inspect.strip())
    stop();start(2);capture('08-reopened-title')
    _,after=saved();assert after==before,'Load changed the committed save'
    enter_world()
    assert 'Starsoil: loaded seed=' in (out/'simulator-2.log').read_text(errors='replace')
    history.append(f'PASS atomic save and process restart; seed={seed}')
    stop()
    path.write_bytes(b'');start(20);enter_world();key('ENTER');time.sleep(.6)
    assert path.read_bytes()==b'', 'An empty corrupted save was overwritten'
    capture('corrupt-world-preserved')
    history.append('PASS empty/corrupt world is preserved and writes disabled')
    stop();path.write_bytes(before)
    if args.fixture_tool:
        subprocess.run([str(args.fixture_tool.resolve()),'wardrobe',str(path)],check=True)
        start(3);enter_world();tap(215,9,"header");tap(145,35,"tab");time.sleep(4.6);capture('09-crystal-armor')
        tap(62,115);capture('09b-crystal-left')
        key('RIGHT');capture('09c-crystal-right')
        # Remove a real equipped helmet via the equipment slot, then persist.
        tap(210,56);capture('10-helmet-removed')
        tap(210,77);tap(210,97);capture('10b-traveler-outfit')
        key('ENTER');time.sleep(.6)
        inspect=subprocess.check_output([str(args.fixture_tool.resolve()),'inspect',str(path)],text=True)
        assert '"head":0' in inspect,inspect
        outfit=json.loads(inspect)
        assert outfit['defense']==0 and outfit['shirt'] and outfit['cape'],inspect
        history.append('PASS pointer unequip updates rendered layers and persisted armor')
        stop()
        # Exercise the final signed package, capturing original simulator PNGs.
        for number,fixture in enumerate(('cloth','copper','iron','robe','bow','wand'),30):
            subprocess.run([str(args.fixture_tool.resolve()),fixture,str(path)],check=True)
            start(number);enter_world();time.sleep(1.2)
            if fixture in ('bow','wand'):
                command('KEY_DOWN X');time.sleep(.2);capture(f'{fixture}-using-right');command('KEY_UP X')
                key('LEFT',.15);command('KEY_DOWN X');time.sleep(.2);capture(f'{fixture}-using-left');command('KEY_UP X')
            tap(215,9,"header");tap(145,35,"tab");capture(f'{fixture}-outfit-right')
            tap(62,115);capture(f'{fixture}-outfit-left');stop()
        history.append('PASS simulator captures: copper/iron/crystal, tunic/robe, both facings, bow and wand use')
        for number,fixture in enumerate(('desert','snow','cave','warden','jungle','night','joins','forest'),4):
            subprocess.run([str(args.fixture_tool.resolve()),fixture,str(path)],check=True)
            start(number);enter_world()
            if fixture=='warden':key('TAB');tap(63,38,"fixed")
            time.sleep(4.6);capture(f'{number+7:02d}-{fixture}');stop()
    logs=''.join(p.read_text(errors='replace') for p in out.glob('simulator-*.log'))
    assert 'Guest exception' not in logs and 'save failed' not in logs.lower(), 'Runtime error; inspect logs'
    (out/'checks.txt').write_text('\n'.join(history)+'\n')
    print('\n'.join(history))
finally:
    stop()
    Path(sockpath).unlink(missing_ok=True)
