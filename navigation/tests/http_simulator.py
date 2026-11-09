#!/usr/bin/env python3
"""End-to-end HTTP fixture for a PXA_NAV_TEST_ORIGIN simulator build.

Only fake test credentials and illustrative coordinates are used. This does not
validate real AMap credentials, actual roads or beaconDB coverage.
"""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import threading
import time
from urllib.parse import urlparse,parse_qs
import zlib
from PIL import Image,ImageDraw

p=argparse.ArgumentParser()
p.add_argument('--package',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--port',type=int,default=18787)
p.add_argument('--width',type=int,default=480)
p.add_argument('--height',type=int,default=480)
p.add_argument('--runner',type=Path,default=Path('build/simulator/pai-touch/pxsys_product_simulator'))
p.add_argument('--key',type=Path,default=Path('deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'))
a=p.parse_args();out=a.output.resolve()
if '/local/artifacts/' not in str(out):raise SystemExit('Output must be under local/artifacts/')
origin=f'http://127.0.0.1:{a.port}'
assert origin.encode() in (a.package/'artifacts/main.linux-x86_64.aot').read_bytes(),'Build with PXA_NAV_TEST_ORIGIN='+origin
out.mkdir(parents=True,exist_ok=True)
points=[(114.05,22.54),(114.051,22.54),(114.052,22.54),(114.052,22.541),(114.052,22.542),(114.053,22.542),(114.054,22.542)]
calls=[];requests=[];fixes=0
map_fail=False
manual_started=threading.Event();manual_release=threading.Event()
poll_started=threading.Event();poll_release=threading.Event()
map_hold=threading.Event();map_started=threading.Event();map_release=threading.Event()
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        global fixes
        u=urlparse(self.path);q=parse_qs(u.query);calls.append(u.path);requests.append({'path':u.path,'query':q})
        kind='application/json';status=200
        if u.path=='/fix':
            i=min(fixes,len(points)-1);fixes+=1
            if fixes==1:
                manual_started.set();manual_release.wait(8)
            if fixes==3:
                poll_started.set();poll_release.wait(8)
            body={'lat':points[i][1],'lng':points[i][0],'accuracy':8,'age_ms':0,'datum':'wgs84','sample_id':f'fixture:{2 if fixes==3 else fixes}'}
        elif u.path=='/v3/assistant/coordinate/convert':
            body={'status':'1','locations':q['locations'][0]}
        elif u.path=='/v3/place/text':
            body={'status':'1','pois':[{'name':f'测试终点 {i+1}','location':f'{114.054+i*.00001:.6f},22.542000','adname':'测试区域','address':'仅用于接口联调'} for i in range(5)]}
        elif u.path.startswith('/v5/direction/'):
            body={'status':'1','route':{'paths':[{'distance':'634','cost':{'duration':'480'},'steps':[
                {'instruction':'沿测试道路直行，左转','road':'测试道路 A','distance':'205','polyline':';'.join(f'{x:.6f},{y:.6f}' for x,y in points[:3])},
                {'instruction':'左转后直行，右转','road':'测试道路 B','distance':'222','polyline':';'.join(f'{x:.6f},{y:.6f}' for x,y in points[2:5])},
                {'instruction':'右转到达测试终点','road':'测试道路 C','distance':'207','polyline':';'.join(f'{x:.6f},{y:.6f}' for x,y in points[4:])}]}]}}
        elif u.path=='/v3/staticmap':
            if map_fail:
                self.send_response(503);self.send_header('Content-Length','0');self.end_headers();return
            if map_hold.is_set() and not map_started.is_set():
                map_started.set();map_release.wait(8)
            width,height=map(int,q['size'][0].split('*'))
            image=Image.new('RGB',(width,height),'#e6eee8');draw=ImageDraw.Draw(image)
            for x in range(0,width,32):draw.line((x,0,x,height),fill='#cddbd2')
            for y in range(0,height,32):draw.line((0,y,width,y),fill='#cddbd2')
            draw.text((8,height-18),'HTTP TEST FIXTURE - NOT A REAL MAP',fill='#304a3a')
            encoded=io.BytesIO();image.save(encoded,'PNG');body=encoded.getvalue();kind='image/png'
        else:status=404;body={'error':'unknown fixture path'}
        if not isinstance(body,bytes):body=json.dumps(body,ensure_ascii=False).encode()
        self.send_response(status);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(body)));self.end_headers()
        try:self.wfile.write(body)
        except (BrokenPipeError,ConnectionResetError):pass
server=ThreadingHTTPServer(('127.0.0.1',a.port),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
state=out/'state';storage=state/'app-data'/(hashlib.sha256(a.key.read_bytes()).hexdigest()+'-pxa-navigation');storage.mkdir(parents=True,exist_ok=True)
values={'amap.key':b'0'*32,'location.url':(origin+'/fix').encode(),'location.source':struct.pack('<I',1),'search.city':b''}
body=struct.pack('<H',len(values))
for k,v in sorted(values.items()):body+=bytes([len(k)])+k.encode()+struct.pack('<H',len(v))+v
header=b'PXKV'+struct.pack('<HHQII',1,0,1,len(body),zlib.crc32(body))
(storage/'.pxa-kv-a').write_bytes(header+body)
sock=f'/tmp/pxa-navigation-http-{os.getpid()}.sock'
def command(text,png=None):
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as c:
        c.settimeout(10);c.connect(sock);c.sendall((text+'\n').encode());f=c.makefile('rb');h=f.readline()
        if png:
            assert h.startswith(b'PNG '),h;n=int(h.split()[1]);data=f.read(n);assert len(data)==n;(out/png).write_bytes(data);return data
        assert h.startswith(b'OK'),(text,h)
def tap(x,y):command(f'TAP {x} {y}');time.sleep(.35)
def map_pixels(png):
    return Image.open(io.BytesIO(png)).crop((20,80,a.width-20,max(81,a.height-120))).tobytes()
def drag(x,y,dx,dy):
    command(f'POINTER DOWN {x} {y}')
    for i in range(1,13):command(f'POINTER MOVE {x+int(dx*i/12)} {y+int(dy*i/12)}');time.sleep(.03)
    command(f'POINTER UP {x+dx} {y+dy}');time.sleep(.6)
def until(predicate,seconds=10):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        if process.poll() is not None:raise RuntimeError('Simulator exited')
        if predicate():return
        time.sleep(.1)
    raise AssertionError('Timed out')
env=os.environ.copy();env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='none',PXA_SIMULATOR_PERF='0')
with (out/'simulator.log').open('w') as log:
    arguments=[str(a.runner.resolve()),'--package',str(a.package.resolve()),'--publisher-key',str(a.key.resolve()),'--state-root',str(state),
        '--pxadb-control-socket',sock,'--width',str(a.width),'--height',str(a.height),'--density-dpi','160','--locale','zh-CN']
    def launch():return subprocess.Popen(arguments,stdout=log,stderr=subprocess.STDOUT,env=env)
    process=launch()
    try:
        until(lambda:Path(sock).exists() and 'Navigation: ready' in (out/'simulator.log').read_text(errors='replace'))
        w,h=a.width,a.height;compact=h<400;m=10 if compact else 16
        px=w-m-12-(55 if compact else 75);py=h-m-30
        time.sleep(.5);tap(50,py)
        tap(w-40,120) if compact else tap(w//2,253)
        until(lambda:manual_started.is_set())
        tap(20,24);before=command('SCREENSHOT AFTER_PRESENT','manual-position-wait.png')
        command(f'POINTER DOWN {w//2} {100 if compact else 150}')
        command(f'POINTER MOVE {w//2+27} {112 if compact else 162}');time.sleep(.25)
        during=command('SCREENSHOT AFTER_PRESENT','manual-position-drag.png')
        assert map_pixels(before)!=map_pixels(during),'Manual position request blocked map dragging'
        command(f'POINTER UP {w//2+27} {112 if compact else 162}')
        manual_release.set();time.sleep(.8);tap(50,py)
        tap(w//2,159 if compact else 182);tap(100,72);command('TEXT 终点');tap(w-38,72)
        until(lambda:'/v3/place/text' in calls);time.sleep(.5);command('SCREENSHOT AFTER_PRESENT','search.png')
        if compact:
            # Start the swipe on a selectable result, as a person would. It
            # must scroll rather than accidentally select the touched row.
            drag(w//2,h-40,0,-240);command('SCREENSHOT AFTER_PRESENT','search-scrolled.png');tap(w//2,h-35)
        else:tap(w//2,133)
        maps=calls.count('/v3/staticmap');tap(w//2,h-28)
        until(lambda:any(v.startswith('/v5/direction/') for v in calls) and calls.count('/v3/staticmap')>maps);time.sleep(.8)
        route_request=next(v for v in requests if v['path'].startswith('/v5/direction/'))
        assert route_request['query']['destination']==[('114.054040' if compact else '114.054000')+',22.542000'],route_request
        planned_size=[v['query']['size'] for v in requests if v['path']=='/v3/staticmap'][-1]
        first=command('SCREENSHOT AFTER_PRESENT','planned-map.png')
        tap(px,py);until(lambda:fixes>=2);time.sleep(.8)
        second=command('SCREENSHOT AFTER_PRESENT','live-map.png');assert first!=second
        until(lambda:poll_started.is_set(),20)
        before=command('SCREENSHOT AFTER_PRESENT','poll-before-drag.png')
        command(f'POINTER DOWN {w//2} {105 if compact else 160}')
        command(f'POINTER MOVE {w//2+32} {117 if compact else 172}')
        time.sleep(.25)
        during=command('SCREENSHOT AFTER_PRESENT','poll-during-drag.png')
        assert map_pixels(before)!=map_pixels(during),'Background position request blocked map dragging'
        poll_release.set();command(f'POINTER UP {w//2+32} {117 if compact else 172}')
        time.sleep(.5);command('SCREENSHOT AFTER_PRESENT','live-progress.png')
        assert calls.count('/v3/assistant/coordinate/convert')==2,'Repeated sample consumed another coordinate-conversion request'
        tap(px,h-48 if compact else py)
        maps=calls.count('/v3/staticmap');center=[v['query']['location'][0] for v in requests if v['path']=='/v3/staticmap'][-1]
        map_hold.set();drag(w//2,80 if compact else 112,30,10)
        until(lambda:map_started.is_set())
        before=command('SCREENSHOT AFTER_PRESENT','map-loading-before.png')
        command(f'POINTER DOWN {w//2} {100 if compact else 160}')
        command(f'POINTER MOVE {w//2+27} {113 if compact else 173}')
        time.sleep(.25)
        during=command('SCREENSHOT AFTER_PRESENT','map-loading-drag.png')
        assert map_pixels(before)!=map_pixels(during),'Basemap download blocked the next drag'
        command(f'POINTER UP {w//2+27} {113 if compact else 173}')
        map_release.set()
        until(lambda:calls.count('/v3/staticmap')>=maps+2);time.sleep(.6)
        assert [v['query']['location'][0] for v in requests if v['path']=='/v3/staticmap'][-1]!=center
        assert [v['query']['size'] for v in requests if v['path']=='/v3/staticmap'][-1]==planned_size,'Stopping navigation left the bitmap at the live viewport size'
        command('SCREENSHOT AFTER_PRESENT','map-dragged.png')
        # Alternate away from the starting zoom, including zoom 17. A clamped
        # tap intentionally has no HTTP job, so every step here stays in range.
        maps=calls.count('/v3/staticmap')
        zoom=int([v['query']['zoom'][0] for v in requests if v['path']=='/v3/staticmap'][-1])
        first_delta=-1 if zoom==17 else 1
        for i in range(9):
            delta=first_delta if i%2==0 else -first_delta
            point=(w-67 if delta>0 else w-29,h-153) if compact else (w-m-19,105 if delta>0 else 139)
            tap(*point);until(lambda:calls.count('/v3/staticmap')>=maps+i+1);time.sleep(.2)
        base_zoom=int([v['query']['zoom'][0] for v in requests if v['path']=='/v3/staticmap'][-1])
        map_started.clear();map_release.clear()
        preview=command('SCREENSHOT AFTER_PRESENT','before-rapid-zoom.png')
        rapid_delta=-1 if base_zoom>=15 else 1
        control=(w-67 if rapid_delta>0 else w-29,h-153) if compact else (w-m-19,105 if rapid_delta>0 else 139)
        tap(*control);until(lambda:map_started.is_set());tap(*control);tap(*control)
        during=command('SCREENSHOT AFTER_PRESENT','rapid-zoom-preview.png')
        assert map_pixels(preview)!=map_pixels(during),'Zoom did not preview while the bitmap request was blocked'
        target_zoom=max(1,min(17,base_zoom+3*rapid_delta));map_release.set()
        until(lambda:[v['query']['zoom'][0] for v in requests if v['path']=='/v3/staticmap'][-1]==str(target_zoom));time.sleep(.5)
        map_fail=True
        minus=(w-29,h-153) if compact else (w-m-19,139)
        tap(*minus);time.sleep(.5);command('SCREENSHOT AFTER_PRESENT','map-failed.png')
        maps=calls.count('/v3/staticmap');map_fail=False;tap(95,74)
        until(lambda:calls.count('/v3/staticmap')>maps);time.sleep(.6);command('SCREENSHOT AFTER_PRESENT','map-retried.png')
        expected=[v['query'] for v in requests if v['path']=='/v3/staticmap'][-1]
        time.sleep(2.7)
        process.terminate();process.wait(10);map_count=calls.count('/v3/staticmap');process=launch()
        until(lambda:calls.count('/v3/staticmap')>map_count);time.sleep(.6)
        restored=[v['query'] for v in requests if v['path']=='/v3/staticmap'][-1]
        assert restored['location']==expected['location'] and restored['zoom']==expected['zoom'],(expected,restored)
        command('SCREENSHOT AFTER_PRESENT','reopened-view.png')
        assert not any(word in (out/'simulator.log').read_text(errors='replace') for word in ('Exception:', 'PXA task failed', 'map frame rejected'))
        print(f'PASS: {w}x{h}, HTTP search, result scrolling, route, PNG decode, live fixes, map drag, rapid zoom, offline retry, saved viewport')
    finally:
        (out/'requests.json').write_text(json.dumps(requests,ensure_ascii=False,indent=2))
        process.terminate()
        try:process.wait(10)
        except subprocess.TimeoutExpired:process.kill();process.wait()
        manual_release.set();poll_release.set();map_release.set();server.shutdown();server.server_close();Path(sock).unlink(missing_ok=True)
