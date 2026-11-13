"""Original synthetic images and request traps for responsive-image checks."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import urlsplit
from comic_fixture import picture

SIZE=" width=240 height=360 alt='响应式漫画页'"
PAGES={
    'picture':"<picture><source type=image/webp srcset='/blocked-type 1x'>"
              "<source type=image/jpeg srcset='/page1.jpg 640w'><img src='/blocked-fallback.svg'"+SIZE+"></picture>",
    'srcset':"<img src='/unused.jpg' srcset='/invalid.jpg 0w, /blocked.webp 1x, /page2.png 2x, /unused-large.jpg 3x'"+SIZE+">",
    'lazy':"<img class=lazy src='/loading.gif' data-src='javascript:bad' data-original='/page1.jpg'"+SIZE+">",
    'lazyset':"<img src=about:blank data-srcset='/blocked.avif 1x, /cdn,page2.png 640w'"+SIZE+">",
    'script':"<img id=image src=about:blank"+SIZE+"><script>document.getElementById('image').srcset='/page2.png 2x';</script>",
    'script-picture':"<picture><source id=choice type=image/webp srcset='/blocked-type 1x'>"
                     "<img src=about:blank"+SIZE+"></picture><script>"
                     "let s=document.getElementById('choice');s.type='image/jpeg';s.srcset='/page3.jpg 1x';</script>",
    'linked':"<a href='/target'><picture><source type=image/png srcset='/page2.png 1x'>"
             "<img src='/blocked-fallback.svg'"+SIZE+"></picture></a>",
    'limits':"<img src='/page3.jpg' srcset='"+"/blocked.webp 1x, "*32+"/late.jpg 1x'"+SIZE+">",
}

class Fixture:
    def __init__(self,bind='127.0.0.1',port=0):
        self.counts=Counter();fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                path=urlsplit(self.path).path;fixture.counts[path]+=1
                name=path.lstrip('/');mime='text/html; charset=utf-8';status=200
                if name in PAGES:body='<title>响应式图片</title><p>图片回退 · '+name+'</p>'+PAGES[name]
                elif name=='target':body='<h1>Picture link PASS</h1>'
                elif name in ('page1.jpg','page2.png','page3.jpg','cdn,page2.png'):
                    body=picture('page2.png' if name=='cdn,page2.png' else name)
                    mime='image/png' if name.endswith('.png') else 'image/jpeg'
                else:body='Unexpected request';status=404
                if isinstance(body,str):body=body.encode()
                self.send_response(status);self.send_header('Content-Type',mime)
                self.send_header('Content-Length',str(len(body)));self.end_headers()
                try:self.wfile.write(body)
                except (BrokenPipeError,ConnectionResetError):pass
        self.server=ThreadingHTTPServer((bind,port),Handler)
    def start(self):
        self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
