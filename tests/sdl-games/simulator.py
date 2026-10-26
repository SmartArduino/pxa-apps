#!/usr/bin/env python3
"""Exercise an SDL guest inside the actual PXA product simulator.

Output goes to an explicit ignored artifact directory. Input uses the simulator
controller injector; it is not a physical controller or device performance test.
"""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import tempfile
import time


def command(path, text):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(20)
        client.connect(str(path))
        client.sendall((text+'\n').encode())
        parts=[]
        while part:=client.recv(65536):
            parts.append(part)
    data=b''.join(parts)
    if data.startswith(b'ERR'):
        raise RuntimeError(data.decode())
    return data


def run(args):
    args.output.mkdir(parents=True, exist_ok=True)
    state=args.output/'state'
    state.mkdir(exist_ok=True)
    result={'input':'PXA simulator controller injection', 'hardware_test':False,
            'metric':'SDL presentation calls; not LCD completion FPS',
            'resolution':[args.width,args.height], 'purpose':'graceful exit' if args.expect_exit else 'gameplay', 'passed':False}
    with tempfile.TemporaryDirectory(prefix='sdl-pxa-') as temp, (args.output/'runtime.log').open('wb') as log:
        path=Path(temp)/'control.sock'
        env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',
                 PXA_SIMULATOR_PERF='0',PXA_SIM_NAV_MODE='gesture-no-bar')
        proc=subprocess.Popen([str(args.simulator.resolve()),'--package',str(args.package.resolve()),
            '--publisher-key',str(args.publisher_key.resolve()),'--state-root',str(state.resolve()),
            '--width',str(args.width),'--height',str(args.height),'--corner-radius','0',
            '--pxadb-control-socket',str(path)],stdout=log,stderr=subprocess.STDOUT,
            env=env,start_new_session=True)
        try:
            deadline=time.monotonic()+40
            while not path.exists():
                if proc.poll() is not None:raise RuntimeError('Simulator exited during startup')
                if time.monotonic()>deadline:raise TimeoutError('Simulator control socket')
                time.sleep(.1)
            def shot(name):
                header,png=command(path,'SCREENSHOT AFTER_PRESENT').split(b'\n',1)
                assert header==f'PNG {len(png)}'.encode()
                (args.output/(name+'.png')).write_bytes(png)
            time.sleep(args.startup_wait)
            shot('00-startup')
            sequence=json.loads(args.sequence.read_text()) if args.sequence else []
            for number,step in enumerate(sequence,1):
                if 'until_log' in step:
                    deadline=time.monotonic()+step.get('timeout',45)
                    while step['until_log'] not in (args.output/'runtime.log').read_text(errors='replace'):
                        if time.monotonic()>deadline:raise TimeoutError('Waiting for '+step['until_log'])
                        command(path,'KEY_DOWN '+step.get('repeat_key','Z'))
                        time.sleep(.12)
                        command(path,'KEY_UP '+step.get('repeat_key','Z'))
                        time.sleep(step.get('interval',.8))
                if 'key' in step:
                    command(path,'KEY_DOWN '+step['key'])
                    time.sleep(step.get('hold',.12))
                    command(path,'KEY_UP '+step['key'])
                if 'command' in step:command(path,step['command'])
                time.sleep(step.get('wait',.5))
                if args.expect_exit and proc.poll() is not None:break
                shot(f'{number:02}-{step.get("label","input")}')
            if args.expect_exit:
                proc.wait(timeout=15)
            else:
                time.sleep(args.duration)
                shot('99-final')
                command(path,'KEY HOME')
                proc.wait(timeout=15)
            assert proc.returncode==0,f'Simulator exit {proc.returncode}'
            text=(args.output/'runtime.log').read_text(errors='replace')
            for marker in ('Guest exception','callback failed','CORE SAFETY STOP','failed at','must be RGB565','Game exited with status','SDL game returned status=1','The game stopped after an engine error.','PXA SDL assertion:','Failed to render texture','cannot create texture'):
                assert marker not in text,marker
            assert 'Starting ' in text,'Guest did not start'
            if args.expect_exit:
                assert 'PXA SDL game returned status=0' in text,'Game did not return normally'
            else:
                assert 'PXA SDL frames=' in text,'No SDL frames/audio sampling'
            result['samples']=[dict(zip(('frames','elapsed_us','submit_us','audio_frames','linear_bytes'),map(int,m)))
                for m in re.findall(r'PXA SDL frames=(\d+) elapsed_us=(\d+) submit_us=(\d+) audio_frames=(\d+) linear_bytes=(\d+)',text)]
            result['pcm']=[dict(zip(('submitted','accepted','queued'),map(int,m))) for m in re.findall(r'PXA SDL PCM submitted=(\d+) accepted=(\d+) queued=(\d+)',text)]
            if args.require_audio:assert any(x['accepted']>0 for x in result['pcm']),'No PCM accepted by backend'
            result['passed']=True
        except Exception as exc:
            result['error']=str(exc)
        finally:
            if proc.poll() is None:
                os.killpg(proc.pid,signal.SIGTERM)
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait()
    (args.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,ensure_ascii=False))
    return 0 if result['passed'] else 1


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('simulator','package','publisher-key','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--expect-exit',action='store_true')
    parser.add_argument('--require-audio',action='store_true')
    parser.add_argument('--sequence',type=Path)
    parser.add_argument('--startup-wait',type=float,default=5)
    parser.add_argument('--duration',type=float,default=10)
    parser.add_argument('--width',type=int,default=320)
    parser.add_argument('--height',type=int,default=240)
    raise SystemExit(run(parser.parse_args()))
