"""Bounded HTTP header/body failures, redirects and POST request accounting."""
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
from urllib.parse import urlsplit


class Fixture:
    def __init__(self):
        self.gets = Counter(); self.posts = Counter(); fixture = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_): pass

            def reply(self, body, policy=None, status=200, location=None):
                if isinstance(body, str): body = body.encode()
                self.send_response(status)
                self.send_header('Content-Type', 'text/html; charset=utf-8')
                if policy: self.send_header('Content-Security-Policy', policy)
                if location: self.send_header('Location', location)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try: self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError): pass

            def do_GET(self):
                path = urlsplit(self.path).path; fixture.gets[path] += 1
                if path == '/ok':
                    self.reply('<title>正常脚本</title><p>普通网页仍执行脚本</p><script>fetch("/script-proof")</script>')
                elif path == '/body-large': self.reply(b'x' * 524289)
                elif path == '/header-large':
                    self.reply('<title>大响应头</title><p>回退后仍可阅读</p><script>fetch("/trap")</script>',
                               "script-src 'none';" + ';' * 5000)
                elif path == '/both-large': self.reply(b'x' * 524289, "script-src 'none';" + ';' * 5000)
                elif path == '/policy':
                    self.reply('<title>安全策略</title><p>策略页保持可读</p><script>fetch("/trap")</script>', "script-src 'none'")
                elif path == '/redirect-body': self.reply('', status=302, location='/body-large')
                elif path == '/form':
                    self.reply('<form method=post action=/post-large><input type=submit value="发送验证"></form>')
                elif path in ('/script-proof', '/trap'): self.reply('ok')
                else: self.send_error(404)

            def do_POST(self):
                path = urlsplit(self.path).path; fixture.posts[path] += 1
                self.rfile.read(int(self.headers.get('Content-Length', '0')))
                self.reply(b'x' * 524289, "script-src 'none';" + ';' * 5000)

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.thread = Thread(target=self.server.serve_forever, daemon=True)

    def start(self): self.thread.start(); return self
    def close(self): self.server.shutdown(); self.server.server_close(); self.thread.join()
