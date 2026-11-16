"""Original images served with CDN MIME aliases and HTTP content codings."""
from collections import Counter
import gzip
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
import time
from urllib.parse import urlsplit
import zlib

from image_fixtures import fixtures


class Fixture:
    def __init__(self, port=0):
        self.counts = Counter()
        from io import BytesIO
        from random import Random
        from PIL import Image
        pixels = bytearray(Random(21).randbytes(640*960*3))
        pixels[0::3] = bytes(192+(value>>2) for value in pixels[0::3])
        pixels[1::3] = bytes(value>>1 for value in pixels[1::3])
        pixels[2::3] = bytes(value>>1 for value in pixels[2::3])
        encoded = BytesIO()
        Image.frombytes('RGB', (640, 960), bytes(pixels)).save(encoded, 'JPEG', quality=98, subsampling=0)
        self.large_bytes = encoded.getvalue()
        assert 524288 < len(self.large_bytes) < 2097152
        fixture = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_GET(self):
                path = urlsplit(self.path).path
                fixture.counts[path] += 1
                cases = {
                    '/small': ('/normal.jpg', None),
                    '/mime': ('/alias.jpg', None),
                    '/plain': ('/plain.jpg', None),
                    '/gzip': ('/gzip.jpg', None),
                    '/deflate': ('/deflate.png', None),
                    '/broken': ('/broken.jpg', None),
                    '/bomb': ('/bomb.jpg', None),
                    '/slow': ('/slow.jpg', None),
                    '/html': ('/html.jpg', None),
                    '/large': ('/large.jpg', None),
                    '/oversize': ('/oversize.jpg', None),
                }
                images = {
                    '/normal.jpg': ('photo.jpg', 'image/jpeg', ''),
                    '/alias.jpg': ('photo.jpg', 'image/jpg', ''),
                    '/plain.jpg': ('photo.jpg', 'text/plain', ''),
                    '/gzip.jpg': ('photo.jpg', 'image/jpeg', 'gzip'),
                    '/deflate.png': ('alpha.png', 'image/x-png', 'deflate'),
                    '/broken.jpg': ('photo.jpg', 'image/jpeg', 'gzip'),
                    '/bomb.jpg': ('photo.jpg', 'image/jpeg', 'gzip'),
                    '/slow.jpg': ('photo.jpg', 'image/jpeg', 'gzip'),
                    '/direct-alias': ('photo.jpg', 'image/pjpeg', ''),
                    '/direct-gzip': ('photo.jpg', 'image/jpeg', 'gzip'),
                }
                coding = ''
                if path in cases:
                    body = ('<title>Image response</title><img src="' +
                            cases[path][0] + '" width=320 height=180>').encode()
                    mime = 'text/html; charset=utf-8'
                elif path in ('/large.jpg', '/oversize.jpg'):
                    mime = 'image/jpeg'
                    body = fixture.large_bytes
                    if path == '/oversize.jpg':
                        body += b'x' * 16777216
                elif path == '/html.jpg':
                    mime = 'image/jpeg'
                    body = b'<script>throw Error("not an image")</script>'
                elif path in images:
                    name, mime, coding = images[path]
                    body = fixtures()[name]
                    if path == '/bomb.jpg':
                        body += b'x' * 524288
                    if path == '/slow.jpg':
                        # Random padding keeps compressed input streaming long
                        # enough to cancel while the temporary source is open.
                        from random import Random
                        body += Random(17).randbytes(32768)
                    if coding == 'gzip':
                        body = gzip.compress(body, mtime=0)
                    elif coding == 'deflate':
                        body = zlib.compress(body)
                    if path == '/broken.jpg':
                        body = body[:-5]
                else:
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header('Content-Type', mime)
                self.send_header('Content-Length', str(len(body)))
                if coding:
                    self.send_header('Content-Encoding', coding)
                self.end_headers()
                try:
                    if path == '/slow.jpg':
                        for at in range(0, len(body), 512):
                            self.wfile.write(body[at:at+512])
                            self.wfile.flush()
                            time.sleep(.06)
                    else:
                        self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass

        self.server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
        self.thread = Thread(target=self.server.serve_forever, daemon=True)

    def start(self):
        self.thread.start()
        return self

    def close(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
