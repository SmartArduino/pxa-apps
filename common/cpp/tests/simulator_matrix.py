#!/usr/bin/env python3
import os,sys,re,json,time,subprocess,shutil,argparse
from pathlib import Path
from PIL import Image
sys.path.insert(0,'tools/pxadb');import pxadb
parser=argparse.ArgumentParser(description='Exercise signed C++ game packages through the real PXA simulator')
parser.add_argument('--workspace',type=Path,default=Path.cwd())
parser.add_argument('--artifacts',type=Path,required=True)
parser.add_argument('--native-build',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--endpoint',default='unix:/tmp/pxa-simulator-1000/pai-touch@cpp-games.sock')
parser.add_argument('--control',default='/tmp/pxa-simulator-1000/pai-touch@cpp-games.control.sock')
args=parser.parse_args();root=args.workspace.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
state=out/'state';state.mkdir(exist_ok=True);sock=args.control;endpoint=args.endpoint
variants=[('touch',296,240,160,58,False),('mosaico',480,480,305,58,False),('round',176,176,160,0,True),('portrait',320,480,240,24,False),('wide',800,480,160,0,False),('tall',480,800,320,48,False)]
report=[]
for game in ['jump','pixel']:
 for name,w,h,dpi,corner,round_ in variants:
  directory=out/f'{game}-{name}';directory.mkdir(exist_ok=True)
  package=args.artifacts.resolve()/f'{game}-all'/('pxa-jump-jump-3d-cpp'if game=='jump'else'pxa-pixel-dungeon-cpp')
  cmd=[str(root/'build/simulator/pai-touch/pxsys_product_simulator'),'--package',str(package),'--publisher-key',str(root/'deps/pxa-system/apps/pxa/.dev-signing/publisher-public.der'),'--state-root',str(state),'--pxadb-control-socket',sock,'--width',str(w),'--height',str(h),'--density-dpi',str(dpi),'--safe-insets','8,10,8,10','--locale','zh-CN']
  cmd+=['--round']if round_ else ['--corner-radius',str(corner)]
  env=os.environ|{'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'}
  case_state=state/'cases'/f'{game}-{name}';shutil.rmtree(case_state,ignore_errors=True);case_state.mkdir(parents=True)
  cmd[cmd.index('--state-root')+1]=str(case_state)
  log=directory/'simulator.log';row={'game':game,'variant':name,'width':w,'height':h,'dpi':dpi,'passed':False};report.append(row)
  with log.open('w')as stream:
   proc=subprocess.Popen(cmd,stdout=stream,stderr=subprocess.STDOUT,env=env)
   try:
    end=time.monotonic()+45
    while time.monotonic()<end:
     if proc.poll()is not None:raise RuntimeError('simulator exited '+str(proc.returncode))
     if ' ready pixels='in log.read_text():break
     time.sleep(.2)
    else:raise RuntimeError('initialization did not finish')
    time.sleep(.5)
    with pxadb.open_client(endpoint,10)as c:
     def screenshot(name):
      cap=pxadb.screenshot_capture(c.request('SCREENSHOT',timeout=20));pxadb.write_screenshot(cap,directory/(name+'.png'))
      image=Image.open(directory/(name+'.png'));assert image.size==(w,h)
      assert len(image.getcolors(w*h)or[])>30,'empty/blank game frame'
     screenshot('ready')
     if game=='jump':
      c.request(f'INPUT POINTER DOWN {w//2} {h//2} 0');c.request('INPUT SYNC');time.sleep(.36)
      c.request(f'INPUT POINTER UP {w//2} {h//2} 0');c.request('INPUT SYNC');time.sleep(.2);screenshot('jump')
      c.request(f'INPUT POINTER CANCEL {w//2} {h//2} 0')
     else:
      ready=re.search(r'PDCPP ready pixels=\d+,\d+ dpi_q16=\d+ render=(\d+),(\d+) first=(\d+),(\d+)',log.read_text());assert ready
      rw,rh,x,y=map(int,ready.groups());c.request(f'INPUT TAP {x*w//rw} {y*h//rh}');c.request('INPUT SYNC');time.sleep(.3);screenshot('slots')
      shape=2 if round_ else (1 if corner else 0)
      layout_args=[str(args.native_build.resolve()/'pixel_layout_probe'),str(rw),str(rh),str(shape),str(8*rh//h),str(10*rw//w),str(8*rh//h),str(10*rw//w),str(corner*rw//w)]
      points=json.loads(subprocess.check_output(layout_args,text=True))
      def tap(key):
       x,y=points[key];c.request(f'INPUT TAP {x*w//rw} {y*h//rh}');c.request('INPUT SYNC');time.sleep(.65)
      def phase(expected):
       actual=re.findall(r'PDCPP phase=(\d+)',log.read_text());assert actual and int(actual[-1])==expected,(expected,actual)
      tap('slot');phase(6);screenshot('class');tap('class');phase(1);screenshot('play')
      tap('wait');tap('bag');phase(2);screenshot('bag');tap('bag_close');phase(1)
      tap('pause');phase(9);screenshot('pause');tap('pause_settings');phase(8);screenshot('settings');tap('settings_back');phase(9)
      tap('pause_menu');phase(0);screenshot('title-returned')
      # Reopen the process against the same private storage, exercising the
      # catalog loader and actual committed save rather than a memory copy.
      proc.terminate();proc.wait(timeout=5)
      log=directory/'simulator-relaunch.log'
      with log.open('w')as relaunch_stream:
       proc=subprocess.Popen(cmd,stdout=relaunch_stream,stderr=subprocess.STDOUT,env=env)
       end=time.monotonic()+45
       while ' ready pixels='not in log.read_text():
        assert time.monotonic()<end and proc.poll()is None,'relaunch failed'
        time.sleep(.2)
       time.sleep(.65)
       tap('title');phase(5);tap('slot_saved');phase(1);screenshot('loaded-after-relaunch')
     row['passed']=True;row['ready_log']=[line for line in log.read_text().splitlines()if 'ready pixels='in line][0]
   except Exception as error:row['error']=str(error)
   finally:
    proc.terminate()
    try:proc.wait(timeout=5)
    except subprocess.TimeoutExpired:proc.kill();proc.wait()
  (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(game,name,row['passed'],row.get('error',''),flush=True)
assert all(row['passed']for row in report)
