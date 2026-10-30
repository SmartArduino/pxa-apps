"""Compact 20x32 adventurer, rebuilt from design-reference-v1.png.

The indexed head below traces reference A on the native grid. The coat,
rounded armor, cuffs, boots and articulated limbs follow the same design.
"""
from PIL import Image, ImageDraw
from functools import lru_cache
from math import hypot

WIDTH, HEIGHT, FRAMES, POSES, PARTS = 20,32,10,4,20
WALK,CARRY,BOW_AIM,TOOL_USE=range(POSES)
WALK_FRAMES,STAND,JUMP=8,8,9
STRIDE=(3,2,0,-2,-3,-2,0,2,0,0)
BOB=(0,1,0,1,0,1,0,1,0,0)
NEAR_LIFT=(0,0,0,0,0,1,2,1,0,2)
FAR_LIFT=(0,1,2,1,0,0,0,0,0,2)
# 0 transparent; 1 outline; 2/3/c skin; 4/5/f hair; e eye white.
# Traced from the first generated reference, not from Terraria game assets.
HEAD=(
 '00000000001100000000',
 '00000001104ff5111000',
 '000000155145f5451000',
 '00001545ff54455f1100',
 '0014f54fffff44544100',
 '0001455555f555f55100',
 '0014f5f45445f45f4510',
 '015f5455f54f545f4410',
 '0055444f545423544100',
 '00014444441e12414000',
 '00014412142e12400000',
 '00011413332e32210000',
 '00000114432222400000',
)
assert all(len(row)==WIDTH for row in HEAD)


# All bones use the SAME forward flexion direction in a right-facing view.
# Camera-facing right hand is always the item hand; rear left hand supports it.
ARM_LENGTHS={'camera':(4.25,4.25),'rear':(3.5,3.8)}
CAMERA_FEET=(14,12,10,8,6,8,10,12,8,9)
REAR_FEET=(6,8,10,12,14,12,10,8,12,12)
CAMERA_SWING=((5,20),(6,21),(9,22),(11,22),(12,21),(11,22),(9,22),(6,21))
REAR_SWING=((15,21),(14,21),(13,21),(10,21),(10,20),(10,21),(13,21),(14,21))
TOOL_HAND=((12,20),(11,13),(12,7),(14,9),(15,14),(14,18),(12,21),(11,21))


def arm_elbow(shoulder,hand,lengths,bend=1):
 """Snap a forward-flexing two-bone chain; never mirror one limb separately."""
 upper,lower=lengths;sx,sy=shoulder;hx,hy=hand
 best=None
 for y in range(max(0,int(sy-upper-1)),min(HEIGHT,int(sy+upper+2))):
  for x in range(max(0,int(sx-upper-1)),min(WIDTH,int(sx+upper+2))):
   if ((hx-sx)*(y-sy)-(hy-sy)*(x-sx))*bend<0:continue
   a,b=hypot(x-sx,y-sy),hypot(hx-x,hy-y)
   score=(a-upper)**2+(b-lower)**2+.3*(a+b-upper-lower)**2
   candidate=(score,y,x)
   if best is None or candidate<best:best=candidate
 return best[2],best[1]


@lru_cache(maxsize=40)
def rig(frame,pose=WALK):
 f=frame%FRAMES
 # Tool frames describe only the arm action. Runtime adds locomotion bob.
 body_frame=STAND if pose==TOOL_USE else f
 b=BOB[body_frame];s=STRIDE[body_frame]
 camera_shoulder,rear_shoulder=(8,14+b),(12,15+b)
 if f<WALK_FRAMES:
  hand,off=CAMERA_SWING[f],REAR_SWING[f]
 elif f==JUMP:
  hand,off=(10,20),(15,19)
 else:
  hand,off=(9,22),(15,21)
 if pose==CARRY:hand=(12,20 if f==JUMP else 21)
 elif pose==BOW_AIM:
  # Dominant/camera hand pulls the string; rear hand supports the bow grip.
  hand,off=(11,17),(18,17)
 elif pose==TOOL_USE:
  hand,off=TOOL_HAND[f%WALK_FRAMES],(15,21)
 hand=(hand[0],hand[1]+b);off=(off[0],off[1]+b)
 elbow=arm_elbow(camera_shoulder,hand,ARM_LENGTHS['camera'])
 rear_elbow=arm_elbow(rear_shoulder,off,ARM_LENGTHS['rear'])
 return dict(bob=b,stride=s,
             far=(11,REAR_FEET[body_frame],FAR_LIFT[body_frame]),
             near=(9,CAMERA_FEET[body_frame],NEAR_LIFT[body_frame]),
             hip_y=23,hand=hand,far_hand=off,neck=(10,13+b),
             shoulder=camera_shoulder,far_shoulder=rear_shoulder,elbow=elbow,far_elbow=rear_elbow,
             main_hand=hand,support_hand=off,
             camera_shoulder=camera_shoulder,rear_shoulder=rear_shoulder,
             camera_elbow=elbow,rear_elbow=rear_elbow,camera_hand=hand,rear_hand=off,pendant=(11,18+b))


def leg_joints(lane):
 hip,foot,lift=lane
 # Knees flex forward (+X), even while the foot moves backward in stance.
 knee=int((hip+foot)/2+.8*(lift+1)+.5)
 return ((hip,23),(knee,26-lift//2),(foot,28-lift),(foot,30-lift))


def leg(d,lane,part,far=False):
 (hip,hy),(knee,ky),(foot,ankle),(_,bottom)=leg_joints(lane)
 if part==0:
  d.line((hip,hy,knee,ky,foot,ankle),fill=1,width=4)
  d.line((hip,hy,knee,ky,foot,ankle),fill=10,width=2)
  d.line((hip+1,hy,knee+1,ky,foot+1,ankle),fill=9)
  d.point((hip,hy),fill=10);d.point((knee,ky),fill=10)
  d.rectangle((foot-1,ankle,foot+2,bottom+1),fill=1)
  d.rectangle((foot,bottom-1,foot+1,bottom),fill=13)
  d.line((foot,ankle,foot+1,ankle),fill=13)
  d.line((foot,bottom,foot+2,bottom),fill=13)
  d.point((foot,bottom-1),fill=15)
 elif part==4:
  d.polygon([(hip-1,24),(hip+2,24),(knee+2,ky),(foot+2,bottom-2),
             (foot-1,bottom-2),(knee-1,ky)],fill=1)
  d.line((hip,24,knee,ky,foot,bottom-3),fill=3,width=2)
  d.point((hip,24),fill=4)
  d.point((foot,bottom-2),fill=6)
 elif part==6:
  d.polygon([(foot-1,ankle),(foot+2,ankle),(foot+2,bottom-1),(foot+3,bottom),
             (foot+3,bottom+1),(foot-1,bottom+1)],fill=1)
  d.rectangle((foot,ankle,foot+1,bottom),fill=3)
  d.line((foot,bottom,foot+2,bottom),fill=3)
  d.point((foot,ankle+1),fill=4);d.line((foot,ankle,foot+1,ankle),fill=9)


def arm(d,r,rear,body=True,armor=False):
 side='rear' if rear else 'camera'
 shoulder=r[side+'_shoulder'];elbow=r[side+'_elbow'];hx,hy=r[side+'_hand']
 width=3 if rear else 4
 if body:
  d.line((*shoulder,*elbow,hx,hy),fill=1,width=width)
  d.line((*shoulder,*elbow),fill=7,width=2)
  d.point(shoulder,fill=7 if rear else 8)
  d.line((*elbow,hx,hy),fill=2,width=2)
  d.line((elbow[0]-1,elbow[1],hx-1,hy),fill=12)
  d.line((elbow[0]+1,elbow[1],hx+1,hy),fill=3)
  d.rectangle((hx-1,hy-1,hx+1,hy+1),fill=2)
  d.point((hx-1,hy-1),fill=12);d.point((hx+1,hy+1),fill=3)
 else:
  d.line((*shoulder,*elbow),fill=1,width=width)
  d.line((*shoulder,*elbow),fill=3,width=2)
  d.line((shoulder[0]-1,shoulder[1],elbow[0]-1,elbow[1]),fill=4)
  d.line((shoulder[0]+1,shoulder[1],elbow[0]+1,elbow[1]),fill=2)
  d.point(shoulder,fill=3 if rear else 4)
  ex,ey=elbow
  d.line((ex-1,ey,ex+1,ey),fill=6 if armor else 8)
  d.rectangle((hx-1,hy-1,hx+1,hy+1),fill=0)


def neck(frame):
 im=Image.new('L',(WIDTH,HEIGHT));d=ImageDraw.Draw(im);b=BOB[frame]
 d.line((9,13+b,11,13+b),fill=2);d.point((9,13+b),fill=3);d.point((11,13+b),fill=12)
 return im


def face(d,b):
 for y,row in enumerate(HEAD):
  for x,value in enumerate(row):
   if value!='0':d.point((x,y+b),fill=int(value,16))


def torso(d,b,body=False):
 d.polygon([(7,14+b),(9,14+b),(12,14+b),(14,14+b),(15,16+b),
            (14,20+b),(14,23),(6,23),(5,20+b),(5,16+b)],fill=1)
 d.polygon([(7,15+b),(12,15+b),(14,17+b),(13,22),(7,22),(6,18+b)],fill=7 if body else 3)
 d.line((7,16+b,7,20+b),fill=8 if body else 4)
 d.line((13,17+b,13,22),fill=6 if body else 2)
 # A short folded collar sits directly under the large jaw.
 d.line((8,14+b,10,16+b,12,14+b),fill=14 if body else 8)
 d.line((9,14+b,11,14+b),fill=6 if body else 2)
 d.line((10,17+b,10,21),fill=6 if body else 2)
 d.line((6,23,13,23),fill=13 if body else 9)
 d.point((11,23),fill=15 if body else 5)


def human(frame,part,pose=WALK):
 im=Image.new('L',(WIDTH,HEIGHT));d=ImageDraw.Draw(im)
 r=rig(frame,pose if 10<=part<=15 else WALK);b=r['bob']
 if part==0:
  torso(d,b,True)
  n=neck(frame);im.paste(n,(0,0),n.point(lambda v:255 if v else 0));face(d,b)
 elif part in (1,3,8):
  torso(d,b)
  if part==3:
   for x in (5,12):
    d.polygon([(x,14+b),(x+2,14+b),(x+3,16+b),(x+2,18+b),(x,18+b),(x-1,16+b)],fill=1)
    d.line((x,15+b,x+2,15+b),fill=4);d.line((x,16+b,x+2,16+b),fill=3)
    d.point((x+1,17+b),fill=2)
   d.polygon([(8,17+b),(12,17+b),(13,19+b),(12,22),(8,22),(7,19+b)],fill=3)
   d.line((8,18+b,8,21),fill=4);d.line((12,18+b,12,21),fill=2)
   d.line((7,23,13,23),fill=9);d.point((11,23),fill=5)
  elif part==8:
   sway=r['stride']//2
   d.polygon([(7,21),(13,21),(15+sway,26),(12,27),(9,26),(5+sway,27),(4+sway,25)],fill=1)
   d.polygon([(8,22),(12,22),(13+sway,25),(10,25),(6+sway,25)],fill=3)
   # Bodice and skirt are one cloth surface, not two black-outlined pieces.
   d.line((7,21,13,21),fill=3);d.point((7,21),fill=4);d.point((13,21),fill=2)
   d.line((8,23,7+sway,25),fill=4);d.line((6+sway,26,13+sway,26),fill=5)
 elif part==2:return helmet(frame,2)
 elif part in (4,6):
  leg(d,r['near'],part)
 elif part==5:
  wave=(0,1,2,2,1,0,-1,-1,0,0)[frame]
  d.polygon([(6,14+b),(8,16+b),(6,21),(5,26),(3,28),(0,27+wave),(1,21),(2,17+b)],fill=1)
  d.polygon([(5,16+b),(6,17+b),(4,22),(4,26),(1,26+wave),(2,21)],fill=2)
  d.line((4,17+b,3,22,2,25),fill=3);d.line((1,26+wave,3,27),fill=6)
  d.point((6,15+b),fill=5)
 elif part==7:
  d.polygon([(7,b),(12,b),(15,3+b),(15,5+b),(18,5+b),(19,6+b),
             (18,7+b),(2,7+b),(1,6+b),(4,5+b),(5,2+b)],fill=1)
  d.polygon([(7,1+b),(11,1+b),(14,4+b),(5,4+b)],fill=3)
  d.line((4,5+b,15,5+b),fill=2);d.line((3,6+b,18,6+b),fill=4)
  d.point((14,5+b),fill=5)
 elif part==9:return neck(frame)
 elif part in (10,11,12):
  arm(d,r,False,part==10,part==12)
 elif part in (13,14,15):
  arm(d,r,True,part==13,part==15)
 elif part==16:leg(d,r['near'],0)
 elif part==17:leg(d,r['far'],0,True)
 elif part==18:leg(d,r['far'],4,True)
 elif part==19:leg(d,r['far'],6,True)
 return im


def helmet(frame,style):
 im=Image.new('L',(WIDTH,HEIGHT));d=ImageDraw.Draw(im);b=BOB[frame]
 d.polygon([(6,2+b),(9,1+b),(12,1+b),(15,3+b),(16,5+b),(16,7+b),
            (10,7+b),(8,9+b),(8,12+b),(5,12+b),(4,10+b),(3,7+b),(4,4+b)],fill=1)
 d.polygon([(6,3+b),(9,2+b),(12,2+b),(14,4+b),(15,6+b),(9,6+b),
            (7,8+b),(7,11+b),(5,10+b),(4,7+b),(5,4+b)],fill=3)
 d.line((6,4+b,8,3+b,12,3+b),fill=4);d.line((10,7+b,15,7+b),fill=6)
 d.line((5,8+b,5,10+b),fill=2)
 if style==1:
  d.line((9,6+b,15,6+b),fill=5);d.point((6,9+b),fill=5)
 elif style==2:
  d.line((10,2+b,10,5+b),fill=6);d.point((6,9+b),fill=8)
 elif style==3:
  d.polygon([(8,4+b),(9,b),(10,b),(12,4+b)],fill=3)
  d.line((9,b,9,3+b),fill=8);d.point((10,3+b),fill=7)
  d.line((6,8+b,6,11+b),fill=6)
 return im


def mail(frame,style,pose=WALK):
 im=human(frame,3,pose);d=ImageDraw.Draw(im);b=BOB[frame]
 if style==1:
  d.polygon([(9,17+b),(12,18+b),(12,21),(9,22),(8,20+b)],fill=6)
  d.line((9,18+b,9,20),fill=4)
  for x,y in ((8,18+b),(12,18+b),(8,21),(12,21)):d.point((x,y),fill=5)
 elif style==2:
  d.line((9,17+b,10,18+b,12,18+b),fill=4)
  d.line((10,19+b,10,21),fill=6)
  d.line((8,21,12,21),fill=2)
  d.point((6,15+b),fill=5);d.point((13,15+b),fill=5)
 else:
  d.polygon([(10,17+b),(12,19+b),(10,22),(8,19+b)],fill=2)
  d.polygon([(10,18+b),(11,19+b),(10,21),(9,19+b)],fill=7)
  d.line((10,18+b,10,20),fill=8)
  d.point((5,15+b),fill=8);d.point((13,15+b),fill=8)
 return im


def outfit(frame,style,pose=WALK):
 im=human(frame,8 if style==2 else 1,pose);d=ImageDraw.Draw(im);b=BOB[frame]
 if style==0:
  d.line((7,17+b,12,21),fill=9,width=2)
  d.point((8,17+b),fill=10);d.point((10,19+b),fill=10)
  d.point((12,16+b),fill=5);d.line((6,23,13,23),fill=9)
 elif style==1:
  d.line((8,15+b,9,18+b),fill=6)
  d.line((12,15+b,11,18+b),fill=2)
  d.line((7,18+b,12,22),fill=9,width=2)
  d.point((10,20+b),fill=10)
  d.rectangle((5,20,7,22),fill=9);d.point((6,20),fill=5)
 else:
  d.line((8,15+b,10,18+b,13,15+b),fill=7,width=2)
  d.point((11,19+b),fill=5)
  d.line((8,24,7,26),fill=6);d.line((11,23,12,26),fill=2)
 return im
