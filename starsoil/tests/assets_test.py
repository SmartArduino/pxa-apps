#!/usr/bin/env python3
"""Check native font pixels and the shared walking/equipment rig."""
from pathlib import Path
import runpy
from PIL import Image, ImageDraw
from math import hypot

ROOT=Path(__file__).resolve().parents[1]
font=runpy.run_path(str(ROOT/'tools/build_font.py'))
rows={cp:(advance,data) for cp,advance,data in font['collect'](ROOT/'art/fonts/ZLabsPixel_12px_M_CN.kbitx')}
# These are the upstream native glyphs, including baseline/descender pixels.
assert rows[ord('A')]==(6,[0,0,512,512,1280,1280,1792,2176,2176,2176,0,0])
assert rows[ord('a')]!=rows[ord('A')]
assert rows[ord('g')][1][10:]==[2176,1792]
assert rows[ord('星')][0]==12 and rows[ord('星')][1][-1]==4094
assert all(0<=row<4096 for _,data in rows.values() for row in data)
print(f'PASS {len(rows)} original font glyphs, Latin case, descenders and 12px Chinese cells')

characters=runpy.run_path(str(ROOT/'tools/characters.py'))
human,helmet,rig=characters['human'],characters['helmet'],characters['rig']
F,P=characters['FRAMES'],characters['POSES']
assert (characters['WIDTH'],characters['HEIGHT'],F,P,characters['PARTS'])==(20,32,10,4,20)
for pose in range(P):
  composites=set()
  for frame in range(F):
    r=rig(frame,pose)
    layers=[human(frame,part,pose)for part in range(20)]
    for part,im in enumerate(layers):
      assert im.size==(20,32) and im.getbbox()
      assert max(im.getdata())<(16 if part in (0,9,10,13,16,17)else 11)
    for part,key in ((10,'main_hand'),(13,'support_hand')):
      assert layers[part].getpixel(r[key]) in (2,3,12)
    for part,key in ((11,'main_hand'),(12,'main_hand'),(14,'support_hand'),(15,'support_hand')):
      assert layers[part].getpixel(r[key])==0
    for side in ('camera','rear'):
      sh,el,h=[r[side+'_'+j]for j in ('shoulder','elbow','hand')]
      assert (h[0]-sh[0])*(el[1]-sh[1])-(h[1]-sh[1])*(el[0]-sh[0])>=0,'one limb bends as if facing left'
      target=(4.25,4.25)if side=='camera'else(3.5,3.8)
      lens=(hypot(el[0]-sh[0],el[1]-sh[1]),hypot(h[0]-el[0],h[1]-el[1]))
      assert all(abs(v-w)<=.8 for v,w in zip(lens,target)),(pose,frame,side,lens)
    eye=(12,10+characters['BOB'][frame])
    for style in (1,2,3):assert helmet(frame,style).getpixel(eye)==0
    assert human(frame,7).getpixel(eye)==0
    # Isolated source chains cannot borrow trunk/cape pixels to conceal breaks.
    for lane in ('near','far'):
      loc=rig(frame,0);j=characters['leg_joints'](loc[lane])
      assert j[0][1]<j[1][1]<j[2][1]<j[3][1]
      im=Image.new('L',(20,32));characters['leg'](ImageDraw.Draw(im),loc[lane],0,lane=='far')
      for point in j:assert im.getpixel(point)>1,(pose,frame,lane,point)
    comp=Image.new('L',(20,32))
    for part in (17,16,0,13,10):
      layer=layers[part];comp.paste(layer,(0,0),layer.point(lambda v:255 if v else 0))
    if frame<8:composites.add(comp.tobytes())
  assert len(composites)==8,'gait/action frames collapse into duplicate poses'
# Counter-swing is authored in +X, not by mirroring one limb.
for f in range(8):
 r=rig(f,0)
 if f in (0,1,3,4,5,7):
  assert (r['main_hand'][0]-8)*(r['near'][1]-9)<0,'same-side arm and leg swing together'
 assert r['main_hand']==r['camera_hand'] and r['support_hand']==r['rear_hand']
assert rig(0,0)['near'][1]>rig(0,0)['far'][1]
assert rig(4,0)['near'][1]<rig(4,0)['far'][1]
assert rig(8,0)['near'][2]==rig(8,0)['far'][2]==0
assert rig(9,0)['near'][2]==rig(9,0)['far'][2]==2
for f in range(8):
 r=rig(f,2)
 assert r['support_hand'][0]-r['main_hand'][0]==7
 assert r['support_hand'][1]==r['main_hand'][1]
print('PASS right-facing anatomy: invariant main/support hands, forward elbow flexion, opposite arm/leg swing, alternating feet, stand/jump frames and two-hand bow')
for frame in range(F):
 assert len({characters['mail'](frame,s).tobytes()for s in (1,2,3)})==3
 assert len({characters['outfit'](frame,s).tobytes()for s in (0,1,2)})==3
import sys
sys.path.insert(0,str(ROOT/'tools'))
import draw_art
for kind in range(5):
  frames=[draw_art.creature(kind,f) for f in range(6)]
  assert all(im.size==(24,24) and im.getbbox() for im in frames)
  assert len({im.tobytes() for im in frames})>=4
# Connected sides remain opaque, exposed corners round off, and interiors stay solid.
for t in (1,2,3,4,5,6,9,10,11,12,13,14,15,16,17,18,19):
  for mask in range(16):
    im=draw_art.tile(t,mask)
    assert im.getpixel((8,8))[3]==255
    if mask==15:assert im.getextrema()[3]==(255,255)
    if mask==0:assert im.getpixel((0,0))[3]==0 and im.getpixel((15,15))[3]==0
    if mask&2:assert all(im.getpixel((15,y))[3]==255 for y in range(16))
    if mask&8:assert all(im.getpixel((0,y))[3]==255 for y in range(16))
assert draw_art.tile(8,15).getpixel((0,8))[3]==255
assert draw_art.tile(8,15).getpixel((15,8))[3]==255
print('PASS distinct copper/iron/crystal armor and three clothing designs; five animated creature identities and connected terrain')

# Full foliage interiors must cover the vertices, not merely edge midpoints.
for snow in (True,False):
    assert draw_art.visuals.foliage(15,snow).getextrema()[3]==(255,255)
assert draw_art.tile(8,15).getextrema()[3]==(255,255)
print('PASS fully joined temperate, snow and jungle foliage have no transparent corner pixels')
