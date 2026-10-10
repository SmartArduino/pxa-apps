"""Native reader acceptance with isolated identity and one real PXADB connection.

Uses logged widget geometry, not fixed menu coordinates. No production data or
system settings are written. Requires a QA build with READER_VALIDATE=1.
"""
import argparse, importlib.util, json, re, sys, time, hashlib
from pathlib import Path
root=next(p for p in Path(__file__).resolve().parents if (p/'tools/pxadb/pxadb.py').exists())
sys.path.insert(0,str(root/'tools/pxadb'))
spec=importlib.util.spec_from_file_location('reader_db',root/'tools/pxadb/pxadb.py')
db=importlib.util.module_from_spec(spec);sys.modules[spec.name]=db;spec.loader.exec_module(db)
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--identity',default='pxa-novel-reader-qa-ux');p.add_argument('--fixture',action='store_true');p.add_argument('--cache-only',action='store_true',help='Only progressive cache, contents, resume and cold starts');p.add_argument('--menus-only',action='store_true',help='Only menus, font limits, IME and search');p.add_argument('--real-book',type=Path,help='Download and verify actual GITenberg 西游记 bytes in the isolated QA identity');p.add_argument('--restart-only',action='store_true',help='Verify three cold starts of the existing cached QA state');p.add_argument('--reset',action='store_true',help='Clear only this isolated QA identity');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
assert a.identity.startswith('pxa-novel-reader-qa')
assert not (a.restart_only and a.reset), 'Restart verification preserves the completed cache'
client=db.open_client(a.port,15); log=(a.output/'host.log').open('w');records={};monitor=False;validation_start=0
private='pxa-state/data/17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171~'+a.identity
fs=db.NormalFsClient(client)
def write(s): log.write(s);log.flush()
client.log_callback=lambda frame:write(frame.payload+'\n')
client.raw_callback=lambda raw:write(raw.decode(errors='replace'))
def req(value,timeout=30):return client.request(value,timeout=timeout)
def expand_hits(text):
 return re.sub(r'READER-HITS ([0-9,;]+)', lambda m: '\n'.join(
  'READER-HIT action=%s arg=%s x=%s y=%s' % tuple(item.split(','))
  for item in m[1].split(';') if item), text)
def logs():log.flush();return expand_hits((a.output/'host.log').read_text(errors='replace'))
def wait(test,timeout=20):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  req('INFO');s=logs()
  assert 'READER-ERROR' not in s[validation_start:],s[-2000:]
  if test(s):return s
  time.sleep(.08)
 raise AssertionError(logs()[-2000:])
def memory(command='MEMORY'):
 data = {}
 for frame in req(command):
  if frame.kind != 'DATA': continue
  value = db.info_properties(frame.payload)
  key = value['scope'] + (':' + value['name'] if value['scope'] == 'kind' else '')
  data[key] = value
 return data
def screenshot(name):
 for n in range(3):
  try:
   db.write_screenshot(db.screenshot_capture(req('SCREENSHOT JPEG AFTER_PRESENT')),a.output/(name+'.jpg'));return
  except db.PxaDbError as e:
   if n==2:raise
   time.sleep(1.2)
def hit(action,arg=0):
 if action in (5,6,7):
  req('INFO');s=logs();tail=s[s.rfind('READER-UI '):]
  assert 'screen=3 ' in tail and not re.search(r'READER-HIT action=[567] ',tail),tail
  # This hardware harness targets pai-touch's physical 296x240 surface.
  return (49 if action==6 else 246 if action==5 else 148,120)
 # Instant search emits status + metadata + UI traces in one event; the bounded
 # log service may omit its final geometry packet. For the fixed physical 296
 # profile only, derive the single result row from the previous logged shortcut
 # pair. A captured frame and subsequent page assertion verify the real click.
 if action==4 and arg==0:
  req('INFO');s=logs();tail=s[s.rfind('READER-UI '):]
  if 'screen=1 ' in tail and 'READER-HIT action=4 arg=0 ' not in tail and 'READER-SEARCH results=1 instant=1' in s:
   left=re.findall(r'READER-HIT action=29 arg=0 x=(\d+) y=(\d+)',s)
   right=re.findall(r'READER-HIT action=29 arg=1 x=(\d+) y=(\d+)',s)
   if left and right:
    lx,ly=map(int,left[-1]);rx,ry=map(int,right[-1]);assert ly==ry
    screenshot('single-result-geometry-fallback');return ((lx+rx)//2,ly)
 s=wait(lambda value:re.search(rf'READER-HIT action={action} arg={arg} x=(\d+) y=(\d+)', value[value.rfind('READER-UI '):]))
 tail=s[s.rfind('READER-UI '):]
 m=re.search(rf'READER-HIT action={action} arg={arg} x=(\d+) y=(\d+)',tail);assert m,(action,tail)
 return tuple(map(int,m.groups()))
def tap(action,arg=0,held=.35):
 x,y=hit(action,arg);before=len(logs())
 req(f'INPUT POINTER DOWN {x} {y} 0');time.sleep(held if action==4 and 'screen=0 ' in logs()[logs().rfind('READER-UI '):] else max(held,.8));req(f'INPUT POINTER UP {x} {y} 0')
 wait(lambda s:('READER-PAGE ' if action in (5,6) else 'READER-UI ') in s[before:]);time.sleep(1.0)
def saved(name):
 file=a.output/(name+'.json');fs.get(private+'/reader-state.json',file);return json.loads(file.read_text())
def stop():
 try:req('PACKAGE stop '+a.identity)
 except db.PxaDbError as e:
  if 'package_not_running' not in str(e):raise
 # PACKAGE stop acknowledges a graceful-stop request, not completed teardown.
 # Wait for the installed record to become inactive before another launch.
 end=time.monotonic()+15
 while time.monotonic()<end:
  rows=[frame.payload for frame in req('PACKAGES') if frame.kind=='PKG' and a.identity in frame.payload]
  if not rows or all('active=0' in row for row in rows):return
  time.sleep(.1)
 raise AssertionError('QA package did not stop')
def restart_cached():
 for repeat in range(3):
  stop();before=len(logs());req('PACKAGE run '+a.identity);wait(lambda s:'READER-HIT action=4 arg=0 ' in s[before:] and 'screen=0 busy=0 ' in s[s.rfind('READER-UI '):]);time.sleep(1.2);tap(4,1 if a.fixture or a.real_book else 0);wait(lambda s:'READER-PAGE ' in s[before:]);screenshot(f'restart-{repeat}')
 stop();records['after_stop']=memory();records['restarts_passed']=3
try:
 client.subscribe_logs();stop();req('INPUT TAP 148 25');time.sleep(.6);req('INPUT SWIPE 148 220 148 40 400 20');time.sleep(.6);req('INPUT CANCEL');req('INPUT SYNC');req('INPUT KEY HOME');time.sleep(1.2);screenshot('home-ready')
 if a.reset:req('PACKAGE clear-data '+a.identity)
 records['idle']=memory();req('MEMORY START');monitor=True
 if a.restart_only:
  validation_start=len(logs());restart_cached();records['session_end']=memory('MEMORY STOP');monitor=False;records['passed']=True
  print('Physical cached reader: three completed cold starts and page reads OK',flush=True)
  raise SystemExit(0)
 req('INFO');start_log=validation_start=len(logs());req('PACKAGE run '+a.identity);wait(lambda s:'READER-HIT action=4 arg=0 ' in s[start_log:] and 'screen=0 busy=0 ' in s[s.rfind('READER-UI '):]);time.sleep(1.2)
 if a.real_book:
  # Exercise the actual instant catalog + HTTPS source. PXADB deliberately
  # disallows writes to application-private files; do not weaken that boundary.
  raw=a.real_book.read_bytes();tap(1);tap(18);tap(18);tap(29)
  wait(lambda s:'READER-SEARCH results=1 instant=1' in s);screenshot('real-search');before=len(logs());tap(4)
  time.sleep(.6);req('INFO')
  if 'Runtime permission prompt queued:' in logs()[before:]:
   screenshot('real-network-permission');req('INPUT TAP 199 184');time.sleep(.8)
  wait(lambda s:re.search(r'READER-PAGE .*lines=[1-9]',s[before:]),45);screenshot('real-first-reading')
  wait(lambda s:any(int(v)>=230000 for v in re.findall(r'READER-INDEX chapters=\d+ cached=(\d+)',s[before:])),90)
  tap(7);tap(28);screenshot('real-paused-menu');tap(8);screenshot('real-partial-contents')
  for _ in range(10):
   if 'READER-HIT action=25 arg=9 ' in logs()[logs().rfind('READER-UI '):]:break
   tap(24)
  else:raise AssertionError('Chapter ten missing from partial index')
  screenshot('real-tenth-contents');before=len(logs());tap(25,9);wait(lambda s:'READER-PAGE ' in s[before:]);screenshot('real-tenth-reading');tap(5);screenshot('real-next-page');tap(6)
  current=saved('real-state');book=next(b for b in current['shelf'] if 'GITenberg' in b['url']);url=book['url'];value=14695981039346656037
  for byte in url.encode():value=((value^byte)*1099511628211)&((1<<64)-1)
  cache_name=f'b{value:x}.part';size=next(size for kind,size,name in fs.list(private) if name==cache_name);assert 230000<=size<len(raw)
  digest=db.info_properties(req('FSSHA '+db.client_encode_path(private+'/'+cache_name))[-1].payload)['sha256'];assert digest==hashlib.sha256(raw[:size]).hexdigest()
  records['real_book']={'url':url,'source_bytes':len(raw),'source_sha256':hashlib.sha256(raw).hexdigest(),'partial_bytes':size,'partial_sha256':digest,'tenth_chapter_opened':True,'instant_search_opened':True}
  records['session_end']=memory('MEMORY STOP');monitor=False
  book_index=current['shelf'].index(book)
  for repeat in range(3):
   stop();req('INPUT KEY HOME');time.sleep(1.2);before=len(logs());req('PACKAGE run '+a.identity);wait(lambda s:'screen=0 busy=0 ' in s[s.rfind('READER-UI '):] and 'READER-HIT action=4 arg=0 ' in s[before:]);time.sleep(1.2)
   for _ in range(4):
    if f'READER-HIT action=4 arg={book_index} ' in logs()[logs().rfind('READER-UI '):]:break
    tap(24)
   tap(4,book_index);wait(lambda s:'READER-PAGE ' in s[before:]);screenshot(f'real-restart-{repeat}')
  stop();records['restarts_passed']=3;records['after_stop']=memory();records['passed']=True
  print('Physical real novel: HTTPS cache, partial directory, chapter ten, paging, instant search and three cold starts OK',flush=True)
  raise SystemExit(0)
 if a.cache_only:
  records['shelf']=memory();records['initialization_end']=memory('MEMORY STOP');req('MEMORY START');tap(1);tap(17)
 else:
  records['shelf']=memory();screenshot('shelf')
  tap(4,held=.75);assert 'screen=8 ' in logs()[logs().rfind('READER-UI '):];screenshot('book-management');tap(27);screenshot('remove-confirmation');tap(16)
  tap(4);wait(lambda s:'READER-PAGE ' in s);screenshot('reading');records['reading']=memory();records['initialization_end']=memory('MEMORY STOP');req('MEMORY START');tap(7)
  assert 'action=27 ' not in logs()[logs().rfind('READER-UI '):];screenshot('reading-menu');tap(3);screenshot('settings-before-cycle')
  for _ in range(6):tap(11)
  assert saved('font-largest')['settings']['font']==28
  screenshot('preview-largest')
  for _ in range(8):tap(10)
  assert saved('font-smallest')['settings']['font']==12
  screenshot('preview-smallest')
  tap(11);tap(11)
  for _ in range(4):tap(13)
  screenshot('settings');tap(16);tap(7);tap(0);tap(1)
  x,y=hit(30);req(f'INPUT TAP {x} {y}');time.sleep(.6);screenshot('system-ime')
  ime_log=len(logs());req('INPUT TAP 250 185');wait(lambda s:'READER-UI ' in s[ime_log:]);time.sleep(.6)
  screenshot('search-after-ime');tap(17)
 if a.fixture:
  time.sleep(.6);req('INFO');screenshot('permission-or-results')
  if 'READER-SEARCH results=6' not in logs():
   req('INPUT TAP 199 184');time.sleep(.7)
  wait(lambda s:'READER-SEARCH results=6' in s,20)
  if a.menus_only:
   screenshot('fixture-results');records['session_end']=memory('MEMORY STOP');monitor=False;records['saved']=saved('state-final');records['passed']=True
   print('Physical reader menus: long press, confirm/cancel, two-line 12–28 preview, system IME and search OK',flush=True)
   raise SystemExit(0)
  screenshot('fixture-results');records['steady_end']=memory('MEMORY STOP');req('MEMORY START')
  before=len(logs());begin=time.monotonic();tap(4)
  wait(lambda s:re.search(r'READER-PAGE .*lines=[1-9]',s[before:]),30)
  records['first_read_ms']=round((time.monotonic()-begin)*1000,1)
  assert 'READER-DOWNLOAD complete=1' not in logs()[before:]
  screenshot('partial-reading');tap(5);screenshot('partial-next-page');tap(7);tap(8);screenshot('partial-contents');assert 'action=25 arg=0 ' in logs()[logs().rfind('READER-UI '):];assert 'READER-DOWNLOAD complete=1' not in logs()[before:];tap(25);tap(7);tap(28);screenshot('paused-menu')
  assert '缓存已暂停' in logs()[start_log:];saved('paused-state');tap(16);tap(7);tap(0);tap(4,1);screenshot('reopened-partial');tap(7)
  resumed_log=len(logs());tap(9)
  # Subscribe may replay logs from a previous QA launch. Only this download
  # counts; the slow fixture plus internal Flash writes can exceed 90 seconds.
  wait(lambda s:'READER-DOWNLOAD complete=1 bytes=258777' in s[resumed_log:],180)
  wait(lambda s:'screen=4 busy=0 ' in s[s.rfind('READER-UI '):]);time.sleep(.4);tap(16);screenshot('downloaded-reading')
  cache=next(name for kind,size,name in fs.list(private) if kind=='F' and size==258777 and name.endswith('.txt'))
  digest=db.info_properties(req('FSSHA '+db.client_encode_path(private+'/'+cache))[-1].payload)['sha256']
  assert digest=='f1401c49bf6a399717c05f9dbcd86ec63019f66e3e030f2a9a505078a574d7d9'
  records['sha256']=digest
 else:
  wait(lambda s:'READER-SEARCH results=4 instant=1' in s)
  assert 'READER-HIT action=4 arg=0 ' in logs()[logs().rfind('READER-UI '):]
  screenshot('instant-search-results')
 records['session_end']=memory('MEMORY STOP');monitor=False
 records['saved']=saved('state-final')
 restart_cached();records['passed']=True
 print('Physical native reader: menus, long press, confirm/cancel, 12–28 fonts, fast search, 3 restarts OK',flush=True)
finally:
 if monitor:
  try:req('MEMORY STOP')
  except Exception:pass
 try:stop()
 finally:client.close();log.close();(a.output/'results.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n')
