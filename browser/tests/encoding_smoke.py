#!/usr/bin/env python3
"""Verify real PXA decoding/UI with deterministic pages and optional live URLs.

Use a TRACE package. External URLs are supplied explicitly as a JSON list of
{name,url}; they are not performance baselines. No login credentials are used.
"""
import argparse,io,json,re,subprocess,time
from pathlib import Path
from PIL import Image
from harness import Browser

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['simulator','package','publisher-key','ime-package','output']:p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--fixture-url',required=True)
 p.add_argument('--sites',type=Path)
 a=p.parse_args();b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,a.output)
 results={'fixture':[],'sites':[]}
 def ocr(name):
  b.shot(name);im=Image.open(a.output/(name+'.png')).convert('RGB');buf=io.BytesIO();im.resize((im.width*3,im.height*3)).save(buf,format='PNG')
  return subprocess.check_output(['tesseract','stdin','stdout','-l','chi_sim+eng','--psm','6'],input=buf.getvalue(),stderr=subprocess.DEVNULL).decode(errors='replace').replace(' ','')
 def go(url,name,completed=True):
  mark=len(b.logs());b.navigate(url)
  if completed:b.wait(lambda:'BROWSER DONE status=200' in b.logs()[mark:] or 'BROWSER ERROR' in b.logs()[mark:],timeout=60)
  time.sleep(1)
  return b.logs()[mark:],ocr(name)
 try:
  b.wait(lambda:'BROWSER DONE status=200' in b.logs())
  for path in ['encoding','encoding-header']:
   logs,text=go(a.fixture_url.rstrip('/')+'/'+path,path)
   assert 'BROWSER DONE status=200' in logs and 'BROWSER ERROR' not in logs,logs
   assert '网址导航' in text and '手机漫画' in text and '正在加载' not in text,text
   results['fixture'].append({'page':path,'chinese_visible':True,'hidden_loading_absent':True})
  logs,text=go(a.fixture_url.rstrip('/')+'/encoding-bom','encoding-bom')
  assert '中文正常' in text,text
  results['fixture'].append({'page':'encoding-bom','bom_overrides_header':True})
  logs,text=go(a.fixture_url.rstrip('/')+'/encoding-unsupported','encoding-unsupported',False)
  assert '不支持' in text and '此页面' in text,text
  results['fixture'].append({'page':'encoding-unsupported','explicit_warning':True})
  mark=len(b.logs());b.navigate(a.fixture_url.rstrip('/')+'/slow');time.sleep(.7)
  text=ocr('streaming-first-paragraph')
  assert '持续加载' in text and 'BROWSER DONE' not in b.logs()[mark:],text
  b.nav(3)
  results['fixture'].append({'page':'slow','paragraph_visible_before_eof':True})
  if a.sites:
   for item in json.loads(a.sites.read_text()):
    attempts=[]
    for attempt in range(2):
     logs,text=go(item['url'],item['name']+('-retry' if attempt else ''))
     attempts.append(dict(done='BROWSER DONE status=200' in logs,errors=re.findall(r'BROWSER ERROR[^\n]*',logs)))
     if attempts[-1]['done']:break
    row=dict(item,done=attempts[-1]['done'],attempts=attempts,ocr=text,statistics=re.findall(r'BROWSER DONE[^\n]*',logs))
    results['sites'].append(row)
    if 'expect' in item:assert item['expect'] in text,row
  b.nav(2);x,y,w,h=b.box('content');b.tap(x+w//2,y+8+38+16);b.shot('directory-top')
  x,y,w,h=b.box('content');b.command(f'TOUCH DOWN 0 {x+w//2} {y+h-10}');time.sleep(.1);b.command(f'TOUCH MOVE 0 {x+w//2} {y+10}');time.sleep(.1);b.command(f'TOUCH UP 0 {x+w//2} {y+10}');time.sleep(.5);b.shot('directory-bottom')
  results['fixture_passed']=True;results['passed']=all(r['done'] for r in results['sites']);print('PXA encoding smoke PASS; live sites:',sum(r['done'] for r in results['sites']),'/',len(results['sites']),flush=True)
 finally:
  (a.output/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n');b.close()
if __name__=='__main__':main()
