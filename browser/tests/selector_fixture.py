"""Bounded selectors used for real lazy images, navigation and event binding."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import urlsplit
from comic_fixture import picture

HEAD='''<!doctype html><html><head><meta charset="utf-8"><title>选择器兼容</title></head><body>
<h3>选择器 / DOM queries</h3><p id="proof">等待脚本 / Waiting</p>'''
CHECK="function check(ok,label){if(!ok)throw Error(label)};function passed(){let p=document.getElementById('proof');p.textContent='检查通过 / Query PASS';p.style.color='#187a42'};"
PAGES={
 'gallery': HEAD+'''<div class="reader"><section><img src="about:blank" data-image="/page1.jpg" width="64" height="96"></section></div>
 <img src="about:blank" data-image="/unused.jpg" hidden><script>'''+CHECK+'''
 const images=document.querySelectorAll('.reader img[data-image]');check(images.length===1,'image scope');
 images.forEach(img=>{img.src=img.getAttribute('data-image')});passed();</script></body></html>''',
 'order': HEAD+'''<section id="reader"><div><a href="/chosen-first" class="page">第一条链接</a></div>
 <a href="/wrong-second" class="page">第二条链接</a></section>
 <p><button style="color:#d06020" id="open">打开首条 / Open first</button></p><script>'''+CHECK+'''
 const links=document.querySelectorAll('#reader a.page, a[href="/chosen-first"]');
 check(links.length===2 && links[0]===document.querySelector('a'),'tree order');
 document.querySelector('button#open').onclick=()=>location.href=links[0].href;passed();</script></body></html>''',
 'events': HEAD+'''<div class="controls" id="controls"><button data-action="read" type="button" style="color:#d06020">绑定点击 / Bound click</button></div>
 <script>'''+CHECK+'''
 document.querySelectorAll('.controls > button[data-action="read"]').forEach(button=>button.addEventListener('click',e=>{
 check(e.target.matches('button[type=BUTTON]'),'matching enum');
 check(e.target.closest('.controls')===document.getElementById('controls'),'closest');passed();
 }));</script></body></html>''',
 'scope': HEAD+'''<section id="reader" class="reader"><div><p class="target">Child</p></div></section><script>'''+CHECK+'''
 const reader=document.getElementById('reader');
 check(reader.querySelector('.reader')===null,'exclude root');
 check(reader.querySelector('body .target')===reader.querySelector(':scope > div > p.target'),'outside ancestor');
 const detached=document.createElement('div');detached.className='漫画\treader';
 check(detached.matches('.漫画.reader') && detached.closest(':scope')===detached,'detached classes');passed();
 </script></body></html>''',
 'errors': HEAD+'''<script>'''+CHECK+'''
 for(const s of ['div,','[x="bad]',':unsupported']){
 let caught=false;try{document.querySelectorAll(s)}catch(e){caught=e instanceof DOMException && e.name==='SyntaxError' && e.code===12 && !(e instanceof SyntaxError)}check(caught,'syntax error')}
 let caught=false;try{document.querySelector('a'.repeat(513))}catch(e){caught=e instanceof RangeError}check(caught,'budget');
 check(document.querySelectorAll('.missing').length===0,'empty result');passed();</script></body></html>''',
 'static': HEAD+'''<section id="reader"><p class="target">Original</p></section><script>'''+CHECK+'''
 const reader=document.getElementById('reader'),before=reader.querySelectorAll('.target'),second=document.createElement('p');
 second.className='target';second.textContent='Added';reader.appendChild(second);
 check(before.length===1 && reader.querySelectorAll('.target').length===2,'static snapshot');
 second.className='other';check(reader.querySelectorAll('.target').length===1,'cached plan fresh DOM');
 check(reader.getElementsByClassName('other')[0]===second && reader.getElementsByTagName('p')[0]===before[0],'collections');
 passed();</script></body></html>''',
}


class Fixture:
    def __init__(self,bind='127.0.0.1',port=0):
        self.counts=Counter();fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                path=urlsplit(self.path).path;fixture.counts[path]+=1;name=path.lstrip('/')
                mime='text/html; charset=utf-8';status=200
                if name in PAGES:body=PAGES[name]
                elif name=='page1.jpg':body=picture(name);mime='image/jpeg'
                elif name=='chosen-first':body='<p style="color:#187a42">首条链接通过 / First link PASS</p>'
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
