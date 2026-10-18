#!/usr/bin/env python3
"""Measure actual LCD completion FPS; keep screenshots and MEMORY outside timing."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import sys
import time

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port',required=True)
p.add_argument('--baud',default='2000000')
p.add_argument('--serial',required=True)
p.add_argument('--package',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--samples',type=int,default=20)
p.add_argument('--level',type=int,choices=(1,2,3),default=2)
p.add_argument('--reuse-running',action='store_true',help='measure a fresh level transition in the existing foreground instance')
p.add_argument('--allow-audio',action='store_true',help='answer this app audio playback permission on the identified device')
p.add_argument('--inventory',action='store_true',help='also open and close the inventory after timing')
p.add_argument('--pxadb-dir',type=Path,default=Path(__file__).resolve().parents[4]/'tools/pxadb')
a=p.parse_args()
sys.path.insert(0,str(a.pxadb_dir));import pxadb
os.environ['PXADB_BAUD']=a.baud
a.output.mkdir(parents=True,exist_ok=True)
report={'package_sha256':hashlib.sha256(a.package.read_bytes()).hexdigest(),'samples':[], 'passed':False}
ready=False
title_ready=False
permission_pending=False
with (a.output/'serial.jsonl').open('w',buffering=1) as log, pxadb.PxaDbClient(a.port,15) as c:
    def record(kind,payload):
        global ready,title_ready,permission_pending
        log.write(json.dumps({'t':time.monotonic(),'kind':kind,'payload':payload})+'\n')
        if f'OpenLara ready level={a.level}' in payload: ready=True
        if 'OpenLara ready level=0' in payload:title_ready=True
        if 'Runtime permission prompt queued:' in payload and 'pxa-openlara' in payload:permission_pending=True
        if 'OpenLara' in payload: print(payload,flush=True)
    c.log_callback=lambda f:record(f.kind,f.payload)
    c.raw_callback=lambda b:record('RAW',b.decode('utf-8','replace'))
    def request(cmd):
        record('COMMAND',cmd);rows=c.request(cmd,timeout=30)
        for f in rows:
            if not cmd.startswith('SCREENSHOT') or f.kind!='DATA':record(f.kind,f.payload)
        return rows
    def pump(seconds):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            c.pump_logs();time.sleep(.01)
    def capture(name):
        shot=pxadb.screenshot_capture(request('SCREENSHOT JPEG'))
        pxadb.write_screenshot(shot,a.output/(name+'.jpg'))
    try:
        report['hello']=c.hello()
        if pxadb.info_properties(report['hello']).get('serial')!=a.serial:raise RuntimeError('Wrong device')
        report['packages_before']=[f.payload for f in request('PACKAGES') if f.kind=='PKG']
        if any('active=1' in row and ':pxa-openlara\t' not in row for row in report['packages_before']):
            raise RuntimeError('Another app is active')
        c.subscribe_logs();ready=title_ready=permission_pending=False
        if any(':pxa-openlara\t' in row and 'active=1' in row for row in report['packages_before']):
            try:request('PACKAGE stop pxa-openlara')
            except pxadb.PxaDbRejectedError as exc:
                if str(exc)!='package_not_running':raise
            pump(.3)
        request('INPUT KEY HOME');pump(.3)
        request('INPUT POINTER DOWN 160 185 0');request('INPUT SYNC');pump(.12)
        for y in (163,139,115,91,67,53):
            request(f'INPUT POINTER MOVE 160 {y} 0');request('INPUT SYNC');pump(.12)
        request('INPUT POINTER UP 160 53 0');request('INPUT SYNC');pump(.4)
        capture('before-launch')
        ready=title_ready=permission_pending=False
        request('MEMORY START');request('PACKAGE run pxa-openlara')
        # The full port boots to the original title. Wait for it before opening
        # the touch pause menu and explicitly choosing the benchmark level.
        title_end=time.monotonic()+100
        if a.reuse_running:
            pump(1);title_ready=True
            report['startup_boundary']='existing instance, level transition only'
        else:report['startup_boundary']='fresh instance from title startup'
        while not title_ready and time.monotonic()<title_end:
            pump(.2);request('PING')
            if permission_pending:
                if not a.allow_audio:raise RuntimeError('Allow audio on the device, or pass --allow-audio for this identified test device')
                capture('audio-permission');request('INPUT TAP 211 183');request('INPUT SYNC');permission_pending=False;pump(2)
            if title_ready:break
        if not title_ready:raise RuntimeError('Title did not become ready')
        capture('before-level' if a.reuse_running else 'title');pump(.5)
        request('INPUT TAP 180 220');pump(.4)
        selection={1:5,2:3,3:4}[a.level]
        request(f'INPUT TAP 160 {48+selection*20}');pump(.4)
        end=time.monotonic()+100;keep_awake=time.monotonic()+5
        while not ready and time.monotonic()<end:
            pump(.2)
            request('PING')
            if time.monotonic()>=keep_awake:
                request('INPUT TAP 70 170');keep_awake=time.monotonic()+5
        if not ready:raise RuntimeError('OpenLara did not become ready')
        pump(3)
        report['startup_memory']=[f.payload for f in request('MEMORY STOP') if f.kind=='DATA']
        capture('idle')
        request('MEMORY START');pump(1)
        for i in range(a.samples):
            if i%5==0:request('INPUT TAP 70 170') # neutral D-pad point, refresh wake timer
            pump(1)
            props={}
            for f in request('PERF FPS'):
                if f.kind in ('DATA','OK'):props.update(pxadb.info_properties(f.payload))
            report['samples'].append({'index':i,'properties':props})
        report['steady_memory']=[f.payload for f in request('MEMORY STOP') if f.kind=='DATA']
        fps=[int(s['properties']['presentation_fps_x10'])/10 for s in report['samples']]
        report['fps']={'min':min(fps),'median':statistics.median(fps),'max':max(fps),'mean':statistics.mean(fps)}
        print(json.dumps(report['fps']),flush=True)
        request('INPUT POINTER DOWN 120 170 0');request('INPUT SYNC');pump(1.2)
        request('INPUT POINTER UP 120 170 0');request('INPUT SYNC');pump(.3);capture('turned')
        request('INPUT POINTER DOWN 70 115 0');request('INPUT SYNC');pump(1.2)
        request('INPUT POINTER UP 70 115 0');request('INPUT SYNC');pump(.3);capture('walked')
        if a.inventory:
            for name in ('inventory','inventory-return'):
                request('INPUT POINTER DOWN 200 20 0');request('INPUT SYNC');pump(.15)
                request('INPUT POINTER UP 200 20 0');request('INPUT SYNC');pump(.8);capture(name)
        report['packages_active']=[f.payload for f in request('PACKAGES') if f.kind=='PKG']
        report['passed']=any(':pxa-openlara\t' in f and 'active=1' in f for f in report['packages_active']) and all(x>0 for x in fps)
    except Exception as exc:
        report['error']=str(exc);raise
    finally:
        for command in ('INPUT CANCEL','MEMORY STOP','PACKAGE stop pxa-openlara','INPUT KEY HOME'):
            try:request(command)
            except Exception as exc:record('CLEANUP_ERROR',str(exc))
        (a.output/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
if not report['passed']:raise SystemExit(1)
