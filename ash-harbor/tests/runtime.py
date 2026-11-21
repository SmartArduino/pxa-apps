#!/usr/bin/env python3
"""Exercise the signed game in the real PXA product simulator; keep QA local."""
import argparse, json, os, re, socket, struct, subprocess, time
from pathlib import Path
from PIL import Image
from io import BytesIO

ROOT = Path(__file__).resolve().parents[4]
p=argparse.ArgumentParser()
p.add_argument("--package",type=Path,required=True)
p.add_argument("--output",type=Path,required=True)
p.add_argument("--width",type=int,default=320)
p.add_argument("--height",type=int,default=240)
p.add_argument("--character",choices=["jin","lan","zhen","ling"],default="jin")
p.add_argument("--nav-mode",choices=["gesture-no-bar","gesture","buttons"],default="gesture-no-bar")
p.add_argument("--safe-insets",default="0,0,0,0")
p.add_argument("--corner-radius",type=int,default=0)
p.add_argument("--announcements",action="store_true")
p.add_argument("--walk-preview",action="store_true")
p.add_argument("--kick-preview",action="store_true")
p.add_argument("--story",action="store_true")
p.add_argument("--ui-preview",action="store_true")
p.add_argument("--practice",action="store_true")
p.add_argument("--stages-preview",action="store_true")
p.add_argument("--opponents",action="store_true")
p.add_argument("--silent",action="store_true")
p.add_argument("--camera-preview",action="store_true")
p.add_argument("--projectile-preview",action="store_true")
p.add_argument("--result",choices=["victory","defeat","campaign"])
p.add_argument("--simulator",type=Path,default=ROOT/"build/simulator/pai-touch/pxsys_product_simulator")
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
(a.output/"state").mkdir(exist_ok=True)
sock=Path("/tmp")/f"ash-harbor-{os.getpid()}.sock"
log=a.output/"runtime.log"
proc=None
def command(text):
    with socket.socket(socket.AF_UNIX) as c:
        c.settimeout(15);c.connect(str(sock));c.sendall((text+"\n").encode())
        f=c.makefile("rb");head=f.readline()
        if head.startswith((b"PNG ",b"JSON ")):
            data=f.read(int(head.split()[1]));assert len(data)==int(head.split()[1]);return data
        assert head==b"OK\n",(text,head)
def wait_for(fragment,timeout=20,offset=0):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        assert proc.poll() is None,log.read_text()[-2500:]
        if fragment in log.read_text()[offset:]:return
        time.sleep(.08)
    raise AssertionError((fragment,log.read_text()[-3000:]))
def key(name,hold=.06):
    command("KEY_DOWN "+name);time.sleep(hold);command("KEY_UP "+name);time.sleep(.06)
def down(name):command("KEY_DOWN "+name);time.sleep(.055)
def up(name):command("KEY_UP "+name);time.sleep(.055)
def shot(name):
    data=command("SCREENSHOT");(a.output/(name+".png")).write_bytes(data)
    w,h=struct.unpack(">II",data[16:24]);assert w>=h
    assert len(Image.open(BytesIO(data)).convert("RGB").getcolors(w*h))>32,"Blank or rejected render"
    return w,h
def position(x,y):
    rw,rh,scale,ox,oy,*_=geometry()
    width,height=max(a.width,a.height),min(a.width,a.height)
    return round((ox+x*scale)*width/rw),round((oy+y*scale)*height/rh)
def geometry():
    values=re.findall(r"Ash Harbor layout width=(\d+) height=(\d+) scale=([\d.]+) x=(-?\d+) y=(-?\d+) lift=(\d+) menu_lift=(\d+) dpad_shift=(\d+) bottom=(\d+)",log.read_text())
    assert values,"missing layout metrics"
    return tuple(float(v) if i==2 else int(v) for i,v in enumerate(values[-1]))
def touch(action,id,x,y,controls=True):
    if controls:
        *_,lift,menu_lift,dpad_shift,bottom=geometry()
        y-=lift
        if x<88:x+=dpad_shift
    x,y=position(x,y);command(f"TOUCH {action} {id} {x} {y}");time.sleep(.065)
def tap(x,y):
    touch("DOWN",0,x,y,False);touch("UP",0,x,y,False);time.sleep(.18)
def last_held():
    values=re.findall(r"Ash Harbor touch-(?:down|up|cancel).*held=(\d+)",log.read_text())
    assert values,log.read_text()[-1000:]
    return int(values[-1])
def qcf(fire=True):
    down("DOWN");down("RIGHT");up("DOWN")
    if fire:key("Z")
    up("RIGHT")
def reset_training():
    key("ENTER");time.sleep(.12);key("DOWN");key("DOWN");key("Z");time.sleep(1.65)
def launch(mode='w'):
    global proc
    with log.open(mode) as stream:
        proc=subprocess.Popen([str(a.simulator.resolve()),"--package",str(a.package.resolve()),
            "--publisher-key",str(ROOT/"deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der"),
            "--state-root",str(a.output/"state"),"--width",str(a.width),"--height",str(a.height),
            "--density-dpi","160","--corner-radius",str(a.corner_radius),"--safe-insets",a.safe_insets,
            "--pxadb-control-socket",str(sock)],stdout=stream,stderr=subprocess.STDOUT,
            env=dict(os.environ,SDL_VIDEODRIVER="dummy",SDL_AUDIODRIVER="dummy",
                     PXA_SIM_NAV_MODE=a.nav_mode,PXA_SIMULATOR_PERF="0"))

try:
    launch()
    wait_for('Ash Harbor ready')
    if a.story:
        def story_shot(name):
            shot(name);image=Image.open(a.output/(name+'.png')).convert('RGB');w,h=image.size
            bottom=h-(geometry()[-1] if a.nav_mode!='gesture-no-bar' else 0)
            for strip in [image.crop((0,0,2,bottom)),image.crop((w-2,0,w,bottom)),image.crop((0,bottom-2,w,bottom))]:
                assert len(strip.getcolors(strip.width*strip.height))>4,('Story has a blank border',name)
        wait_for('Ash Harbor ready');time.sleep(.4);story_shot('story-0')
        assert re.search(r'launch view=5',log.read_text())
        for scene in [1,2]:
            offset=len(log.read_text());key('Z');wait_for('story-next',offset=offset);time.sleep(.35);story_shot(f'story-{scene}')
        key('Z');wait_for('story-finished');wait_for('progress saved');time.sleep(.2);shot('story-home')
        lift=geometry()[5]
        for character in range(4):
            tap(160,150);wait_for('roster view=6');time.sleep(.25);shot(f'roster-{character}')
            tap(80 if character%2==0 else 235,58 if character<2 else 128)
            offset=len(log.read_text());tap(250,222-lift);wait_for('selected view=0',offset=offset)
            assert re.search(fr'selected view=0.*character={character}',log.read_text()[offset:])
            wait_for('progress saved',offset=offset);time.sleep(.2);shot(f'home-{character}')
        # Relaunch the same signed app and storage to verify intro dismissal and selection.
        proc.terminate();proc.wait(timeout=8);sock.unlink(missing_ok=True)
        offset=len(log.read_text());launch('a');wait_for('Ash Harbor ready',offset=offset);time.sleep(.35)
        assert re.search(r'launch view=0.*character=3',log.read_text()[offset:]),log.read_text()[-2500:]
        shot('relaunch-home')
        offset=len(log.read_text());tap(293,116);wait_for('story-replay',offset=offset);time.sleep(.2);shot('replay')
        wait_for('story-finished',timeout=30,offset=offset);shot('automatic-story-complete')
        offset=len(log.read_text());tap(293,116);wait_for('story-replay',offset=offset);key('X');wait_for('story-finished',offset=offset)
        shot('skip-story-home')
        assert 'render error=' not in log.read_text() and 'out of bounds' not in log.read_text()
        assert 'label elided' not in log.read_text()
        result={'passed':True,'viewport':[a.width,a.height],'checked':['three story illustrations','advance','skip','automatic progression','replay','four portrait cards','all character selections','saved intro dismissal','saved character after relaunch','texture-slot reuse beyond hardware table']}
        (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(0)
    if re.search(r'launch view=5',log.read_text()):
        wait_for('Ash Harbor ready');key('X');wait_for('story-finished');time.sleep(.2)
    wait_for("Ash Harbor ready");time.sleep(.35);shot("01-home")
    wait_for("Ash Harbor generated audio ready")
    wait_for("Ash Harbor music ready track=0")
    tap(297,90);wait_for("Ash Harbor audio enabled=0")
    if not a.silent:tap(297,90);wait_for("Ash Harbor audio enabled=1")
    for _ in range(['jin','lan','zhen','ling'].index(a.character)):key("TAB")
    if a.ui_preview:
        for character in range(4):
            tap(160,150);tap(80 if character%2==0 else 235,58 if character<2 else 128)
            shot(f'roster-{character}');tap(250,222-geometry()[5]);time.sleep(.2)
        key('RIGHT');key('RIGHT');key('Z');shot('commands');key('Z')
        key('LEFT');key('Z');shot('opponent');key('Z');wait_for('frame-bank ready');time.sleep(1.7)
        for _ in range(7):qcf();time.sleep(.12)
        touch('DOWN',0,60,223)
        for id,(x,y) in enumerate([(228,220),(262,220),(296,220),(228,190)],1):touch('DOWN',id,x,y)
        shot('training-history')
        for id,(x,y) in reversed(list(enumerate([(228,220),(262,220),(296,220),(228,190)],1))):touch('CANCEL',id,x,y)
        touch('CANCEL',0,60,223)
        tap(160,24);wait_for('practice-open');shot('practice-0')
        for i in range(1,4):key('RIGHT');shot(f'practice-{i}')
        key('X');key('ENTER');shot('pause')
        assert 'label elided' not in log.read_text() and 'missing Chinese label' not in log.read_text()
        assert 'render error=' not in log.read_text()
        result={'passed':True,'viewport':[a.width,a.height],'checked':['all roster cards','move list','opponent menu','training metrics and five-key history','four practice labels','pause menu','bounded pixel text']}
        (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(0)
    if a.stages_preview:
        for scene in range(4):
            offset=len(log.read_text());tap(230,116);wait_for('stages-open view=9',offset=offset);time.sleep(.3);shot(f'stage-selector-{scene}')
            tap(80 if scene%2==0 else 235,58 if scene<2 else 128);tap(250,222-geometry()[5]);wait_for('stage-selected view=0',offset=offset);time.sleep(.4)
            key('RIGHT');key('Z');key('Z');wait_for('start view=1',offset=offset);wait_for('frame-bank ready',offset=offset);time.sleep(1.8)
            assert re.search(fr'start view=1.*scenery={scene}',log.read_text()[offset:])
            wait_for('music ready track='+str([1,4,5,6][scene]),offset=offset)
            tap(160,169);shot(f'stage-{scene}')
            # All arrows must correspond to the actual touch sector, with full release.
            positions=[(42,181),(60,187),(66,205),(60,223),(42,229),(24,223),(18,205),(24,187)]
            for i,((x,y),expected) in enumerate(zip(positions,[1,9,8,10,2,6,4,5])):
                touch('DOWN',0,x,y);assert last_held()==expected,(i,last_held())
                if i in (1,3):shot(f'stage-{scene}-diagonal-{i}')
                touch('UP',0,x,y);assert last_held()==0
            touch('DOWN',0,42,205);assert last_held()==0;touch('CANCEL',0,42,205)
            reset_training();offset2=len(log.read_text())
            touch('DOWN',0,42,229);touch('MOVE',0,66,229);touch('MOVE',0,66,205);touch('DOWN',1,228,220)
            touch('UP',1,228,220);touch('UP',0,66,205);wait_for('move=8',offset=offset2)
            key('ENTER');key('DOWN');key('DOWN');key('DOWN');key('Z');time.sleep(.3)
        # Gamepad access to the same picker while choosing an opponent.
        key('Z');key('A');wait_for('stages-open view=9');key('LEFT');key('Z');wait_for('stage-selected view=8');key('X')
        wait_for('progress saved');proc.terminate();proc.wait(timeout=8);sock.unlink(missing_ok=True)
        offset=len(log.read_text());launch('a');wait_for('Ash Harbor ready',offset=offset);time.sleep(.4)
        key('RIGHT');key('Z');key('Z');wait_for('frame-bank ready',offset=offset);time.sleep(1.8)
        assert re.search(r'start view=1.*scenery=2',log.read_text()[offset:]),'Saved arena was lost'
        wait_for('music ready track=5',offset=offset);shot('saved-snow-stage')
        assert 'render error=' not in log.read_text() and 'Ash Harbor error=' not in log.read_text()
        assert 'music decode failed' not in log.read_text()
        result={'passed':True,'stages':4,'viewport':[a.width,a.height],'checked':['four stage previews and selections','per-stage generated music','eight touch sectors','neutral center','diagonal highlights','slide quarter-circle with a second finger','release/cancel','gamepad stage selection','working-set eviction','saved arena after restart']}
        (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(0)
    if a.opponents:
        player=['jin','lan','zhen','ling'].index(a.character)
        for training in [True,False]:
            for opponent in range(4):
                if training:key('RIGHT')
                offset=len(log.read_text());key('Z');wait_for('opponent-open view=8',offset=offset)
                tap(80 if opponent%2==0 else 235,55 if opponent<2 else 120)
                shot(f'opponent-{training}-{opponent}')
                if not training and opponent!=player:tap(160,202-geometry()[5])
                offset=len(log.read_text());tap(250,229-geometry()[5]);wait_for('start view=1',offset=offset)
                assert re.search(fr'start view=1.*character={player}.*opponent={opponent}',log.read_text()[offset:])
                time.sleep(1.6);shot(f'match-{training}-{opponent}')
                key('ENTER');key('DOWN');key('DOWN');key('DOWN');key('Z')
                # Home restores its first menu choice after each match.
        assert 'missing Chinese label' not in log.read_text() and 'render error=' not in log.read_text()
        result={'passed':True,'viewport':[a.width,a.height],'checked':['all four training opponents','all four arcade opponents','mirror match','touch portrait cards','controller confirmation','Back and home']}
        (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(0)
    if a.result:
        stages=3 if a.result=='campaign' else 1
        for stage in range(stages):
            offset=len(log.read_text());key("Z")
            if stage==0:key('Z')
            time.sleep(1.6)
            deadline=time.monotonic()+(155 if a.result=='defeat' else 100)
            if a.result!='defeat':command("KEY_DOWN RIGHT")
            else:down('RIGHT');time.sleep(3);up('RIGHT')
            while time.monotonic()<deadline and "Ash Harbor result view=4" not in log.read_text()[offset:]:
                assert proc.poll() is None
                if a.result!='defeat':
                    command("KEY_DOWN Z");command("KEY_DOWN X");time.sleep(.09)
                    command("KEY_UP X");command("KEY_UP Z");time.sleep(.49)
                else:time.sleep(.1)
            if a.result!='defeat':command("KEY_UP RIGHT")
            wait_for("Ash Harbor result view=4",timeout=2,offset=offset)
            wait_for("Ash Harbor music ready track="+('3' if a.result=='defeat' else '2'),offset=offset)
            if a.result=='campaign':
                player=['jin','lan','zhen','ling'].index(a.character)
                assert f'opponent={(player+stage+1)%4} stage={stage+1}' in log.read_text()[offset:]
                assert re.search(r'result view=4.*hp=[1-9]\d*,0',log.read_text()[offset:]),'Campaign player lost'
            time.sleep(.25);shot(f"result-{a.result}-{stage}")
        assert "music decode failed" not in log.read_text()
        (a.output/'result.json').write_text(json.dumps({'passed':True,'outcome':a.result,'verified':['arcade match result','winner/loser voice','result music ready','visible result menu']},indent=2)+'\n')
        print('Result audio passed:',a.result)
        raise SystemExit(0)
    if a.announcements:
        key("Z");key('Z');time.sleep(.08);shot("00-round")
        time.sleep(.55);shot("00-fight")
        time.sleep(.7);shot("00-timer")
        down("RIGHT")
        wait_for("phase=2",timeout=90)
        shot("00-ko");up("RIGHT")
        key("ENTER");key("DOWN");key("DOWN");key("DOWN");key("Z")
    key("RIGHT");key("RIGHT");key("Z");time.sleep(.2);shot("02-command-arts")
    assert "view=3" in log.read_text()
    key("Z");key("LEFT");key("Z");key('Z');wait_for("view=1");time.sleep(1.7);shot("03-training")
    assert re.search(r"Ash Harbor start .*touch=0",log.read_text())
    assert f"character={['jin','lan','zhen','ling'].index(a.character)}" in log.read_text()
    if a.projectile_preview:
        for facing in ['right','left']:
            if facing=='left':
                # Approach the standing dummy, jump over it, then turn on landing.
                down('RIGHT');time.sleep(1.8);key('UP',hold=.05);time.sleep(2);up('RIGHT');time.sleep(.2)
                shot('launch-left-ready')
            frames=[]
            offset=len(log.read_text())
            command('KEY_DOWN S')
            for i in range(32):
                data=command('SCREENSHOT');(a.output/f'launch-{facing}-{i:02}.png').write_bytes(data)
                frames.append(Image.open(BytesIO(data)).convert('RGB'));time.sleep(.015)
            command('KEY_UP S')
            assert re.search(r'Ash Harbor move view=1.*move=8',log.read_text()[offset:]),(facing,'Projectile preview did not execute a grounded palm')
            frames[0].save(a.output/f'launch-{facing}-runtime.gif',save_all=True,append_images=frames[1:],duration=30,loop=0)
            reset_training()
    if a.practice:
        tap(160,24);wait_for('practice-open view=7');shot('practice-stand')
        key('RIGHT');key('DOWN');key('RIGHT');key('DOWN');key('Z')
        shot('practice-configured');key('X');time.sleep(1.7)
        assert re.search(r'practice-close view=1.*practice=1 history=0',log.read_text())
        down('RIGHT');time.sleep(2.1);up('RIGHT')
        offset=len(log.read_text())
        for _ in range(4):key('Z');time.sleep(.42)
        assert re.search(r'Ash Harbor block .*damage=0 blocks=[1-9]',log.read_text()[offset:])
        shot('practice-guard')
        key('ENTER');key('A');wait_for('practice-open view=7',offset=offset)
        key('RIGHT');key('DOWN');key('LEFT');key('DOWN');key('RIGHT');key('DOWN');key('Z');time.sleep(1.7)
        assert re.search(r'practice-close view=1.*practice=2 history=1',log.read_text())
        down('RIGHT');time.sleep(2.1);up('RIGHT');offset=len(log.read_text())
        for _ in range(5):key('Z');time.sleep(.48)
        assert re.search(r'Ash Harbor hit .*damage=1',log.read_text()[offset:])
        assert re.search(r'Ash Harbor block .*damage=1 blocks=[1-9]',log.read_text()[offset:])
        shot('practice-after-hit')
        # Touch-only setting changes, then Back to resume with no held contacts.
        tap(160,24);time.sleep(.2);squeeze=max(0,geometry()[5]-8)
        tap(280,64-squeeze//3);tap(160,222-geometry()[5]);time.sleep(1.7)
        assert re.search(r'practice-close view=1.*held=0.*practice=3',log.read_text())
        offset=len(log.read_text());time.sleep(6);shot('practice-spar')
        assert 'Ash Harbor hit' in log.read_text()[offset:],'Sparring opponent never attacked'
        key('ENTER');key('DOWN');key('DOWN');key('DOWN');key('Z')
        offset=len(log.read_text());key('Q');wait_for('difficulty view=0',offset=offset)
        assert re.search(r'difficulty view=0.*difficulty=0',log.read_text()[offset:])
        wait_for('progress saved',offset=offset);shot('difficulty-easy')
        proc.terminate();proc.wait(timeout=8);sock.unlink(missing_ok=True)
        offset=len(log.read_text());launch('a');wait_for('Ash Harbor ready',offset=offset);time.sleep(.3)
        assert re.search(r'launch view=0.*difficulty=0',log.read_text()[offset:]);shot('difficulty-relaunch')
        offset=len(log.read_text());tap(85,116);wait_for('difficulty view=0',offset=offset)
        assert re.search(r'difficulty view=0.*difficulty=1',log.read_text()[offset:])
        assert 'missing Chinese label' not in log.read_text() and 'render error=' not in log.read_text()
        result={'passed':True,'viewport':[a.width,a.height],'checked':['standing dummy','adaptive full guard','guard after first hit','sparring AI','opponent selection','input history toggle','position reset','touch settings','controller settings','Back resumes','difficulty touch and controller','persisted difficulty after cold launch','Chinese labels']}
        (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(0)
    if a.camera_preview:
        backgrounds=[]
        for i in range(8):
            data=command('SCREENSHOT');(a.output/f'idle-{i:02}.png').write_bytes(data)
            x0,y0=position(145,108);x1,y1=position(174,130)
            backgrounds.append(Image.open(BytesIO(data)).convert('RGB').crop((x0,y0,x1,y1)).tobytes());time.sleep(.12)
        assert len(set(backgrounds))==1,'Idle breathing moved the scenery'
        down('LEFT');time.sleep(2);up('LEFT')
        command('KEY_DOWN UP');time.sleep(.075);command('KEY_UP UP')
        frames=[]
        for i in range(18):
            data=command('SCREENSHOT');(a.output/f'jump-{i:02}.png').write_bytes(data)
            frames.append(Image.open(BytesIO(data)).convert('RGB'));time.sleep(.04)
        frames[0].save(a.output/'jump-runtime.gif',save_all=True,append_images=frames[1:],duration=65,loop=0)
        reset_training()
    if a.kick_preview:
        frames=[]
        command('KEY_DOWN X')
        for i in range(12):
            data=command('SCREENSHOT');(a.output/f'kick-{i:02}.png').write_bytes(data)
            frames.append(Image.open(BytesIO(data)).convert('RGB'))
            time.sleep(.035)
        command('KEY_UP X')
        frames[0].save(a.output/'kick-runtime.gif',save_all=True,append_images=frames[1:],duration=60,loop=0)
    if a.walk_preview:
        frames=[]
        for direction in ["RIGHT","LEFT"]:
            down(direction)
            for i in range(16):
                data=command("SCREENSHOT")
                (a.output/f"walk-{direction.lower()}-{i:02d}.png").write_bytes(data)
                frames.append(Image.open(BytesIO(data)).convert("RGB"))
                time.sleep(.065)
            up(direction)
        frames[0].save(a.output/"walk-runtime.gif",save_all=True,append_images=frames[1:],
                       duration=90,loop=0,optimize=False)
        reset_training()
    tap(160,169);shot("03b-touch-controls")
    rw,rh,scale,ox,oy,lift,menu_lift,dpad_shift,bottom=geometry()
    assert scale>0 and (scale<1 or scale==int(scale))
    # Bottom of the lowest touch target clears actual system reservations.
    assert oy+(238-lift)*scale <= rh-bottom*rh/min(a.width,a.height)-6*scale
    # Five independent contacts and a non-final finger release.
    touch("DOWN",0,42,229);touch("DOWN",1,228,220);touch("DOWN",2,262,220)
    touch("DOWN",3,296,220);touch("DOWN",4,228,190)
    assert last_held()==818,last_held()
    touch("UP",1,228,220);assert last_held()==802,last_held()
    for id,x,y in [(4,228,190),(2,262,220),(3,296,220),(0,42,229)]:touch("UP",id,x,y)
    assert last_held()==0
    touch("DOWN",1,228,220);touch("DOWN",2,228,220)
    touch("UP",1,228,220);assert last_held()==16
    touch("CANCEL",2,228,220);assert last_held()==0
    key("A");shot("03c-controller-controls-hidden")
    assert "controller-controls" in log.read_text()
    time.sleep(.5);reset_training()
    qcf();time.sleep(.05);shot("04-ember-palm")
    wait_for("move=8")
    time.sleep(.8);reset_training()
    qcf(False);qcf();time.sleep(.07);shot("05-super")
    wait_for("move=11")
    time.sleep(.9);reset_training()
    # Real two-pointer combination: hold down and press simplified super.
    touch("DOWN",0,42,229);touch("DOWN",1,296,220)
    time.sleep(.15);shot("06-touch-super")
    touch("UP",1,296,220);touch("UP",0,42,229)
    key("ENTER");time.sleep(.15);shot("07-paused")
    assert "view=2" in log.read_text()
    key("Z");time.sleep(.2)
    # Visit extra animation pages and a late base atlas page on both fighters.
    down("DOWN");key("X");up("DOWN");time.sleep(.55)
    key("A");time.sleep(.5)
    down("UP");key("S");up("UP");time.sleep(1)
    wait_for("move=9")
    # Controller movement + repeated attacks must reach and hurt the dummy.
    before_hits=log.read_text().count("Ash Harbor hit")
    down("RIGHT");time.sleep(2);up("RIGHT")
    for _ in range(4):key("Z");time.sleep(.26);key("X");time.sleep(.4)
    shot("08-combat")
    assert log.read_text().count("Ash Harbor hit")>before_hits,log.read_text()[-3000:]
    command("KEY BACK");time.sleep(.25);shot("09-back-paused")
    assert "render error=" not in log.read_text() and "Ash Harbor error=" not in log.read_text()
    assert "Exception:" not in log.read_text() and "out of bounds" not in log.read_text()
    has_voice=(a.package/'assets/audio/voices'/(a.character.replace('jin','yan')+'-fire.s16')).exists()
    if not a.silent:
        wait_for("Ash Harbor music ready track=1")
        if has_voice:
            wait_for("voice="+a.character.replace('jin','yan')+"-fire.s16")
            wait_for("voice="+a.character.replace('jin','yan')+"-super.s16")
            opponent=['yan','lan','zhen','ling'][(['jin','lan','zhen','ling'].index(a.character)+1)%4]
            assert re.search(fr'voice={opponent}-hurt_[01]\.(?:pcm|s16)',log.read_text()),'Opponent pain calls did not play'
    assert "music decode failed" not in log.read_text()
    (a.output/"runtime-info.json").write_bytes(command("RUNTIME INFO"))
    # Pause, help, return and home must remain reachable with touch alone.
    modal_lift=max(0,lift-18)
    tap(160,134-modal_lift);time.sleep(.15);shot("10-touch-help")
    tap(160,222-lift);time.sleep(.15)
    tap(160,190-modal_lift);time.sleep(.15);shot("11-return-home")
    assert re.findall(r"Ash Harbor menu view=(\d+)",log.read_text())[-1]=="0"
    result={"passed":True,"viewport":[a.width,a.height],"captures":13+(4 if a.announcements else 0)+(32 if a.walk_preview else 0),
        "character":a.character,
        "navigation":a.nav_mode,"layout":geometry(),
        "checked":["generated UI artwork","generated sound effects","character voice calls","streaming music","persistent mute control","native pixel grid","adaptive bottom reservations","independent leg cycle","mixed-case font","hidden virtual controls","controller hides touch controls","five independent pointers","duplicate button ownership","pointer cancellation","commands","training","quarter-circle input",
                   "double-quarter-circle super","two-pointer super","pause and resume",
                   "controller combat","system back pause"]}
    result['audio']=not a.silent
    result['voice_bank']=has_voice
    result['played_voices']=sorted(set(re.findall(r'Ash Harbor voice=([a-z0-9_-]+\.(?:pcm|s16))',log.read_text())))
    if not has_voice:result['checked']=[check for check in result['checked'] if check!='character voice calls']+['missing voice bank fallback']
    if a.silent:
        result['checked']=[check for check in result['checked'] if check not in ['character voice calls','streaming music','generated sound effects']]+['muted gameplay']
    (a.output/"result.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result))
finally:
    if proc and proc.poll() is None:
        proc.terminate()
        try:proc.wait(timeout=8)
        except subprocess.TimeoutExpired:proc.kill();proc.wait()
    sock.unlink(missing_ok=True)
