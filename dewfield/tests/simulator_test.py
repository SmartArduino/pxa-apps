#!/usr/bin/env python3
"""Run the signed game in the real PXA simulator; keep raw captures local."""
import argparse
import json
import os
import re
from pathlib import Path
import socket
import struct
import subprocess
import time
import zlib
from PIL import Image

p=argparse.ArgumentParser()
p.add_argument('--package',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--runner',type=Path,default=Path('build/simulator/doit-gamekit-1/pxsys_product_simulator'))
p.add_argument('--appearance-only',action='store_true')
p.add_argument('--title-only',action='store_true')
p.add_argument('--animation-only',action='store_true')
p.add_argument('--navigation-only',action='store_true')
p.add_argument('--action-only',action='store_true')
p.add_argument('--inventory-only',action='store_true')
p.add_argument('--tools-only',action='store_true')
p.add_argument('--life-only',action='store_true')
p.add_argument('--living-only',action='store_true')
p.add_argument('--atmosphere-only',action='store_true')
p.add_argument('--redesign-only',action='store_true')
p.add_argument('--discoveries-only',action='store_true')
p.add_argument('--actors-only',action='store_true')
p.add_argument('--livestock-only',action='store_true')
p.add_argument('--mines-only',action='store_true')
p.add_argument('--npc-only',action='store_true')
p.add_argument('--world-only',action='store_true')
p.add_argument('--layers-only',action='store_true')
p.add_argument('--cultivation-only',action='store_true')
p.add_argument('--fixture-tool',type=Path)
p.add_argument('--width',type=int,default=480)
p.add_argument('--height',type=int,default=480)
args=p.parse_args()
out=args.output.resolve()
assert '/local/artifacts/' in str(out)
out.mkdir(parents=True,exist_ok=True)
state=out/'state'
state.mkdir(exist_ok=True)
sock=f'/tmp/dewfield-{os.getpid()}.sock'
process=None
log=None
run_id=0
scale=max(1,min(args.width//240,args.height//240))
cw,ch=args.width//scale,args.height//scale
report={}

def palette_color(name):
    source=(Path(__file__).resolve().parents[1]/'palette.hpp').read_text()
    r,g,b=map(int,re.search(r'\b'+name+r'\s*=\s*rgb\((\d+),\s*(\d+),\s*(\d+)\)',source).groups())
    return ((r&248)<<8)|((g&252)<<3)|(b>>3)

def tool_geometry(i):
    cell=min(32,(cw-44)//7)
    left=(cw-(cell*7+4))//2+2
    return left+i*cell, ch-46, cell

def tool_tap(i):
    x,y,w=tool_geometry(i);tap(x+w//2,y+15)

def command(value,png=None):
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as c:
        c.settimeout(15)
        c.connect(sock)
        c.sendall((value+'\n').encode())
        f=c.makefile('rb')
        header=f.readline()
        if png:
            assert header.startswith(b'PNG '),header
            size=int(header.split()[1]);data=f.read(size);assert len(data)==size
            (out/png).write_bytes(data)
        else:assert header.startswith(b'OK'),(value,header)

def tap(x,y,settle=.18):
    x,y=int(x*scale),int(y*scale)
    command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.05)
    command(f'TOUCH UP 0 {x} {y}');time.sleep(settle)

def key(k,duration=.08):
    command('KEY_DOWN '+k);time.sleep(duration);command('KEY_UP '+k);time.sleep(.15)

def capture(name):
    command('SCREENSHOT AFTER_PRESENT',name+'.png')
    im=Image.open(out/(name+'.png')).convert('RGB')
    assert im.size==(args.width,args.height)
    # Full rectangle, including corners; no mask, nav bar or empty matte.
    assert all(im.getpixel(pt)!=(0,0,0) for pt in [(0,0),(0,args.height-1),(args.width-1,0),(args.width-1,args.height-1)])

def start():
    global process,log,run_id
    run_id+=1
    log=(out/f'run-{run_id}.log').open('w')
    env=os.environ.copy();env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='none')
    process=subprocess.Popen([str(args.runner.resolve()),'--package',str(args.package.resolve()),'--publisher-key',str(Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der').resolve()),'--state-root',str(state),'--pxadb-control-socket',sock,'--width',str(args.width),'--height',str(args.height),'--density-dpi','160','--corner-radius','0','--safe-insets','0,0,0,0','--locale','zh-CN'],stdout=log,stderr=subprocess.STDOUT,env=env)
    for _ in range(150):
        assert process.poll() is None,(out/f'run-{run_id}.log').read_text()
        if Path(sock).exists() and 'Pocket: ready' in (out/f'run-{run_id}.log').read_text():break
        time.sleep(.1)
    else:raise AssertionError('simulator startup timeout')
    time.sleep(.3)

def stop():
    global process
    if process and process.poll() is None:
        process.terminate()
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait()
    if log:log.close()
    process=None

def saved():
    values=[]
    for path in state.glob('**/.pxa-kv-*'):
        raw=path.read_bytes()
        if len(raw)<26:continue
        magic,version,_,gen,size,crc=struct.unpack_from('<4sHHQII',raw)
        body=raw[24:]
        if magic!=b'PXKV' or version!=1 or len(body)!=size or zlib.crc32(body)!=crc:continue
        at=2
        for _ in range(struct.unpack_from('<H',body)[0]):
            n=body[at];at+=1;k=body[at:at+n].decode();at+=n
            n=struct.unpack_from('<H',body,at)[0];at+=2;v=body[at:at+n];at+=n
            if k=='dewfield.save.v1':values.append((gen,v))
    assert values,'No durable save'
    raw=max(values,key=lambda x:x[0])[1]
    assert raw[:4]==b'DWF1' and len(raw) in (512,768,1280)
    result={k:struct.unpack_from(fmt,raw,offset)[0] for k,fmt,offset in [('day','<H',6),('energy','<B',10),('water','<B',11),('px','<H',18),('py','<H',20),('gold','<I',22),('shipping','<I',26),('earned','<I',30),('harvested','<I',34),('fish','<H',58),('wood','<H',60),('stone','<H',62)]}
    result.update(seeds=list(struct.unpack_from('<8H',raw,42))[::2],produce=list(struct.unpack_from('<8H',raw,42))[1::2],plots=[list(raw[98+i*4:102+i*4])for i in range(80)])
    if raw[4] >= 2:
        result.update(slots=list(raw[449:477]),active=raw[477],settings=raw[502],damage=list(raw[425:449]))
    if raw[4] >= 3:
        result.update(region=raw[503],story=raw[504],met=raw[505],visited=raw[506],bonds=list(raw[507:511]))
    if raw[4] >= 4:
        result.update(gathered=list(struct.unpack_from('<3H',raw,530)),meals=list(struct.unpack_from('<2H',raw,536)),orders_done=struct.unpack_from('<H',raw,543)[0])
    result['auto_run']=bool(raw[559]) if raw[4]>=6 else True
    if raw[4]>=8:
        result.update(discoveries=struct.unpack_from('<I',raw,560)[0],fish_records=list(raw[564:572]),fish_caught=struct.unpack_from('<H',raw,572)[0],perfect=struct.unpack_from('<H',raw,574)[0],collection_rewards=raw[576])
    if raw[4]>=9:
        result.update(goods=list(struct.unpack_from('<8H',raw,577)),animals=[list(raw[593+i*11:600+i*11])+list(struct.unpack_from('<2H',raw,600+i*11)) for i in range(12)],incubators=[list(raw[725+i*2:727+i*2]) for i in range(3)],hay=struct.unpack_from('<H',raw,731)[0],trough=struct.unpack_from('<H',raw,733)[0],hatched=struct.unpack_from('<H',raw,735)[0],born=struct.unpack_from('<H',raw,737)[0],intro=raw[741],ranch=raw[742],ranch_chapter=raw[743])
        result['slots']+=list(raw[745:759])
    else:result['intro']=5
    if raw[4]>=10:
        result['slots']+=list(raw[759:773])
        result['visited']|=raw[839]<<8
        result.update(wild_age=list(raw[773:798]),wild_damage=list(raw[798:823]),minerals=list(struct.unpack_from('<8H',raw,823)),sword=raw[840],sword_grade=raw[841],health=raw[842],depth=raw[843],best=raw[844],slain=struct.unpack_from('<H',raw,852)[0],ore_mined=struct.unpack_from('<H',raw,854)[0],cave_gathered=struct.unpack_from('<H',raw,856)[0],mine_rewards=raw[862],mine_cleared=list(struct.unpack_from('<30I',raw,863)),mine_defeated=list(raw[983:1013]))
    return result

def wait_for(predicate,label,seconds=12):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        try:
            s=saved()
            if predicate(s):return s
        except AssertionError:pass
        time.sleep(.1)
    raise AssertionError((label,saved()))

def wait_for_panel(label,seconds=15):
    # Observe the real modal, rather than racing a fixed path-travel delay.
    paper=palette_color('Paper')
    x,y=(cw-228)//2,(ch-210)//2
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        command('SCREENSHOT AFTER_PRESENT','panel-poll.png')
        im=Image.open(out/'panel-poll.png').convert('RGB')
        samples=[im.getpixel(((x+dx)*scale,(y+dy)*scale)) for dx,dy in [(5,40),(222,40),(5,196),(222,196)]]
        if all((((r&248)<<8)|((g&252)<<3)|(b>>3))==paper for r,g,b in samples):return
        time.sleep(.12)
    raise AssertionError('No modal: '+label)

def guide(i):
    key('S')  # SDL S maps to game Y: open journal.
    x=(cw-228)//2+62+(i%2)*105
    y=(ch-210)//2+52+(i//2)*29
    tap(x,y)

def world_tap(tx,ty):
    s=saved()
    camx=max(0,min(512-cw,s['px']-cw//2))
    camy=max(0,min(384-ch,s['py']-(ch-8)//2))
    tap(tx*16+8-camx,ty*16+8-camy)

def replace_fixture(raw):
    candidates=[]
    for path in state.glob('**/.pxa-kv-*'):
        data=path.read_bytes()
        if len(data)<26:continue
        magic,version,reserved,generation,size,crc=struct.unpack_from('<4sHHQII',data)
        body=data[24:]
        if magic!=b'PXKV' or version!=1 or len(body)!=size or zlib.crc32(body)!=crc:continue
        at=2;values={}
        for _ in range(struct.unpack_from('<H',body)[0]):
            n=body[at];at+=1;k=body[at:at+n];at+=n
            n=struct.unpack_from('<H',body,at)[0];at+=2;values[k]=body[at:at+n];at+=n
        if b'dewfield.save.v1' in values:candidates.append((generation,path,values))
    generation,path,values=max(candidates,key=lambda x:x[0]);values[b'dewfield.save.v1']=raw
    parts=[struct.pack('<H',len(values))]
    for k,v in values.items():parts.extend([bytes([len(k)]),k,struct.pack('<H',len(v)),v])
    body=b''.join(parts);data=struct.pack('<4sHHQII',b'PXKV',1,0,generation+1,len(body),zlib.crc32(body))+body
    temporary=path.with_suffix('.fixture-tmp');temporary.write_bytes(data);temporary.replace(path)

try:
    start();capture('01-title')
    if args.title_only:
        # Verify an unobscured scanline directly against the original art.
        import math
        source=(Path(__file__).resolve().parents[1]/'title_art.hpp').read_text()
        sw,sh=map(int,re.search(r'Width=(\d+),Height=(\d+)',source).groups())
        colors=list(map(int,re.search(r'pixels\[\]=\{(.*?)\};',source,re.S).group(1).replace('\n','').strip(',').split(',')))
        im=Image.open(out/'01-title.png').convert('RGB');zoom=max(args.width/sw,args.height/sh)
        y=args.height//2
        for x in range(args.width):
            sx=math.floor((x+.5-args.width/2)/zoom+sw/2+1e-9)
            sy=math.floor((y+.5-args.height/2)/zoom+sh/2+1e-9)
            r,g,b=im.getpixel((x,y));actual=((r&248)<<8)|((g&252)<<3)|(b>>3)
            assert actual==colors[sy*sw+sx],('Title stretched or incorrectly cropped',x,y)
        stop();print('PASS: real simulator title uses uniform scale and centred crop');raise SystemExit(0)
    try:needs_intro=saved().get('intro',5)<5
    except AssertionError:needs_intro=True
    tap(cw//2,ch-38)
    if args.livestock_only:
        assert args.fixture_tool
        if needs_intro:
            for scene in range(5):
                time.sleep(.25);capture('opening-'+str(scene));key('Z')
            wait_for(lambda v:v['intro']==5 and v['ranch']==1,'opening completion')
            assert sum(bool(a[0]) for a in saved()['animals'])==2
        key('ENTER');time.sleep(.3);saved();stop()
        raw=out/'livestock.save';subprocess.run([str(args.fixture_tool.resolve()),'livestock-farm',str(raw),str(out/'unused')],check=True)
        replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.25)
        key('UP',.025);key('Z');wait_for_panel('ranch');capture('ranch-ledger')
        mx,my=(cw-228)//2,(ch-210)//2
        tap(mx+44,my+195);capture('ranch-supply')
        before=saved();tap(mx+100,my+138);after=saved();assert after['gold']==before['gold'] # all twelve stalls reserved
        key('X');tap(mx+45,my+78);capture('animal-care')
        tap(mx+100,my+138);wait_for_panel('walk to animal',15);time.sleep(.3)
        tap(mx+100,my+138);wait_for(lambda v:v['goods'][6]==1,'collect fertile egg')
        capture('egg-collected');key('X')
        for _ in range(3):tap(mx+198,my+170)
        tap(mx+65,my+78);tap(mx+100,my+190);wait_for_panel('walk before pork confirmation');capture('pork-confirmation')
        tap(mx+60,my+188);assert sum(bool(a[0]) for a in saved()['animals'])==10
        tap(mx+100,my+190);tap(mx+164,my+188)
        wait_for(lambda v:v['goods'][5]==4 and sum(bool(a[0]) for a in v['animals'])==9,'confirmed adult pig')
        tap(mx+44,my+195);wait_for_panel('return to ranch',15);tap(mx+44,my+195);before=saved();tap(mx+100,my+138)
        wait_for(lambda v:v['gold']==before['gold']-80 and sum(bool(a[0]) for a in v['animals'])==10,'buy new chick')
        key('X');key('X');key('A')
        egg=saved()['slots'].index(35);sx=mx+25+egg%7*29;sy=my+49+egg//7*28
        command(f'TOUCH DOWN 0 {sx*scale} {sy*scale}')
        command(f'TOUCH MOVE 0 {(mx+180)*scale} {(my+15)*scale}');time.sleep(.1)
        command(f'TOUCH MOVE 0 {(mx+25)*scale} {(my+49)*scale}');command(f'TOUCH UP 0 {(mx+25)*scale} {(my+49)*scale}');time.sleep(.2)
        wait_for(lambda v:v['slots'][28]==35,'cross-page egg drag');capture('bag-second-page')
        tap(mx+25,my+49);key('X');capture('holding-fertile-egg');capture('pasture')
        key('ENTER');time.sleep(.3);stop()
        start();tap(cw//2,ch-38);key('ENTER');time.sleep(.3);assert saved()['active']==28 and saved()['slots'][28]==35;stop()
        subprocess.run([str(args.fixture_tool.resolve()),'livestock-night',str(raw),str(out/'unused')],check=True)
        replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);key('UP',.025);key('Z');wait_for_panel('sleep');capture('before-birth')
        key('Z');after=wait_for(lambda v:v['hatched']==1 and v['born']==1 and v['day']==2,'hatching and live birth')
        assert sum(bool(a[0]) for a in after['animals'])==12
        assert after['animals'][10][2]==0 and after['animals'][11][2]==0
        capture('new-morning');key('Z');key('ENTER');time.sleep(.3);durable=saved();stop()
        start();tap(cw//2,ch-38);key('ENTER');time.sleep(.3);again=saved()
        for k in ['animals','goods','incubators','hatched','born','ranch','ranch_chapter','hay','trough','slots']:assert durable[k]==again[k],k
        capture('restart');stop()
        subprocess.run([str(args.fixture_tool.resolve()),'livestock-slow',str(raw),str(out/'unused')],check=True)
        replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.25)
        frames=[];command('KEY_DOWN RIGHT')
        for i in range(12):
            capture('slow-walk-'+str(i));frames.append(Image.open(out/('slow-walk-'+str(i)+'.png')).convert('RGB'));time.sleep(.035)
        command('KEY_UP RIGHT');frames[0].save(out/'slow-walk.gif',save_all=True,append_images=frames[1:],duration=95,loop=0)
        key('ENTER');time.sleep(.25);stop();(out/'livestock-report.json').write_text(json.dumps(again,indent=2)+'\n')
        print('PASS: signed simulator five opening scenes, care, reserved stalls, fertile egg collection, overnight hatching/live birth and v9 restart')
        raise SystemExit(0)
    if needs_intro:
        key('X');key('Z');time.sleep(.2)
    if args.appearance_only or args.animation_only or args.action_only or args.cultivation_only:time.sleep(2.4)
    capture('02-farm')
    if args.mines_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.3);saved();stop()
        results={}
        def fixture(label):
            raw=out/(label+'.save')
            subprocess.run([str(args.fixture_tool.resolve()),'mines-'+label,str(raw),str(out/'unused')],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.2)
        def persist():
            key('ENTER');time.sleep(.3);v=saved();key('X');return v
        fixture('forest');key('RIGHT',.025);capture('forest-before')
        key('Z',1.6);time.sleep(.3);v=persist()
        assert v['wood']==4 and v['wild_age'][1]==6 and v['wild_damage'][1]==0,v
        results['forest']=v;capture('forest-stump');stop()
        start();tap(cw//2,ch-38);v=persist();assert v['wild_age'][1]==6 and v['wood']==4;stop()
        fixture('entry');key('UP',.4);wait_for_panel('first mine letter');capture('mine-welcome');key('Z');time.sleep(.2);v=persist()
        assert v['region']==8 and v['depth']==1 and v['sword'] and v['visited']&256,v
        capture('mine-entry');stop()
        fixture('ore');key('DOWN',.025);key('Z',1.1);time.sleep(.2);v=persist()
        assert v['ore_mined']==1 and sum(v['minerals'][:5])==2 and v['mine_cleared'][0]&1,v
        results['ore']=v;capture('ore-collected');stop()
        fixture('forage');key('DOWN',.025);key('Z');time.sleep(.3);v=persist()
        assert v['cave_gathered']==1 and v['minerals'][5]==1,v
        results['forage']=v;capture('cave-forage');stop()
        fixture('combat');key('DOWN',.025);capture('combat-before');key('Z');time.sleep(.5);v=persist()
        assert v['slain']==1 and v['minerals'][7]==1 and v['mine_defeated'][0]==63,v
        results['combat']=v;capture('combat-after');stop()
        fixture('stairs');key('UP',.025);key('Z');wait_for_panel('stairs');capture('descent-confirm');key('Z');time.sleep(.2);v=persist()
        assert v['depth']==2 and v['best']==2,v
        results['stairs']=v;capture('floor-two');stop()
        for label in ['iron','crystal','guardian']:
            fixture(label);capture('biome-'+label);tap(77,76);wait_for_panel('mine journal');capture('journal-'+label);key('X')
            if label=='guardian':
                key('UP',.025);time.sleep(.3);key('Z',3.8);wait_for_panel('guardian chest');capture('guardian-defeated')
                key('Z');time.sleep(.2);capture('guardian-reward');key('X');v=persist()
                assert v['mine_rewards']==1 and v['gold']==1300 and v['minerals'][4]==4 and v['minerals'][6]==3,v
            else:v=persist()
            results[label]=v;stop()
        start();tap(cw//2,ch-38);v=persist();assert v['depth']==30 and v['best']==30 and v['sword_grade']==1 and v['mine_rewards']==1;stop()
        (out/'mines-report.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
        print('PASS: signed simulator wild chopping/restart, first mine entry, ore/forage, timed combat loot, confirmed descent, three biomes/journal and v10 restart')
        raise SystemExit(0)
    if args.actors_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.3);saved();stop()
        results=[]
        for direction,name in enumerate(['north','east','south','west']):
            raw=out/(name+'.save');subprocess.run([str(args.fixture_tool.resolve()),'actors-'+str(direction),str(raw),str(out/'unused')],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.25)
            key(['UP','RIGHT','DOWN','LEFT'][direction],.025)
            command('KEY_DOWN Z');time.sleep(.02);command('KEY_UP Z')
            frames=[]
            for frame in range(12):
                capture('axe-'+name+'-'+str(frame));frames.append(Image.open(out/('axe-'+name+'-'+str(frame)+'.png')).convert('RGB'));time.sleep(.025)
            key('ENTER');time.sleep(.3);actual=saved();assert actual['damage'][7]==2 and actual['energy']==78 and actual['wood']==0,actual
            frames[0].save(out/('axe-'+name+'.gif'),save_all=True,append_images=frames[1:],duration=70,loop=0)
            results.append(actual);stop()
        raw=out/'villagers.save';subprocess.run([str(args.fixture_tool.resolve()),'actors-villagers',str(raw),str(out/'unused')],check=True)
        replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.3);capture('four-villagers')
        key('S');key('W');capture('villagers-journal')
        (out/'actors-report.json').write_text(json.dumps(results,indent=2)+'\n')
        print('PASS: real signed four-heading axe animation, contact damage/energy, independent villager appearance and people journal')
        raise SystemExit(0)
    if args.discoveries_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.3);saved();stop()
        raw=out/'discoveries.save'
        subprocess.run([str(args.fixture_tool.resolve()),'discoveries-fishing',str(raw),str(out/'fishing-setup.json')],check=True)
        replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.25)
        before=saved();key('RIGHT',.06);key('Z',.04)
        mx,my=(cw-228)//2,(ch-210)//2
        paper,sage,grass_light,brass=(palette_color(k) for k in ('Paper','Sage','GrassLight','Brass'))
        def rgb565(im,x,y):
            r,g,b=im.getpixel((int(x*scale),int(y*scale)));return ((r&248)<<8)|((g&252)<<3)|(b>>3)
        end=time.monotonic()+5
        while time.monotonic()<end:
            command('SCREENSHOT AFTER_PRESENT','bite-poll.png');im=Image.open(out/'bite-poll.png').convert('RGB')
            if rgb565(im,mx+115,my+94)==brass:break
            time.sleep(.035)
        else:raise AssertionError('No bite in real fishing modal')
        key('Z',.035);capture('fishing-timing')
        caught=None
        for attempt in range(2):
            end=time.monotonic()+4
            while time.monotonic()<end:
                command('SCREENSHOT AFTER_PRESENT','reel-poll.png');im=Image.open(out/'reel-poll.png').convert('RGB')
                marker=[x for x in range(mx+25,mx+203) if rgb565(im,x,my+116)==paper]
                green=[x for x in range(mx+25,mx+203) if rgb565(im,x,my+125) in (sage,grass_light)]
                if marker and green and min(green)+12<sum(marker)/len(marker)<max(green)-12:
                    key('Z',.035);break
                time.sleep(.02)
            else:raise AssertionError('Unable to observe a catchable green zone')
            time.sleep(2.3) # Storage deliberately coalesces game saves for two seconds.
            if saved()['fish']>before['fish']:caught=saved();break
        assert caught and caught['fish_caught']==1 and any(caught['fish_records']),caught
        assert caught['energy']==before['energy']-4
        capture('fishing-catch');key('A');capture('fish-collection')
        key('W');key('Z');reward=wait_for(lambda s:s.get('collection_rewards',0)&1,'badge save',4)
        assert reward['gold']==before['gold']+80 and reward['collection_rewards']&1,reward
        key('Z');time.sleep(.3);assert saved()['gold']==reward['gold']
        capture('claimed-badge');key('X');key('X');key('ENTER');time.sleep(.3);stop()
        start();tap(cw//2,ch-38);key('S');key('S');capture('crop-collection');key('W');capture('wild-collection')
        restored=saved();assert restored['fish_records']==caught['fish_records'] and restored['collection_rewards']==reward['collection_rewards'] and restored['gold']==reward['gold']
        (out/'discoveries-report.json').write_text(json.dumps({'caught':caught,'claimed':reward,'restarted':restored},ensure_ascii=False,indent=2)+'\n')
        print('PASS: signed simulator bite, timing catch, fish catalog, one-time badge reward and durable v8 records across restart')
        raise SystemExit(0)
    if args.redesign_only:
        assert args.fixture_tool
        def persist():
            key('ENTER');time.sleep(.25);value=saved();key('ENTER');return value
        def fixture(label):
            raw=out/(label+'.save')
            subprocess.run([str(args.fixture_tool.resolve()),'redesign-'+label,str(raw),str(out/'fixture-unused')],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.25)
        key('ENTER');time.sleep(.3);saved();stop()
        fixture('door');key('RIGHT',.075);time.sleep(.25)
        side=persist();assert side['region']==0 and side['px']//16==6 and side['py']//16==7,side
        capture('door-sideways-no-entry');key('UP',.075);time.sleep(.25)
        assert persist()['region']==4;capture('door-forward-entry');stop()
        fixture('route');world_tap(6,7);time.sleep(2.5)
        assert persist()['region']==4;capture('door-tap-approach');stop()
        fixture('bag');before=persist();key('A')
        mx,my=(cw-228)//2,(ch-210)//2
        tap(mx+199,my+133);capture('bag-held-slot-27')
        command(f'TOUCH DOWN 0 {(mx+199)*scale} {(my+133)*scale}')
        time.sleep(.05);command(f'TOUCH MOVE 0 {(mx+25)*scale} {(my+105)*scale}')
        time.sleep(.1);command(f'TOUCH UP 0 {(mx+25)*scale} {(my+105)*scale}');time.sleep(.2)
        capture('bag-held-slot-14');key('BACK');capture('carrying-from-backpack')
        carried=persist();assert carried['active']==14 and carried['slots'][14]==12
        assert carried['slots'][:7]==before['slots'][:7]
        key('Z',.12);eaten=persist();assert eaten['produce'][0]==2 and eaten['energy']==70,eaten
        key('UP',.075);key('Z',.12);shipped=persist()
        assert shipped['produce'][0]==1 and shipped['shipping']==16 and shipped['energy']==70,shipped
        key('Z',.12);last=persist();assert last['produce'][0]==0 and last['shipping']==32 and last['active']==255,last
        capture('single-item-shipping');stop()
        for label in ['hud-left','hud-right','hud-belt','soil']:
            fixture(label);capture(label);stop()
        (out/'redesign-report.json').write_text(json.dumps({'door_sideways':'stayed on farm','forward_and_tap':'entered home','bag':'slot27 to slot14 without hotbar rearrangement','eat':'one item per press','ship':'one item per press, empty stack deselected','hud':['left','right','belt'],'tilled_ground':'dry and wet cropped patches'},indent=2)+'\n')
        print('PASS: actual signed simulator directional and intentional doors, backpack carrying/drag/eating/shipping, HUD occlusion and organic tilled ground')
        raise SystemExit(0)
    if args.atmosphere_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.3);saved();stop()
        samples=[]
        for name,minute in [('dawn',360),('day',720),('afternoon',1020),('sunset',1080),('twilight',1130),('night',1260)]:
            raw=out/(name+'.save')
            subprocess.run([str(args.fixture_tool.resolve()),'atmosphere-'+str(minute),str(raw),str(out/'fixture-unused')],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(.5);capture('time-'+name)
            im=Image.open(out/('time-'+name+'.png')).convert('RGB')
            samples.append(im.getpixel((8*scale,122*scale)))
            key('ENTER');time.sleep(.2);stop()
        assert len(set(samples))>=5,samples
        assert sum(samples[-1])<sum(samples[1])*.8,samples
        images=[Image.open(out/('time-'+n+'.png')).convert('RGB') for n in ['dawn','day','afternoon','sunset','twilight','night']]
        images[0].save(out/'day-cycle.gif',save_all=True,append_images=images[1:],duration=950,loop=0)
        # Compare the same real input for the default run and Settings' walk.
        distances=[]
        for run in [True,False]:
            raw=out/'movement.save';subprocess.run([str(args.fixture_tool.resolve()),'atmosphere-720',str(raw),str(out/'fixture-unused')],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38)
            if not run:
                key('ENTER');tap(cw//2,(ch-180)//2+53+3*30)
                mx,my=(cw-228)//2,(ch-210)//2
                tap(mx+110,my+51+3*32);capture('auto-run-disabled');key('BACK')
            key('ENTER');time.sleep(.2);before=saved();key('ENTER')
            command('KEY_DOWN RIGHT');time.sleep(.8);command('KEY_UP RIGHT');time.sleep(.3)
            key('ENTER');time.sleep(.2);after=saved();assert after['auto_run']==run
            assert after['energy']==before['energy'];distances.append(after['px']-before['px']);stop()
        assert distances[0]>distances[1]*1.4,(distances,samples)
        start();tap(cw//2,ch-38);key('ENTER');time.sleep(.2);assert not saved()['auto_run'];stop()
        (out/'atmosphere-report.json').write_text(json.dumps({'running_vs_walking_px':distances,'same_ground_rgb':samples,'toggle_persisted':True},indent=2)+'\n')
        print('PASS: actual signed simulator six day phases, terrain tint, default running, touch toggle, faster travel without energy cost and restart persistence')
        raise SystemExit(0)
    if args.npc_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.4);saved();stop()
        raw=out/'npc.save';unused=out/'npc.csv'
        subprocess.run([str(args.fixture_tool.resolve()),'living-npc',str(raw),str(unused)],check=True)
        replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38,settle=0)
        images=[]
        for frame in range(12):
            capture('npc-walk-'+str(frame));images.append(Image.open(out/('npc-walk-'+str(frame)+'.png')).convert('RGB'));time.sleep(.005)
        # On the wide viewport this rectangle contains the moving village
        # elder, with no HUD, player or animated water in its bounds.
        assert args.width==800 and args.height==480
        crops=[im.crop((150*scale,30*scale,250*scale,86*scale)).tobytes()for im in images]
        assert len(set(crops))>=8,'Villager walking did not refresh smoothly'
        images[0].save(out/'villager-walk.gif',save_all=True,append_images=images[1:],duration=100,loop=0)
        stop();print('PASS: actual villager displacement and shared gait animate in the real simulator');raise SystemExit(0)
    if args.living_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.4);saved();stop()
        for label in ('home','shop','town','lodge','orchard','shipping','night','snow'):
            raw=out/(label+'.save');unused=out/(label+'.csv')
            subprocess.run([str(args.fixture_tool.resolve()),'living-'+label,str(raw),str(unused)],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(1)
            capture('living-'+label)
            if label=='home':
                world_tap(15,7);wait_for_panel('TV forecast');capture('living-forecast');key('BACK')
                world_tap(13,5);wait_for_panel('Calendar');capture('living-calendar');key('BACK')
            elif label=='shop':
                world_tap(15,10);wait_for_panel('Store counter');capture('living-store-counter');key('BACK')
            elif label=='shipping':
                before=saved();world_tap(8,9)
                after=wait_for(lambda s:s['shipping']==16,'one held item shipped')
                assert after['produce'][0]==before['produce'][0]-1 and after['produce'][2]==before['produce'][2] and after['fish']==before['fish']
                capture('living-one-item-shipped')
            key('ENTER');time.sleep(.3);value=saved();stop()
            start();tap(cw//2,ch-38);key('ENTER');time.sleep(.3);again=saved()
            assert again['day']==value['day'] and again['region']==value['region'] and again['produce']==value['produce']
            stop()
        (out/'living-report.json').write_text(json.dumps({'result':'passed','scenes':8,'shipping':'one held item only','restart':'persisted'},indent=2))
        print('PASS: real signed simulator TV/calendar/store input, eight scenes, individual shipping and restarts')
        raise SystemExit(0)
    if args.life_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.4);saved();stop()
        for label in ('orchard','kitchen','orders'):
            raw=out/(label+'.save');unused=out/(label+'.csv')
            subprocess.run([str(args.fixture_tool.resolve()),'life-'+label,str(raw),str(unused)],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(1)
            capture('life-'+label+'-area')
            if label=='orchard':
                world_tap(7,8)
                report['forage']=wait_for(lambda s:s['gathered'][0]>=10,'orchard apples')
                capture('life-orchard-collected')
            elif label=='kitchen':
                world_tap(21,10);wait_for_panel('kitchen');capture('life-kitchen-recipes')
                tap((cw-228)//2+180,(ch-210)//2+58)
                report['cooked']=wait_for(lambda s:s['meals'][0]==2,'cooked stew')
                assert report['cooked']['gathered'][1]==4 and report['cooked']['produce'][1]==19
                capture('life-kitchen-cooked')
            else:
                world_tap(18,10);wait_for_panel('requests');capture('life-daily-requests')
                tap((cw-228)//2+180,(ch-210)//2+50)
                report['delivered']=wait_for(lambda s:s['orders_done']==1,'request delivered')
                assert report['delivered']['gold']>480 and report['delivered']['bonds'][0]==3
                tap((cw-228)//2+180,(ch-210)//2+50)
                time.sleep(.4);assert saved()['orders_done']==1
                capture('life-request-complete')
            stop();start();tap(cw//2,ch-38);time.sleep(.5)
            key('ENTER');time.sleep(.3);value=saved()
            if label=='orchard':assert value['gathered'][0]>=10
            elif label=='kitchen':assert value['meals'][0]==2
            else:assert value['orders_done']==1
            stop()
        (out/'life-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
        print('PASS: real simulator orchard gathering, kitchen cooking, daily requests, duplicate guard and restart saves')
        raise SystemExit(0)
    if args.world_only:
        assert args.fixture_tool
        def persist():
            key('ENTER');time.sleep(.3);value=saved();key('ENTER');return value
        def journal_tab(tab):
            key('S');tap((cw-228)//2+36+tab*52,(ch-210)//2+174)
        def wait_region(region):
            return wait_for(lambda s:s['region']==region,'region '+str(region),30)
        time.sleep(2.4);persist()
        journal_tab(0);capture('world-map')
        tap((cw-228)//2+167,(ch-210)//2+52)
        wait_region(1);time.sleep(.4);capture('walked-farm-to-town')
        journal_tab(2);capture('story-journal');key('BACK')
        journal_tab(1);capture('villager-directory');key('BACK')
        persist();stop()
        for label in ('town','forest','lake','home','festival'):
            raw=out/(label+'.save');unused=out/(label+'.csv')
            subprocess.run([str(args.fixture_tool.resolve()),'world-'+label,str(raw),str(unused)],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(2.4);capture('area-'+label)
            if label=='forest':
                world_tap(17,12);time.sleep(.4);capture('alan-dialogue')
                assert persist()['met']==15
                tap((cw-228)//2+44,(ch-210)//2+190)
                capture('bridge-repaired-dialogue');s=persist();assert s['story']==3 and s['wood']==0 and s['stone']==0,s
                key('BACK');journal_tab(0)
                tap((cw-228)//2+167,(ch-210)//2+81)
                wait_region(3);time.sleep(.4);capture('walked-forest-to-lake')
            elif label=='town':
                world_tap(14,12);time.sleep(.3);capture('granny-dialogue')
                tap((cw-228)//2+44,(ch-210)//2+190)
                s=persist();assert s['story']==1,s
            elif label=='home':
                guide(0);wait_for_panel('home sleep');capture('bed-sleep')
                tap(cw//2,(ch-210)//2+175);time.sleep(.4)
                s=persist();assert s['day']==2 and s['region']==4,s
            persist();stop()
        start();tap(cw//2,ch-38);s=persist();assert s['region']==1 and s['story']==5
        capture('world-reloaded');stop()
        print('PASS: real simulator walking across regions, five areas, NPC dialogue, bridge costs/unlock, cottage bed, story journal and v3 restart');raise SystemExit(0)
    if args.tools_only:
        assert args.fixture_tool
        key('ENTER');time.sleep(.3);saved();stop()
        frames=[]
        for label in ('axe','pick','rod'):
            raw=out/(label+'.save');unused=out/(label+'.csv')
            subprocess.run([str(args.fixture_tool.resolve()),'tool-'+label,str(raw),str(unused)],check=True)
            replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(2.4)
            key('RIGHT',.025)
            command('KEY_DOWN Z');time.sleep(.025);command('KEY_UP Z')
            for i in range(7):
                capture(f'tool-{label}-{i}');frames.append(Image.open(out/f'tool-{label}-{i}.png').convert('RGB'));time.sleep(.045)
            key('ENTER');time.sleep(.3);s=saved()
            if label!='rod':assert s['damage'][0 if label=='axe' else 1]==2,s
            stop()
        frames[0].save(out/'tool-actions.gif',save_all=True,append_images=frames[1:],duration=90,loop=0)
        print('PASS: real simulator held copper axe/pick strikes, persistent damage and rod cast animation');raise SystemExit(0)
    if args.inventory_only:
        def persist():
            key('ENTER');time.sleep(.3);value=saved();key('ENTER');return value
        def drag(x,y,xx,yy,cancel=False):
            command(f'TOUCH DOWN 0 {x*scale} {y*scale}')
            time.sleep(.05)
            command(f'TOUCH MOVE 0 {xx*scale} {yy*scale}');time.sleep(.1)
            command(f'TOUCH {"CANCEL" if cancel else "UP"} 0 {xx*scale} {yy*scale}');time.sleep(.2)
        time.sleep(2.4)
        initial=persist();assert initial['active']==0
        tool_tap(0);assert persist()['active']==255
        tool_tap(0);assert persist()['active']==0
        key('A');capture('bag-before')
        mx,my=(cw-228)//2,(ch-210)//2
        drag(mx+54,my+49,mx+25,my+49)
        capture('bag-seeds-equipped');key('BACK')
        after=persist()
        assert after['slots'][0]==8 and after['slots'][1]==1 and after['seeds']==initial['seeds']
        capture('carrying-seeds')
        x,y,w=tool_geometry(0);xx,yy,ww=tool_geometry(4)
        drag(x+w//2,y+15,xx+ww//2,yy+15)
        after=persist();assert after['slots'][0]==5 and after['slots'][4]==8
        key('ENTER')
        # Pause row four opens the application's Settings page.
        tap(cw//2,(ch-180)//2+53+3*30)
        capture('settings')
        for row in (0,1,2):tap(mx+110,my+51+row*32)
        capture('settings-disabled');key('BACK')
        after=persist();assert after['settings']==0,after
        capture('controls-disabled')
        stop();start();tap(cw//2,ch-38)
        again=persist()
        assert again['slots']==after['slots'] and again['active']==after['active'] and again['settings']==0
        stop();print('PASS: actual simulator bag/hotbar drag, empty hands, settings and restart persistence');raise SystemExit(0)
    if args.navigation_only:
        rows=list(map(int,re.search(r'\{26149,12,\{([^}]+)\}',(Path(__file__).resolve().parents[1]/'font.hpp').read_text()).group(1).split(',')))
        yy,bits=next((yy,bits) for yy,bits in enumerate(rows) if bits)
        xx=next(xx for xx in range(12) if bits&(1<<(11-xx)))
        def hud_visible():
            command('SCREENSHOT AFTER_PRESENT','nav-poll.png')
            r,g,b=Image.open(out/'nav-poll.png').convert('RGB').getpixel(((12+xx)*scale,(9+yy)*scale))
            return ((r&248)<<8)|((g&252)<<3)|(b>>3)==palette_color('Ink')
        def wait_hud(expected):
            deadline=time.monotonic()+5
            while time.monotonic()<deadline:
                if hud_visible()==expected:return
                time.sleep(.05)
            raise AssertionError(f'HUD visibility did not become {expected}')
        wait_hud(True)
        for i in range(3):
            key('BACK');wait_hud(False);capture(f'back-pause-{i}')
            key('BACK');wait_hud(True)
        key('A');wait_hud(False);capture('backpack')
        key('BACK');wait_hud(True);capture('back-closes-panel')
        stop();print('PASS: repeated system Back pauses/resumes without exiting; modal Back closes the current panel');raise SystemExit(0)
    if args.cultivation_only:
        assert args.fixture_tool,'--cultivation-only requires --fixture-tool'
        key('ENTER');time.sleep(.4);saved();stop()
        raw=out/'cultivation.save';unused=out/'cultivation.csv'
        subprocess.run([str(args.fixture_tool.resolve()),'cultivation',str(raw),str(unused)],check=True)
        replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);key('W')
        time.sleep(.4);capture('cultivation-dry-wet')
        key('ENTER');time.sleep(.4)
        actual=saved()
        assert all(p[0] for p in actual['plots'])
        assert {p[1] for p in actual['plots']}=={0,1,2,3,4}
        assert {p[3] for p in actual['plots']}=={0,1}
        stop();print('PASS: actual simulator dry/wet cultivated soil, all crop species and growth stages');raise SystemExit(0)
    if args.action_only:
        frames=[]
        for label in ['hoe','seeds','water']:
            if label!='hoe':key('W')
            command('KEY_DOWN Z');time.sleep(.035);command('KEY_UP Z')
            for i in range(7):
                capture(f'action-{label}-{i}')
                frames.append(Image.open(out/f'action-{label}-{i}.png').convert('RGB'))
                time.sleep(.04)
            time.sleep(.15)
        key('ENTER');time.sleep(.5);actual=saved()
        assert actual['seeds'][0]==11 and actual['water']==29 and actual['plots'][64][:2]==[1,1]
        frames[0].save(out/'planting-actions.gif',save_all=True,append_images=frames[1:],duration=70,loop=0)
        stop();print('PASS: actual simulator hoe, planting and water animation captures with real state changes');raise SystemExit(0)
    if args.animation_only:
        all_frames=[]
        for direction in ['RIGHT','DOWN','LEFT','UP']:
            command('KEY_DOWN '+direction)
            images=[]
            try:
                for frame in range(8):
                    time.sleep(.075);name=f'walk-{direction.lower()}-{frame}'
                    capture(name)
                    images.append(Image.open(out/(name+'.png')).convert('RGB'))
            finally:command('KEY_UP '+direction)
            assert len({im.tobytes() for im in images})>=3,'Walk did not visibly animate'
            images[0].save(out/f'walk-{direction.lower()}.gif',save_all=True,append_images=images[1:],duration=100,loop=0)
            all_frames.extend(images)
            time.sleep(.35);capture('idle-'+direction.lower())
        all_frames[0].save(out/'walk-four-directions.gif',save_all=True,append_images=all_frames[1:],duration=100,loop=0)
        stop();print('PASS: actual simulator four-direction movement captures and return to idle');raise SystemExit(0)
    if args.layers_only:
        assert args.fixture_tool,'--layers-only requires --fixture-tool'
        key('ENTER');time.sleep(.5);saved();stop();checks=[]
        for subject in ['chest','well','house','villager','crop','sprinkler']:
            for side in ['behind','front']:
                label=subject+'-'+side;raw=out/(label+'.bin');expected=out/(label+'.csv')
                subprocess.run([str(args.fixture_tool.resolve()),label,str(raw),str(expected)],check=True,stdout=subprocess.DEVNULL)
                replace_fixture(raw.read_bytes());start();tap(cw//2,ch-38);time.sleep(2.4);capture(label)
                im=Image.open(out/(label+'.png')).convert('RGB');data=raw.read_bytes();px,py=struct.unpack_from('<HH',data,18)
                camx=max(0,min(512-cw,px-cw//2));camy=max(0,min(384-ch,py-(ch-8)//2))
                count=0
                for line in expected.read_text().splitlines():
                    wx,wy,color=map(int,line.split(','));x,y=(wx-camx)*scale,(wy-camy)*scale
                    for dy in range(scale):
                        for dx in range(scale):
                            rr,gg,bb=im.getpixel((x+dx,y+dy));actual=((rr&248)<<8)|((gg&252)<<3)|(bb>>3)
                            assert actual==color,(label,wx,wy,color,actual)
                    count+=1
                checks.append(dict(scene=label,overlap_pixels=count));stop()
        (out/'layers.json').write_text(json.dumps(checks,indent=2)+'\n')
        print('PASS: real simulator foreground/background pixel checks for chest, well, house, villager, crops and sprinklers');raise SystemExit(0)
    if args.appearance_only:
        for direction in ['DOWN','RIGHT','UP','LEFT']:
            key(direction,.12);time.sleep(.25);capture('appearance-'+direction.lower())
        key('A');capture('appearance-backpack');key('X')
        guide(0);wait_for_panel('sleep');key('X');capture('appearance-cottage')
        guide(2);time.sleep(3);capture('appearance-shipping-well')
        guide(4);time.sleep(5);key('X');time.sleep(2.4);capture('appearance-pond')
        guide(5);wait_for_panel('villager');key('X');capture('appearance-village')
        stop();print('PASS: actual simulator four directions, proportions, item icons, cottage, chest, well, shoreline and village props');raise SystemExit(0)
    # Compare the real pixels of 春 against the embedded original font.
    rows=list(map(int,re.search(r'\{26149,12,\{([^}]+)\}',(Path(__file__).resolve().parents[1]/'font.hpp').read_text()).group(1).split(',')))
    im=Image.open(out/'02-farm.png').convert('RGB')
    for yy,bits in enumerate(rows):
        for xx in range(12):
            pixels=[im.getpixel(((12+xx)*scale+sx,(9+yy)*scale+sy))for sy in range(scale)for sx in range(scale)]
            assert len(set(pixels))==1,'Font pixels were stretched'
            native_color=((pixels[0][0]&248)<<8)|((pixels[0][1]&252)<<3)|(pixels[0][2]>>3)
            ink=palette_color('Ink')
            assert (native_color==ink)==bool(bits&(1<<(11-xx))),'Font glyph differs from source'
    report['native_font_verified']=True
    # The compiled tool artwork is also expanded in exact integer pixel blocks.
    atlas=(Path(__file__).resolve().parents[1]/'sprites.hpp').read_text()
    hoe_w,hoe_h=map(int,re.search(r'Sprite hoe=\{(\d+),(\d+)',atlas).groups())
    colors=list(map(int,re.search(r'hoe_pixels\[\]=\{([^}]+)',atlas).group(1).split(',')))
    spans=[tuple(map(int,m.split(',')))for m in re.findall(r'\{([^{}]+)\}',re.search(r'hoe_spans\[\]=\{(.*?)\};',atlas).group(1))]
    for yy,xx,length,offset in spans:
        for j in range(length):
            c=colors[offset+j];expected=(((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31)
            for sy in range(scale):
                for sx in range(scale):
                    rr,gg,bb=im.getpixel(((tool_geometry(0)[0]+tool_geometry(0)[2]//2-hoe_w//2+xx+j)*scale+sx,(tool_geometry(0)[1]+12-hoe_h//2+yy)*scale+sy))
                    assert ((rr&248)<<8)|((gg&252)<<3)|(bb>>3)==c,'Tool sprite pixels changed during scaling'
    report['native_tool_verified']=True

    # Save through the actual pause menu and inspect the Host KV transaction.
    key('ENTER');time.sleep(.5);initial=saved();key('ENTER')
    report['initial']=initial
    world_tap(10,12);s=wait_for(lambda v:v['harvested']==1,'first harvest');capture('03-harvest')
    world_tap(11,12);wait_for(lambda v:v['harvested']==2,'second harvest')
    # Replant the actual harvested plot, water it, then ship and sleep.
    tool_tap(1);world_tap(10,12)
    wait_for(lambda v:v['seeds'][0]==11,'seed consumed')
    tool_tap(2);world_tap(10,12)
    wait_for(lambda v:v['water']==29,'water consumed');capture('04-planted-watered')
    guide(2);wait_for(lambda v:v['shipping']==32,'shipping crate',15);capture('05-shipping')
    guide(0);wait_for_panel('sleep');capture('06-sleep')
    tap(cw//2,(ch-210)//2+175)
    morning=wait_for(lambda v:v['day']==2,'new day');assert morning['gold']==132 and morning['earned']==32 and morning['water']==30
    assert morning['plots'][30][2]==1,morning['plots'][30]
    capture('07-morning');tap(cw//2,(ch-210)//2+181)
    guide(1);wait_for_panel('shop');capture('08-shop')
    tap((cw-228)//2+188,(ch-210)//2+51)
    wait_for(lambda v:v['gold']==127 and v['seeds'][0]==12,'shop purchase')
    tap((cw-228)//2+209,(ch-210)//2+16)
    guide(5);wait_for_panel('villager');capture('09-villager')
    tap((cw-228)//2+209,(ch-210)//2+16)
    # Catch a fish by observing the actual bite button in raw simulator pixels.
    guide(4)
    for _ in range(90):
        command('SCREENSHOT AFTER_PRESENT','fish-poll.png')
        im=Image.open(out/'fish-poll.png').convert('RGB')
        r,g,b=im.getpixel((((cw-228)//2+30)*scale,((ch-210)//2+167)*scale))
        if (((r&248)<<8)|((g&252)<<3)|(b>>3))==palette_color('Selected'):
            capture('10-fish-bite');tap(cw//2,(ch-210)//2+175);break
        time.sleep(.1)
    else:raise AssertionError('No fishing bite was observed')
    wait_for(lambda v:v['fish']==1,'fishing catch')
    # Rendering cadence and movement use the real WAMR event loop.
    key('RIGHT',1.0);time.sleep(10.1);capture('10-farm-after-play')
    key('ENTER');time.sleep(.5);durable=saved();capture('11-pause');stop()
    start();tap(cw//2,ch-38);key('ENTER');time.sleep(.4);again=saved()
    for k in ['day','gold','shipping','earned','harvested','fish','seeds','produce','plots']:
        assert durable[k]==again[k],(k,durable[k],again[k])
    capture('12-reloaded');stop()
    report.update(final=again,restart_preserved=True)
    (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print('PASS: actual simulator farming, paths, shop, overnight growth, shipping, pause and save/restart')
finally:stop()
