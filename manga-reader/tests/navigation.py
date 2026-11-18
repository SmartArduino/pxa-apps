"""Real simulator checks for directory reuse, return-to-reader and row layout.

Use the same arguments and localhost fixture build as continuous_scroll.py.
"""
from simulator import *

BOOK='11111111-1111-1111-1111-111111111111'
def node(d,label,kind='button'):
 return next(n for n in d.tree()['nodes'] if n.get('visible') and n['type']==kind and n.get('text')==label)
def tools(d):
 if not any(n.get('visible') and n.get('text')=='≡' for n in d.tree()['nodes']):
  d.pointer('DOWN',d.w/2,d.h/2,0);time.sleep(.1);d.pointer('UP',d.w/2,d.h/2,0)
 d.wait('≡')
def counts():return json.load(urlopen('http://127.0.0.1:18929/_metrics'))
def feed_count():return counts().get('/manga/'+BOOK+'/feed',0)
def assert_disjoint_buttons(d):
 buttons=[n for n in d.tree()['nodes'] if n.get('visible') and n['type']=='button']
 for i,a in enumerate(buttons):
  for b in buttons[i+1:]:
   aa=a['bounds'];bb=b['bounds']
   assert min(aa['x']+aa['width'],bb['x']+bb['width'])<=max(aa['x'],bb['x']) or min(aa['y']+aa['height'],bb['y']+bb['height'])<=max(aa['y'],bb['y']),(a,b)
def centered_button_labels(d,screenshot):
 image=Image.open(d.state/(screenshot+'.png')).convert('RGB');tolerance=max(2,d.dpi/160*1.5)
 labels={'设置','书架','发现','书源','返回阅读','返回','继续阅读','已收藏','收藏','正序','刷新','清除缓存'}
 for n in d.tree()['nodes']:
  if not n.get('visible') or n['type']!='button' or not (n.get('text') in labels or (n.get('text') or '').startswith(('阅读模式：','自动缓存：'))):continue
  b=n['bounds'];x,y,w,h=[b[k] for k in ['x','y','width','height']]
  ink=[(xx+.5,yy+.5) for yy in range(y+2,y+h-2) for xx in range(x+2,x+w-2) if max(image.getpixel((xx,yy)))<180]
  assert ink,n
  for axis,start,size in [(0,x,w),(1,y,h)]:
   center=(min(p[axis] for p in ink)+max(p[axis] for p in ink))/2
   assert abs(center-start-size/2)<=tolerance,(n['text'],axis,center,start+size/2)
def shelf_layout(d):
 row=node(d,'触摸测试漫画')['bounds'];menu=node(d,'触摸测试漫画 · 目录')['bounds'];setting=node(d,'设置')['bounds']
 assert row['y']>=setting['y']+setting['height']+3,(row,setting)
 assert menu['y']==row['y'] and menu['height']==row['height'],(row,menu)
 # Icon strokes are actual boxes, centered independently of font glyph metrics.
 strokes=[n['bounds'] for n in d.tree()['nodes'] if n.get('visible') and n['type']=='box' and n['bounds']['width']<menu['width'] and n['bounds']['height']<=4 and menu['x']<=n['bounds']['x']<menu['x']+menu['width'] and menu['y']<=n['bounds']['y']<menu['y']+menu['height']]
 assert len(strokes)==3,strokes
 for s in strokes:assert abs(s['x']+s['width']/2-menu['x']-menu['width']/2)<=max(1,d.dpi/160),(s,menu)
 assert abs(sum(s['y']+s['height']/2 for s in strokes)/3-menu['y']-menu['height']/2)<=max(1,d.dpi/160),(strokes,menu)
 assert_disjoint_buttons(d)
 d.shot('centering-and-arrows')
 image=Image.open(d.state/'centering-and-arrows.png').convert('RGB');tolerance=max(2,d.dpi/160*1.5)
 for label in ['书架','发现','书源','设置','<','>']:
  r=node(d,label)['bounds'];x,y,w,h=[r[k] for k in ['x','y','width','height']]
  points=[(xx,yy) for yy in range(y+2,y+h-2) for xx in range(x+2,x+w-2) if max(image.getpixel((xx,yy)))<185]
  assert points,label
  for axis,origin,size in [(0,x,w),(1,y,h)]:
   middle=(min(p[axis] for p in points)+max(p[axis] for p in points)+1)/2
   assert abs(middle-origin-size/2)<=tolerance,(label,axis,middle,r)
 nodes=d.tree()['nodes'];label=next(n for n in nodes if n.get('visible') and n['type']=='text' and re.match(r'\d+–\d+ / ',n.get('text') or ''))
 panel=next(n for n in nodes if n['id']==label['parent'] and n['surface']==label['surface']);a=label['bounds'];b=panel['bounds'];arrow=node(d,'<')['bounds']
 for axis,size in [('x','width'),('y','height')]:assert abs(a[axis]+a[size]/2-b[axis]-b[size]/2)<=1,(a,b)
 assert abs(b['y']+b['height']/2-arrow['y']-arrow['height']/2)<=max(1,d.dpi/160),(b,arrow)

results={}
for name,w,h,dpi in [('gamekit',320,240,125),('mosaico',480,480,305),('p4',480,854,254),('wide',1024,600,160)]:
 server=subprocess.Popen([sys.executable,str(Path(__file__).with_name('fixture.py')),'--port','18929','--two-books','--pages','3'],stdout=(out/(name+'-fixture.log')).open('w'),stderr=subprocess.STDOUT)
 try:
  time.sleep(.3);os.environ['PXA_RESOURCE_EXTERNAL_BYTES']=str(8*1024*1024)
  with Sim(name,w,h,dpi) as d:
   enter(d);assert feed_count()==1 # Discovery title opens the first chapter directly.
   swipe(d,-h*.2);tools(d);d.tap('设置');d.wait('阅读模式：自动');d.shot('direct-reading-settings');centered_button_labels(d,'direct-reading-settings');d.tap('返回');tools(d)
   d.shot('reader-before-directory');ready=text(d).count('MANGA page_ready_us=')
   d.tap('≡');d.wait('返回阅读');assert_disjoint_buttons(d);d.shot('contents-from-reader');centered_button_labels(d,'contents-from-reader');assert feed_count()==1
   d.tap('返回阅读');d.wait('≡');assert text(d).count('MANGA page_ready_us=')==ready;d.shot('reader-after-directory')
   a=Image.open(d.state/'reader-before-directory.png').convert('RGB');b=Image.open(d.state/'reader-after-directory.png').convert('RGB');region=(20,h//3,w-20,h*2//3);assert a.crop(region).tobytes()==b.crop(region).tobytes()
   d.tap('≡');d.wait('继续阅读');d.tap('继续阅读');d.wait('≡');assert text(d).count('MANGA page_ready_us=')==ready
   d.tap('≡');d.wait('刷新');d.tap('刷新');d.wait('第 1 话 · 出发');wait_for(lambda:feed_count()==2)
   d.tap('正序');d.wait('倒序');d.wait('第 1 话 · 出发');wait_for(lambda:feed_count()==3)
   d.tap('倒序');d.wait('正序');d.wait('第 1 话 · 出发');assert feed_count()==3
   d.tap('书架');d.wait('1 部漫画 · 已保存','text');shelf_layout(d);d.shot('aligned-shelf');centered_button_labels(d,'aligned-shelf')
   d.tap('触摸测试漫画 · 目录');d.wait('第 1 话 · 出发');assert feed_count()==3;d.tap('返回');d.wait('设置')
   private=next((d.state/'private-files').glob('*pxa-manga-reader'));wait_for(lambda:(private/'reader-directory.json').exists())
   d.c.request('PACKAGE stop pxa-manga-reader');time.sleep(.3);d.c.request('PACKAGE run pxa-manga-reader');d.wait('设置');d.tap('触摸测试漫画 · 目录');d.wait('第 1 话 · 出发');assert feed_count()==3;d.shot('cached-directory-after-restart')
   d.tap('返回');d.wait('设置');d.tap('设置');d.wait('清除缓存');d.tap('清除缓存');d.wait('缓存已清除，书架和进度已保留','text');d.tap('返回');d.wait('设置');assert not (private/'reader-directory.json').exists()
   d.tap('触摸测试漫画 · 目录');d.wait('第 1 话 · 出发');wait_for(lambda:feed_count()==4)
   results[name]={'passed':True,'discovery_direct_read':True,'settings_direct':True,'reader_pixels_and_position_retained':True,'directory_cached_on_reopen_and_restart':True,'manual_refresh':True,'sort_scoped_cache':True,'clear_cache_removes_directory':True,'shelf_buttons_aligned':True,'labels_and_pagination_centered':True,'vector_arrows_centered':True}
   print(name,'navigation PASS',flush=True)
 finally:server.terminate();server.wait(timeout=5)
(out/'navigation-result.json').write_text(json.dumps(results,indent=2))
