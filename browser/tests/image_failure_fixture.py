"""Original synthetic images for request accounting and failed-image recovery."""
from collections import Counter
from functools import cache
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
from random import Random
from threading import Thread
import time
from urllib.parse import urlsplit

from PIL import Image
from comic_fixture import picture


@cache
def oversized():
    image = Image.frombytes('RGB', (1024, 1024), Random(1).randbytes(1024 * 1024 * 3))
    data = BytesIO(); image.save(data, 'JPEG', quality=93)
    body = data.getvalue()
    assert len(body) > 524288
    return body


@cache
def gif():
    image = Image.new('RGB', (200, 200), (50, 130, 210))
    data = BytesIO(); image.save(data, 'GIF')
    return data.getvalue()


class Fixture:
    def __init__(self):
        self.counts = Counter(); fixture = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_): pass

            def reply(self, body, mime='text/html; charset=utf-8', status=200, slow=False):
                if isinstance(body, str): body = body.encode()
                self.send_response(status)
                self.send_header('Content-Type', mime)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try:
                    if slow:
                        for at in range(0, len(body), 1024):
                            self.wfile.write(body[at:at+1024]); self.wfile.flush(); time.sleep(.08)
                    else: self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError): pass

            def do_GET(self):
                path = urlsplit(self.path).path; fixture.counts[path] += 1
                sources = {'/permanent': 'unsupported.jpg', '/limited': 'oversized.jpg',
                           '/transient': 'flaky.jpg', '/manual': 'manual.jpg',
                           '/cancelled': 'slow.jpg', '/reorder': 'unsupported.jpg',
                           '/gif-disabled': 'page.gif'}
                if path in sources:
                    button = '<button onclick="flip()" style="color:#d06020">重新排序</button>' if path == '/reorder' else ''
                    body = ('<title>图片恢复验证</title>' + button + '<div id=pictures>'
                            '<img src="/' + sources[path] + '" width=640 height=960>'
                            '<img src="/ok.jpg" width=640 height=960></div>')
                    if path == '/reorder':
                        body += '''<script>function flip(){document.getElementById('pictures').innerHTML=
                            '<img src="/ok.jpg" width=640 height=960><img src="/unsupported.jpg" width=640 height=960>'}</script>'''
                    self.reply(body)
                elif path == '/unsupported.jpg': self.reply('<p>No image</p>')
                elif path == '/oversized.jpg': self.reply(oversized(), 'image/jpeg')
                elif path == '/invalid.jpg': self.reply(b'Not an image. '*32, 'image/jpeg')
                elif path == '/streamed-oversized.jpg':
                    # Without Content-Length, navigation reaches its image
                    # decoder before the streamed body exceeds the byte cap.
                    self.protocol_version = 'HTTP/1.1'
                    self.send_response(200)
                    self.send_header('Content-Type', 'image/jpeg')
                    self.send_header('Transfer-Encoding', 'chunked')
                    self.send_header('Connection', 'close')
                    self.end_headers()
                    body = oversized()
                    try:
                        for at in range(0, len(body), 16384):
                            chunk = body[at:at+16384]
                            self.wfile.write(f'{len(chunk):x}\r\n'.encode() + chunk + b'\r\n')
                        self.wfile.write(b'0\r\n\r\n')
                    except (BrokenPipeError, ConnectionResetError): pass
                elif path == '/page.gif': self.reply(gif(), 'image/gif')
                elif path in ('/flaky.jpg', '/manual.jpg') and fixture.counts[path] == 1:
                    self.reply('Try again', 'text/plain', status=503)
                elif path in ('/ok.jpg', '/flaky.jpg', '/manual.jpg', '/slow.jpg'):
                    self.reply(picture('page1.jpg'), 'image/jpeg', slow=path == '/slow.jpg')
                else: self.send_error(404)

        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.thread = Thread(target=self.server.serve_forever, daemon=True)

    def start(self): self.thread.start(); return self
    def close(self): self.server.shutdown(); self.server.server_close(); self.thread.join()
