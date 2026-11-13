#!/usr/bin/env python3
"""Synthetic comic chapters: no downloaded artwork or account/session data."""
from collections import Counter
from functools import cache
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
import argparse
import json
import threading
import time
from urllib.parse import urlsplit

from PIL import Image, ImageDraw, ImageFont


@cache
def picture(name):
    width, height = (400, 2400) if name == 'long.jpg' else (640, 960)
    image = Image.new('RGB', (width, height), 'white')
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype('DejaVuSans.ttf', 28)
    for index, top in enumerate(range(0, height, 240)):
        color = [(50, 130, 210), (220, 100, 50), (50, 160, 90)][index % 3]
        draw.rectangle((8, top + 8, width - 9, min(top + 230, height - 1)), outline=color, width=6)
        draw.rectangle((25, top + 30, width - 26, top + 110), fill=color)
        draw.text((35, top + 42), f'{name} / panel {index + 1}', fill='white', font=font)
        draw.text((35, top + 140), 'Read small text clearly.', fill='black', font=font)
    data = BytesIO()
    image.save(data, 'PNG' if name.endswith('.png') else 'JPEG', quality=85)
    body = data.getvalue()
    return body[:-100] if name == 'broken.jpg' else body


class Fixture:
    def __init__(self, bind='127.0.0.1', port=0):
        self.counts = Counter()
        fixture = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_): pass

            def reply(self, body, mime='text/html; charset=utf-8', slow=False):
                if isinstance(body, str): body = body.encode()
                self.send_response(200)
                self.send_header('Content-Type', mime)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try:
                    if slow:
                        for at in range(0, len(body), 1024):
                            self.wfile.write(body[at:at+1024]); self.wfile.flush(); time.sleep(.05)
                    else: self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError): pass

            def do_GET(self):
                path = urlsplit(self.path).path
                fixture.counts[path] += 1
                if path in ('/', '/chapter1'):
                    self.reply("<head><title>Comic fixture</title><link rel='next' href='/chapter2'></head>"
                               "<p>Comic chapter one</p><img src='/icon.svg' width=25 height=25>"
                               "<img class='lazy' src='/placeholder.gif' data-original='/page1.jpg' width=640 height=960>"
                               "<img class='lazyload' src='/placeholder.gif' data-src='/page2.png' width=640 height=960>"
                               "<img src='/long.jpg' width=400 height=2400>"
                               "<img src='/slow.jpg' width=640 height=960>"
                               "<img src='/broken.jpg' width=640 height=960><p>End of chapter</p>")
                elif path == '/chapter2':
                    self.reply("<head><link rel='prev' href='/chapter1'></head><p>Second chapter</p><img src='/page3.jpg'>")
                elif path == '/counts': self.reply(json.dumps(fixture.counts), 'application/json')
                elif path == '/icon.svg':
                    # Entering the reader must cancel a slow unrelated logo,
                    # without requiring every visible page image to finish.
                    time.sleep(3)
                    self.reply('<svg/>', 'image/svg+xml')
                elif path.lstrip('/') in ('page1.jpg', 'page2.png', 'long.jpg', 'slow.jpg', 'broken.jpg', 'page3.jpg'):
                    name = path.lstrip('/')
                    self.reply(picture(name), 'image/png' if name.endswith('.png') else 'image/jpeg', name == 'slow.jpg')
                else:
                    self.send_error(404)

        self.server = ThreadingHTTPServer((bind, port), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    def start(self):
        self.thread.start(); return self

    def close(self):
        self.server.shutdown(); self.server.server_close(); self.thread.join()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bind', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=31878)
    args = parser.parse_args()
    fixture = Fixture(args.bind, args.port).start()
    print(f'http://{args.bind}:{fixture.server.server_port}/chapter1', flush=True)
    try: threading.Event().wait()
    except KeyboardInterrupt: pass
    finally: fixture.close()
