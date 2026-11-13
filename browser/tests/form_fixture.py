"""Real-network request validation for bounded static and script forms."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import urlsplit

HEAD = '''<!doctype html><html><head><meta charset="utf-8"><title>表单兼容</title></head><body>
<h3>表单提交 / Form submission</h3>'''
WORDS = 'q=A+B%2B*%21%7E%27%28%29%E4%B8%AD&q=last'


def page(mode, method, choice):
    action = '/verify/' + method + '/' + choice + ('?stale=1' if method == 'get' else '?kept=1') + '#result'
    body = HEAD + f'''<form action="{action}" method="{method.upper()}">
<div><input type="hidden" name="q" value="A B+*!~'()中"></div><input type="hidden" name="q" value="last">
<input id="send" type="SUBMIT" name="go" value="Send" style="color:#d06020">
<input type="submit" name="other" value="Other" style="color:#187a42"></form>'''
    return body + ('<script>void 0;</script>' if mode == 'script' else '') + '</body></html>'

CONTROLS = HEAD + '''<form method="post" action="/unexpected">
<button type="BUTTON" style="color:#d06020" onclick="this.textContent='普通按钮成功 / Button OK'">普通按钮 / Button</button>
<button disabled style="color:#8030a0" onclick="invalid syntax )">禁用按钮 / Disabled</button>
<button type="RESET" style="color:#187a42" onclick="this.textContent='重置回调成功 / Reset handler OK'">重置回调 / Reset handler</button>
</form><script>void 0;</script></body></html>'''
REQUEST = HEAD + '''<form id="f" action="/verify/request" method="post"><input type="hidden" name="q" value="ok">
<button type="BUTTON" style="color:#d06020" onclick="document.getElementById('f').requestSubmit(document.getElementById('send'))">请求提交 / Request</button>
<button id="send" name="go" value="pick" hidden>Hidden submitter</button></form>
<p id="status" style="color:#2468ac">等待点击 / Waiting</p>
<script>let count=0,f=document.getElementById('f'),send=document.getElementById('send');
f.onsubmit=e=>{if(e.submitter!==send)throw Error('wrong submitter');++count;
f.requestSubmit(send);if(count===1){e.preventDefault();document.getElementById('status').textContent='已取消 / Cancelled'}};
</script></body></html>'''
DIRECT = HEAD + '''<form method="post" action="/verify/direct" onsubmit="this.submit();return false">
<input type="hidden" name="q" value="ok"><button name="go" value="excluded" style="color:#d06020">直接提交 / Direct</button>
</form><script>void 0;</script></body></html>'''
PASS = '''<!doctype html><html><head><meta charset="utf-8"><title>Form PASS</title></head>
<body><h3 id="result" style="color:#187a42">表单通过 / Form PASS</h3></body></html>'''


class Fixture:
    def __init__(self, bind='127.0.0.1', port=0):
        self.requests = Counter(); self.submissions = []
        fixture = self
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_): pass
            def handle_request(self, method):
                fixture.requests[self.path] += 1
                url = urlsplit(self.path); body = b''
                if method == 'post': body = self.rfile.read(int(self.headers.get('Content-Length', '0')))
                if url.path.startswith('/verify/'):
                    if url.path in ('/verify/request', '/verify/direct'):
                        expected = 'q=ok&go=pick' if url.path.endswith('request') else 'q=ok'
                        valid = method == 'post' and body.decode() == expected
                    else:
                        _, _, wanted_method, choice = url.path.split('/')
                        expected = WORDS + ('&go=Send' if choice == 'first' else '&other=Other')
                        valid = method == wanted_method and (url.query == expected if method == 'get' else
                                url.query == 'kept=1' and body.decode() == expected)
                    if method == 'post': valid = valid and self.headers.get('Content-Type', '').split(';')[0] == 'application/x-www-form-urlencoded'
                    fixture.submissions.append(dict(method=method, path=self.path, body=body.decode(), valid=valid))
                    text, status = (PASS, 200) if valid else ('Form data FAIL', 422)
                elif url.path == '/controls': text, status = CONTROLS, 200
                elif url.path == '/request': text, status = REQUEST, 200
                elif url.path == '/direct': text, status = DIRECT, 200
                elif url.path.startswith('/static-') or url.path.startswith('/script-'):
                    mode, wanted_method, choice = url.path[1:].split('-')
                    text, status = page(mode, wanted_method, choice), 200
                else: text, status = 'Unexpected request', 404
                data = text.encode(); self.send_response(status)
                self.send_header('Content-Type', 'text/html; charset=utf-8'); self.send_header('Content-Length', str(len(data)))
                self.end_headers(); self.wfile.write(data)
            def do_GET(self): self.handle_request('get')
            def do_POST(self): self.handle_request('post')
        self.server = ThreadingHTTPServer((bind, port), Handler)
    def start(self):
        self.thread=Thread(target=self.server.serve_forever,daemon=True);self.thread.start();return self
    def close(self): self.server.shutdown();self.server.server_close();self.thread.join()


if __name__ == '__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--bind',default='127.0.0.1');p.add_argument('--port',type=int,default=31881)
    a=p.parse_args();f=Fixture(a.bind,a.port);print(f'http://{a.bind}:{f.server.server_port}',flush=True)
    try:f.server.serve_forever()
    except KeyboardInterrupt:pass
    finally:f.server.server_close()
