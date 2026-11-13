"""Text containers with request traps; no third-party scripts or artwork."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import urlsplit

HEAD='<!doctype html><html><head><meta charset=utf-8><title>文本容器</title></head><body>'
PROOF='<p id=proof class=proof style="color:#187a42">解析检查 / Text PASS</p>'
TRAPS='<script src="/trap.js"></script><img src="/trap.jpg">'
UPDATE=HEAD+'''<style id=s></style><p class=proof id=proof>等待更新 / Waiting</p>
<button type=button onclick="update()" style="color:#d06020">更新文字 / Update</button>
<textarea id=area>Initial</textarea><xmp id=legacy>Original</xmp><script>
function update(){
 const area=document.getElementById('area'),legacy=document.getElementById('legacy'),style=document.getElementById('s');
 area.innerHTML='A</textarea><img src=/trap-area.jpg>';
 legacy.innerHTML='B</xmp><img src=/trap-xmp.jpg>&amp;';
 if(area.children.length || legacy.children.length)throw Error('text context');
 style.innerHTML='/* <'+ '/style><img src=/trap-style.jpg> */ .proof{color:#187a42}';
 document.getElementById('proof').textContent='更新检查通过 / Update PASS';
}
</script></body></html>'''
PLAIN_SCRIPT=HEAD+'''<p id=proof>等待解析 / Waiting</p><script>
document.addEventListener('DOMContentLoaded',()=>{
 const node=document.getElementById('plain');
 if(node.children.length || node.textContent.indexOf('<img src=/trap-plain.jpg>')<0)throw Error('plaintext');
 const proof=document.getElementById('proof');proof.textContent='全文文字通过 / Plaintext PASS';proof.style.color='#187a42';
});</script><plaintext id=plain>A</plaintext><img src=/trap-plain.jpg>'''


class Fixture:
    def __init__(self,bind='127.0.0.1',port=0):
        self.counts=Counter();fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                path=urlsplit(self.path).path;fixture.counts[path]+=1;name=path.lstrip('/');status=200
                if name.startswith('static-'):
                    tag=name[7:]
                    if tag=='title':body=HEAD.replace('<title>文本容器</title>','<title>A '+TRAPS+' &amp; B</title>')+PROOF
                    elif tag=='plaintext':body=HEAD+PROOF+'<plaintext>A &amp; '+TRAPS+'</plaintext><p>Literal tail</p>'
                    else:body=HEAD+PROOF+'<'+tag+(' src="/frame-target"' if tag=='iframe' else '')+'>'+TRAPS+' &amp;</'+tag+'>'
                elif name=='frame-target':body=HEAD+PROOF+'<p>嵌入地址通过 / Frame target PASS</p>'
                elif name=='update':body=UPDATE
                elif name=='script-plaintext':body=PLAIN_SCRIPT
                else:body='Unexpected request';status=404
                data=body.encode();self.send_response(status);self.send_header('Content-Type','text/html; charset=utf-8')
                self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
        self.server=ThreadingHTTPServer((bind,port),Handler)
    def start(self):
        self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
