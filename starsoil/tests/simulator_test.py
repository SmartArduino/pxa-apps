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
parser.add_argument('--performance-only',action='store_true',help='Measure submitted FPS while idle and moving across several autosaves at one headless profile')
parser.add_argument('--performance-world',choices=('medium','huge'),default='medium')
parser.add_argument('--housing-only',action='store_true',help='Two-cell door collision, interaction, save/restart and indoor survival at one display profile')
parser.add_argument('--inventory-only',action='store_true',help='Long-press drag, swaps/merges, direct backpack use, persistence and background jump/depth captures at one display profile')
parser.add_argument('--exploration-only',action='store_true',help='Single-tap backpack holding, precise furniture taps with tools, deliberate hold-to-mine and higher jump at one display profile')
parser.add_argument('--interactions-only',action='store_true',help='Focused furniture taps with picks/axes, hold-to-mine, cancellation and save/restart')
parser.add_argument('--polish-only',action='store_true',help='Protected sorting, bulk storage, ready-recipe filtering, crafting and save/restart at one display profile')
parser.add_argument('--multitouch-only',action='store_true',help='Real runtime multi-ID joystick, jump, use, pause cancellation, release order and full-bleed controls')
parser.add_argument('--weapons-only',action='store_true',help='Fast focused regression for mining feedback and held weapons')
parser.add_argument('--terrain-only',action='store_true',help='Focused regression for terrain seams and forest/snow/jungle trees')
parser.add_argument('--appearance-only',action='store_true',help='Focused natural tree and character wardrobe captures at one display profile')
parser.add_argument('--fixtures',nargs='+',help='Limit a focused run to named save fixtures')
parser.add_argument('--motion-only',action='store_true',help='Record walking and planted tool swings; verify idle preview remains still')
parser.add_argument('--aim-jump-only',action='store_true',help='Focused directional bow/grip/recoil captures, airborne poses, ammunition and restart at one profile')
parser.add_argument('--smart-cursor-only',action='store_true',help='Gamepad automatic mining, precise touch override, mining through foreground, fixed/smart menu persistence, language and bridge placement')
parser.add_argument('--bestiary-only',action='store_true',help='All seven guide pages, nine animated creatures, touch/controller wraparound and both languages')
parser.add_argument('--worlds-only',action='store_true',help='World catalogue, legacy saves, seed keypad, all sizes, switching and deletion at one display profile')
parser.add_argument('--legacy-fixture-tool',type=Path,help='Optional original v0.8 fixture binary for backwards compatibility QA')
parser.add_argument('--input-hints-only',action='store_true',help='Check touch/controller hint transitions in both languages at one display profile')
parser.add_argument('--building-only',action='store_true',help='Wall replacement/refunds behind furniture, placement ghosts and recipe category touch/controller navigation at one profile')
parser.add_argument('--walls-only',action='store_true',help='Craft/place three backing materials, tap furniture with a hammer, demolish/recover backing and restart at one profile')
parser.add_argument('--adaptive-controls-only',action='store_true',help='Hide floating controls for buttons, preserve the mode through pause/release, and restore on touch')
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
    elif layout=='footer':x+=ox;y+=ch-224-26
    elif layout=='header':x+=cw-288
    elif layout=='tab':x=5+(x-5)*(cw-8)/280
    elif layout=='world':x+=ox;y+=ch-224-13
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

def read_save(path):
    data=path.read_bytes()
    if data[:4]!=b'SSZ1':return data
    assert len(data)>=8
    size=struct.unpack_from('<I',data,4)[0]
    assert 128<=size<=526072
    output=bytearray();at=8
    while at<len(data):
        token=data[at];at+=1
        count=(token&127)+3 if token&128 else token+1
        assert len(output)+count<=size
        if token&128:
            assert at<len(data);output.extend(bytes([data[at]])*count);at+=1
        else:
            assert at+count<=len(data);output.extend(data[at:at+count]);at+=count
    assert len(output)==size
    return bytes(output)

def saved():
    files=list((state/'private-files').glob('*/starsoil-world-v1.bin'))
    assert len(files)==1,files
    data=read_save(files[0])
    assert data[:4]==b'SSR1'
    w,h=struct.unpack_from('<HH',data,6)
    assert len(data)==760+w+w*h*2,len(data)
    return files[0],data

def enter_world():
    files=list((state/'private-files').glob('*/starsoil-world*.bin'))
    if files:
        tap(202,208)
    else:
        tap(144,208);tap(202,194)
    time.sleep(.7)

try:
    if args.building_only:
        assert args.fixture_tool
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'walls',str(path)],check=True)
        initial=read_save(path);wood,brick,glass=initial[64],initial[67],initial[70]
        def total(data,item):return sum(struct.unpack_from('<H',data,65+i*3)[0]for i in range(40)if data[64+i*3]==item)
        def wall_at(data,x,y):return data[612+384*112+y*384+x]
        def pause_data():
            key('ENTER');time.sleep(.6);return saved()[1]
        def point(x,y,hold=.08):
            px,py=display_point(x,y,'fixed');command(f'TOUCH DOWN 0 {px} {py}');time.sleep(hold)
            command(f'TOUCH UP 0 {px} {py}');time.sleep(.25)
        start(270);enter_world();time.sleep(1)
        point(192,177);capture('wall-tap-opens-storage');tap(236,22,'header')
        point(192,177,.85);capture('wall-behind-chest')
        data=pause_data();assert wall_at(data,103,59)==9 and total(data,wood)==11
        key('ENTER');tap(79,45,'fixed');point(192,177,.85);capture('glass-replaced-behind-chest')
        data=pause_data();assert wall_at(data,103,59)==40 and total(data,glass)==11 and total(data,wood)==12
        key('ENTER');point(192,177,.85)
        data=pause_data();assert total(data,glass)==11 and total(data,wood)==12,'Duplicate replacement spent materials'
        key('ENTER');tap(175,45,'fixed');time.sleep(.3);capture('door-two-cell-preview')
        tap(230,22,'header')
        for group,(x,name) in enumerate([(31,'all'),(87,'building'),(143,'tools'),(199,'gear'),(255,'supplies')]):
            tap(x,180,'footer');capture('recipes-'+name)
        # Tools category starts at its first recipe: UP focuses the category row.
        tap(143,180,'footer');key('UP');key('RIGHT');capture('controller-gear-category')
        key('DOWN');tap(178,21,'fixed');capture('ready-gear-empty')
        key('UP');key('RIGHT');key('DOWN');capture('empty-category-navigation')
        tap(31,180,'footer');tap(178,21,'fixed');capture('all-recipes-restored')
        data=pause_data();assert wall_at(data,103,59)==40 and total(data,glass)==11 and total(data,wood)==12
        assert data[612+59*384+103]==37,'Replacement removed foreground furniture'
        before=path.read_bytes();stop();start(271);assert path.read_bytes()==before
        enter_world();time.sleep(.5);capture('building-restored');data=pause_data()
        assert wall_at(data,103,59)==40 and data[612+59*384+103]==37
        logs=''.join(p.read_text(errors='replace')for p in out.glob('simulator-*.log'))
        assert 'save failed'not in logs.lower() and 'Guest exception'not in logs
        print('PASS real PXA: furniture tap/hold, wall replacement and exact refunds, duplicates, door preview, recipe categories touch/controller/empty-ready navigation, save/restart; one412x412 profile')
        raise SystemExit(0)
    if args.walls_only:
        assert args.fixture_tool
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'walls',str(path)],check=True)
        def total(data,item):return sum(struct.unpack_from('<H',data,65+i*3)[0] for i in range(40) if data[64+i*3]==item)
        initial=read_save(path);wood,brick,glass=initial[64],initial[67],initial[70]
        def tile_at(data,x,y):return data[612+y*384+x]
        def wall_at(data,x,y):return data[612+384*112+y*384+x]
        def pause_data():
            key('ENTER');time.sleep(.6);return saved()[1]
        def point(x,y,hold=.08):
            px,py=display_point(x,y,'fixed')
            command(f'TOUCH DOWN 0 {px} {py}');time.sleep(hold)
            command(f'TOUCH UP 0 {px} {py}');time.sleep(.25)
        start(260);enter_world();time.sleep(1)
        tap(230,22,'header');tap(178,21,'fixed');capture('wall-recipes')
        tap(144,209,'footer');data=pause_data()
        assert total(data,wood)==16 and total(data,9)==0, 'Wall recipe did not exchange one plank for four walls'
        key('ENTER');tap(236,22,'header');point(192,161)
        tap(55,45,'fixed');point(176,161)
        tap(79,45,'fixed');point(208,161);capture('three-wall-materials')
        data=pause_data();assert [wall_at(data,x,58)for x in (102,103,104)]==[10,9,40]
        assert total(data,wood)==15 and total(data,brick)==11 and total(data,glass)==11
        key('ENTER');tap(103,45,'fixed');point(192,161,.08)
        data=pause_data();assert wall_at(data,103,58)==9, 'Short hammer tap destroyed backing'
        key('ENTER');point(192,161,.9);capture('wood-hammer-swing')
        data=pause_data();assert wall_at(data,103,58)==0 and total(data,wood)==16
        key('ENTER');tap(127,45,'fixed');point(176,161,.8)
        data=pause_data();assert wall_at(data,102,58)==0 and total(data,brick)==12
        key('ENTER');tap(151,45,'fixed');point(208,161,.8)
        data=pause_data();assert wall_at(data,104,58)==40, 'Pickaxe removed a backing wall'
        key('ENTER');tap(127,45,'fixed');point(208,161,.8)
        data=pause_data();assert wall_at(data,104,58)==0 and total(data,glass)==12
        key('ENTER');point(128,177,.08)
        data=pause_data();assert tile_at(data,99,59)==39 and tile_at(data,99,58)==40, 'Hammer short-tap did not open both door halves'
        key('ENTER');point(128,177,.08)
        data=pause_data();assert tile_at(data,99,59)==23 and tile_at(data,99,58)==38
        assert tile_at(data,103,59)==37, 'Hammer removed foreground furniture'
        key('ENTER');tap(31,45,'fixed');point(192,161);capture('wall-rebuilt')
        data=pause_data();assert wall_at(data,103,58)==9;before=path.read_bytes();stop()
        start(261);assert path.read_bytes()==before;enter_world();time.sleep(.5);capture('walls-restored')
        data=pause_data();assert wall_at(data,103,58)==9 and tile_at(data,103,59)==37
        logs=''.join(p.read_text(errors='replace')for p in out.glob('simulator-*.log'))
        assert 'save failed'not in logs.lower() and 'Guest exception'not in logs
        print('PASS real PXA: wall recipe, three backing materials, independent hammer/pick targets, short-tap door operation, exact recovery and save/restart; one412x412 profile')
        raise SystemExit(0)
    if args.adaptive_controls_only:
        start(0);enter_world();time.sleep(.8)
        tap(100,9,'fixed');capture('touch-controls')
        def last_mode():
            lines=(out/'simulator-0.log').read_text().splitlines()
            modes=[line for line in lines if 'Pocket: input ' in line]
            return ('controller' if 'Pocket: input controller' in modes[-1] else 'touch') if modes else 'touch'
        key('RIGHT',.06);capture('buttons-released')
        assert last_mode()=='controller'
        time.sleep(.3);assert last_mode()=='controller'
        key('ENTER');capture('buttons-paused');assert last_mode()=='controller'
        time.sleep(.6)
        path,data=saved()
        key('ENTER');capture('buttons-resumed');assert last_mode()=='controller'
        tap(100,9,'fixed');capture('touch-restored');assert last_mode()=='touch'
        offset=len((out/'simulator-0.log').read_text())
        command('KEY_DOWN RIGHT');time.sleep(10)
        assert 'Starsoil: world committed'not in (out/'simulator-0.log').read_text()[offset:], 'Autosave interrupted continuous input'
        command('KEY_UP RIGHT');time.sleep(1.5)
        assert 'Starsoil: world committed'in (out/'simulator-0.log').read_text()[offset:], 'Idle autosave did not flush after release'
        key('ENTER');time.sleep(.8);before=path.read_bytes();stop()
        start(1);assert path.read_bytes()==before
        enter_world();time.sleep(.4);capture('reloaded-touch-controls');stop()
        logs=''.join(p.read_text(errors='replace')for p in out.glob('simulator-*.log'))
        assert 'save failed'not in logs and 'Guest exception'not in logs
        print('PASS real runtime button/touch mode, release and pause persistence, touch restoration and save/restart')
        raise SystemExit(0)
    if args.performance_only:
        import re
        start(0);enter_world();key('ENTER');time.sleep(.8)
        path,data=saved();stop()
        assert args.fixture_tool
        subprocess.run([str(args.fixture_tool.resolve()),'performance-'+args.performance_world,str(path)],check=True)
        start(1);enter_world();time.sleep(1.5)
        capture('performance-world')
        results=[]
        for mode,seconds in (('idle',22),('moving',34)):
            offset=len((out/'simulator-1.log').read_text(errors='replace'))
            for elapsed in range(0,seconds,2):
                direction='RIGHT' if (elapsed//4)%2==0 else 'LEFT'
                if mode=='moving':command('KEY_DOWN '+direction)
                time.sleep(2)
                if mode=='moving':command('KEY_UP '+direction)
            text=(out/'simulator-1.log').read_text(errors='replace')[offset:]
            samples=[]
            for line in text.splitlines():
                if 'Pocket: perf ' not in line:continue
                fields={k:int(v) for k,v in re.findall(r'(frames|period_ms|mean_draw_us|max_gap_us|save_step_us|snapshot_us|save_blocks)=(\d+)',line)}
                if fields:samples.append(fields)
            assert samples,'Missing actual runtime performance counters'
            frames=sum(s['frames'] for s in samples);period=sum(s['period_ms'] for s in samples)
            results.append({'mode':mode,'submitted_fps':round(frames*1000/period,3),'samples':samples,'world_commits':text.count('Starsoil: world committed')})
        key('ENTER');time.sleep(1.2)
        data=read_save(path);w,h=struct.unpack_from('<HH',data,6)
        assert len(data)==760+w+w*h*2
        checksum=2166136261
        for value in data[:-4]:checksum=((checksum^value)*16777619)&0xffffffff
        assert struct.unpack_from('<I',data,len(data)-4)[0]==checksum
        capture('performance-paused');stop()
        before=path.read_bytes()
        start(2);time.sleep(.5);assert path.read_bytes()==before
        enter_world();time.sleep(1);capture('performance-reloaded');stop()
        report={'profile':{'width':args.width,'height':args.height,'headless':not args.headed},'measurement':'real product simulator guest submitted frames; not physical LCD FPS','world':{'width':w,'height':h,'save_bytes':len(data)},'segments':results,'checksum_valid':True,'restart_preserved':True}
        (out/'performance.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2),flush=True)
        raise SystemExit(0)
    if args.multitouch_only:
        assert args.fixture_tool
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'exploration',str(path)],check=True)
        fixture=read_save(path)
        initial_boards=struct.unpack_from('<H',fixture,64+3*3+1)[0]
        initial_planks=fixture[612:612+384*112*2].count(9)
        def finger(phase,number,x,y):
            px,py=display_point(x,y,'fixed');command(f'TOUCH {phase} {number} {px} {py}')
        def pause_data():
            key('ENTER');time.sleep(.6);return saved()[1]
        def position(data):return tuple(v/16 for v in struct.unpack_from('<II',data,46))
        start(220);enter_world();time.sleep(1)
        tap(103,45,'fixed') # Boards in hotbar slot 3.
        time.sleep(4.5);capture('floating-controls')
        finger('DOWN',1,62,223);finger('MOVE',1,87,223)
        finger('DOWN',3,245,223);finger('DOWN',2,191,237)
        time.sleep(.22);capture('three-fingers')
        data=pause_data();x,y=position(data)
        assert x>1618 and y<610,(x,y)
        consumed=initial_boards-struct.unpack_from('<H',data,64+3*3+1)[0]
        assert 0<consumed<=initial_boards, 'USE contact was ignored while moving/jumping'
        # A held USE may legally place more than once across its 180ms
        # recovery. Verify conservation rather than screenshot timing.
        assert data[612:612+384*112*2].count(9)-initial_planks==consumed, 'Concurrent placement lost or duplicated boards'
        # Resume with old fingers still down: no stale movement may revive.
        key('ENTER');finger('MOVE',1,95,223);time.sleep(.25)
        data=pause_data();assert abs(position(data)[0]-x)<1
        key('ENTER')
        for number in (3,2,1):finger('UP',number,20,30)
        time.sleep(.8)
        # Reuse IDs and release jump/use first. The left finger keeps moving.
        before=pause_data();key('ENTER')
        finger('DOWN',1,62,223);finger('MOVE',1,87,223)
        finger('DOWN',2,191,237);finger('DOWN',3,245,223)
        time.sleep(.12);finger('UP',2,191,237);finger('UP',3,245,223)
        time.sleep(.35);finger('UP',1,87,223)
        data=pause_data();assert position(data)[0]>position(before)[0]+25
        key('ENTER');time.sleep(.3);stable=pause_data()
        assert abs(position(stable)[0]-position(data)[0])<1,'Movement stuck after final release'
        key('ENTER');time.sleep(1.2);ground=pause_data();key('ENTER')
        # Two-contact devices can jump via the stick while holding USE.
        x=position(ground)[0];finger('DOWN',1,62,223);finger('MOVE',1,84,198)
        finger('DOWN',3,245,223);time.sleep(.2);capture('two-finger-jump-use')
        data=pause_data();assert position(data)[0]>x+5 and position(data)[1]<position(ground)[1]-25
        key('ENTER');finger('UP',3,20,30);finger('UP',1,20,30)
        tap(195,22,'header');capture('inventory-after-multitouch');tap(236,22,'header')
        data=pause_data();stop();checkpoint=path.read_bytes();start(221)
        assert path.read_bytes()==checkpoint;enter_world();time.sleep(4.6);capture('world-restored')
        print('PASS real runtime multi-ID: move+jump+use, independent releases, pause cancellation, ID reuse, two-contact jump gesture, full-bleed UI and restart',flush=True)
        raise SystemExit(0)
    if args.polish_only:
        assert args.fixture_tool, '--polish-only requires --fixture-tool'
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'polish',str(path)],check=True)
        def slots(data,start=64,n=40):return [struct.unpack_from('<BH',data,start+i*3)for i in range(n)]
        def total(data,item,start=64,n=40):return sum(q for i,q in slots(data,start,n)if i==item)
        def pause_data():
            key('ENTER');time.sleep(.6);return saved()[1]
        initial=read_save(path);hotbar=slots(initial)[:8];hand=slots(initial)[23]
        start(210);enter_world();time.sleep(1)
        tap(195,22,'header');tap(220,209,'footer');capture('sorted-backpack')
        data=pause_data()
        assert slots(data)[:8]==hotbar and slots(data)[23]==hand and data[62]==23
        assert total(data,7)==1017 and sorted(q for n,(i,q) in enumerate(slots(data)) if i==7 and n!=23)==[11,999], 'Bag sort failed to consolidate stacks'
        key('ENTER');tap(236,22,'header');key('TAB');tap(280,40,'fixed');time.sleep(4.5);capture('shared-storage')
        tap(45,209,'footer');capture('quick-stacked')
        data=pause_data()
        assert total(data,7)==7 and total(data,7,184,12)==1990
        assert slots(data)[:8]==hotbar and slots(data)[23]==hand, 'Quick stack moved protected items'
        key('ENTER');tap(240,209,'footer');capture('sorted-chest')
        data=pause_data();assert total(data,3,184,12)==30 and slots(data,184,12)[0]==(3,30)
        key('ENTER');tap(145,209,'footer')
        data=pause_data();assert total(data,7)==1997 and total(data,3)==30 and not any(i for i,q in slots(data,184,12))
        before_crafted=struct.unpack_from('<I',data,38)[0];before_boards=total(data,9)
        key('ENTER');tap(236,22,'header');tap(230,22,'header');capture('all-recipes')
        tap(178,21,'fixed');capture('ready-recipes')
        tap(213,209,'footer');capture('crafted-ready-recipe')
        data=pause_data()
        assert total(data,7)==1992 and total(data,9)==before_boards+20
        assert struct.unpack_from('<I',data,38)[0]==before_crafted+5
        stop();checkpoint=path.read_bytes();start(211)
        assert path.read_bytes()==checkpoint,'Title altered the existing save'
        enter_world();tap(195,22,'header');time.sleep(4.6);capture('backpack-restored');data=pause_data()
        assert slots(data)==slots(checkpoint) and data[62]==23
        assert not any('save failed' in p.read_text(errors='replace').lower()for p in out.glob('simulator-*.log'))
        print('PASS real simulator: protected bag sort/quick stack, partial-capable storage, take all, filtered batch crafting and save/restart',flush=True)
        raise SystemExit(0)
    if args.exploration_only or args.interactions_only:
        assert args.fixture_tool, 'Interaction checks require --fixture-tool'
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'exploration',str(path)],check=True)
        start(200);enter_world();time.sleep(1.2)
        # Legacy saves reconstruct NPCs at their original fixed town positions;
        # the exact NPC path is covered by native pointer tests.
        def pause_data():
            key('ENTER');time.sleep(.6);return saved()[1]
        def tile_at(data,x,y):return data[612+y*384+x]
        def tap_world(x,y,hold=.08):
            px,py=display_point(x,y,'fixed')
            command(f'TOUCH DOWN 0 {px} {py}');time.sleep(hold)
            command(f'TOUCH UP 0 {px} {py}');time.sleep(.2)
        tap(195,22,'header');tap(167,83);tap(236,22,'header')
        data=pause_data()
        assert data[62]==14 and struct.unpack_from('<I',data,50)[0]/16==640, 'An empty bag slot was ignored or triggered jump'
        key('ENTER');tap(195,22,'header');tap(117,107);capture('single-tap-held-in-bag')
        tap(236,22,'header');capture('single-tap-held-in-world')
        data=pause_data();assert data[62]==20, 'A single backpack tap did not select the held item'
        key('ENTER')
        tap_world(192,177,.7);capture('direct-open-door')
        data=pause_data();assert tile_at(data,103,38)==40 and tile_at(data,103,39)==39
        assert struct.unpack_from('<I',data,34)[0]==0, 'Door interaction mined or placed instead'
        key('ENTER');tap_world(192,161,.7)
        data=pause_data();assert tile_at(data,103,38)==38 and tile_at(data,103,39)==23
        key('ENTER');tap(31,45,'fixed') # Pick: short taps still interact.
        tap_world(160,177,.1)
        data=pause_data();assert tile_at(data,101,39)==0
        info=json.loads(subprocess.check_output([str(args.fixture_tool.resolve()),'inspect',str(path)],text=True))
        assert info['treasure_chests']==1, 'Chest tap did not loot exactly once'
        key('ENTER');tap_world(128,177)
        data=pause_data();assert struct.unpack_from('<H',data,54)[0]==99, 'Bed tap did not set home'
        key('ENTER');tap_world(208,177);capture('direct-storage');tap(236,22,'header')
        # The left half of this bench is covered by the floating stick's
        # activation region; tap its exposed right edge in the world.
        tap_world(118,177);capture('direct-workbench');tap(236,22,'header')
        tap_world(192,177,.1);capture('pick-tap-open-door')
        data=pause_data();assert tile_at(data,103,39)==39 and struct.unpack_from('<I',data,34)[0]==0
        key('ENTER');tap_world(192,161,.1)
        data=pause_data();assert tile_at(data,103,39)==23 and struct.unpack_from('<I',data,34)[0]==0
        # Pausing a pending tool tap cancels it, even when the finger is
        # released after returning to the world.
        key('ENTER');px,py=display_point(192,177,'fixed')
        command(f'TOUCH DOWN 0 {px} {py}');time.sleep(.08)
        data=pause_data();assert tile_at(data,103,39)==23
        key('ENTER');command(f'TOUCH UP 0 {px} {py}');time.sleep(.15)
        data=pause_data();assert tile_at(data,103,39)==23 and struct.unpack_from('<I',data,34)[0]==0
        key('ENTER');tap(55,45,'fixed') # Axe has the same short-tap behavior.
        tap_world(192,177,.1)
        data=pause_data();assert tile_at(data,103,39)==39 and struct.unpack_from('<I',data,34)[0]==0
        key('ENTER');tap_world(192,161,.1);tap_world(208,177,.1)
        capture('axe-tap-storage');tap(236,22,'header')
        # Allow both the hold threshold and the door's mining work to finish,
        # including on the slower portable-WASM interpreter. Native pointer
        # tests check the precise 299/300 ms boundary separately.
        tap(31,45,'fixed');tap_world(192,177,8.0 if
            (args.package/'artifacts/main.wasm').exists() else 2.0)
        capture('pick-hold-dismantled-door')
        data=pause_data();assert tile_at(data,103,38)==0 and tile_at(data,103,39)==0
        assert struct.unpack_from('<I',data,34)[0]==1, 'Pick hold did not dismantle the door exactly once'
        if args.exploration_only:
            key('ENTER');command('KEY_DOWN Z');time.sleep(.42);capture('higher-jump')
            key('ENTER',.05);command('KEY_UP Z');time.sleep(.6);_,data=saved()
            jump_height=40*16-struct.unpack_from('<I',data,50)[0]/16
            assert 70<jump_height<110, ('Jump did not rise above four tiles',jump_height)
        stop();start(201);capture('exploration-restart-title');assert read_save(path)==data
        enter_world();time.sleep(.6);capture('exploration-reloaded')
        print('PASS real headless PXA backpack holding, pick/axe door taps, pick loot/bed/storage/workbench, pending-tap pause cancellation, deliberate pick hold dismantling and exact title preservation; one display profile')
        raise SystemExit(0)
    if args.inventory_only:
        assert args.fixture_tool, '--inventory-only requires --fixture-tool'
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'inventory',str(path)],check=True)
        original=read_save(path)
        def bag(data,index):return struct.unpack_from('<BH',data,64+index*3)
        def bag_point(index):return display_point(17+index%8*25,59+index//8*24)
        def drag_slot(source,target,name=None):
            x,y=bag_point(source);xx,yy=bag_point(target)
            command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.4)
            command(f'TOUCH MOVE 0 {xx} {yy}');time.sleep(.12)
            if name:capture(name)
            command(f'TOUCH UP 0 {xx} {yy}');time.sleep(.2)
        start(100);enter_world();time.sleep(1.2);tap(195,22,'header');capture('bag-before')
        drag_slot(0,20,'bag-drag-preview')
        drag_slot(1,2)
        drag_slot(9,8)
        # Release on the preview area: cancellation must not equip the helmet.
        x,y=bag_point(10);xx,yy=display_point(260,112)
        command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.4)
        command(f'TOUCH MOVE 0 {xx} {yy}');command(f'TOUCH UP 0 {xx} {yy}');time.sleep(.2)
        capture('bag-reordered')
        # A first tap only selects. Fast second tap equips, independent of selection.
        tap(67,83);tap(67,83)
        tap(117,107);tap(236,22,'header');time.sleep(.4);capture('held-from-backpack')
        x,y=display_point(176,177,'fixed')
        command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.9);command(f'TOUCH UP 0 {x} {y}')
        key('ENTER');time.sleep(.7);_,data=saved()
        assert bag(data,20)==bag(original,0) and data[62]==20, 'Direct backpack use moved/replaced the source stack'
        assert bag(data,1)==bag(original,2) and bag(data,2)==bag(original,1), 'Drag swap failed'
        assert bag(data,8)[1]==999 and bag(data,9)[1]==11, 'Drag merge lost overflow'
        assert data[220]==bag(original,10)[0], 'Double-tap equipment failed'
        assert struct.unpack_from('<I',data,34)[0]>0, 'Tool selected outside hotbar could not mine'
        stop();start(101);capture('backpack-restart-title');assert read_save(path)==data
        enter_world();time.sleep(1.2);capture('backpack-selection-restored');stop()
        for number,fixture in enumerate(('background-surface','background-shallow','background-cave','background-crystal'),110):
            subprocess.run([str(args.fixture_tool.resolve()),fixture,str(path)],check=True)
            start(number);enter_world();time.sleep(1.2);capture(fixture)
            if fixture=='background-surface':
                command('KEY_DOWN Z');time.sleep(.08);command('KEY_UP Z')
                for frame in range(6):capture(f'background-jump-{frame}');time.sleep(.08)
            stop()
        print('PASS real headless PXA: long-press drag preview, move/swap/overflow merge, cancel, double-tap equip, mine from backpack slot 20, exact title preservation and selection restart; jump/depth background captures at one profile')
        raise SystemExit(0)
    if args.worlds_only:
        assert args.fixture_tool, '--worlds-only requires --fixture-tool'
        def metadata(path):
            data=read_save(path)
            assert data[:4]==b'SSR1'
            w,h=struct.unpack_from('<HH',data,6)
            assert (w,h)in((256,96),(384,112),(512,144),(1024,256))
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
            tap(72,208);tap(56+size*60,101);tap(170,148)
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
        create(3,'42');go_home();time.sleep(1)
        huge=root/'starsoil-world-5.bin'
        huge_info=metadata(huge)
        assert (huge_info['width'],huge_info['height'],huge_info['seed'])==(1024,256,42)
        capture('huge-world-catalogue')
        capture('finished-catalogue');stop()
        logs=''.join(p.read_text(errors='replace')for p in out.glob('simulator-*.log'))
        assert 'Guest exception'not in logs and 'save failed'not in logs.lower()
        print(f'PASS real PXA world menu: legacy v1 preserved and loaded; numeric keypad and overflow rejection; actual small/medium/large saves; identical seed terrain; four-world pagination; switch/restart; cancel and confirm active-world deletion; no resurrection; corrupt save isolation and explicit deletion; one {args.width}x{args.height} profile')
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
        tap(195,22,'header')
        probe('bag-cn',['UP','DOWN'],((6,174),(282,219)))
        tap(85,35,'tab')
        probe('craft-cn',['UP','DOWN'],((111,199),(234,222)))
        tap(145,35,'tab')
        probe('gear-cn',['RIGHT'],((7,188),(281,222)))
        verify_native_font('gear-cn-touch','旅人的衣橱',8,12)
        tap(258,35,'tab')
        probe('guide-cn',['UP'],((8,49),(278,194)),'content')
        key('X');tap(260,22,'header')
        probe('pause-cn',['UP','DOWN'],((68,198),(222,211)),'content',(144,43,'content'))
        tap(144,176) # Language row in the real pause menu.
        probe('pause-en',['UP','DOWN'],((68,198),(222,211)),'content',(144,43,'content'))
        tap(144,85) # Continue, then open the field guide using touch.
        tap(195,22,'header');tap(258,35,'tab')
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
    if args.bestiary_only:
        from PIL import Image,ImageChops
        start(0);enter_world()
        tap(195,22,'header');tap(257,35,'tab')
        key('RIGHT');key('LEFT');capture('guide-first')
        for page in range(1,7):
            key('RIGHT');capture(f'guide-cn-{page}')
        key('RIGHT');capture('guide-wrapped')
        assert not ImageChops.difference(Image.open(out/'guide-first.png'),Image.open(out/'guide-wrapped.png')).getbbox()
        key('LEFT');capture('bestiary-last-controller')
        tap(36,210,'footer');capture('bestiary-previous-touch')
        tap(250,210,'footer');capture('bestiary-last-touch')
        key('ENTER')
        for _ in range(4):key('DOWN')
        key('Z');key('ENTER');time.sleep(.5)
        capture('guide-en-6')
        for page in (5,4):
            key('LEFT');capture(f'guide-en-{page}')
        key('X');key('ENTER');time.sleep(.6);saved()
        logs=''.join(p.read_text(errors='replace') for p in out.glob('simulator-*.log'))
        assert 'save failed' not in logs.lower() and 'Guest exception' not in logs
        print('PASS real PXA bestiary: seven guide pages, nine animated creatures, controller wraparound, touch paging, both languages and save')
        raise SystemExit(0)
    if args.smart_cursor_only:
        assert args.fixture_tool
        def tile_at(data,x,y):return data[612+y*384+x]
        def mined(data):return struct.unpack_from('<I',data,34)[0]
        def pause_data():
            key('ENTER');time.sleep(.6);return saved()[1]
        def toggle_smart():
            key('DOWN');key('DOWN');key('Z')
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'smart-cursor',str(path)],check=True)
        start(240);enter_world();time.sleep(4.6);capture('smart-preview')
        # A precise touch can still remove a chosen floor tile while smart
        # mode protects it during unpointed controller mining.
        px,py=display_point(208,193,'fixed')
        command(f'TOUCH DOWN 0 {px} {py}');time.sleep(1.1)
        command(f'TOUCH UP 0 {px} {py}');time.sleep(.15)
        data=pause_data();assert mined(data)==1 and tile_at(data,104,60)==0 and tile_at(data,101,57)==3
        key('ENTER');command('KEY_DOWN X');time.sleep(1.1);capture('controller-chain')
        command('KEY_UP X');data=pause_data()
        assert mined(data)==4 and all(tile_at(data,x,57)==0 for x in (101,102,103))
        assert tile_at(data,100,60)==3 and tile_at(data,101,60)==3
        toggle_smart();time.sleep(.6);capture('smart-disabled-menu');stop()
        subprocess.run([str(args.fixture_tool.resolve()),'smart-cursor',str(path)],check=True)
        checkpoint=path.read_bytes();start(241);assert path.read_bytes()==checkpoint
        enter_world();time.sleep(.5);command('KEY_DOWN X');time.sleep(1.1);command('KEY_UP X')
        data=pause_data();assert mined(data)==0 and tile_at(data,101,57)==3,'Fixed mode did not persist'
        capture('fixed-mode-restored');toggle_smart()
        key('DOWN');key('DOWN');key('Z') # Language is the fifth menu row.
        time.sleep(.6);capture('smart-english-menu');stop()
        checkpoint=path.read_bytes();start(242);assert path.read_bytes()==checkpoint
        enter_world();command('KEY_DOWN X');time.sleep(1.1);command('KEY_UP X');data=pause_data()
        assert mined(data)==3,'Smart mode did not persist after restart'
        capture('smart-mode-restored');stop()
        subprocess.run([str(args.fixture_tool.resolve()),'smart-bridge',str(path)],check=True)
        start(243);enter_world();time.sleep(4.6);capture('bridge-preview')
        command('KEY_DOWN X');time.sleep(2);command('KEY_UP X');capture('bridge-built')
        data=pause_data();assert all(tile_at(data,x,60)==9 for x in range(101,106))
        assert all(tile_at(data,x,59)==0 for x in range(99,107))
        assert struct.unpack_from('<H',data,65)[0]==7,'Bridge placement wasted materials'
        stop();checkpoint=path.read_bytes();start(244);assert path.read_bytes()==checkpoint
        enter_world();capture('bridge-restored');data=pause_data()
        assert all(tile_at(data,x,60)==9 for x in range(101,106)) and struct.unpack_from('<H',data,65)[0]==7
        stop()
        for number,mode in enumerate(('touch','controller'),245):
            subprocess.run([str(args.fixture_tool.resolve()),'through-wall',str(path)],check=True)
            initial=read_save(path)
            start(number);enter_world();time.sleep(4.6)
            if mode=='touch':
                px,py=display_point(176,177,'fixed')
                command(f'TOUCH DOWN 0 {px} {py}');time.sleep(1.3)
                command(f'TOUCH UP 0 {px} {py}')
            else:
                command('KEY_DOWN X');time.sleep(1.3);command('KEY_UP X')
            capture('through-wall-'+mode)
            data=pause_data()
            assert mined(data)==1 and tile_at(data,102,59)==0,mode+' cannot mine through foreground'
            assert all(tile_at(data,101,y)==tile_at(initial,101,y) for y in range(54,60)), 'Weak pick damaged crystal barrier'
            assert data[612+384*112+59*384+102]==9, 'Pick removed background wall'
            stop()
        logs=''.join(p.read_text(errors='replace') for p in out.glob('simulator-*.log'))
        assert 'smart cursor off' in logs and 'smart cursor on' in logs
        assert 'save failed' not in logs.lower() and 'Guest exception' not in logs
        print('PASS real PXA gamepad: elevated targets and continuous mining; protected standing floor; exact touch; fixed/smart and language preferences survive restart; five-cell bridge with exact material use; touch and controller mining through a solid barrier with background retained; save/restart; one display profile')
        raise SystemExit(0)
    if args.aim_jump_only:
        assert args.fixture_tool
        from PIL import Image,ImageChops
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'aim-jump',str(path)],check=True)
        start(230);enter_world();time.sleep(4.6);capture('bow-rest')
        angles=[('right',202,169),('up-right',192,112),('up',144,100),
                ('up-left',118,112),('left',119,169),('down',144,256)]
        for i,(name,x,y) in enumerate(angles):
            px,py=display_point(x,y,'fixed')
            command(f'TOUCH {"DOWN" if i==0 else "MOVE"} 0 {px} {py}')
            time.sleep(.45);capture('aim-'+name)
        command(f'TOUCH UP 0 {px} {py}')
        region=(*display_point(120,136,'fixed'),*display_point(169,180,'fixed'))
        images=[Image.open(out/('aim-'+name+'.png')).convert('RGB') for name,_,_ in angles]
        assert len({im.crop(region).tobytes() for im in images})>=5,'Bow pose did not follow directional aiming'
        key('ENTER');time.sleep(.6);_,data=saved()
        before_arrows=struct.unpack_from('<H',data,64+2*3+1)[0]
        assert 980<before_arrows<999,('Arrows did not fire/consume',before_arrows)
        key('ENTER');tap(199,45,'fixed');time.sleep(.3);capture('before-jump')
        jx,jy=display_point(191,237,'fixed')
        command(f'TOUCH DOWN 1 {jx} {jy}');jump=[]
        for i in range(18):
            time.sleep(.065);capture(f'jump-{i:02d}');jump.append(Image.open(out/f'jump-{i:02d}.png').convert('RGB'))
            if i==6:command(f'TOUCH UP 1 {jx} {jy}')
        time.sleep(.8)
        key('ENTER');time.sleep(.6);_,data=saved();assert abs(struct.unpack_from('<I',data,50)[0]/16-960)<1
        stop();checkpoint=path.read_bytes();start(231);assert path.read_bytes()==checkpoint
        enter_world();capture('aim-jump-restored');key('ENTER');time.sleep(.6);_,data=saved()
        assert struct.unpack_from('<H',data,64+2*3+1)[0]==before_arrows
        images[0].save(out/'bow-aim.gif',save_all=True,append_images=images[1:],duration=500,loop=0)
        jump[0].save(out/'jump.gif',save_all=True,append_images=jump[1:],duration=90,loop=0)
        logs=''.join(p.read_text(errors='replace') for p in out.glob('simulator-*.log'))
        assert 'Guest exception' not in logs and 'save failed' not in logs.lower()
        print('PASS real PXA: bow aims in six directions with visible pose changes and ammunition use; airborne animation, landing, equipment and save/restart; one display profile')
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
        tap(195,22,'header');tap(145,35,'tab');capture('idle-preview-a');time.sleep(.9);capture('idle-preview-b')
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
                tap(195,22,'header');tap(145,35,'tab');capture(fixture+'-portrait-right')
                tap(62,115);capture(fixture+'-portrait-left')
            stop()
        print('PASS headless focused appearance/terrain regression; one display profile; no black borders')
        raise SystemExit(0)
    if args.housing_only:
        assert args.fixture_tool, '--housing-only requires --fixture-tool'
        start(0);enter_world();key('ENTER');time.sleep(.6);path,_=saved();stop()
        subprocess.run([str(args.fixture_tool.resolve()),'doors',str(path)],check=True)
        start(90);enter_world();time.sleep(1.2);capture('door-closed')
        key('RIGHT',.9);key('ENTER');time.sleep(.6)
        _,data=saved();x=struct.unpack_from('<I',data,46)[0]/16
        assert x+5<=53*16+.1, 'Closed door did not stop player'
        key('ENTER');key('TAB');time.sleep(.3);capture('door-open')
        key('RIGHT',.8);key('ENTER');time.sleep(.6)
        _,data=saved();x=struct.unpack_from('<I',data,46)[0]/16
        assert x-5>54*16, 'Player cannot pass the two-cell open door'
        assert data[612+29*384+53]==40 and data[612+30*384+53]==39
        stop();start(91);capture('open-door-restart-title')
        _,restarted=saved();assert restarted==data, 'Starting the title rewrote the world'
        enter_world();time.sleep(1.2);capture('open-door-reloaded')
        key('LEFT',.06);key('TAB');time.sleep(.3);capture('door-closed-outside')
        key('ENTER');time.sleep(.6);_,data=saved()
        assert data[612+29*384+53]==38 and data[612+30*384+53]==23
        stop()
        subprocess.run([str(args.fixture_tool.resolve()),'doors',str(path)],check=True)
        start(92);enter_world();time.sleep(14);capture('protected-house')
        key('ENTER');time.sleep(.6);_,data=saved()
        assert struct.unpack_from('<H',data,58)[0]==100, 'Closed starter house did not protect its idle player'
        stop()
        subprocess.run([str(args.fixture_tool.resolve()),'doors-legacy-gap',str(path)],check=True)
        start(93);enter_world();time.sleep(1.2);capture('legacy-door-gap')
        key('ENTER');time.sleep(.6);_,data=saved()
        assert data[612+29*384+53]==38 and data[612+30*384+53]==23
        assert data[612+31*384+53]==0, 'Legacy door upgrade refilled edited ground'
        print('PASS real PXA two-cell door: blocks movement, opens whole, passes player, persists open, reloads and closes outside; day-time room survival; one display profile')
        raise SystemExit(0)
    if args.weapons_only:
        assert args.fixture_tool, '--weapons-only requires --fixture-tool'
        start(0);enter_world()
        px,py=display_point(152,146,'world')
        command(f'TOUCH DOWN 0 {px} {py}');time.sleep(.35);capture('mining-feedback')
        command(f'TOUCH UP 0 {px} {py}');key('ENTER');time.sleep(.6)
        path,_=saved();stop()
        for number,fixture in enumerate(args.fixtures or ('pick','axe','bow','wand'),40):
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
    tap(151,38,"fixed");tap(136,98,"world");tap(31,38,"fixed")
    # Header tap opens bag at 125 DPI; a missing conversion opens pause instead.
    tap(195,22,"header");capture('03-bag')
    tap(145,35,"tab");capture('04-equipment')
    tap(85,35,"tab");capture('05-recipes');tap(168,208,"footer");time.sleep(.4)
    # Real touch batch crafting and controller Y both complete five recipes.
    # Starting with 24 wood leaves enough for each, even after plank placement.
    tap(213,208,"footer");capture('05b-batch-touch')
    key('S');capture('05c-batch-controller')
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
    assert crafted==11,'Single + touch batch + controller batch should craft 1 + 5 + 5 recipes'
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
        start(3);enter_world();tap(195,22,"header");tap(145,35,"tab");time.sleep(4.6);capture('09-crystal-armor')
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
            tap(195,22,"header");tap(145,35,"tab");capture(f'{fixture}-outfit-right')
            tap(62,115);capture(f'{fixture}-outfit-left');stop()
        history.append('PASS simulator captures: copper/iron/crystal, tunic/robe, both facings, bow and wand use')
        for number,fixture in enumerate(('desert','snow','cave','warden','jungle','night','joins','forest'),4):
            subprocess.run([str(args.fixture_tool.resolve()),fixture,str(path)],check=True)
            start(number);enter_world()
            if fixture=='warden':
                # The aimed workbench now opens its actual crafting UI. Tap
                # the altar itself with a sword to summon the guard instead.
                tap(79,45,'fixed');tap(144,126,'world');tap(63,38,'fixed')
            time.sleep(4.6);capture(f'{number+7:02d}-{fixture}')
            if fixture=='warden':
                key('ENTER');time.sleep(.6)
                info=json.loads(subprocess.check_output([str(args.fixture_tool.resolve()),'inspect',str(path)],text=True))
                assert info['star_cores']==0, 'Altar tap did not summon the guard'
                history.append('PASS direct altar tap consumes the summoning core')
            stop()
    logs=''.join(p.read_text(errors='replace') for p in out.glob('simulator-*.log'))
    assert 'Guest exception' not in logs and 'save failed' not in logs.lower(), 'Runtime error; inspect logs'
    (out/'checks.txt').write_text('\n'.join(history)+'\n')
    print('\n'.join(history))
finally:
    stop()
    Path(sockpath).unlink(missing_ok=True)
