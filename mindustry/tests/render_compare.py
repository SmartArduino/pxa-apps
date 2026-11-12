#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compare identical input scenes in the real PXA Host, with bounded profiling.
These are desktop measurements; they do not predict ESP display throughput.
"""
import argparse,io,json,os,re,signal,socket,subprocess,time
from pathlib import Path
from PIL import Image,ImageChops
from simulator import command
from ui_controls import Hud

def run(args,label,package):
 output=args.output/label;output.mkdir(parents=True,exist_ok=True);state=output/'state';state.mkdir(exist_ok=True)
 sock=Path(f'/tmp/md-perf-{os.getpid()}-{label}.sock');logpath=output/'runtime.log';stages={}
 argv=[str(args.simulator),'--package',str(package),'--publisher-key',str(args.publisher_key),'--state-root',str(state),'--width','320','--height','240','--corner-radius','0','--safe-insets','0,0,0,0','--pxadb-control-socket',str(sock)]
 with logpath.open('wb') as log:
  proc=subprocess.Popen(argv,stdout=log,stderr=subprocess.STDOUT,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PXA_SIM_NAV_MODE='gesture-no-bar'),start_new_session=True)
  def logs():return logpath.read_text(errors='replace')
  def wait(predicate):
   end=time.monotonic()+30
   while time.monotonic()<end:
    assert proc.poll() is None,logs()[-3000:]
    if predicate():return
    time.sleep(.05)
   raise TimeoutError(label)
  def tap(x,y):
   command(sock,f'TOUCH DOWN 1 {x} {y}');time.sleep(.07);command(sock,f'TOUCH UP 1 {x} {y}');time.sleep(.35)
  def stage(name):
   offset=len(logs());time.sleep(7)
   lines=[x for x in logs()[offset:].splitlines() if ('Mindustry perf' in x or 'Mindustry legacy perf' in x)]
   assert len(lines)>=2,logs()[-3000:]
   stats=[{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)} for line in lines[1:]]
   stages[name]={'samples':stats,'median_cpu_us':sorted(x['cpu_us'] for x in stats)[len(stats)//2]}
   header,sep,png=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n');assert sep and header==f'PNG {len(png)}'.encode();assert Image.open(io.BytesIO(png)).size==(320,240);(output/f'{name}.png').write_bytes(png)
   if name=='title':
    time.sleep(.15);png2=command(sock,'SCREENSHOT AFTER_PRESENT').partition(b'\n')[2];assert ImageChops.difference(Image.open(io.BytesIO(png)).convert('RGB'),Image.open(io.BytesIO(png2)).convert('RGB')).getbbox() is None,'Static title accumulates alpha or stale pixels'
  try:
   wait(lambda:sock.exists() and 'Pocket: ready app=' in logs())
   if label=='gamerender':wait(lambda:'glyph cache ready' in logs())
   stage('title');tap(160,81);tap(160,60);stage('world')
   hud=Hud(320,240,tap);hud.tools(3)
   # Tool drawer stays open for repeated zoom presses; fold it for capture.
   top=240-4*hud.n-20
   tap(8+(320-12)//2+(320-20)//4,top+4+(hud.n+2)+hud.n//2)
   hud.dock(3);stage('zoom8')
   command(sock,'KEY HOME');proc.wait(timeout=8);assert proc.returncode==0
   assert not re.search(r'Guest exception|out of bounds memory|submit failed|glyph failed',logs(),re.I)
  finally:
   if proc.poll() is None:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=5)
   sock.unlink(missing_ok=True)
 return {'command':argv,'stages':stages}

def main():
 p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--new-package',type=Path,required=True);p.add_argument('--legacy-package',type=Path,required=True);p.add_argument('--publisher-key',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 result={'scope':'Desktop PXA simulator; Per-frame application draw/encode time before submit. Host raster/submit are separate; legacy Canvas composition is asynchronous. This does not measure total display throughput.'}
 result['gamerender']=run(a,'gamerender',a.new_package);result['legacy']=run(a,'legacy',a.legacy_package)
 result['ratios']={}
 for stage in ['title','world','zoom8']:
  new=result['gamerender']['stages'][stage]['median_cpu_us'];old=result['legacy']['stages'][stage]['median_cpu_us'];result['ratios'][stage]={'new_us':new,'legacy_us':old,'speedup':round(old/max(1,new),2)}
 (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result['ratios'],indent=2))
if __name__=='__main__':main()
