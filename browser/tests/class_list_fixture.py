"""Own CSS/classList/dataset pages, request accounting and a generated image."""
from collections import Counter
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from io import BytesIO
from threading import Thread
from urllib.parse import urlsplit
from PIL import Image

HEAD='''<!doctype html><html><head><meta charset=utf-8><title>动态类名</title>
<style>.blue{color:#2468ac}.green{color:#187a42}.folded{display:none}</style></head><body>'''
ACTION='<button type=button onclick="advance()" style="color:#d06020">切换状态</button>'
PROOF='<p id=proof style="color:#555555">准备检查</p>'
CHECK='''function check(ok){if(!ok)throw Error('class state')}function proof(text){document.getElementById('proof').textContent=text}'''
MAIN=HEAD+ACTION+'<p id=target class=blue>正文颜色检查</p>'+PROOF+'''<script>'''+CHECK+'''
const n=document.getElementById('target'),list=n.classList;let phase=0;
function advance(){switch(++phase){
 case 1:list.add('green');check(n.matches('.blue.green'));break;
 case 2:check(list.toggle('folded',true));break;
 case 3:check(!list.toggle('folded',false));break;
 case 4:check(list.replace('green','blue'));check(list.length===1 && list[0]==='blue');break;
 case 5:n.className='green green';check(list.length===1 && list[0]==='green');break;
 case 6:n.setAttribute('class','blue');check(list.value==='blue');break;
 case 7:list.value='green extra';check(list[1]==='extra');break;
 case 8:n.removeAttribute('class');check(!list.length && list[0]===undefined);break;
 case 9:n.classList='blue';check(n.classList===list && list[0]==='blue');break;
 case 10:list.remove('blue');list.add('green');break;
 case 11:let caught=false;try{list.add('valid','bad token')}catch(e){caught=e instanceof DOMException && e.name==='InvalidCharacterError' && e.code===5}
  check(caught && n.className==='green' && list.length===1);break;
 default:
  check(list.contains('green'));list.add('green');list.remove('missing');check(list.toggle('green',true));
  check(!list.replace('missing','other'));list.value='green';return;
 }proof('状态检查 '+phase);
}
</script></body></html>'''
QUOTA=HEAD+ACTION+'<p id=target class=green>额度恢复检查</p>'+PROOF+'''<script>'''+CHECK+'''
const n=document.getElementById('target'),list=n.classList;let phase=0;
function advance(){switch(++phase){
 case 1:let caught=false;try{list.add('x'.repeat(4097))}catch(e){caught=e.name==='RangeError'}
  check(caught && n.className==='green' && list[0]==='green');break;
 case 2:n.removeAttribute('class');for(let i=0;i<23;++i)n.setAttribute('data-'+i,'x');
  let limited=false;try{list.add('green')}catch(e){limited=e.name==='RangeError'}
  list.add();list.remove('missing');check(limited && !n.hasAttribute('class') && !list.length);break;
 case 3:n.removeAttribute('data-0');list.add('green');check(list[0]==='green');break;
 }proof('额度检查 '+phase);
}
</script></body></html>'''
IMAGE=HEAD+ACTION+'<div id=panel class=folded><img src=/class-image.png width=48 height=48 alt="类名显示图片"></div>'+PROOF+'''<script>'''+CHECK+'''
const panel=document.getElementById('panel');let phase=0;
function advance(){const hidden=panel.classList.toggle('folded');check(hidden===Boolean((++phase)%2===0));proof('图片检查 '+phase)}
</script></body></html>'''

DATASET=HEAD+ACTION+'<div id=panel class=folded data-page-number=1><img id=photo class=lazy data-src=/class-image.png width=48 height=48 alt="脚本懒加载"></div>'+PROOF+'''<script>'''+CHECK+'''
const panel=document.getElementById('panel'),image=document.getElementById('photo'),d=image.dataset;let phase=0;
check(d===image.dataset && d.src==='/class-image.png' && panel.dataset.pageNumber==='1');
function advance(){switch(++phase){
 case 1:image.src=d.src;delete d.src;image.classList.remove('lazy');panel.classList.remove('folded');
  panel.dataset.pageNumber=2;check(!image.hasAttribute('data-src') && panel.matches('[data-page-number="2"]'));break;
 case 2:panel.setAttribute('data-page-number','3');check(panel.dataset.pageNumber==='3');
  Object.assign(d,{src:'/MiXeD.png',loaded:true});check(Object.keys(d).join(',')==='src,loaded');image.src=d.src;break;
 case 3:delete d.src;delete d.loaded;panel.classList.add('folded');check(!Object.keys(d).length);break;
 case 4:let invalid=false;try{d['bad-key']='no'}catch(e){invalid=e.name==='SyntaxError' && e.code===12}
  check(invalid && !Object.keys(d).length);panel.dataset.__proto__='safe';panel.dataset.constructor='data';
  check(panel.dataset.__proto__==='safe' && panel.dataset.constructor==='data' && panel.dataset instanceof DOMStringMap);
  d.object={toString(){d.side='effect';return 'outer'}};check(d.object==='outer' && d.side==='effect');
  d.exotic={[Symbol.toPrimitive](hint){check(hint==='string');return null}};check(d.exotic==='null');
  check(image.tagName==='IMG' && !Reflect.set(image,'tagName','WRONG'));
  panel.classList.remove('folded');break;
 default:panel.dataset.__proto__='safe';check(d.src===undefined);delete d.missing;return;
 }proof('数据属性检查 '+phase);
}
</script></body></html>'''

DATAQUOTA=HEAD+ACTION+'<p id=target class=green>数据额度恢复</p>'+PROOF+'''<script>'''+CHECK+'''
const n=document.getElementById('target'),d=n.dataset;d.x='old';let phase=0;
function advance(){switch(++phase){
 case 1:let limited=false;try{d.x='x'.repeat(4097)}catch(e){limited=e.name==='RangeError'}check(limited && d.x==='old');break;
 case 2:for(let i=0;i<21;++i)d['n'+i]='v';let full=false;try{d.extra='no'}catch(e){full=e.name==='RangeError'}
  check(full && !('extra' in d) && Object.keys(d).length===22);d.x='new';check(d.x==='new');break;
 case 3:delete d.n0;d.extra='recovered';check(d.extra==='recovered' && n.getAttribute('data-extra')==='recovered');break;
 }proof('数据额度检查 '+phase);
}
</script></body></html>'''

ATTRIBUTES=HEAD.replace('</head>','<StYlE ID=MiXeDSheet MeDiA=ScReEn>.MiXeD{color:#187a42}</StYlE></head>')+\
    '<BuTtOn OnClIcK="advance()" StYlE="color:#d06020">属性检查</BuTtOn>'+\
    '<div id=host><P ID=MiXeD iD=Ignored ClAsS=MiXeD TiTlE="VaLuE&amp;X" StYlE="color:#2468ac">属性值检查</P><b></b></div>'+PROOF+\
    '<script>'+CHECK+'''
const n=document.getElementById('MiXeD'),host=document.getElementById('host');let phase=0;
check(!document.getElementById('Ignored') && n.id==='MiXeD' && n.className==='MiXeD' && n.title==='VaLuE&X');
check(document.getElementById('MiXeDSheet').media==='ScReEn' && host.lastChild.nodeName==='B');
function advance(){
 if(++phase===1){
  host.innerHTML='<StYlE ID=DynamicSheet media=ScReEn>.Unused{color:red}</StYlE><P ID=Next ClAsS=MiXeD TITLE=NeXt>动态属性检查</P><IMG ID=Photo SrC=/MiXeD.png width=48 height=48>';
  check(document.getElementById('DynamicSheet').media==='ScReEn' && document.getElementById('Next').title==='NeXt');
  check(document.getElementById('Photo').getAttribute('src')==='/MiXeD.png');
 }else{
  host.innerHTML='<p ID=Fallback K=Keep ClAsS=MiXeD>回退路径检查</p>';
  check(document.getElementById('Fallback').getAttribute('k')==='Keep' && host.firstChild.className==='MiXeD');
 }proof('属性检查 '+phase);
}
</script></body></html>'''



class Fixture:
    def __init__(self,bind='127.0.0.1',port=0):
        self.counts=Counter();fixture=self
        buffer=BytesIO();Image.new('RGB',(48,48),(24,122,66)).save(buffer,format='PNG');image=buffer.getvalue()
        class Handler(BaseHTTPRequestHandler):
            def log_message(self,*_):pass
            def do_GET(self):
                path=urlsplit(self.path).path;fixture.counts[path]+=1
                if path in ('/class-image.png','/MiXeD.png'):data=image;content='image/png';status=200
                else:
                    page={'/classes':MAIN,'/quota':QUOTA,'/image':IMAGE,'/attributes':ATTRIBUTES,'/dataset':DATASET,'/data-quota':DATAQUOTA}.get(path)
                    data=(page or 'Unexpected request').encode();content='text/html; charset=utf-8';status=200 if page else 404
                self.send_response(status);self.send_header('Content-Type',content);self.send_header('Content-Length',str(len(data)));self.end_headers()
                try:self.wfile.write(data)
                except (BrokenPipeError,ConnectionResetError):pass
        self.server=ThreadingHTTPServer((bind,port),Handler)
    def start(self):self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join()
