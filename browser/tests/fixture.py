#!/usr/bin/env python3
"""Deterministic browser pages. Bind LAN for device tests; no external service."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs
import argparse, html, json, time
ROOT='''<html lang="zh-CN"><head><meta charset="utf-8"><title>浏览器验证</title><style>hidden</style></head><body><h1>你好，PXA！</h1><p><a href="/next">下一页 · 链接</a></p><p><a href="/form">表单 · 中文输入</a></p><p><a href="/redirect">重定向</a> <a href="/slow">慢页面 / 停止</a></p><p><a href="/large">大页面</a> <a href="/missing">404 页面</a></p><p><a href="/plain">纯文本</a> <a href="/gzip">压缩类型提示</a></p><p>左右安全区和 DPI 自适应。 &amp; &lt; &gt; &#x4e2d;&#25991;</p><img src="/ignored.png" alt="蓝色海岸"><script>document.write('SCRIPT SHOULD NEVER APPEAR')</script></body></html>'''
FORM='''<title>表单</title><h1>中文表单</h1><form method="post" action="/echo"><input name="q" value="中文 abc" placeholder="关键词"><input type="checkbox" name="ok" checked><input type="hidden" name="token" value="a&amp;b"><input type="submit" name="go" value="send" placeholder="提交"></form><p><a href="/">返回测试首页</a></p>'''
class Handler(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self, fmt, *args):
        print(json.dumps({'request':self.path,'ua':self.headers.get('User-Agent',''),
                          'save_data':self.headers.get('Save-Data',''),'message':fmt%args},
                         ensure_ascii=False),flush=True)
    def respond(self, text, code=200, mime='text/html; charset=utf-8', headers=()):
        data=text.encode() if isinstance(text,str) else text;self.send_response(code);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)))
        for name,value in headers:self.send_header(name,value)
        self.end_headers();self.wfile.write(data)
    def do_GET(self):
        path=urlsplit(self.path).path
        try:
            if path=='/session/login':
                prefilled='prefilled=1' in self.path
                user='tester' if prefilled else ''
                password='a&amp;b=+pw' if prefilled else ''
                self.respond(f'<title>登录验证</title><form method="post" action="/session/login"><input name="user" value="{user}" placeholder="账号"><input type="password" name="pass" value="{password}" placeholder="密码"><input type="hidden" name="csrf" value="fixed-token"><input type="submit" value="登录"></form>', headers=(('Set-Cookie','csrf=fixed; Path=/session; HttpOnly'),('Set-Cookie','other=also; Path=/session; HttpOnly')))
            elif path=='/session/member':
                if 'session=logged-in' in self.headers.get('Cookie',''):
                    self.respond('<title>登录成功</title><h1>登录成功</h1><p>会话与表单编码验证通过。</p><a href="/session/login">返回登录</a><p><a href="/session/logout">退出登录</a></p>')
                else:self.respond('<title>未登录</title><h1>尚未登录</h1>',401)
            elif path=='/session/logout':
                self.respond('<title>已退出</title><h1>已退出登录</h1>',headers=(('Set-Cookie','session=; Path=/session; Max-Age=0'),))
            elif path=='/':self.respond(ROOT)
            elif path=='/form':self.respond(FORM)
            elif path in ('/encoding', '/encoding-header'):
                content='<meta http-equiv="Content-Type" content="text/html; charset=gb2312"><title>中文编码</title><h1>网址导航</h1><p>当前位置：手机漫画</p><div style="display: none"><div>正在加载,请稍候...</div></div><p><a href="/next">继续阅读</a></p>'
                self.respond(content.encode('gbk'), mime='text/html' if path=='/encoding' else 'text/html; charset=GBK')
            elif path=='/encoding-bom':
                self.respond(b'\xef\xbb\xbf'+ '<title>UTF-8</title><h1>中文正常</h1>'.encode(),mime='text/html; charset=gbk')
            elif path=='/encoding-unsupported':
                self.respond('<meta charset=big5><p>Unsupported</p>',mime='text/html')
            elif path in ('/gif', '/gif-alpha', '/gif-disguised', '/gif-still', '/gif-errors', '/gif-slow'):
                name={'/gif':'animation.gif', '/gif-alpha':'alpha.gif', '/gif-disguised':'animation.bin',
                      '/gif-still':'still.gif', '/gif-errors':'broken.gif', '/gif-slow':'slow.gif'}[path]
                self.respond('<title>GIF</title><p><a href="/gif-alpha">Alpha</a> <a href="/gif">Opaque</a></p>'
                             '<img src="/picture/'+name+'" width="160" height="96" alt="GIF">'
                             + '<p>GIF animation / transparency / bounded cache.</p>' * 30)
            elif path in ('/images','/image-alpha','/image-jpeg','/image-errors','/image-capacity','/image-slow','/image-tall','/image-linked'):
                pictures={'/images':['alpha.png','photo.jpg'], '/image-alpha':['alpha.png'],
                          '/image-jpeg':['photo.jpg'], '/image-errors':['broken.png','progressive.jpg','oversize.png','unsupported.svg'],
                          '/image-capacity':['alpha.png?1','photo.jpg?2','alpha.png?3','photo.jpg?4','alpha.png?5'],
                          '/image-slow':['slow.png'], '/image-tall':['tall.png'],
                          '/image-linked':['alpha.png']}[path]
                body='<title>图片验证</title><h1>PNG / JPEG</h1>'
                for index,picture in enumerate(pictures):
                    image=f'<img src="/picture/{picture}" width="320" height="180" alt="图片 {index+1}">'
                    if path=='/image-linked':image=f'<a href="/next">{image}</a>'
                    body+=image+'<p>下面的文字应当正常排列。</p>'
                body+='<p><a href="/image-alpha">透明 PNG</a> <a href="/image-jpeg">JPEG</a></p><p><a href="/image-errors">异常图片</a></p><p><a href="/picture/photo.jpg">直接打开图片</a></p>'
                self.respond(body)
            elif path.startswith('/picture/'):
                from image_fixtures import fixtures
                name=path.removeprefix('/picture/');data=fixtures().get(name)
                if name in ('slow.png','slow.gif'):
                    data=fixtures()['alpha.png' if name=='slow.png' else 'animation.gif'];self.send_response(200);self.send_header('Content-Type','image/png' if name=='slow.png' else 'image/gif');self.send_header('Content-Length',str(len(data)));self.end_headers()
                    for at in range(0,len(data),32):self.wfile.write(data[at:at+32]);self.wfile.flush();time.sleep(.15)
                elif name=='oversize.png':
                    import struct,zlib
                    data=bytearray(fixtures()['alpha.png']);struct.pack_into('>II',data,16,4096,4096);struct.pack_into('>I',data,29,zlib.crc32(data[12:29]));self.respond(bytes(data),mime='image/png')
                elif data is None:self.respond('missing',404,mime='text/plain')
                else:self.respond(data,mime='image/jpeg' if name.endswith('.jpg') else 'image/svg+xml' if name.endswith('.svg') else 'image/gif' if name.endswith('.gif') else 'application/octet-stream' if name.endswith('.bin') else 'image/png')
            elif path=='/headers':
                self.respond('<title>请求模式</title><h1>请求模式</h1><p>'+html.escape(self.headers.get('User-Agent',''))+'</p><p>Save-Data: '+html.escape(self.headers.get('Save-Data',''))+'</p>')
            elif path=='/reading':
                # Real reading sites may send a CSP larger than our entire
                # response-head budget. It must be filtered before storage.
                self.respond('<title>换行验证</title><h1>Clear reading</h1><p>Ordinary English words should remain together on narrow screens. 中文和 English 混排。</p><p><a href="/headers">查看请求模式</a></p><pre>abcdefghijklmnopqrstuvwxyz0123456789</pre>',
                             headers=(('Content-Security-Policy',"default-src 'none'; report-uri https://unused.invalid/"+'x'*5000),))
            elif path=='/next':self.respond('<title>下一页</title><h1>链接导航成功</h1><p>返回和前进应保持页面滚动位置。</p><p><a href="/">回到首页</a></p><p><a href="https://example.com/">HTTPS Example</a></p>')
            elif path=='/redirect':self.send_response(302);self.send_header('Location','/next');self.send_header('Content-Length','0');self.end_headers()
            elif path=='/large':self.respond('<title>大页面</title><h1>容量边界</h1>'+('<p>这是大页面的中文内容，内存必须有界。0123456789</p>'*1000))
            elif path=='/slow':
                self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Transfer-Encoding','chunked');self.end_headers()
                for i in range(20):
                    data=f'<p>第 {i} 段 · 持续加载</p>'.encode();self.wfile.write(f'{len(data):x}\r\n'.encode()+data+b'\r\n');self.wfile.flush();time.sleep(.25)
                self.wfile.write(b'0\r\n\r\n')
            elif path=='/plain':self.respond('纯文本 UTF-8\n<a>不是链接</a>\n第二行',mime='text/plain; charset=utf-8')
            elif path=='/gzip':
                import gzip
                data=gzip.compress(ROOT.encode());self.send_response(200);self.send_header('Content-Type','text/html');self.send_header('Content-Encoding','gzip');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
            elif path=='/status-wait':
                first='<p>持续加载</p>'.encode();last='<p>完成</p>'.encode()
                self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.send_header('Content-Length',str(len(first)+len(last)));self.end_headers()
                self.wfile.write(first);self.wfile.flush();time.sleep(8);self.wfile.write(last)
            else:self.respond('<title>没有找到</title><h1>404</h1><p>错误页面仍然可以阅读。</p><a href="/">返回</a>',404)
        except (BrokenPipeError,ConnectionResetError):pass
    def do_POST(self):
        data=self.rfile.read(int(self.headers.get('Content-Length','0'))).decode()
        if urlsplit(self.path).path=='/session/login':
            fields=parse_qs(data)
            okay=(fields=={'user':['tester'],'pass':['a&b=+pw'],'csrf':['fixed-token']} and 'csrf=fixed' in self.headers.get('Cookie','') and 'other=also' in self.headers.get('Cookie',''))
            if okay:self.respond('',303,headers=(('Location','/session/member'),('Set-Cookie','session=logged-in; Path=/session; HttpOnly')))
            else:self.respond('<title>登录验证失败</title><h1>登录验证失败</h1>',403)
            return
        self.respond('<title>提交结果</title><h1>POST 成功</h1><pre>'+html.escape(data)+'</pre><p><a href="/form">重新填写</a></p>')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--bind',default='127.0.0.1');p.add_argument('--port',type=int,default=31878);a=p.parse_args()
    print(f'http://{a.bind}:{a.port}',flush=True);ThreadingHTTPServer((a.bind,a.port),Handler).serve_forever()
