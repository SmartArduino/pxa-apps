#!/usr/bin/env python3
"""Audit visible font pixels in real signed simulator frames across UI profiles."""
import argparse,json,math,re,time
from pathlib import Path
from PIL import Image,ImageFont,ImageDraw
from net_harness import Game,configure

p=argparse.ArgumentParser();p.add_argument('--package',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--profile');a=p.parse_args()
configure(a.package,a.output)
root=Path(__file__).resolve().parents[1]
font=ImageFont.truetype(str(root/'tools/fonts/fusion-pixel-12px-monospaced-zh_hans.ttf'),12,layout_engine=ImageFont.Layout.BASIC)
# Simulator CLI order is top,right,bottom,left.
profiles=[('small',320,240,'0,0,0,0'),('wide',854,480,'0,0,0,0'),('square',480,480,'0,0,0,0'),('bottom',320,240,'0,0,78,0'),('inset',320,240,'0,44,54,44')]
checks=[];captures=[]
def expected_pixels(text):
 im=Image.new('L',(max(1,round(font.getlength(text))),12));d=ImageDraw.Draw(im);d.fontmode='1';d.text((0,10),text,font=font,fill=255,anchor='ls');return sum(v!=0 for v in im.tobytes())

for name,w,h,safe in profiles:
 if a.profile and a.profile!=name:continue
 g=Game(name,w,h,'buttons',safe)
 try:
  g.wait('Ash Harbor ready');time.sleep(.35)
  rw,rh,s,ox,oy,lift,ml,*_=g.geometry();sides=list(map(int,safe.split(',')))
  left=max(12,math.ceil((sides[3]*rw/w-ox)/s)+4);right=min(308,math.floor((rw-sides[1]*rw/w-ox)/s)-4)
  def shot(label):
   time.sleep(.18);g.shot(label);captures.append(str(g.out/(label+'.png')));return Image.open(g.out/(label+'.png')).convert('RGB')
  def audit(im,label,box,frame=True,center=True,text=None):
   x,y,bw,bh=box;pad=3 if frame else 0
   def px(v):return math.floor(ox+v*s+.5)*w/rw
   def py(v):return math.floor(oy+v*s+.5)*h/rh
   rect=(math.floor(px(x+pad)),math.floor(py(y+pad)),math.ceil(px(x+bw-pad)),math.ceil(py(y+bh-pad)))
   crop=im.crop(rect);rgb=crop.tobytes();mask=Image.new('1',crop.size);mask.putdata([rgb[i:i+3]==b'\xff\xe6\xac' for i in range(0,len(rgb),3)]);bounds=mask.getbbox()
   assert bounds,(name,label,'missing visible text',rect)
   bx,by,ex,ey=bounds;actual=(rect[0]+(bx+ex)/2,rect[1]+(by+ey)/2);target=((px(x)+px(x+bw))/2,(py(y)+py(y+bh))/2)
   tolerance=max(1.5,s*w/rw)
   assert abs(actual[1]-target[1])<=tolerance,(name,label,'vertical',actual,target,bounds)
   if center:assert abs(actual[0]-target[0])<=tolerance,(name,label,'horizontal',actual,target,bounds)
   count=mask.convert('L').tobytes().count(255);zoom=s*w/rw
   if text and abs(zoom-round(zoom))<.001:
    expected=expected_pixels(text)*round(zoom)**2
    assert count==expected,(name,label,'clipped or extra foreground',count,expected,bounds)
   checks.append(dict(profile=name,label=label,box=box,vertical_error=round(abs(actual[1]-target[1]),3),pixel_count=count))
  if 'launch view=5' in g.text():
   for i in range(3):
    shot('story-'+str(i))
    if i<2:g.key('Z')
   g.key('X');g.wait('story-finished')
  actions=198-ml;character=min(137,actions-34);toolbar=min(106,character-30)
  def homebox(i):
   if right-left>=296:return [(left,toolbar,82,20),(104,toolbar,64,20),(right-130,toolbar,64,20),(right-56,toolbar,56,20)][i]
   first=max(60,min(82,(right-left-18)//4));bw=(right-left-first-18)//3;x=left+first+6+(i-1)*(bw+6) if i else left
   return (x,toolbar,(right-x if i==3 else bw) if i else first,20)
  for focus in range(4):
   im=shot(f'home-{focus}')
   for i,text in enumerate(['街机','训练','招式']):
    bw=(right-left-38)//3;audit(im,text,(left+10+i*(bw+9),actions,bw,26),text=text)
   audit(im,'character',(126,character,68,26),text=['陆炎','霁岚','韩震','唐绫'][0])
   for i,text in enumerate(['联机','场景','剧情'],1):audit(im,text,homebox(i),text=text)
   g.key('RIGHT')
  g.tap(160,character+13);g.wait('roster view=6')
  for i in range(4):
   cw=(right-left-8)//2;gh=min(64,(210-lift-6-26-6)//2);x=left+(i%2)*(cw+8);y=26+(i//2)*(gh+6)
   g.tap(x+cw/2,y+gh/2);im=shot(f'roster-{i}')
   for text,box in [('返回',(left+8,210-lift,76,24)),('选定',(right-98,210-lift,90,24))]:audit(im,text,box,text=text)
  g.key('X')
  b=homebox(2);g.tap(b[0]+b[2]/2,b[1]+10);g.wait('stages-open')
  for i in range(4):
   g.key('RIGHT' if i%2 else 'DOWN');im=shot(f'stages-{i}')
   for j,text in enumerate(['雨港夜渡','竹影晨院','雪关古门','灯河石桥']):
    x=left+(j%2)*(cw+8);y=26+(j//2)*(gh+6);audit(im,'stage-'+str(j),(x+8,y+gh-19,cw-16,14),False,text=text)
  g.key('X');g.tap(255,actions+13);im=shot('commands');g.key('Z')
  g.tap(65,actions+13);g.wait('opponent-open');shot('arcade-opponent');g.key('TAB');shot('arcade-mode');g.key('X')
  g.tap(160,actions+13);g.wait('opponent-open');im=shot('training-opponent')
  audit(im,'start-training',(right-110,218-lift,102,22),text='开始训练')
  g.key('Z');g.wait('frame-bank ready');time.sleep(1.7);shot('training')
  shift=g.geometry()[7]
  points=[(60+shift,223-lift),(228,220-lift),(262,220-lift),(296,220-lift),(228,190-lift)]
  for i,(x,y) in enumerate(points):g.pointer('DOWN',i,x,y)
  shot('five-key-history')
  for i,(x,y) in reversed(list(enumerate(points))):g.pointer('UP',i,x,y)
  g.tap((left+right)//2,24);g.wait('practice-open')
  for i in range(4):
   im=shot(f'practice-{i}');audit(im,'return-practice',(106,210-lift,108,24),text='返回练习');g.key('RIGHT')
  g.key('X');g.key('ENTER');im=shot('pause');pl=max(0,lift-18)
  for i,text in enumerate(['继续','招式','重开','主菜单']):audit(im,text,(83,94+i*28-pl,154,24),text=text)
  for _ in range(3):g.key('DOWN')
  g.key('Z');b=homebox(1);g.tap(b[0]+b[2]/2,b[1]+10);g.wait('online-open');im=shot('online')
  footer=210-lift;ay=min(145,footer-28);act=min(105,ay-32);cy=min(65,act-32);bw=min(124,(right-left-32)//2)
  for text,x,y in [('创建房间',left+12,act),('搜索房间',right-12-bw,act)]:audit(im,text,(x,y,bw,26),text=text)
  # Go directly to the numeric keyboard without needing another peer.
  g.tap(160,ay+12);g.wait('Ash LAN opened host=0');im=shot('address')
  cols=4 if footer<177 else 3;kw=54 if cols==4 else 60;stepx=60 if cols==4 else 65;top=62 if footer<140 else 81;step=min(24,(footer-top-4)//(12//cols));kl=(320-((cols-1)*stepx+kw))//2
  for i,text in enumerate('123456789.0<'):audit(im,'key-'+text,(kl+i%cols*stepx,top+i//cols*step,kw,step-2),text=text)
  g.tap(right-50,footer+12);shot('invalid-address')
  # Full length ASCII address tests descenders/digits and horizontal fit.
  digits='123456789.0'
  for ch in '255.255.255.255':
   i=digits.index(ch);g.tap(kl+i%cols*stepx+kw/2,top+i//cols*step+(step-2)/2)
  im=shot('long-address');iy=43 if footer<140 else 62;audit(im,'full-ip',(left+20,iy,right-left-40,12),False,text='255.255.255.255')
  assert not re.search(r'label elided|missing Chinese label|render error=|out of bounds',g.text()),g.text()[-5000:]
  print(json.dumps({'profile':name,'passed':True,'checks':sum(v['profile']==name for v in checks)},ensure_ascii=False),flush=True)
 finally:g.stop()
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'result.json').write_text(json.dumps({'passed':True,'captures':captures,'checks':checks},ensure_ascii=False,indent=2))
