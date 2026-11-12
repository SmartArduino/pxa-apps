#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify signed landscape apps with the real PXA product simulator."""
import argparse
import concurrent.futures
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
from PIL import Image

APPS=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('pxa_simulator',APPS/'doom/tests/simulator.py')
protocol=importlib.util.module_from_spec(spec);spec.loader.exec_module(protocol)
parser=argparse.ArgumentParser(description=__doc__)
for name in ('simulator','packages','publisher-key','output'):
    parser.add_argument('--'+name,type=lambda s:Path(s).resolve(),required=True)
parser.add_argument('--apps',nargs='+',required=True)
parser.add_argument('--profiles',nargs='+',default=['480x854','854x480','480x480'])
parser.add_argument('--media',type=Path,help='Optional directory containing <app>/files media directories')
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
FAULT=re.compile(r'Guest exception|callback failed|surface create failed|failed at |event failed|unreachable|out of bounds',re.I)

def run(app,profile):
    w,h=map(int,profile.split('x'));out=args.output/(app+'-'+profile);out.mkdir()
    metadata=json.loads((APPS/app/'package.json').read_text())
    package=args.packages/('pxa-'+app)
    if not package.is_dir():package=args.packages/metadata['id']
    with tempfile.TemporaryDirectory(prefix='pxa-landscape-') as temporary:
        state=Path(temporary)/'state';state.mkdir();sock=Path(temporary)/'control.sock'
        def launch(label,seconds):
            log_path=out/(label+'.log')
            with log_path.open('w') as log:
                proc=subprocess.Popen([str(args.simulator),'--package',str(package),
                    '--publisher-key',str(args.publisher_key),'--state-root',str(state),
                    '--width',str(w),'--height',str(h),'--corner-radius','0',
                    '--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)],
                    stdout=log,stderr=subprocess.STDOUT,
                    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
                try:
                    limit=time.monotonic()+25
                    while not sock.exists():
                        if proc.poll() is not None:raise RuntimeError(log_path.read_text())
                        if time.monotonic()>limit:raise TimeoutError('Control socket unavailable')
                        time.sleep(.05)
                    time.sleep(seconds)
                    data=protocol.command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n')[2]
                    shot=Image.open(io.BytesIO(data))
                    if label=='run':
                        assert shot.size==(max(w,h),min(w,h)),f'Unexpected orientation: {shot.size}'
                        assert shot.convert('RGB').getbbox(),'Blank screen'
                        (out/'screen.png').write_bytes(data)
                        assert not FAULT.search(log_path.read_text()),log_path.read_text()[-3000:]
                    protocol.command(sock,'KEY HOME');proc.wait(timeout=10)
                    assert proc.returncode==0,f'Abnormal shutdown {proc.returncode}'
                    return log_path.read_text()
                finally:
                    if proc.poll() is None:proc.kill();proc.wait()
                    sock.unlink(missing_ok=True)
        media=args.media/app/'files' if args.media else None
        if media and media.is_dir():
            logs=launch('discover',.2)
            match=re.search(r'PXA SIM app-media-root=([^\r\n]+)',logs)
            assert match,'Host media binding missing'
            destination=Path(match[1])
            assert destination.parent==state/'media'
            if app=='freedoom':shutil.copyfile(media/'freedoom2.wad',destination/'freedoom2.wad')
            else:shutil.copytree(media,destination,dirs_exist_ok=True)
        logs=launch('run',3)
    return {'app':app,'native':profile,'logical':f'{max(w,h)}x{min(w,h)}','passed':True}

jobs=[(a,p) for a in args.apps for p in args.profiles]
results=[]
def case(job):
    try:
        result=run(*job);print('PASS',*job,flush=True);return result
    except Exception as error:
        print('FAIL',*job,str(error)[-500:],flush=True)
        return {'app':job[0],'native':job[1],'passed':False,'error':str(error)}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:results=list(pool.map(case,jobs))
(args.output/'results.json').write_text(json.dumps({'passed':all(r['passed'] for r in results),'cases':results},indent=2)+'\n')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
