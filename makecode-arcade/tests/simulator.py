#!/usr/bin/env python3
"""Exercise a PXT64 cartridge in the real signed PXA product simulator."""
import argparse
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time
from PIL import Image

parser=argparse.ArgumentParser(description=__doc__)
for name in ['simulator','package','publisher-key','cartridge','output']:
    parser.add_argument('--'+name,type=lambda x:Path(x).resolve(),required=True)
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
state=args.output/'state'; state.mkdir(exist_ok=True)
control=Path('/tmp')/f'pxa-arcade-{os.getpid()}.sock'

def command(text):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(10);client.connect(str(control));client.sendall((text+'\n').encode())
        result=bytearray()
        while chunk:=client.recv(65536):result.extend(chunk)
    if result.startswith(b'ERR'):raise RuntimeError(result.decode())
    return result

def tap(x,y):
    command(f'TOUCH DOWN 0 {x} {y}');time.sleep(.08)
    command(f'TOUCH UP 0 {x} {y}');time.sleep(.2)

for stage in ['discover','play']:
    control.unlink(missing_ok=True)
    logfile=args.output/(stage+'.log')
    with logfile.open('w') as log:
        proc=subprocess.Popen([str(args.simulator),'--package',str(args.package),
            '--publisher-key',str(args.publisher_key),'--state-root',str(state),
            '--width','320','--height','240','--corner-radius','0',
            '--safe-insets','0,0,0,0','--pxadb-control-socket',str(control)],
            stdout=log,stderr=subprocess.STDOUT,
            env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
        try:
            limit=time.monotonic()+25
            while not control.exists():
                if proc.poll() is not None:raise RuntimeError(logfile.read_text())
                if time.monotonic()>limit:raise TimeoutError('Simulator startup')
                time.sleep(.05)
            time.sleep(1)
            if stage=='play':
                (args.output/'library.png').write_bytes(command('SCREENSHOT AFTER_PRESENT').partition(b'\n')[2])
                tap(100,73)
                time.sleep(6)
                (args.output/'playing.png').write_bytes(command('SCREENSHOT AFTER_PRESENT').partition(b'\n')[2])
                # Press the real gamepad input path and capture another frame.
                command('KEY_DOWN RIGHT');command('KEY_DOWN Z');time.sleep(1)
                command('KEY_UP Z');command('KEY_UP RIGHT')
                (args.output/'input.png').write_bytes(command('SCREENSHOT AFTER_PRESENT').partition(b'\n')[2])
            command('KEY HOME');proc.wait(timeout=10)
            assert proc.returncode==0,proc.returncode
        finally:
            if proc.poll() is None:proc.kill();proc.wait()
    logs=logfile.read_text()
    assert not re.search(r'Guest exception|callback failed|unreachable|out of bounds|EMU ERROR',logs,re.I),logs[-5000:]
    if stage=='discover':
        match=re.search(r'PXA SIM app-media-root=([^\r\n]+)',logs);assert match,logs
        destination=Path(match[1]);assert destination.is_relative_to(state)
        shutil.copyfile(args.cartridge,destination/'ping.pxt64')
    else:
        assert 'EMU READY name=ping.pxt64' in logs,logs[-5000:]
        assert 'EMU PERF system=ARCADE' in logs,logs[-5000:]
        if args.cartridge.name=='probe.pxt64':
            before=Image.open(args.output/'playing.png').convert('RGB')
            after=Image.open(args.output/'input.png').convert('RGB')
            assert before.getpixel((160,60))==(255,255,255)
            r,g,b=after.getpixel((160,60))
            assert r==255 and 24<=g<=40 and 24<=b<=40,'A button did not select the red probe background'
        print(logs[-5000:])
control.unlink(missing_ok=True)
print('PASS: signed PXA cartridge launch and input')
