#!/usr/bin/env python3
"""Bounded public-content fixtures; bind LAN explicitly for physical-device tests."""
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import gzip,zlib,json,time,argparse
PAGE='<title>Compatibility</title><h1>Compatibility</h1><p>Fixture server is ready.</p>'+''.join('<p><a href="'+path+'">'+label+'</a></p>'for path,label in [('/gzip','Gzip page'),('/gzip-css','Large compressed CSS'),('/deflate','Deflate page'),('/compressed-script','Compressed JavaScript'),('/large','Large text'),('/json','JSON'),('/markdown','Markdown'),('https://example.com/','Example HTTPS')])
TEXT='<title>Compression works</title><h1>Compression works</h1><p>中文可以阅读，gzip and deflate streaming.</p><p><a href="/compat">Back</a></p>'
SCRIPT='<title>Compressed script</title><h1 id="result">Waiting for script</h1><script src="/script.js"></script><p><a href="/compat">Back</a></p>'
class Handler(BaseHTTPRequestHandler):
 protocol_version='HTTP/1.1'
 def log_message(self,fmt,*args):print(json.dumps({'path':self.path,'accept_encoding':self.headers.get('Accept-Encoding'),'status':fmt%args}),flush=True)
 def do_GET(self):
  path=self.path.split('?')[0];mime='text/html; charset=utf-8';coding=None;data=TEXT.encode()
  if path in ['/', '/compat']:data=PAGE.encode()
  elif path=='/gzip':coding='gzip';data=gzip.compress(data,mtime=0)
  elif path=='/gzip-css':coding='gzip';data=gzip.compress(('<head><style>'+('x'*1572864)+'</style></head>'+TEXT).encode(),mtime=0)
  elif path=='/deflate':coding='deflate';data=zlib.compress(data)
  elif path=='/compressed-script':data=SCRIPT.encode()
  elif path=='/script.js':mime='application/javascript';coding='gzip';data=gzip.compress(b"document.getElementById('result').textContent='Compressed script works';",mtime=0)
  elif path=='/large':data=('<title>Large</title>'+('<p>'+('Bounded memory 中文 '*30)+'</p>')*500).encode()
  elif path=='/json':mime='application/json';data=json.dumps({'message':'JSON works 中文'},ensure_ascii=False).encode();coding='gzip';data=gzip.compress(data,mtime=0)
  elif path=='/markdown':mime='text/markdown';data=b'# Markdown works\nLightweight text response.\n'
  elif path=='/broken':coding='gzip';data=gzip.compress(data,mtime=0)[:-4]
  else:self.send_error(404);return
  self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)))
  if coding:self.send_header('Content-Encoding',coding)
  self.end_headers()
  try:
   for at in range(0,len(data),197):self.wfile.write(data[at:at+197]);self.wfile.flush()
  except(BrokenPipeError,ConnectionResetError):pass
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--bind',default='127.0.0.1');p.add_argument('--port',type=int,default=31878);a=p.parse_args()
 print(f'http://{a.bind}:{a.port}/compat',flush=True)
 ThreadingHTTPServer((a.bind,a.port),Handler).serve_forever()
