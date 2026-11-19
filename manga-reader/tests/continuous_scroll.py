"""Continuous scrolling and buffering regression; see simulator.py for arguments."""
from simulator import *
results={}
def drawn_page(d):
 values=re.findall(r"MANGA strip_page=(\d+)",text(d));return int(values[-1]) if values else position(d)
for name,w,h,dpi in [('continuous-small',320,240,125),('continuous-mosaico',480,480,305)]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','24','--height','64'],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
  with Sim(name,w,h,dpi) as d:
   enter(d,True);wait_for(lambda:f'MANGA prefetched={7 if w==480 else 5} ' in text(d));end=hold(d,-h*.85)
   wait_for(lambda:drawn_page(d)>=1);held=drawn_page(d);d.shot('stitched-while-held');assert sum(n>500 for n in colours(d.state/'stitched-while-held.png'))>=2
   release(d,end);wait_for(lambda:position(d)==held);assert text(d).count('MANGA page_ready_us=')==1
   for i in range(4):swipe(d,-h*.65);time.sleep(.2)
   wait_for(lambda:position(d)>=5);forward=position(d)
   d.pointer('DOWN',w*.3,h*.5,0);d.pointer('DOWN',w*.7,h*.5,1);d.pointer('MOVE',w*.15,h*.5,0);d.pointer('MOVE',w*.85,h*.5,1);d.pointer('UP',w*.15,h*.5,0);d.pointer('UP',w*.85,h*.5,1)
   d.shot('whole-column-zoom');assert 'MANGA pinch_permille=' in text(d)
   for i in range(20):
    swipe(d,h*.65);time.sleep(.2)
    if position(d)==0:break
   wait_for(lambda:position(d)==0);d.shot('backward-continuous')
   private=next((d.state/'private-files').glob('*pxa-manga-reader'));assert not list(private.glob('reader-cache-*.png'));assert not (private/'reader-reserve.tmp').exists()
   assert text(d).count('MANGA page_ready_us=')==1
   results[name]={'passed':True,'disk_cache_off':True,'held_finger_advanced_to':held,'forward_page':forward,'returned_to_zero':True,'whole_column_zoom':True,'no_page_reload_during_scroll':True}
   print(name,'PASS',results[name],flush=True)
 finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','8','--delay-ms','2500'],stdout=(out/'slow-edge-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3)
 with Sim('continuous-slow-edge',320,240,125) as d:
  enter(d);swipe(d,-150);swipe(d,-150);end=hold(d,-190);d.wait('正在加载下一页…','text');centered_notice(d,'正在加载下一页…');d.wait('没有下一页','text',count=0);d.shot('waiting-while-held')
  wait_for(lambda:'MANGA prefetched=1 ' in text(d));time.sleep(.2);d.shot('loaded-without-release')
  assert sum(n>500 for n in colours(d.state/'loaded-without-release.png'))>=2
  release(d,end);assert text(d).count('MANGA page_ready_us=')==1
  end=hold(d,-180);wait_for(lambda:drawn_page(d)>=1);d.shot('next-image-before-release');release(d,end)
  end=hold(d,180);wait_for(lambda:drawn_page(d)==0);d.shot('previous-image-before-release');release(d,end)
  d.wait('正在加载下一页…','text',count=0)
  assert text(d).count('MANGA page_ready_us=')==1
  results['slow-edge']={'passed':True,'delay_ms':2500,'kept_current_image':True,'joined_without_finger_release':True,'backward_without_page_reload':True,'visible_loading_notice':True,'loading_not_mistaken_for_end':True,'notice_hidden_away_from_edge':True}
  print('slow edge PASS',flush=True)
finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','12'],stdout=(out/'low-budget-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(2*1024*1024)
 with Sim('continuous-low-budget',320,240,125) as d:
  enter(d);wait_for(lambda:'MANGA prefetched=1 ' in text(d));wait_for(lambda:'MANGA buffer_capacity=' in text(d))
  for i in range(8):
   swipe(d,-200);time.sleep(.25)
   if position(d)>=2:break
  wait_for(lambda:position(d)>=2);d.shot('low-budget-forward')
  for i in range(20):
   swipe(d,180);time.sleep(.2)
   if position(d)==0:break
  wait_for(lambda:position(d)==0);d.shot('low-budget-backward');assert text(d).count('MANGA page_ready_us=')==1
  private=next((d.state/'private-files').glob('*pxa-manga-reader'));assert not (private/'reader-reserve.tmp').exists()
  results['low-budget']={'passed':True,'host_resource_budget_bytes':2*1024*1024,'kept_neighbors_on_denial':True,'forward_and_backward_continuous':True}
  print('low budget PASS',flush=True)
finally:server.terminate();server.wait(timeout=5)
for name,options in [('network-partial',['--noise']),('network-lossy',['--webp-lossy','--width','640','--height','1000'])]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','12',*options],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
  with Sim(name,480,480,305) as d:
   enter(d);wait_for(lambda:'MANGA prefetched=2 ' in text(d),40)
   counts=json.load(urlopen('http://127.0.0.1:18929/_metrics'))
   for i in range(1,4):assert counts.get('/data-saver/abc123/'+str(i)+'.png')==1,counts
   assert 'MANGA wire_prefetched=1 ' in text(d)
   if name=='network-partial':assert 'MANGA wire_prefetched=1 bytes=262144' in text(d)
   else:assert ' jpeg=1 ' in text(d) and 'MANGA wire_consumed_bytes=' in text(d)
   d.shot('network-ready');d.pointer('DOWN',240,240,0);time.sleep(.15);d.pointer('UP',240,240,0);d.wait('≡');d.tap('≡');d.wait('书架');d.tap('书架');d.wait('设置');before=text(d).count('MANGA page_ready_us=');d.tap('触摸测试漫画');wait_for(lambda:text(d).count('MANGA page_ready_us=')>before,40);d.shot('after-cancel-and-reopen')
   results[name]={'passed':True,'single_request_per_first_three_images':True,'wire_partial_response_reused':name=='network-partial','lossy_webp_jpeg_conversion':name=='network-lossy','cancel_and_resume':True}
   print(name,'PASS',flush=True)
 finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','8','--delay-ms','1500'],stdout=(out/'previous-edge-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
 with Sim('continuous-previous-edge',320,240,125) as d:
  enter(d,True)
  for i in range(20):
   swipe(d,-180)
   if position(d)>=2:break
   time.sleep(.2)
  wait_for(lambda:position(d)>=2);saved=position(d)
  d.pointer('DOWN',160,120,0);time.sleep(.15);d.pointer('UP',160,120,0);d.wait('≡');d.tap('≡');d.wait('书架');d.tap('书架');d.wait('设置');before=text(d).count('MANGA page_ready_us=');d.tap('触摸测试漫画');wait_for(lambda:text(d).count('MANGA page_ready_us=')>before)
  saved=position(d);end=hold(d,180);d.wait('正在加载上一页…','text');centered_notice(d,'正在加载上一页…');d.wait('没有上一页','text',count=0);d.shot('previous-loading-while-held');wait_for(lambda:drawn_page(d)==saved-1);release(d,end)
  for i in range(30):
   swipe(d,190);time.sleep(.15)
   if any(n.get('visible') and n.get('text')=='没有上一页' for n in d.tree()['nodes']):break
  d.wait('没有上一页','text');centered_notice(d,'没有上一页');d.wait('正在加载上一页…','text',count=0);d.shot('chapter-start-notice');swipe(d,-160);d.wait('没有上一页','text',count=0)
  results['previous-edge']={'passed':True,'previous_loaded_without_cache':True,'loading_centered_while_held':True,'no_false_start_while_loading':True,'true_start_centered':True,'hidden_when_scrolling_forward':True}
  print('previous edge PASS',flush=True)
finally:server.terminate();server.wait(timeout=5)
for name,w,h,dpi,pages,height in [('strip-end-small',320,240,125,3,400),('strip-end-large',480,854,254,1,64)]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages',str(pages),'--height',str(height)],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
  with Sim(name,w,h,dpi) as d:
   enter(d,True)
   if pages>1:
    wait_for(lambda:'MANGA prefetched=2 ' in text(d));d.wait('没有下一页','text',count=0)
    for i in range(20):
     swipe(d,-h*.7)
     if position(d)==pages-1:break
    swipe(d,-h*.8)
   end_notice='没有下一章' if pages>1 else '没有下一页'
   d.wait(end_notice,'text');centered_notice(d,end_notice);d.wait('正在加载下一页…','text',count=0);d.shot('chapter-end-notice')
   if pages==1:d.wait('没有上一页','text');centered_notice(d,'没有上一页')
   if pages>1:
    swipe(d,h*.8);d.wait(end_notice,'text',count=0)
   assert text(d).count('MANGA page_ready_us=')==1
   results[name]={'passed':True,'known_end_visible':True,'no_loading_at_end':True,'single_short_image':pages==1,'hidden_when_scrolling_back':pages>1}
   print(name,'PASS',flush=True)
 finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','8','--body-delay-ms','800'],stdout=(out/'transfer-overlap-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
 with Sim('transfer-overlap',480,480,305) as d:
  enter(d,True);wait_for(lambda:'MANGA prefetched=1 ' in text(d));events=json.load(urlopen('http://127.0.0.1:18929/_timings'))
  starts={e['path']:e['time'] for e in events if e['event']=='start'};ends={e['path']:e['time'] for e in events if e['event']=='end'}
  first='/data-saver/abc123/1.png';second='/data-saver/abc123/2.png';overlap=ends[first]-starts[second]
  assert overlap>.3,events
  counts=json.load(urlopen('http://127.0.0.1:18929/_metrics'));assert counts[first]==counts[second]==1,counts
  results['transfer-overlap']={'passed':True,'body_delay_ms':800,'request_overlap_seconds':round(overlap,3),'no_duplicate_requests':True}
  print('transfer overlap PASS',results['transfer-overlap'],flush=True)
finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','40','--height','64'],stdout=(out/'progress-coalescing-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3)
 with Sim('progress-coalescing',480,480,305) as d:
  enter(d,True);wait_for(lambda:'MANGA prefetched=7 ' in text(d));time.sleep(.5)
  old=text(d);crossings=old.count('MANGA strip_page=');saves=old.count('MANGA progress_saved=')
  for gesture in range(4):
   # Schedule the gesture in the bridge; per-point control round trips can
   # turn a fast swipe into seconds of idle time on a busy build machine.
   d.c.request('INPUT SWIPE 260 450 260 90 180 13')
  time.sleep(1.8);log=text(d);crossings=log.count('MANGA strip_page=')-crossings;saves=log.count('MANGA progress_saved=')-saves
  last=int(re.findall(r'MANGA strip_page=(\d+)',log)[-1]);assert position(d)==last
  assert crossings>=6 and 1<=saves<=4,(crossings,saves)
  results['progress-coalescing']={'passed':True,'crossed_pages':crossings,'state_writes':saves,'latest_position_saved':True}
  print('progress coalescing PASS',results['progress-coalescing'],flush=True)
finally:server.terminate();server.wait(timeout=5)
server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','8','--height','400','--two-books'],stdout=(out/'book-preferences-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
 with Sim('book-preferences',320,240,125) as d:
  def tools():
   if not any(n.get('visible') and n.get('text')=='≡' for n in d.tree()['nodes']):d.pointer('DOWN',160,120,0);time.sleep(.1);d.pointer('UP',160,120,0)
   d.wait('≡')
  def settings():tools();d.tap('设置')
  def reader():d.tap('返回');tools()
  def shelf():tools();d.tap('≡');d.wait('书架');d.tap('书架');d.wait('设置')
  enter(d);settings();d.wait('阅读模式：自动');d.tap('阅读模式：自动');d.wait('设置已保存','text');d.wait('阅读模式：条漫');reader();tools();d.tap('放大');d.wait('整页');shelf()
  d.tap('发现');d.wait('分页测试漫画');d.tap('分页测试漫画');wait_for(lambda:text(d).count('MANGA page_ready_us=')>=2);settings();d.wait('阅读模式：自动');d.tap('阅读模式：自动');d.wait('设置已保存','text');d.tap('阅读模式：条漫');d.wait('设置已保存','text');d.wait('阅读模式：分页');reader();tools();d.wait('放大');shelf();d.wait('2 部漫画 · 已保存','text')
  d.c.request('PACKAGE stop pxa-manga-reader');time.sleep(.3);d.c.request('PACKAGE run pxa-manga-reader');d.wait('设置');d.tap('触摸测试漫画');d.wait('取消',count=0);tools();d.wait('整页');settings();d.wait('阅读模式：条漫');d.shot('first-book-strip-settings');reader();shelf();d.tap('分页测试漫画');d.wait('取消',count=0);tools();d.wait('放大');settings();d.wait('阅读模式：分页');d.shot('second-book-page-settings');reader();shelf()
  private=next((d.state/'private-files').glob('*pxa-manga-reader'));state=json.loads((private/'manga-state.json').read_text());assert state['version']==4;books={b['title']:b for b in state['shelf']};assert books['触摸测试漫画']['readingMode']==1 and books['触摸测试漫画']['viewMode']==2;assert books['分页测试漫画']['readingMode']==2 and books['分页测试漫画']['viewMode']==1
  results['book-preferences']={'passed':True,'separate_reading_modes':True,'separate_zoom_preferences':True,'preserved_after_restart_and_book_switch':True};print('per-book preferences PASS',flush=True)
finally:server.terminate();server.wait(timeout=5)
(out/'continuous-result.json').write_text(json.dumps(results,indent=2));print('All continuous scrolling cases PASS',flush=True)
