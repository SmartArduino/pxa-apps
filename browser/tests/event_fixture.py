"""Deterministic event pages for simulator and LAN-device checks."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread

HEAD = '''<!doctype html><html><head><meta charset="utf-8"><title>事件更新</title>
<style>.target {color:#2468ac;font-size:18px}</style></head><body>'''
CONTROL = '<p><button onclick="change()" style="color:#d06020;font-size:18px">切换处理器 / Toggle</button></p>'
IDLE = HEAD + '''<h3>脚本空闲 / Idle callbacks</h3>
<p id="t" class="target" style="color:#2468ac">等待 / Waiting</p><p id="empty"></p>
<script>let count=0,t=document.getElementById('t');
const timer=setInterval(()=>{
 ++count;t.textContent='等待 / Waiting';t.style.color='#2468ac';
 document.getElementById('empty').textContent='';
 Promise.resolve().then(()=>{if(count===12){clearInterval(timer);t.textContent='空闲检查通过 / Idle PASS 12'}});
},120);</script></body></html>'''
EVENTS = HEAD + '<h3>动态点击 / Click handlers</h3>' + CONTROL + '''
<p id="t" class="target">事件目标 / Target</p><script>
let phase=0,count=0,t=document.getElementById('t');
function clicked(){t.textContent='已点击 / Clicked '+(++count)}
function change(){switch(++phase){
 case 1:t.onclick=clicked;break;case 2:t.onclick=null;break;
 case 3:t.addEventListener('click',clicked);break;case 4:t.removeEventListener('click',clicked);break;
}}</script></body></html>'''
INLINE = HEAD + '<h3>行内事件 / Inline handlers</h3>' + CONTROL + '''
<p id="t" class="target" onclick="clicked()">事件目标 / Target</p><script>
let phase=0,count=0,t=document.getElementById('t');
function clicked(){t.textContent='已点击 / Clicked '+(++count)}
function change(){switch(++phase){
 case 1:t.onclick=undefined;break;
 case 2:t.setAttribute('onclick',t.getAttribute('onclick'));break;
 case 3:t.onclick=null;break;
}}</script></body></html>'''
XHR = HEAD + '''<h3>异步事件 / XHR handler</h3><p id="t" class="target">事件目标 / Target</p>
<script>let count=0,t=document.getElementById('t');
setTimeout(()=>{let x=new XMLHttpRequest();
 x.onload=()=>t.addEventListener('click',()=>{t.textContent='已点击 / Clicked '+(++count)});
 x.open('GET','/bind-handler');x.send();
},500);</script></body></html>'''
PAGES = {'/idle': IDLE, '/events': EVENTS, '/inline-events': INLINE, '/xhr-events': XHR}


class Fixture:
    def __init__(self, bind='127.0.0.1', port=0):
        self.requests = Counter()
        fixture = self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_): pass
            def do_GET(self):
                fixture.requests[self.path] += 1
                body = 'Bound' if self.path == '/bind-handler' else PAGES.get(self.path, 'Missing')
                data = body.encode()
                self.send_response(200 if self.path in PAGES or self.path == '/bind-handler' else 404)
                self.send_header('Content-Type', 'text/html; charset=utf-8')
                self.send_header('Content-Length', str(len(data)))
                self.end_headers(); self.wfile.write(data)
        self.server = ThreadingHTTPServer((bind, port), Handler)
    def start(self):
        self.thread = Thread(target=self.server.serve_forever, daemon=True); self.thread.start(); return self
    def close(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join()


if __name__ == '__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--bind',default='127.0.0.1');p.add_argument('--port',type=int,default=31880)
    a=p.parse_args();f=Fixture(a.bind,a.port)
    print(f'http://{a.bind}:{f.server.server_port}',flush=True)
    try:f.server.serve_forever()
    except KeyboardInterrupt:pass
    finally:f.server.server_close()
