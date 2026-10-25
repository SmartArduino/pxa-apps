#!/usr/bin/env python3
"""Exercise a signed port in the real PXA product simulator and save its screen."""
import argparse, json, os, signal, socket, subprocess, tempfile, time
from pathlib import Path

def command(sock, text):
    with socket.socket(socket.AF_UNIX) as client:
        client.settimeout(30); client.connect(str(sock)); client.sendall((text+'\n').encode())
        result=bytearray()
        while chunk:=client.recv(65536):result.extend(chunk)
    if result.startswith(b'ERR'):raise RuntimeError(result.decode())
    return bytes(result)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for arg in ('simulator','package','publisher-key','output'):p.add_argument('--'+arg,type=Path,required=True)
    p.add_argument('--seconds',type=float,default=4)
    p.add_argument('--width',type=int,default=320)
    p.add_argument('--height',type=int,default=240)
    p.add_argument('--tap',action='append',default=[])
    p.add_argument('--settle',type=float,default=0.6)
    p.add_argument('--drag',action='append',default=[])
    p.add_argument('--expect-log')
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pxa-port-') as tmp:
        sock=Path(tmp)/'control.sock';state=Path(tmp)/'state';state.mkdir()
        log=(args.output/'runtime.log').open('wb')
        env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
        proc=subprocess.Popen([str(args.simulator.resolve()),'--package',str(args.package.resolve()),
            '--publisher-key',str(args.publisher_key.resolve()),'--state-root',str(state),
            '--width',str(args.width),'--height',str(args.height),'--corner-radius','0','--pxadb-control-socket',str(sock)],
            env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        try:
            end=time.monotonic()+30
            while not sock.exists():
                if proc.poll() is not None:raise RuntimeError((args.output/'runtime.log').read_text())
                if time.monotonic()>end:raise TimeoutError('Simulator did not open its control socket')
                time.sleep(.05)
            time.sleep(args.seconds)
            for pair in args.tap:
                x,y=map(int,pair.split(','));command(sock,f'TOUCH DOWN 0 {x} {y}');time.sleep(.1)
                command(sock,f'TOUCH UP 0 {x} {y}');time.sleep(args.settle)
            for sequence in args.drag:
                x1,y1,x2,y2,hold=map(float,sequence.split(','))
                command(sock,f'TOUCH DOWN 0 {int(x1)} {int(y1)}');time.sleep(.1)
                command(sock,f'TOUCH MOVE 0 {int(x2)} {int(y2)}');time.sleep(hold)
                command(sock,f'TOUCH UP 0 {int(x2)} {int(y2)}');time.sleep(.3)
            payload=command(sock,'SCREENSHOT');header,png=payload.split(b'\n',1)
            assert header==f'PNG {len(png)}'.encode()
            (args.output/'screen.png').write_bytes(png)
            from PIL import Image
            import io
            colors=Image.open(io.BytesIO(png)).convert('RGB').getcolors(320*240)
            if not args.expect_log:assert colors and len(colors)>6, 'Blank simulator screen'
        finally:
            if proc.poll() is None:
                # Exercise normal PXA deactivation. SDL's SIGTERM handler
                # deletes LVGL input devices before the simulator's control
                # socket cleanup, leaving its injected touch pointers stale.
                try:
                    command(sock,'KEY HOME')
                    proc.wait(timeout=8)
                except (OSError,RuntimeError,subprocess.TimeoutExpired):
                    if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM)
            try:proc.wait(timeout=8)
            except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL);proc.wait()
            log.close()
        logs=(args.output/'runtime.log').read_text(errors='replace')
        assert proc.returncode==0, f'Simulator exited with {proc.returncode}:\n{logs}'
        if args.expect_log:assert args.expect_log in logs,logs
        assert 'Guest exception' not in logs and 'failed at' not in logs and 'callback failed' not in logs and 'surface create failed' not in logs,logs
        (args.output/'result.json').write_text(json.dumps({'passed':True,'png_bytes':len(png),'exit_code':proc.returncode},indent=2))
        print('Simulator passed:',args.output)

if __name__=='__main__':main()
