"""Automatic chapter continuation through the real Guest and delayed HTTP."""
from simulator import *
results={}
def book(d):
 shelf=json.loads(next((d.state/'private-files').glob('*pxa-manga-reader/manga-state.json')).read_text())['shelf']
 return shelf[0] if shelf else {}
def tools(d):
 if not any(n.get('visible') and n.get('text')=='≡' for n in d.tree()['nodes']):d.pointer('DOWN',160,120,0);time.sleep(.1);d.pointer('UP',160,120,0)
 d.wait('≡')
def finish(d):
 for i in range(12):
  if any(n.get('visible') and n.get('text') in ['正在加载下一章…','重试'] for n in d.tree()['nodes']):return
  d.pointer('DOWN',24,210,0)
  for step in range(1,7):d.pointer('MOVE',24,210-step*30,0);time.sleep(.025)
  d.pointer('UP',24,30,0)
  time.sleep(.08)
 d.shot('stuck-edge');(d.state/'stuck-ui.json').write_text(json.dumps(d.tree(),ensure_ascii=False,indent=2))
 raise AssertionError('did not start next chapter')
for name,extra in [('strip',['--next-delay-ms','1200']),('retry',['--fail-next']),('cancel',['--next-delay-ms','4000']),('paged',[])]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--pages','2','--height','400','--chapters','3','--toc-batch','1',*extra],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
  with Sim(name,320,240,125) as d:
   enter(d);wait_for(lambda:'MANGA prefetched=1 ' in text(d));wait_for(lambda:bool(book(d)));original=book(d)['chapter']
   if name=='paged':
    tools(d);d.tap('设置');d.tap('阅读模式：自动');d.wait('设置已保存','text');d.tap('阅读模式：条漫');d.wait('设置已保存','text');d.tap('返回');tools(d)
    d.tap('下一页');wait_for(lambda:book(d)['page']==1);assert book(d)['chapter']==original;tools(d);d.tap('下一页')
   else:finish(d)
   if name=='cancel':
    d.wait('正在加载下一章…','text');d.tap('取消');d.wait('正在加载下一章…','text',count=0);assert book(d)['chapter']==original
    time.sleep(.4);assert book(d)['chapter']==original;d.shot('cancel-preserves-chapter');results[name]={'passed':True,'original_chapter_retained':True};continue
   if name=='retry':
    d.wait('重试');assert book(d)['chapter']==original;d.shot('failure-preserves-chapter');urlopen('http://127.0.0.1:18929/_recover').read();d.tap('重试')
   wait_for(lambda:book(d)['chapter']!=original,30);assert book(d)['page']==0
   assert book(d)['chapter']=='55555555-5555-5555-5555-000000000001'
   d.shot('next-chapter-ready');assert 'MANGA next_chapter=ready' in text(d)
   if name=='paged':assert book(d)['readingMode']==2
   results[name]={'passed':True,'next_chapter_persisted':True,'pagination_crossed':True,'reading_mode_retained':True}
   print(name,'automatic chapter PASS',flush=True)
 finally:server.terminate();server.wait(timeout=5)
(out/'auto-chapter-result.json').write_text(json.dumps(results,indent=2))
