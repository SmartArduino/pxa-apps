#!/usr/bin/env python3
"""Synthetic original documents on two origins; no site-specific adapters."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
import json
from pathlib import Path
import struct
import threading
import time
from urllib.parse import urlsplit, parse_qs
from PIL import Image


def portrait():
    image = Image.new('RGB', (1199, 2533))
    image.putdata([(25+x*190//1199, 20+y*210//2533, 205-y*130//2533)
                   for y in range(2533) for x in range(1199)])
    encoded = BytesIO(); image.save(encoded, format='JPEG', quality=88)
    data = encoded.getvalue()
    padding = (b'\xff\xe1'+struct.pack('>H',50002)+b'\0'*50000)*5
    return data[:2]+padding+data[2:]


class Fixture:
    def __init__(self, bind='127.0.0.1', port=0, events=None, image_chunk_delay=0):
        self.events = []
        self.events_path = events
        self.image = portrait()
        fixture = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args): pass
            def reply(self, body, mime='text/html; charset=utf-8', headers=(), code=200,
                      chunk_delay=0):
                if isinstance(body,str): body=body.encode()
                self.send_response(code)
                self.send_header('Content-Type',mime)
                self.send_header('Content-Length',str(len(body)))
                for k,v in headers:self.send_header(k,v)
                self.end_headers()
                if chunk_delay:
                    for at in range(0,len(body),4096):
                        self.wfile.write(body[at:at+4096]);self.wfile.flush()
                        time.sleep(chunk_delay)
                else:self.wfile.write(body)
            def do_GET(self):
                route = urlsplit(self.path)
                if route.path == '/portrait.jpg':
                    case = parse_qs(route.query).get('case',['default'])[0]
                    host = self.headers['Host'].rsplit(':',1)[0]
                    origin = f'http://{host}:{fixture.page.server_port}'
                    expected = '' if case in ('none','element') else origin+'/' if case=='default' else origin+f'/{case}?query=keep'
                    actual = self.headers.get('Referer','')
                    event=dict(case=case,passed=actual==expected,referrer=actual,
                               started_monotonic_ns=time.monotonic_ns())
                    fixture.events.append(event)
                    if fixture.events_path:
                        Path(fixture.events_path).write_text(json.dumps(fixture.events,indent=2))
                    try:
                        self.reply(fixture.image,'image/jpeg',code=200 if actual==expected else 403,
                                   chunk_delay=image_chunk_delay)
                    except (BrokenPipeError,ConnectionResetError):
                        event['client_aborted']=True
                    finally:
                        event['completed_monotonic_ns']=time.monotonic_ns()
                        if fixture.events_path:
                            Path(fixture.events_path).write_text(json.dumps(fixture.events,indent=2))
                elif route.path == '/loader.js':
                    self.reply("document.getElementById('status').textContent='Original page script OK';",'application/javascript')
                elif route.path[1:] in ('default','meta','header','none','element'):
                    case=route.path[1:];host=self.headers['Host'].rsplit(':',1)[0]
                    meta = '<meta name=referrer content=no-referrer-when-downgrade>' if case=='meta' else ''
                    policy = 'no-referrer' if case=='none' else 'unsafe-url'
                    headers = [('Referrer-Policy',policy)] if case in ('header','none','element') else []
                    attribute = ' referrerpolicy=no-referrer' if case=='element' else ''
                    self.reply(f'''<!doctype html><html><head>{meta}<title>Original document</title></head><body>
<p id=status>Original page</p><img width=1199 height=2533 alt="Synthetic portrait"{attribute}
src="http://{host}:{fixture.images.server_port}/portrait.jpg?case={case}">
<p>Original footer</p><script>var s=document.createElement('script');s.src='/loader.js';document.head.appendChild(s);</script>
</body></html>''',headers=headers)
                else:self.reply('missing',code=404)
        self.page=ThreadingHTTPServer((bind,port),Handler)
        self.images=ThreadingHTTPServer((bind,port+1 if port else 0),Handler)
        self.threads=[]
    def start(self):
        for server in (self.page,self.images):
            t=threading.Thread(target=server.serve_forever,daemon=True);t.start();self.threads.append(t)
        return self
    def close(self):
        for server in (self.page,self.images):server.shutdown();server.server_close()
        for thread in self.threads:thread.join()


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bind',default='127.0.0.1')
    p.add_argument('--port',type=int,default=31933);p.add_argument('--events',type=Path)
    p.add_argument('--image-chunk-delay-ms',type=float,default=0,
                   help='Delay between 4 KiB image writes for concurrent device stress.')
    a=p.parse_args()
    if a.image_chunk_delay_ms < 0:p.error('image chunk delay must be nonnegative')
    f=Fixture(a.bind,a.port,a.events,a.image_chunk_delay_ms/1000).start()
    print(f'Fixture page={f.page.server_port} images={f.images.server_port} bytes={len(f.image)}',flush=True)
    try:threading.Event().wait()
    except KeyboardInterrupt:pass
    finally:f.close()
