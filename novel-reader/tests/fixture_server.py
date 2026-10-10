"""Deterministic CC0 book server: range/chapters, search and failure cases."""
import argparse,json,re,time
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import urlparse,parse_qs
body=('第一章 林间来信\n\n'+'清晨的风沿着山路吹来，白鸟带来一封信。\n'*1700+'\n第二章 河边书屋\n\n'+'午后的河水映着云影，书屋的铜铃轻轻响了。\n'*1500+'\n第三章 未完的旅程\n\n'+'林舟收好书本，沿着小路走向远方。\n'*1400).encode()
class Handler(BaseHTTPRequestHandler):
 fail_refresh=False
 def do_GET(self):
  u=urlparse(self.path);p=u.path
  if p=='/control/refresh-failure':
   Handler.fail_refresh=parse_qs(u.query).get('enabled',['0'])[0]=='1'
   return self.reply(200,b'OK')
  if p=='/catalog.json':
   data={'books':[{'title':'林间长篇（验收）','author':'书页 CC0','url':'/book.txt'},{'title':'分章小说','author':'书页 CC0','toc':'/toc.json'},{'title':'中断下载','url':'/broken.txt'},{'title':'变化正文','url':'/changing.txt'},{'title':'空目录','toc':'/empty.json'},{'title':'错误编码','url':'/invalid.txt'}]}
   key=parse_qs(u.query).get('q',[''])[0]
   if key:data['books']=[b for b in data['books'] if key in b['title']]
   return self.reply(200,json.dumps(data,ensure_ascii=False).encode(),mime='application/json')
  if p=='/source.json':return self.reply(200,json.dumps({'format':'pxa-reader-source-1','name':'本地 CC0 书源','kind':'json','searchUrl':'/catalog.json?q={{key}}'},ensure_ascii=False).encode(),mime='application/json')
  if p=='/toc.json':return self.reply(200,json.dumps({'chapters':[{'title':f'第{i}章 旅途','url':f'/chapter{i}.txt'} for i in range(1,4)]},ensure_ascii=False).encode(),mime='application/json')
  if p.startswith('/chapter'):return self.reply(200,('山间的书屋打开了门。\n'*60).encode())
  if p=='/empty.json':return self.reply(200,b'{"chapters":[]}',mime='application/json')
  if p in ['/book.txt','/broken.txt','/changing.txt','/invalid.txt']:
   content=body if p!='/invalid.txt' else b'not utf8\xff\xfe\n'
   match=re.fullmatch(r'bytes=(\d+)-(\d+)',self.headers.get('range',''))
   if not match:return self.reply(200,content)
   start,end=map(int,match.groups());end=min(end,len(content)-1)
   if start>=len(content):return self.reply(416,b'')
   if p=='/book.txt' and start and Handler.fail_refresh:return self.reply(503,b'try again')
   if p=='/broken.txt' and start:return self.reply(503,b'try again')
   tag='"fixture-v2"' if p=='/changing.txt' and start else '"fixture-v1"'
   if p=='/book.txt':time.sleep(.05)
   return self.reply(206,content[start:end+1],range=f'bytes {start}-{end}/{len(content)}',etag=tag)
  return self.reply(404,b'not found')
 def reply(self,status,data,mime='text/plain; charset=utf-8',range=None,etag=None):
  self.send_response(status);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(data)))
  if range:self.send_header('Content-Range',range)
  if etag:self.send_header('ETag',etag)
  self.end_headers();self.wfile.write(data)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',type=int,default=18764);p.add_argument('--bind',default='127.0.0.1');a=p.parse_args()
 print(f'CC0 fixture bytes={len(body)}',flush=True);ThreadingHTTPServer((a.bind,a.port),Handler).serve_forever()
