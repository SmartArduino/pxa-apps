#!/usr/bin/env python3
"""Run script_fixture.py separately. Test the real Wasm/AOT browser UI."""
import argparse,json,re,time
from pathlib import Path
from harness import Browser
p=argparse.ArgumentParser()
for key in ('simulator','package','publisher-key','ime-package','output'):p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--fixture-url',default='http://127.0.0.1:31925')
p.add_argument('--width',type=int,default=296);p.add_argument('--height',type=int,default=240);p.add_argument('--dpi',type=int,default=160);p.add_argument('--corner',type=int,default=58);p.add_argument('--round',action='store_true');a=p.parse_args()
b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output,a.width,a.height,a.dpi,a.corner,round_screen=a.round)
try:
 b.wait(lambda:'BROWSER VIEWPORT' in b.logs());b.shot('home')
 def load(path,script=True):
  mark=len(b.logs());b.navigate(a.fixture_url+path)
  b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:],timeout=35)
  if script:b.wait(lambda:'BROWSER JS READY' in b.logs()[mark:],timeout=35)
  return mark
 load('/js');b.wait(lambda:'BROWSER JS UPDATE revision=2' in b.logs());b.shot('network')
 x,y,w,h=b.box('content');mark=len(b.logs());b.tap(x+a.width//8,y+b.dp(42))
 b.wait(lambda:'BROWSER JS UPDATE' in b.logs()[mark:]);b.shot('click')
 mark=load('/js-inline');b.wait(lambda:'BROWSER JS UPDATE revision=1' in b.logs()[mark:])
 x,y,w,h=b.box('content')
 for revision in (2,3):
  b.tap(x+b.dp(12),y+b.dp(8));b.wait(lambda:f'BROWSER JS UPDATE revision={revision}' in b.logs()[mark:])
 b.shot('inline-replaced');b.tap(x+b.dp(12),y+b.dp(8));time.sleep(.6)
 assert 'BROWSER JS UPDATE revision=4' not in b.logs()[mark:], 'Removed inline handler ran again'
 assert 'BROWSER JS ERROR' not in b.logs()[mark:], 'Inline handler compiler or cache failed'
 mark=load('/js-inline-stop');b.wait(lambda:'BROWSER JS UPDATE revision=1' in b.logs()[mark:]);x,y,w,h=b.box('content')
 b.tap(x+b.dp(12),y+b.dp(8));b.wait(lambda:'BROWSER JS UPDATE revision=2' in b.logs()[mark:])
 b.tap(x+b.dp(12),y+b.dp(8));time.sleep(.4)
 assert 'BROWSER JS ERROR' not in b.logs()[mark:], 'Unvisited ancestor handler was compiled'
 for path in ['/js-hang','/js-regexp','/js-oom']:
  mark=load(path,False)
  reason='脚本内存达到上限' if path=='/js-oom' else '脚本执行超时'
  b.wait(lambda:'BROWSER JS ERROR '+reason in b.logs()[mark:]);b.shot(path[1:])
  # The browser survives and can navigate again through its normal toolbar.
  load('/static-scroll',False)
  assert not re.search(r'BROWSER (ERROR|DRAW ERROR)',b.logs()[mark:])
 mark=load('/js-stack');assert re.search(r'BROWSER JS READY[^\n]* errors=1',b.logs()[mark:]);b.shot('js-stack')
 mark=load('/js-parser');assert re.search(r'BROWSER JS READY[^\n]* errors=1',b.logs()[mark:]);b.shot('js-parser')
 for path in ('/js-projection-small','/js-projection-large'):
  mark=load(path)
  b.wait(lambda:re.search(r'BROWSER IMAGE DONE[^\n]*pixels=180x180',b.logs()[mark:]),timeout=20)
  b.wait(lambda:len(re.findall('BROWSER JS UPDATE',b.logs()[mark:]))>=3)
  time.sleep(.4);b.shot(path[1:])
  assert len(re.findall('BROWSER IMAGE DONE',b.logs()[mark:]))==1, 'Projection restarted inline image decoding'
  assert not re.search(r'BROWSER (?:JS|IMAGE) ERROR',b.logs()[mark:]), 'Long attribute projection failed'
 for path in ['/js-image','/js-gif']:
  mark=load(path)
  b.wait(lambda:'BROWSER IMAGE DONE' in b.logs()[mark:],timeout=20)
  if path=='/js-gif':b.wait(lambda:'BROWSER GIF FRAME' in b.logs()[mark:])
  b.wait(lambda:len(re.findall('BROWSER JS UPDATE',b.logs()[mark:]))>=3)
  time.sleep(.5);b.shot(path[1:])
  assert len(re.findall('BROWSER IMAGE DONE',b.logs()[mark:]))==1, 'DOM changes restarted image download'
  assert not re.search(r'BROWSER (IMAGE|GIF) ERROR',b.logs()[mark:])
 mark=load('/js-two-images')
 b.wait(lambda:'BROWSER IMAGE DONE' in b.logs()[mark:],timeout=20)
 x,y,w,h=b.box('content');cx=x+w//2
 for _ in range(4):
  if len(re.findall('BROWSER IMAGE DONE',b.logs()[mark:]))>=2:break
  b.command(f'TOUCH DOWN 0 {cx} {y+h*4//5}');time.sleep(.06)
  b.command(f'TOUCH MOVE 0 {cx} {y+h//5}');time.sleep(.08)
  b.command(f'TOUCH UP 0 {cx} {y+h//5}');time.sleep(.25)
 b.wait(lambda:len(re.findall('BROWSER IMAGE DONE',b.logs()[mark:]))==2,timeout=20)
 b.wait(lambda:len(re.findall('BROWSER JS UPDATE',b.logs()[mark:]))>=3)
 time.sleep(.3);b.shot('js-two-images')
 assert len(re.findall('BROWSER IMAGE DONE',b.logs()[mark:]))==2, 'DOM changes reloaded duplicate images'
 assert not re.search(r'BROWSER (IMAGE|GIF) ERROR',b.logs()[mark:])
 mark=load('/js-image-error');b.wait(lambda:'BROWSER IMAGE ERROR' in b.logs()[mark:])
 time.sleep(.8);assert len(re.findall('BROWSER IMAGE ERROR',b.logs()[mark:]))==1, 'DOM changes retried failed image'
 load('/js-scroll');x,y,w,h=b.box('content')
 cx=x+w//2;start=y+h*4//5;end=y+h//5
 b.command(f'TOUCH DOWN 0 {cx} {start}');time.sleep(.08);mark=len(b.logs())
 for i in range(48):b.command(f'TOUCH MOVE 0 {cx} {start+(end-start)*i//47}');time.sleep(.025)
 assert 'BROWSER JS UPDATE' not in b.logs()[mark:], 'script painted during drag'
 b.command(f'TOUCH UP 0 {cx} {end}');time.sleep(.3);b.shot('scroll')
 load('/static-scroll',False);time.sleep(.3)
 idle=len(re.findall('BROWSER PERF',b.logs()));time.sleep(.4);assert idle==len(re.findall('BROWSER PERF',b.logs()))
 assert 'PXA Guest exception:' not in b.logs(), 'Guest trapped instead of reporting a bounded JS exception'
 (a.output/'result.json').write_text(json.dumps({'passed':True,'width':a.width,'height':a.height,'dpi':a.dpi,'round':a.round,'js':re.findall('BROWSER JS[^\n]*',b.logs()),'profiles':re.findall('BROWSER PERF[^\n]*',b.logs())},ensure_ascii=False,indent=2))
 print('Guest script/network/limits/navigation/drag/idle smoke passed')
finally:b.close()
