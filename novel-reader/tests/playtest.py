"""Operate the real reader UI through PXADB; inspect app logs and visible frames."""
import argparse,json,re,subprocess,time
from urllib.request import urlopen
from pathlib import Path
root=next(p for p in Path(__file__).resolve().parents if (p/'tools/pxadb/pxadb.py').is_file())
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--simulator',default='pai-touch@reader');p.add_argument('--port');p.add_argument('--log',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--fixture',default='http://127.0.0.1:18764');p.add_argument('--sample-only',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
endpoint=['--port',a.port] if a.port else ['--simulator',a.simulator]
records=[]
def cmd(*args):
 return subprocess.run(['python3',str(root/'tools/pxadb/pxadb.py'),*map(str,args),*endpoint],check=True,capture_output=True,text=True).stdout

def logs():
 if a.port:
  with a.log.open('a') as f:f.write(cmd('logcat','--dump'))
 return re.sub(r'\x1b\[[0-9;]*m','',a.log.read_text(errors='replace'))
def wait(predicate,timeout=10):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  value=logs()
  if predicate(value):return value
  time.sleep(.08)
 raise AssertionError('Timed out: '+logs()[-1400:])
def ui():
 text=logs();start=text.rfind('READER-UI ');text=text[start:];return {int(x):(int(arg),int(px),int(py)) for x,arg,px,py in re.findall(r'READER-HIT action=(\d+) arg=(-?\d+) x=(\d+) y=(\d+)',text)}
def tap(action,arg=None):
 end=time.monotonic()+10
 while time.monotonic()<end:
  text=logs();start=text.rfind('READER-UI ')
  hits=re.findall(r'READER-HIT action=(\d+) arg=(-?\d+) x=(\d+) y=(\d+)',text[start:])
  match=next((h for h in hits if int(h[0])==action and (arg is None or int(h[1])==arg)),None)
  if match:
   before=len(text);t=time.monotonic();cmd('input','tap',match[2],match[3]);wait(lambda s:'READER-UI ' in s[before:]);records.append({'action':action,'arg':arg,'ui_ms':round((time.monotonic()-t)*1000,1)});return
  time.sleep(.08)
 raise AssertionError(f'Missing action {action}/{arg}: '+logs()[-1400:])
def capture(name):
 time.sleep(.1);cmd('screenshot',a.output/(name+'.png'))
def book(index):
 for _ in range(20):
  if any(x[0]==index for k,x in ui().items() if k==4):tap(4,index);return
  # ui() loses duplicate action IDs; inspect all matching hits instead.
  tail=logs()[logs().rfind('READER-UI '):]
  if f'action=4 arg={index} ' in tail:tap(4,index);return
  tap(24)
 raise AssertionError('Missing book '+str(index))

try:
 current=logs()[logs().rfind('READER-UI '):]
 if 'screen=3 ' in current:tap(7);tap(0)
 elif 'screen=0 ' not in current:
  if 0 in ui():tap(0)
  elif 16 in ui():tap(16);tap(7);tap(0)
 wait(lambda s:'READER-HIT action=4 arg=0 ' in s,60);capture('shelf');book(0)
 wait(lambda s:'READER-PAGE offset=' in s);capture('reading');tap(5);tap(6)
 tap(7);capture('menu');tap(3);capture('settings');tap(11);tap(10);tap(13);tap(13);capture('night-settings');tap(13);tap(16)
 tap(7);tap(8);capture('contents');tap(25,2);tap(6)
 tap(7);tap(8);tap(25,0);tap(7);tap(3)
 for _ in range(6):
  current=logs()[logs().rfind('READER-UI '):]
  if 'seconds=5 ' in current:break
  tap(15)
 assert 'seconds=5 ' in logs()[logs().rfind('READER-UI '):]
 if 'auto=0 ' in logs()[logs().rfind('READER-UI '):]:tap(14)
 tap(16);before=len(logs());wait(lambda s:'READER-PAGE ' in s[before:],8);tap(7);tap(3);tap(14);tap(16)
 if not a.sample_only:
  tap(7);tap(0);tap(1);capture('search');tap(17);wait(lambda s:'READER-SEARCH results=6' in s);capture('results');book(0)
  wait(lambda s:'READER-UI screen=3 busy=0 ' in s[s.rfind('READER-UI '):],45)
  # Refresh explicitly so an existing cache does not make a repeated test pass.
  tap(7);before=len(logs());tap(9)
  wait(lambda s:'READER-DOWNLOAD complete=1' in s[before:],45)
  wait(lambda s:'READER-UI screen=4 busy=0 ' in s[s.rfind('READER-UI '):]);tap(16)
  capture('downloaded');tap(7);tap(8);capture('downloaded-contents');tap(16)
  urlopen(a.fixture+'/control/refresh-failure?enabled=1',timeout=5).close()
  try:
   tap(7);before=len(logs());tap(9)
   wait(lambda s:'保留原有离线版本' in s[before:] and 'READER-UI screen=4 busy=0 ' in s[s.rfind('READER-UI '):],20)
   tap(16);capture('refresh-preserved')
  finally:urlopen(a.fixture+'/control/refresh-failure?enabled=0',timeout=5).close()
  tap(7);tap(0);tap(1)
  for index in [1,2,3,4,5]:
   before=len(logs());book(index);wait(lambda s: ("READER-STATUS " in s[before:] or "READER-PAGE " in s[before:] or "READER-DOWNLOAD complete=1" in s[before:]) and re.search(r'READER-UI screen=3 busy=0 ',s[s.rfind('READER-UI '):]) is not None,20)
   if index==1:
    tap(7);before=len(logs());tap(9);wait(lambda s:'READER-DOWNLOAD complete=1' in s[before:],20)
    wait(lambda s:'READER-UI screen=4 busy=0 ' in s[s.rfind('READER-UI '):]);tap(16)
   capture(f'fixture-{index}')
   tap(7);tap(0);tap(1)
  tap(2);tap(19);capture('source-url');tap(20);wait(lambda s:'READER-UI screen=2 busy=0 ' in s[s.rfind('READER-UI '):]);capture('sources-imported')
 (a.output/'actions.json').write_text(json.dumps(records,indent=2)+'\n');print('Reader UI playtest passed:',len(records),'actions')
except Exception:
 capture('failure');(a.output/'failure.log').write_text(logs());raise
