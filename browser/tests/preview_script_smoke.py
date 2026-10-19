#!/usr/bin/env python3
"""Real AOT UI/Wasm: callbacks continue under a stable, unobscured image fit."""
import argparse
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
from pathlib import Path
import struct
import threading
import time
import zlib
from PIL import Image,ImageChops
from harness import Browser

p=argparse.ArgumentParser(description=__doc__)
for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--width',type=int,default=296);p.add_argument('--height',type=int,default=240)
p.add_argument('--dpi',type=int,default=160);p.add_argument('--round',action='store_true');a=p.parse_args()
release=threading.Event();requested=threading.Event()
def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
def pixel(x,y):
    if y<4:return bytes((255,0,0))
    if y>=92:return bytes((0,255,0))
    if x<4:return bytes((0,0,255))
    if x>=92:return bytes((255,150,0))
    return bytes((30,30,30) if (x//6+y//6)%2 else (255,255,255))
raw=b''.join(b'\0'+b''.join(pixel(x,y) for x in range(96))for y in range(96))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',96,96,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        if self.path=='/':
            body=b"<img src='/image.png'><script>fetch('/wait').then(()=>{document.body.innerHTML='<p>Deferred projection updated</p>';return fetch('/done')});</script>";kind='text/html'
        elif self.path=='/image.png':body=png;kind='image/png'
        else:
            if self.path=='/wait':release.wait(20)
            if self.path=='/done':requested.set()
            body=b'{}';kind='application/json'
        self.send_response(200);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(body)));self.end_headers()
        try:self.wfile.write(body)
        except (BrokenPipeError,ConnectionResetError):pass
server=ThreadingHTTPServer(('127.0.0.1',0),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();b=None
try:
    b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,
              a.width,a.height,a.dpi,58,round_screen=a.round)
    b.wait(lambda:'BROWSER METRICS' in b.logs());b.navigate(f'http://127.0.0.1:{server.server_port}/')
    b.wait(lambda:'BROWSER JS READY' in b.logs());b.wait(lambda:'BROWSER IMAGE DONE' in b.logs())
    x,y,w,h=b.box('content');b.tap(x+w//2,y+min(48,h//2))
    b.wait(lambda:'BROWSER IMAGE VIEW' in b.logs());b.shot('fit-before-callback')
    before=Image.open(a.output/'fit-before-callback.png').convert('RGB')
    side=min(w,h);left=x+(w-side)//2;top=y+(h-side)//2
    # Surface/canvas rounding can move an edge by a few pixels at fractional DPI.
    tolerance=max(3,(3*a.dpi+159)//160)
    colors=lambda center:[before.getpixel((left+side//2,yy))
                          for yy in range(max(y,center-tolerance),min(y+h,center+tolerance+1))]
    assert any(r>180 and g<80 and blue<80 for r,g,blue in colors(top+1)),'top border obscured'
    assert any(g>180 and r<80 and blue<80 for r,g,blue in colors(top+side-2)),'bottom border obscured'
    mark=b.logs().count('BROWSER JS UPDATE revision=')
    release.set();b.wait(requested.is_set,timeout=10);time.sleep(.6);b.shot('fit-after-callback')
    after=Image.open(a.output/'fit-after-callback.png').convert('RGB')
    crop=(x,y,x+w,y+h)
    assert not ImageChops.difference(before.crop(crop),after.crop(crop)).getbbox(),'callback invalidated preview pixels'
    assert b.logs().count('BROWSER JS UPDATE revision=')==mark,'DOM projected while preview owns image indices'
    b.nav(0);b.wait(lambda:b.logs().count('BROWSER JS UPDATE revision=')>mark);b.shot('deferred-projection')
    assert b.logs().count('BROWSER IMAGE DONE')==1,'preview fetched or decoded again'
    assert 'Guest exception' not in b.logs() and 'BROWSER JS ERROR' not in b.logs()
    (a.output/'result.json').write_text(json.dumps({'passed':True,'screen':[a.width,a.height,a.dpi],
        'round':a.round,'callbacks_in_preview':True,'stable_preview':True,'deferred_projection':True,'both_borders_visible':True},indent=2))
    print('Image fit borders/callbacks/deferred projection passed')
finally:
    release.set()
    if b:b.close()
    server.shutdown()
