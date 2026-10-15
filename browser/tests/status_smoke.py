#!/usr/bin/env python3
"""Real PXA pixels: spare bottom space, overlay expiry and loading visibility."""
import argparse,json,time
from pathlib import Path
from PIL import Image,ImageChops
from harness import Browser

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['simulator','package','publisher-key','ime-package','output']:p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--fixture-url',required=True)
 a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);results=[]
 for name,w,h,dpi,corner,circle in [('small',296,240,160,58,False),('round-high-dpi',480,480,320,240,True)]:
  out=a.output/name;b=Browser(a.simulator,a.package,a.publisher_key,a.ime_package,out,width=w,height=h,dpi=dpi,corner=corner,round_screen=circle)
  def crop(name):
   b.shot(name);x,y,w,h=b.box('status');return Image.open(out/(name+'.png')).convert('RGB').crop((x,y,x+w,y+h))
  try:
   b.wait(lambda:'BROWSER DONE status=200' in b.logs())
   b.nav(2);visible=crop('visible');status=b.box('status');toolbar=b.box('toolbar');content=b.box('content')
   below=status[1]>=toolbar[1]+toolbar[3];assert below==circle,(status,toolbar)
   assert content[1]+content[3] == toolbar[1]-b.dp(3)
   time.sleep(4.4);hidden=crop('hidden')
   assert ImageChops.difference(visible,hidden).getbbox(), 'status did not disappear'
   time.sleep(.5);assert not ImageChops.difference(hidden,crop('idle')).getbbox(),'hidden status kept changing'
   b.nav(2);assert ImageChops.difference(hidden,crop('repeated-message')).getbbox(),'repeated status stayed hidden'
   if name=='small':
    mark=len(b.logs());b.navigate(a.fixture_url.rstrip('/')+'/status-wait')
    b.wait(lambda:'BROWSER NAV '+a.fixture_url.rstrip('/')+'/status-wait' in b.logs()[mark:])
    time.sleep(4.5);loading=crop('loading-after-four-seconds')
    assert 'BROWSER DONE status=200' not in b.logs()[mark:],'fixture ended too soon'
    assert loading.getextrema() != ((247,247),(249,249),(252,252)),'loading status disappeared'
    b.nav(3);crop('stopped');time.sleep(4.4);crop('stopped-hidden')
   assert 'BROWSER DRAW ERROR'not in b.logs() and 'BROWSER ERROR'not in b.logs()
   results.append(dict(profile=name,status_below_toolbar=below,status_box=status,content_box=content,auto_hidden=True,repeated_message=True,passed=True))
   (a.output/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(name,'PASS',flush=True)
  finally:b.close()
if __name__=='__main__':main()
