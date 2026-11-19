"""Exercise bounded asynchronous spooling with and without Content-Length."""
from simulator import *
results={}
for unknown in (False,True):
 name='unknown-length' if unknown else 'known-length'
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','2','--jpeg-baseline','--padding-bytes','600000',*(['--unknown-length']if unknown else [])],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3)
  with Sim(name,480,480,160) as d:
   d.wait('书架为空，去发现漫画','text');d.tap('书源')
   if not any(n.get('visible')and n.get('text')=='MangaDex'for n in d.tree()['nodes']):d.tap('>')
   d.tap('MangaDex');d.wait('触摸测试漫画');d.tap('触摸测试漫画')
   wait_for(lambda:'MANGA page_ready_us='in text(d),90)
   wait_for(lambda:'MANGA prefetched=1 'in text(d),90)
   assert re.search(r'MANGA input_ready_bytes=6\d{5} ram=0',text(d))
   assert 'Guest callback exceeded'not in text(d)
   private=next((d.state/'private-files').glob('*pxa-manga-reader'))
   wait_for(lambda:not list(private.glob('manga-source-part-*.tmp')))
   wait_for(lambda:not list(private.glob('*.pending')))
   d.shot('loaded-large-image');swipe(d,-300)
   orphan=private/'manga-source-part-8.tmp';orphan.write_bytes(b'cancelled partial body')
   pending=private/'manga-source-part-8.tmp.pending';pending.write_bytes(b'interrupted write')
   d.pointer('DOWN',240,200,0);time.sleep(.12);d.pointer('UP',240,200,0)
   d.wait('设置');d.tap('设置');d.tap('清除缓存');d.wait('缓存已清除，书架和进度已保留','text')
   assert not orphan.exists() and not pending.exists()
   assert json.loads((private/'manga-state.json').read_text())['shelf']
   results[name]={'passed':True,'clear_preserves_shelf':True,'spooled':True,'adjacent_image_buffered':True,'temporary_parts_cleaned':True}
   print(name,'async spool PASS',flush=True)
 finally:server.terminate();server.wait(timeout=5)
(out/'async-spool-result.json').write_text(json.dumps(results,indent=2))
