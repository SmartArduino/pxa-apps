#!/usr/bin/env python3
"""Replay real device clock timestamps against the unchanged C timing rule.

Install an ESP diagnostic package built with
PXA_APP_DEFINES=J3_TRACE_TIMING=1,J3_FORCE_SCALE_SHIFT=0 first. Trace logging is
compiled out of production packages; this is not a frame-rate benchmark.
Run from the PXA workspace. Stops this test application after each fresh run.
"""
import argparse,json,re,sys,time
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'tools/pxadb'))
import pxadb

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--holds',type=float,nargs='+',default=[.15,.36,.7,.82,1.2,2])
parser.add_argument('--allow-audio',action='store_true',help='Accept the game audio prompt on the 296x240 test display')
parser.add_argument('--unlock-swipe',type=int,nargs=4,help='Wake/unlock before testing: X1 Y1 X2 Y2')
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
identity='pxa-jump-jump-3d-cpp'
report={'port':args.port,'diagnostic':True,'fps_benchmark':False,'runs':[],'passed':False}
seen=set();logs=[]
with (args.output/'serial.jsonl').open('w',buffering=1) as output:
    with pxadb.PxaDbClient(args.port,15) as client:
        def capture(frame):
            key=(frame.kind,frame.payload)
            if key in seen:return
            seen.add(key);logs.append(frame.payload)
            output.write(json.dumps({'kind':frame.kind,'payload':frame.payload})+'\n')
        client.log_callback=capture
        def request(command):
            output.write(json.dumps({'command':command})+'\n')
            return client.request(command,timeout=60)
        def pump(seconds):
            end=time.monotonic()+seconds
            while time.monotonic()<end:client.pump_logs();time.sleep(.005)
        report['hello']=client.hello();report['info']=[f.payload for f in request('INFO')]
        assert not any('active=1' in f.payload for f in request('PACKAGES') if f.kind=='PKG'),'stop the current app before testing'
        client.subscribe_logs()
        running=False
        try:
            if args.unlock_swipe:
                request('INPUT KEY HOME');pump(.3)
                request('INPUT SWIPE '+' '.join(map(str,args.unlock_swipe))+' 400 8');pump(2)
            for index,hold in enumerate(args.holds):
                at=len(logs)
                request(f'PACKAGE run {identity}');running=True;pump(5)
                assert any('J3CPP ready ' in line for line in logs[at:]),'application not ready'
                if any('Runtime permission prompt queued:' in line and identity in line for line in logs[at:]):
                    assert args.allow_audio,'audio prompt needs --allow-audio for this test'
                    request('INPUT TAP 214 182');request('INPUT SYNC');pump(3)
                    # Accepting an overlay can also deliver a pointer edge to
                    # the game. Start a fresh ready scene after this setup.
                    request(f'PACKAGE stop {identity}');running=False;pump(1)
                    request(f'PACKAGE run {identity}');running=True;pump(5)
                at=len(logs)
                request('INPUT POINTER DOWN 148 120 0');request('INPUT SYNC');pump(hold)
                request('INPUT POINTER UP 148 120 0');request('INPUT SYNC');pump(2)
                screenshot=pxadb.screenshot_capture(request('SCREENSHOT JPEG'))
                pxadb.write_screenshot(screenshot,args.output/f'after-{index}.jpg')
                request(f'PACKAGE stop {identity}');running=False;pump(1)
                lines=logs[at:]
                ticks=[tuple(map(int,m.groups())) for line in lines
                    if (m:=re.search(r'J3CPP tick previous=(\d+) now=(\d+) steps=(\d+) charge_us=(\d+)',line))]
                inputs=[tuple(map(int,m.groups())) for line in lines
                    if (m:=re.search(r'J3CPP input phase=(\d+) now=(\d+) state=(\d+) charge_us=(\d+) vx_q6=(-?\d+) vy_q6=(-?\d+) vz_q6=(-?\d+) land_x_q6=(-?\d+) land_z_q6=(-?\d+)',line))]
                inputs=[sample for sample in inputs if sample[0]!=1] # regular held-pointer move samples
                assert len(inputs)==2 and ticks,(index,'missing press/release/tick trace',inputs)
                down,up=inputs
                assert down[2]==1 and up[2]==2,(index,'press/release did not charge/fly',inputs)
                total=0
                for previous,now,count,charge_us in ticks:
                    # This is the original pxa_clock_tick_steps formula. The
                    # native suite also calls the actual C helper and core.
                    expected=1 if previous==0 else min(2,(now-previous+10000)//20000) if now>previous else 0
                    assert count==expected,(previous,now,count,expected)
                    total+=expected
                    assert abs(charge_us-total*20000)<=10,(index,'charge diverged',charge_us,total)
                charge=total*.02
                vx=min(15,7*charge);vy=min(18,13.5+1.5*charge)
                land_x=vx*2*vy/72
                assert abs(up[3]-charge*1e6)<=10
                assert abs(up[4]-vx*1e6)<=20 and abs(up[5]-vy*1e6)<=20
                assert up[6]==0 and abs(up[7]-land_x*1e6)<=30 and up[8]==0
                row={'requested_hold_seconds':hold,'device_hold_us':up[1]-down[1],
                    'clock_callbacks':len(ticks),'original_steps':total,
                    'charge_us':up[3],'vx_q6':up[4],'vy_q6':up[5],'land_x_q6':up[7],
                    'all_callbacks_match_C':True,'screenshot':f'after-{index}.jpg'}
                report['runs'].append(row);print(json.dumps(row),flush=True)
            report['passed']=True
        finally:
            request('INPUT CANCEL')
            if running:request(f'PACKAGE stop {identity}')
            (args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
assert report['passed']
