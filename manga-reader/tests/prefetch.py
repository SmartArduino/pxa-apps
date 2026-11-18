"""Verify short strips are loaded before scrolling and refilled before an edge."""
from simulator import *

results={}
for name,w,h,dpi,extra in [('short-panels',480,854,254,[]),('long-then-short',480,480,305,['--first-height','1000'])]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','64','--height','64','--delay-ms','100',*extra],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(16*1024*1024)
  with Sim(name,w,h,dpi) as d:
   enter(d,True)
   if name=='short-panels':
    wait_for(lambda:'MANGA prefetched=18 ' in text(d),30)
    assert position(d)==0
    metrics=json.load(urlopen('http://127.0.0.1:18929/_metrics'))
    for i in range(1,20):assert metrics.get('/data-saver/abc123/'+str(i)+'.png')==1,metrics
    d.shot('buffered-before-first-scroll');swipe(d,-h*.7);swipe(d,-h*.7);wait_for(lambda:position(d)>=10)
    d.wait('正在加载下一页…','text',count=0);d.shot('two-swipes-without-waiting')
    assert text(d).count('MANGA page_ready_us=')==1
    results[name]={'passed':True,'segments_prefetched_before_first_scroll':18,'delayed_network_ms':100,'two_swipes_without_loading_edge':True,'no_page_reload':True}
   else:
    wait_for(lambda:'MANGA prefetched=4 ' in text(d));time.sleep(.3);assert 'MANGA prefetched=5 ' not in text(d)
    swipe(d,-h*.8);swipe(d,-h*.8);wait_for(lambda:'MANGA prefetched=8 ' in text(d),20)
    assert position(d)==0;d.wait('正在加载下一页…','text',count=0);d.shot('refilled-while-still-in-long-panel')
    assert text(d).count('MANGA page_ready_us=')==1
    results[name]={'passed':True,'preloaded_more_while_inside_current_image':True,'current_page_unchanged':True,'scrolled_height_excluded_from_coverage':True}
   print(name,'prefetch PASS',flush=True)
 finally:server.terminate();server.wait(timeout=5)

server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','64','--height','64','--first-height','1000'],stdout=(out/'pressure-fixture.log').open('w'),stderr=subprocess.STDOUT)
try:
 time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(1600*1024)
 with Sim('pressure-large-then-short',480,480,305) as d:
  enter(d,True);wait_for(lambda:'MANGA buffer_capacity=' in text(d))
  before=max(map(int,re.findall(r'MANGA prefetched=(\d+)',text(d))))
  assert before==3
  # A 1500px cover and three 96px neighbors: the neighbors cannot fill a
  # 480px viewport. Still reach the next panel and retire the large cover.
  for i in range(4):swipe(d,-375)
  wait_for(lambda:position(d)==1)
  wait_for(lambda:max(map(int,re.findall(r'MANGA prefetched=(\d+)',text(d))))>before)
  time.sleep(.5);assert position(d)==1
  metrics=json.load(urlopen('http://127.0.0.1:18929/_metrics'))
  # Repeated replacement of the last neighbor used to download these two
  # panels hundreds of times without letting the reader leave the cover.
  for i in (3,4):assert metrics.get('/data-saver/abc123/'+str(i)+'.png')==1,metrics
  assert text(d).count('MANGA page_ready_us=')==1
  d.shot('refilled-after-large-image')
  results['pressure-large-then-short']={'passed':True,'host_resource_budget_bytes':1600*1024,'advanced_with_less_than_one_screen_buffered':True,'prefetch_resumed_after_large_image_retired':True,'no_repeated_neighbor_downloads':True}
  print('pressure large then short PASS',flush=True)
finally:server.terminate();server.wait(timeout=5)
(out/'prefetch-result.json').write_text(json.dumps(results,indent=2))
