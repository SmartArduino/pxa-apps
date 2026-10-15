#!/usr/bin/env python3
"""Real input -> masked password -> POST -> cookies -> redirect -> protected page.

Uses only fixture credentials. Never supply a real account to this test or
save real credentials in fixtures, screenshots, command arguments or logs.
"""
import argparse,json,time
from pathlib import Path
from harness import Browser

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['simulator','package','publisher-key','ime-package','output']:p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--fixture-url',default='http://127.0.0.1:31878/')
 a=p.parse_args();b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output)
 root=a.fixture_url.rstrip('/');results=[]
 def done(mark,status=200):b.wait(lambda:f'BROWSER DONE status={status}' in b.logs()[mark:])
 def go(path):
  mark=len(b.logs());b.navigate(root+path);done(mark)
 def edit(index,value):
  x,y,w,h=b.box('content');b.tap(x+w//2,y+index*30+15)
  time.sleep(.4)
  for at in range(0,len(value),30):b.command('TEXT '+value[at:at+30]);time.sleep(.1)
  if index==1:b.shot('password-masked')
  x,y,w,h=b.box('address');b.tap(x+w-15,y+h//2);time.sleep(.5)
 def menu_end():
  b.nav(4);x,y,w,h=b.box('content');edge=x+w-3
  b.command(f'TOUCH DOWN 0 {edge} {y+3}');time.sleep(.08)
  b.command(f'TOUCH MOVE 0 {edge} {y+h-1}');time.sleep(.12)
  b.command(f'TOUCH UP 0 {edge} {y+h-1}');time.sleep(.25)
 def row(index,count=9):
  x,y,w,h=b.box('content');offset=max(0,count*35-h)
  b.tap(x+w//2,y+index*35-offset+16)
 try:
  b.wait(lambda:'BROWSER METRICS' in b.logs())
  go('/session/login');b.shot('login-form')
  edit(0,'tester');edit(1,'a&b=+pw');b.shot('filled-form')
  mark=len(b.logs());x,y,w,h=b.box('content');b.tap(x+w//2,y+75);done(mark)
  assert 'BROWSER ERROR' not in b.logs()[mark:]
  b.shot('logged-in');results.append('password input, special-character POST, duplicate Set-Cookie and 303 login redirect')
  mark=len(b.logs());b.nav(3);done(mark);b.shot('session-reload')
  results.append('protected page GET retains session')
  menu_end();b.shot('menu-more');row(5);b.shot('history')
  # Newest protected page entry is first, followed by the previous home.
  x,y,w,h=b.box('content');b.tap(x+w//2,y+51);time.sleep(.6);b.shot('history-open')
  results.append('menu history list opens selected entry')
  menu_end();row(5);x,y,w,h=b.box('content');b.tap(x+w//2,y+16);b.shot('history-cleared')
  b.command('KEY BACK');time.sleep(.3);b.nav(4) # Close tools.
  menu_end();row(8);b.shot('sessions-cleared');b.nav(4)
  mark=len(b.logs());b.nav(3);done(mark,401);b.shot('session-expired')
  results.append('clear sessions removes cookies; protected page rejects after reload')
  # Toggle images and GIF separately; atomic preferences persist both flags.
  menu_end();row(6);row(7)
  b.wait(lambda:any('#images 0\n' in p.read_text() and '#gif 0\n' in p.read_text() for p in b.state.rglob('bookmarks.txt')))
  b.shot('image-settings');results.append('independent image/GIF settings persisted')
  b.nav(2);x,y,w,h=b.box('content');b.tap(x+w//2,y+8+38+16);b.shot('site-directory')
  results.append('home navigation directory opens without replacing custom sites')
  assert 'a&b=+pw' not in b.logs() and 'a%26b%3D%2Bpw' not in b.logs()
  (a.output/'results.json').write_text(json.dumps({'passed':results},ensure_ascii=False,indent=2))
  print('session smoke PASS:',len(results),'checks',flush=True)
 finally:b.close()
if __name__=='__main__':main()
