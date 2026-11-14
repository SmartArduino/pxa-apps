"""Bounded fragment failures and recovery, without third-party resources."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import urlsplit

CASES={
    'bytes': "'<img src=/trap-bytes.jpg><p>'+'x'.repeat(8200)+'</p>'",
    'nodes': "'<img src=/trap-nodes.jpg>'+'<b>x</b>'.repeat(49)",
    'attribute': "'<style>.proof{color:red}</style><img src=/trap-attribute.jpg><p title=\"'+'x'.repeat(4097)+'\">Bad</p>'",
    'radio': "'<input type=radio name=mode checked><p title=\"'+'x'.repeat(4097)+'\">Bad</p>'",
    'style': "'<style>.proof{color:red}</style><img src=/trap-style.jpg><p style=\"color:rgb('+'('.repeat(17)+'1'+')'.repeat(17)+')\">Bad</p>'",
    'text': "'\\0'.repeat(3000)",
}

TAGS='''<!doctype html><html><head><meta charset=utf-8><title>标签与快照</title></head><body>
<button type=button onclick="advance()" style="color:#d06020">切换检查</button>
<p id=proof style="color:#187a42">初始标签检查</p>
<div id=host><DiV><p id=initial>初始正文</p></unmatched></dIv><p id=tail>尾部正文</p></div><script>
const h=document.getElementById('host'),proof=document.getElementById('proof');let phase=0;
function check(ok){if(!ok)throw Error('tag snapshot')}
check(document.getElementById('initial').parentNode.parentNode===h && document.getElementById('tail').parentNode===h);
function advance(){proof.style.color='#555555';switch(++phase){
 case 1:
  h.innerHTML='<DiV>'.repeat(28)+'<p id=leaf>嵌套正文</p>'+'</unmatched>'.repeat(28)+'</dIv>'.repeat(28)+'<p id=tail>尾部正文</p>';
  let at=document.getElementById('leaf');for(let i=0;i<28;++i){at=at.parentNode;check(at.nodeName==='DIV')}
  check(at.parentNode===h && document.getElementById('tail').parentNode===h);break;
 case 2:
  h.innerHTML='<button type=button onclick="void 0">可点&amp;&lt;&gt;按钮</button><button disabled>禁用按钮</button><input type=submit value=提交><input type=text value="正文&amp;&quot;引号&quot;">';
  check(h.childNodes.length===4 && h.firstChild.nodeName==='BUTTON' && h.childNodes[1].disabled && h.lastChild.type==='text');
  const raw=h.innerHTML;check(raw.indexOf('<a')<0 && raw.includes('<button') && raw.includes('<input') && raw.includes('&amp;') && raw.includes('&lt;') && raw.includes('&gt;') && raw.includes('&quot;'));break;
 case 3:
  h.innerHTML='<p>重复段落</p>'.repeat(8);check(h.querySelectorAll('p').length===8 && h.textContent==='重复段落'.repeat(8));break;
 }proof.textContent='标签检查 '+phase;proof.style.color='#187a42';
}
</script></body></html>'''


def page(case):
    body='''<!doctype html><html><head><meta charset=utf-8><title>片段更新</title></head><body>
<button id=go type=button onclick="advance()" style="color:#d06020">尝试更新</button>
<div id=host><button id=old type=button onclick="oldClick()" style="color:#1469ac">保留按钮</button></div>
<p id=proof class=proof>等待检查</p>'''+('<textarea id=area>Original</textarea>' if case=='text' else '')+'''<script>
const h=document.getElementById('host'),old=h.firstChild,proof=document.getElementById('proof'),
  root=document.documentElement,body=document.body,head=document.head;
const target='''+("document.getElementById('area')" if case=='text' else 'h')+''',saved=target.childNodes;
let phase=0;
function check(ok){if(!ok)throw Error('fragment recovery')}
function pass(text){proof.textContent=text;proof.style.color='#187a42'}
function roots(){check(document.documentElement===root && document.body===body && document.head===head)}
function advance(){
 if(!phase){let caught=false;try{target.innerHTML='''+CASES[case]+'''}catch(e){caught=e.name==='RangeError'}
  check(caught && target.childNodes===saved && document.getElementById('old')===old && old.parentNode===h);
  roots();phase=1;pass('拒绝检查通过');
 }else{
  check(phase===2);h.innerHTML='<button id=new type=button onclick="newClick()" style="color:#1469ac">新按钮</button>';
  check(old.parentNode===null && h.firstChild.parentNode===h && !document.getElementById('old'));
  roots();phase=3;pass('恢复检查通过');
 }
}
function oldClick(){check(phase===1 && h.firstChild===old && old.parentNode===h);roots();phase=2;pass('原事件检查通过')}
function newClick(){check(phase===3 && document.getElementById('new')===h.firstChild);roots();pass('新事件检查通过')}
</script></body></html>'''
    if case=='radio':
        body=body.replace('<div id=host>','<form><input id=peer type=radio name=mode value=old checked style="display:none"><div id=host>')
        body=body.replace('</div>\n<p id=proof','</div></form>\n<p id=proof')
        body=body.replace("roots();phase=1;", "check(document.getElementById('peer').checked);roots();phase=1;")
        body=body.replace("新按钮</button>'", "新按钮</button><input id=first type=radio name=mode value=a checked style=\"display:none\"><input id=last type=radio name=mode value=b checked style=\"display:none\">'")
        body=body.replace("roots();phase=3;", "check(!document.getElementById('peer').checked && !document.getElementById('first').checked && document.getElementById('last').checked);roots();phase=3;")
    return body


class Fixture:
    def __init__(self,bind='127.0.0.1',port=0):
        self.counts=Counter();fixture=self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                path=urlsplit(self.path).path;fixture.counts[path]+=1;case=path.lstrip('/')
                status=200 if case in CASES or case=='tags' else 404
                data=(TAGS if case=='tags' else page(case) if status==200 else 'Unexpected request').encode()
                self.send_response(status);self.send_header('Content-Type','text/html; charset=utf-8')
                self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
        self.server=ThreadingHTTPServer((bind,port),Handler)
    def start(self):
        self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
