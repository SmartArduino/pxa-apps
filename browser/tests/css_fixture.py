"""Small style pages served locally for simulator and LAN device checks."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread

PAGE = '''<!doctype html><html><head><meta charset="utf-8"><title>轻量网页排版</title>
<style id="theme">
.title {font-size:24px;text-align:center;color:#9c2436}
.article {color:#2468ac} .gone {display:none}
.poem {white-space:pre-wrap;text-align:center}
.action {color:#187a42;text-align:right;font-size:18px}
</style><style media="print">p {display:none}</style></head><body>
<h1 class="title">轻量网页排版</h1>
<p class="article" id="message">中文阅读 · Colors and layout</p>
<p class="article">嵌套 <span style="color:#9c2436">文字颜色</span> 正常恢复。</p>
<div class="gone">Hidden content<img src="/hidden-image.png"><a href="/hidden-link">Hidden link</a></div>
<div class="poem">保留换行，居中阅读
  中文诗文与文章
  Read with PXA</div>
<p class="action"><a href="/target">继续阅读 Next</a></p>
</body></html>'''
SCRIPT = "<script>setTimeout(()=>{document.getElementById('message').textContent='脚本更新后保留样式 / Updated';},0);</script>"
RESTYLE = "<script>setTimeout(()=>{document.getElementById('theme').textContent+=' .title {color:#8030a0}';document.getElementById('message').textContent='样式更新后重新匹配 / Restyled';},0);</script>"
INLINE = '''<!doctype html><html><head><meta charset="utf-8"><title>行内样式</title>
<style>.target {color:#187a42;font-size:20px}</style></head><body>
<h3>行内样式 / Inline style</h3>
<p><button onclick="advanceStyle()" style="color:#d06020;font-size:18px">下一种样式 / Next</button></p>
<p id="target" class="target" style="display:none;color:#9c2436">样式同步 / Style update</p>
<script>
let phase=0;
function advanceStyle() {
    const n=document.getElementById('target');
    switch(++phase) {
        case 1: n.setAttribute('style','color:#2468ac'); break;
        case 2: n.style.cssText='color:#8030a0;display:none'; break;
        case 3: n.style.removeProperty('display'); break;
        case 4: n.removeAttribute('style'); break;
        case 5: n.style.setProperty('color','#9c2436','important'); break;
        case 6: n.style='text-align:center;color:#2468ac'; break;
        default: n.style.color='#2468ac'; break;
    }
}
</script></body></html>'''

class Fixture:
    def __init__(self, bind='127.0.0.1', port=0):
        self.requests = Counter()
        fixture = self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_): pass
            def do_GET(self):
                fixture.requests[self.path] += 1
                if self.path == '/target': body = '<h1>Navigation PASS</h1>'
                elif self.path == '/script': body = PAGE.replace('</body>',SCRIPT+'</body>')
                elif self.path == '/restyle': body = PAGE.replace('</body>',RESTYLE+'</body>')
                elif self.path == '/inline': body = INLINE
                elif self.path in ('/', '/static'): body = PAGE
                else: body = '<p>Unexpected hidden request</p>'
                data = body.encode()
                self.send_response(200); self.send_header('Content-Type','text/html; charset=utf-8')
                self.send_header('Content-Length',str(len(data))); self.end_headers(); self.wfile.write(data)
        self.server = ThreadingHTTPServer((bind,port),Handler)
    def start(self):
        self.thread = Thread(target=self.server.serve_forever,daemon=True); self.thread.start(); return self
    def close(self): self.server.shutdown(); self.server.server_close(); self.thread.join()

if __name__ == '__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--bind',default='127.0.0.1');p.add_argument('--port',type=int,default=31879)
    args=p.parse_args();f=Fixture(args.bind,args.port)
    print(f'http://{args.bind}:{f.server.server_port}',flush=True)
    try: f.server.serve_forever()
    except KeyboardInterrupt: pass
    finally: f.server.server_close()
