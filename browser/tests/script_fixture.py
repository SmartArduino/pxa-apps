#!/usr/bin/env python3
"""Local, repeatable JS/network fixtures; no external account or service."""
from fixture import Handler as Base
from http.server import ThreadingHTTPServer
from urllib.parse import urlsplit
import argparse,json,time
class Handler(Base):
    def do_GET(self):
        p=urlsplit(self.path).path
        if p=='/bench-menu':
            self.respond(''.join('<p><a href="'+path+'">'+name+'</a></p>' for path,name in [('/static-scroll','静态滚动'),('/js-scroll','脚本滚动'),('/js','脚本与网络'),('/js-hang','超时保护'),('/js-oom','内存保护'),('/js-stack','递归保护'),('/js-parser','复杂解析保护')])+'<p>向上滑动查看更多测试</p>')
        elif p in ('/js-csp','/js-csp-big','/js-csp-meta'):
            body="<h1>策略保留静态</h1><script>document.body.textContent='不应执行';</script>"
            headers=() if p.endswith('-meta') else (('Content-Security-Policy',"script-src 'none';"*(500 if p.endswith('-big') else 1)),)
            if p.endswith('-meta'):body="<meta http-equiv='Content-Security-Policy' content=\"script-src 'none'\">"+body
            self.respond(body,headers=headers)
        elif p=='/js':
            self.respond('''<html><head><meta charset="utf-8"><title>JS 测试</title></head><body>
<h1 id="title">等待脚本</h1><button id="tap" onclick="document.getElementById('title').textContent='点击成功';return false">点击测试</button>
<input id="query" placeholder="输入测试" value="初始文字"><p id="network">等待网络</p><p id="cookie"></p><div id="hide">隐藏内容不应显示</div>
<script src="/js-external.js"></script><script>
window.onload=function(){document.getElementById('title').textContent='脚本已运行';document.getElementById('hide').style.display='none';
document.getElementById('cookie').textContent=document.cookie.indexOf('private=')<0?'HttpOnly 隔离成功':'隔离失败';
fetch('/js-data').then(r=>r.json()).then(r=>{document.getElementById('network').textContent=r.text});};
</script></body></html>''',headers=(('Set-Cookie','private=hidden; HttpOnly; Path=/'),('Set-Cookie','public=visible; Path=/')))
        elif p=='/js-input-slow':
            self.respond("<html><body><form><input name='q' id='q' value='old' placeholder='值'></form><p id='echo'>等待输入</p><script src='/js-delay.js'></script></body></html>")
        elif p=='/js-delay.js':
            time.sleep(2);self.respond("document.getElementById('q').onchange=function(){document.getElementById('echo').textContent='输入:'+this.value};",mime='application/javascript')
        elif p=='/js-external.js':self.respond("window.externalLoaded=true;",mime='application/javascript')
        elif p=='/js-data':self.respond(json.dumps({'text':'异步网络成功'},ensure_ascii=False),mime='application/json')
        elif p in ('/js-projection-small','/js-projection-large'):
            self.respond('''<h1 id="n">投影测试</h1><img id="image" width="180" height="180"><p id="text"></p>
<script>fetch('''+json.dumps(p+'.json')+''').then(r=>r.json()).then(r=>{
document.getElementById('image').src=r.data;
document.getElementById('text').textContent=r.text;
let n=0;setInterval(()=>document.getElementById('n').textContent='投影测试 '+(++n),20);
});</script>''')
        elif p in ('/js-projection-small.json','/js-projection-large.json'):
            import base64
            from image_fixtures import projection_image
            data='data:image/png;base64,'+base64.b64encode(projection_image(4 if 'small' in p else 3)).decode()
            assert 2800<len(data)<4096
            self.respond(json.dumps({'data':data,'text':'<>&"\0 中文𠳐'}),mime='application/json')
        elif p in ('/js-image','/js-gif','/js-image-error','/js-two-images'):
            source={'/js-image':'/js-image.png','/js-gif':'/picture/animation.gif','/js-image-error':'/missing-image.png','/js-two-images':'/picture/alpha.png'}[p]
            self.respond('<h1 id="n">图片与脚本</h1><img src="'+source+'"'+(' width="32" height="32"' if p=='/js-two-images' else '')+'>'+('<img src="'+source+'" width="32" height="32">' if p=='/js-two-images' else '')+'<script>let n=0;setInterval(()=>document.getElementById("n").textContent="图片与脚本 "+(++n),20);</script>')
        elif p=='/js-image.png':
            from image_fixtures import fixtures
            time.sleep(1);self.respond(fixtures()['alpha.png'],mime='image/png')
        elif p=='/js-scroll':
            self.respond('<html><body><h1 id="n">脚本滚动压力</h1>'+''.join(f'<p>第 {i} 行 · 中文阅读与触摸优先测试。</p>' for i in range(60))+"<script>let n=0;setInterval(()=>{document.getElementById('n').textContent='滚动测试 '+(++n)},20);</script></body></html>")
        elif p=='/static-scroll':self.respond('<html><body><h1>静态滚动基线</h1>'+''.join(f'<p>第 {i} 行 · 中文阅读与触摸优先测试。</p>' for i in range(60))+'</body></html>')
        elif p=='/js-hang':self.respond('<h1>静态内容保留</h1><script>while(true){}</script>')
        elif p=='/js-regexp':self.respond("<h1>静态内容保留</h1><script>/(a+)+$/.test('a'.repeat(100)+'!');</script>")
        elif p=='/js-stack':self.respond("<h1 id='a'>等待</h1><script>function recurse(){return recurse()};recurse();</script><script>let caught=false;try{eval('!'.repeat(1000)+'true')}catch(e){caught=String(e).indexOf('stack overflow')>=0}document.getElementById('a').textContent=caught?'递归异常后继续':'深层解析保护失败';</script>")
        elif p=='/js-parser':
            expression='1'
            for _ in range(24):expression='1+2*3<4&5^6|7&&('+expression+')'
            self.respond('<p id="a">静态内容保留</p><script>'+expression+';</script><script>document.getElementById("a").textContent="复杂解析异常后继续";</script>')
        elif p=='/js-oom':self.respond("<h1>静态内容保留</h1><script>let a=[];while(true)a.push('x'.repeat(1024));</script>")
        elif p=='/js-form':self.respond('''<h1 id="a">表单</h1><form action="/js-echo" method="post"><input id="q" name="q" value="test"><input type="submit" value="提交"></form><script>document.getElementById('q').onchange=function(){document.getElementById('a').textContent=this.value};</script>''')
        elif p=='/js-inline':self.respond(r'''<button onclick="this.setAttribute('onclick', 'this.textContent=\'Second\';this.removeAttribute(\'onclick\')');this.textContent='First'">Tap</button><script>Function=function(){throw Error('replaced global compiler')};</script>''')
        elif p=='/js-inline-stop':self.respond('''<div onclick="invalid syntax )"><button onclick="event.stopPropagation();this.textContent='Child'">Tap</button></div><script>window.fixture=true;</script>''')
        else:super().do_GET()
    def do_POST(self):
        if urlsplit(self.path).path=='/js-echo':
            data=self.rfile.read(int(self.headers.get('Content-Length','0')));self.respond('<h1>POST 成功</h1><p>'+data.decode()+'</p>')
        else:super().do_POST()
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--port',type=int,default=31925);parser.add_argument('--bind',default='127.0.0.1');args=parser.parse_args();ThreadingHTTPServer((args.bind,args.port),Handler).serve_forever()
