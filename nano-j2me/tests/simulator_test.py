#!/usr/bin/env python3
"""Build an original MIDlet and exercise the signed PXA guest in the real simulator.
Run from the workspace root. All generated material belongs in local/artifacts.
"""
import argparse
import os
import re
import json
from pathlib import Path
import shutil
import socket
import subprocess
import time
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--package', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--runner', type=Path, default=Path('build/simulator/doit-gamekit-1/pxsys_product_simulator'))
p.add_argument('--key', type=Path, default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
p.add_argument('--width', type=int, default=320)
p.add_argument('--height', type=int, default=240)
p.add_argument('--density', type=int, default=160)
p.add_argument('--round', action='store_true')
a = p.parse_args()
out = a.output.resolve()
if '/local/artifacts/' not in str(out):
    raise SystemExit('Use an output directory under local/artifacts/')
out.mkdir(parents=True, exist_ok=True)
app = Path(__file__).resolve().parents[1]
classes = out / 'classes'
classes.mkdir(exist_ok=True)
subprocess.run(['python3', str(app/'upstream/tools/midp_stubs.py'), '--out', str(out/'stubs'), '--quiet'], check=True, capture_output=True)
subprocess.run(['javac', '-source', '8', '-target', '8', '-nowarn', '-cp', str(out/'stubs/classes'), '-d', str(classes), str(app/'tests/PxaProbe.java')], check=True)
jar = out / 'A中文Probe.jar'
with zipfile.ZipFile(jar, 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\r\nMIDlet-Name: PXA Probe\r\nMIDlet-Version: 1.0\r\nMIDlet-Vendor: PXA\r\nMIDlet-1: PXA Probe,,PxaProbe\r\n\r\n')
    for f in classes.glob('*.class'):
        z.write(f, f.name)
state = out / f'state-{os.getpid()}'
state.mkdir(exist_ok=True)
sock = f'/tmp/pxa-j2me-{os.getpid()}.sock'
process = log = None
logpath = None

def command(text, png=None):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
        s.settimeout(15)
        s.connect(sock)
        s.sendall((text+'\n').encode())
        stream = s.makefile('rb')
        header = stream.readline()
        if png:
            assert header.startswith(b'PNG '), header
            size = int(header.split()[1])
            data = stream.read(size)
            assert len(data) == size
            (out/png).write_bytes(data)
        else:
            assert header.startswith(b'OK'), (text, header)

def wait_for(text, occurrences=1):
    for _ in range(150):
        if process.poll() is not None:
            raise AssertionError(logpath.read_text())
        if logpath.read_text(errors='replace').count(text) >= occurrences:
            return
        time.sleep(.1)
    raise AssertionError(f'Missing {text!r}:\n{logpath.read_text()}')

def start(number):
    global process, log, logpath
    logpath = out/f'simulator-{number}.log'
    log = logpath.open('w')
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', PXA_SIM_NAV_MODE='none')
    process = subprocess.Popen([str(a.runner.resolve()), '--package', str(a.package.resolve()), '--publisher-key', str(a.key.resolve()), '--state-root', str(state), '--pxadb-control-socket', sock, '--width', str(a.width), '--height', str(a.height), '--density-dpi', str(a.density)]+(['--round'] if a.round else []), env=env, stdout=log, stderr=subprocess.STDOUT)
    wait_for('J2ME library jars=')

def stop():
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(10)
        except subprocess.TimeoutExpired:
            process.kill(); process.wait()
    if log:
        log.close()
    Path(sock).unlink(missing_ok=True)

def key(name):
    command('KEY_DOWN '+name);time.sleep(.1)
    command('KEY_UP '+name);time.sleep(.15)

def geometry():
    matches = re.findall(r'J2ME UI canvas=(\d+)x(\d+) safe=(\d+),(\d+),(\d+),(\d+) view=(\d+),(\d+),(\d+),(\d+)',logpath.read_text())
    assert matches
    return tuple(map(int,matches[-1]))

def display_point(x,y):
    cw,ch,*_ = geometry()
    scale=max(1,min(a.width//cw,a.height//ch))
    return ((a.width-cw*scale)//2+int(x*scale), (a.height-ch*scale)//2+int(y*scale))

def touch(phase,number,x,y):
    x,y=display_point(x,y)
    command(f'TOUCH {phase} {number} {x} {y}')

def tap(x,y):
    touch('DOWN',0,x,y);time.sleep(.1);touch('UP',0,x,y);time.sleep(.25)

def keys(*names):
    for name in names:key(name)

def back():
    command('KEY BACK');time.sleep(.2)

def pause_settings():
    keys('ENTER','DOWN','Z')


try:
    start(0)
    command('SCREENSHOT AFTER_PRESENT', 'empty-library.png')
    keys('DOWN','Z',*(['DOWN']*6),'Z',*(['DOWN']*5),'Z')
    command('SCREENSHOT AFTER_PRESENT','java-memory.png')
    keys('DOWN','DOWN','Z');back()
    roots = list((state/'media').iterdir())
    assert len(roots) == 1, roots
    shutil.copyfile(jar, roots[0]/jar.name)
    (roots[0]/'ZBroken.jar').write_bytes(b'invalid archive')
    key('S');wait_for('J2ME library jars=2')
    command('SCREENSHOT AFTER_PRESENT', 'library.png')
    key('Z');wait_for('PXA_PROBE READY');wait_for('PXA_PROBE RMS_OLD=0');wait_for('PXA_PROBE CANVAS=240x320');wait_for('heap=3072KiB')
    wait_for('J2ME system font ready:');wait_for('PXA PCM source=16000Hz/1ch')
    from PIL import Image
    for _ in range(100):
        command('SCREENSHOT AFTER_PRESENT', 'midlet.png')
        im=Image.open(out/'midlet.png').convert('RGB')
        cw,ch,sx,sy,sw,sh,vx,vy,vw,vh=geometry()
        point=display_point(vx+40*vw/240,vy+110*vh/320)
        red,green,blue=im.getpixel(point)
        if green>150 and red<120:break
        time.sleep(.1)
    else:raise AssertionError(('MIDlet pixels missing',point,red,green,blue))
    key('UP');wait_for('PXA_PROBE KEY_DOWN=-1');wait_for('PXA_PROBE KEY_UP=-1')
    # Auto touch mode consumes the first contact to reveal its cached controls.
    tap(vx+vw/2,vy+vh/2);tap(vx+vw/2,vy+vh/2)
    wait_for('PXA_PROBE POINTER_DOWN=');wait_for('PXA_PROBE POINTER_UP=')
    # Expand the numeric pad; press 5 and 4 with independent contacts.
    top_w=max(34,min(58,sw//4));tap(sx+3+top_w//2,sy+15)
    k=max(18,min(40,min(sw,sh)*27//240));kw=max(22,min(40,k+5));kh=max(20,min(36,k))
    nx=sx+sw-(kw*3+6)-4;ny=max(sy+sh-(kh*4+9)-4,sy+max(22,min(30,k))+7)
    five=(nx+kw+3+kw//2,ny+kh+3+kh//2);four=(nx+kw//2,five[1])
    touch('DOWN',1,*five);time.sleep(.1);touch('DOWN',2,*four);time.sleep(.1)
    touch('UP',2,*four);time.sleep(.1);touch('UP',1,*five);time.sleep(.1)
    wait_for('PXA_PROBE KEY_DOWN=53');wait_for('PXA_PROBE KEY_UP=53')
    wait_for('PXA_PROBE KEY_DOWN=52');wait_for('PXA_PROBE KEY_UP=52')
    command('SCREENSHOT AFTER_PRESENT','numeric-controls.png')
    pause_settings();keys(*(['DOWN']*4),'Z')
    command('SCREENSHOT AFTER_PRESENT','mapping.png')
    # Map physical A from FIRE (17) to numeric 5 (6), leaving the menu chord.
    keys(*(['DOWN']*4),*(['LEFT']*11))
    # Capture the actual R button and bind it to numeric 9 through the picker.
    keys(*(['DOWN']*8),'Z','W','UP','UP','Z')
    back();key('X');key('Z')
    old=logpath.read_text().count('PXA_PROBE KEY_DOWN=53')
    key('Z');wait_for('PXA_PROBE KEY_DOWN=53',old+1)
    key('W');wait_for('PXA_PROBE KEY_DOWN=57');wait_for('PXA_PROBE KEY_UP=57')
    # Running resolution changes explicitly restart the MIDlet.
    pause_settings();key('Z');keys(*(['DOWN']*4),'Z')
    command('SCREENSHOT AFTER_PRESENT','resolution.png')
    keys(*(['DOWN']*8),'Z')
    wait_for('resolution=320x240');wait_for('PXA_PROBE RMS_OLD=1');wait_for('PXA_PROBE CANVAS=320x240')
    command('SCREENSHOT AFTER_PRESENT','landscape-game.png')
    # Stop normally through the pause menu; malformed archives recover to UI.
    keys('ENTER',*(['DOWN']*4),'Z');wait_for('J2ME returned result=0',2)
    keys('DOWN','Z');wait_for('J2ME returned result=1')
    command('SCREENSHOT AFTER_PRESENT','bad-jar-recovered.png')
    assert 'PXA_PROBE ERROR=' not in logpath.read_text()
    stop();start(1);key('Z');wait_for('PXA_PROBE RMS_OLD=2');wait_for('resolution=320x240')
    key('Z');wait_for('PXA_PROBE KEY_DOWN=53')
    keys('ENTER',*(['DOWN']*4),'Z');wait_for('J2ME returned result=0')
    assert len(list((state/'private-files').rglob('*.rms'))) == 1
    assert len(list((state/'private-files').rglob('*.njp'))) >= 1
    (out/'result.json').write_text(json.dumps({'passed':True,'width':a.width,'height':a.height,'density':a.density},indent=2))
    print('PASS: adaptive native menus; floating numeric multitouch; gamepad remapping; resolution restart; malformed JAR recovery; profile and RMS process persistence')
finally:
    stop()
