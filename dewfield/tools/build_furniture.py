#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compile original furniture atlases into bounded, native 4-bit pixel sprites.

Target dimensions author the game's overhead projection and existing room
footprints. Source PNGs and exact prompts live in art/reference/furniture*.json.
"""
from pathlib import Path
from PIL import Image
import argparse
ROOT=Path(__file__).resolve().parents[1]
PRIMARY=[('Bed',38,46),('Table',42,28),('Chair',20,26),('Cabinet',38,35),
 ('Stove',43,49),('Fireplace',43,46),('Workbench',65,33),('Bookcase',69,44),
 ('Divider',73,25),('Armchair',31,32),('WoodRack',46,38),('Barrels',43,29)]
SECONDARY=[('Television',31,37,(65,65,415,465)),('CounterBay',44,26,(425,175,945,455)),
 ('ShelfBay',64,36,(950,75,1536,455)),('Bench',37,24,(45,520,510,945)),
 ('Planter',27,21,(525,520,995,945)),('Calendar',25,31,(1040,495,1460,945))]
def isolate(cell):
 mask=cell.getchannel('A').point(lambda a:255 if a>=192 else 0);w,h=cell.size;bits=bytearray(mask.tobytes());largest=[]
 for at in range(len(bits)):
  if not bits[at]:continue
  bits[at]=0;part=[at];head=0
  while head<len(part):
   n=part[head];head+=1;x,y=n%w,n//w
   for dx,dy in ((-1,-1),(0,-1),(1,-1),(-1,0),(1,0),(-1,1),(0,1),(1,1)):
    xx,yy=x+dx,y+dy
    if 0<=xx<w and 0<=yy<h:
     q=yy*w+xx
     if bits[q]:bits[q]=0;part.append(q)
  if len(part)>len(largest):largest=part
 assert largest
 cleaned=bytearray(w*h)
 for at in largest:cleaned[at]=255
 mask=Image.frombytes('L',(w,h),bytes(cleaned));cell.putalpha(mask)
 return cell.crop(mask.getbbox())
def compile_image(name,source,width,height):
 image=isolate(source).resize((width,height),Image.Resampling.NEAREST)
 alpha=image.getchannel('A').point(lambda a:255 if a>=192 else 0)
 # Only opaque pixels participate in palette quantisation. Invisible matte
 # RGB must not consume a palette entry or leave a dark/bright fringe.
 opaque=[image.getpixel((x,y))[:3] for y in range(height) for x in range(width) if alpha.getpixel((x,y))]
 strip=Image.new('RGB',(len(opaque),1));strip.putdata(opaque);indexed=strip.quantize(colors=16,dither=Image.Dither.NONE)
 palette=indexed.getpalette()[:48];values=list(indexed.getdata());runs=[];at=0
 for y in range(height):
  x=0
  while x<width:
   if not alpha.getpixel((x,y)):x+=1;continue
   first=x;offset=at
   while x<width and alpha.getpixel((x,y)):x+=1;at+=1
   runs.append((y,first,x-first,offset))
 packed=[values[i]|((values[i+1]if i+1<len(values)else 0)<<4) for i in range(0,len(values),2)]
 colors=[tuple(palette[i:i+3])for i in range(0,48,3)];palettes=[colors]
 if name=='Bed' or name.startswith('Chair'):
  for target in ((105,127,149),(102,148,147),(182,128,117)):
   row=[]
   for red,green,blue in colors:
    if red<190 and green>=red*.85 and green>blue*1.3:
     # Keep cloth light/shadow variation; cream pillows and timber stay intact.
     brightness=max(.5,min(1.2,(red+green+blue)/(116+139+88)))
     row.append(tuple(min(255,round(c*brightness))for c in target))
    else:row.append((red,green,blue))
   palettes.append(row)
 if name=='Planter':
  palettes=[]
  for flowers in (True,False):
   for season in range(4):
    row=[]
    for red,green,blue in colors:
     bloom=(red>205 and green>190 and blue>155) or (red>140 and blue>green*.75 and red>green*1.05)
     leaf=red<190 and green>=red*.85 and green>blue*1.3
     c=(red,green,blue)
     if bloom and not flowers:c=(110,140,68)
     if season==2 and (leaf or bloom and not flowers):c=(145,141,77)
     if season==3 and (leaf or bloom):c=(191,207,190)
     row.append(c)
    palettes.append(row)
 def rgb(c):return ((c[0]&248)<<8)|((c[1]&252)<<3)|(c[2]>>3)
 # Contact is restricted to actual soles/plinths in the bottom three rows.
 # Air under the tabletop, chair seat and raised apron stays transparent.
 points={}
 for x in range(width):
  yy=next((y for y in range(height-1,max(-1,height-4),-1)if alpha.getpixel((x,y))),None)
  if yy is None:continue
  for xx,y,a in ((x,yy+1,2),(x-1,yy+1,1),(x+1,yy+1,1),(x,yy+2,1)):
   if 0<=xx<width and 0<=y<height and alpha.getpixel((xx,y)):continue
   points[xx,y]=max(a,points.get((xx,y),0))
 contacts=[]
 for y in sorted({y for x,y in points}):
  xs=sorted(x for x,yy in points if yy==y);i=0
  while i<len(xs):
   x=xs[i];a=points[x,y];end=x+1;i+=1
   while i<len(xs)and xs[i]==end and points[xs[i],y]==a:end+=1;i+=1
   contacts.append((y,x,end-x,a))
 key=name;out=[]
 out.append(f'inline constexpr uint16_t {key}Palette[]={{'+','.join(str(rgb(c))for row in palettes for c in row)+'};')
 out.append(f'inline constexpr uint8_t {key}Pixels[]={{'+','.join(map(str,packed))+'};')
 out.append(f'inline constexpr sprites::Span {key}Spans[]={{'+','.join('{'+','.join(map(str,r))+'}'for r in runs)+'};')
 out.append(f'inline constexpr sprites::ShadowSpan {key}Contacts[]={{'+','.join('{'+','.join(map(str,r))+'}'for r in contacts)+'};')
 out.append(f'inline constexpr sprites::Shadow {key}Shadow={{{len(contacts)},{key}Contacts}};')
 out.append(f'inline constexpr Image {key}={{{{{width},{height},{len(runs)},{key}Spans,nullptr,{width//2},{height},&{key}Shadow}},{key}Pixels,{key}Palette,{len(palettes)}}};')
 preview=Image.new('RGBA',(width,height));idx=0
 for y in range(height):
  for x in range(width):
   if alpha.getpixel((x,y)):preview.putpixel((x,y),(*colors[values[idx]],255));idx+=1
 size=len(packed)+len(runs)*6+len(contacts)*4+len(palettes)*32
 print(f'{name}: {width}x{height}, {len(opaque)} opaque pixels, {size} bytes')
 return out,preview,size

def main():
 p=argparse.ArgumentParser();p.add_argument('--preview',type=Path);args=p.parse_args()
 source=Image.open(ROOT/'art/reference/furniture-v1.png').convert('RGBA')
 details=Image.open(ROOT/'art/reference/furniture-details-v1.png').convert('RGBA')
 output=['// SPDX-License-Identifier: MIT','// Generated by tools/build_furniture.py from original imagegen source PNGs.','#pragma once','#include "pixel_sprite.hpp"','namespace furniture_sprites {','struct Image {sprites::Sprite shape;const uint8_t*indices;const uint16_t*palette;uint8_t palettes;};']
 images=[];total=0
 for i,(name,w,h)in enumerate(PRIMARY):
  x,y=i%4,i//4;cell=source.crop((x*source.width//4,y*source.height//3,(x+1)*source.width//4,(y+1)*source.height//3))
  lines,image,size=compile_image(name,cell,w,h);output+=lines;images.append((name,image));total+=size
 for name,w,h,box in SECONDARY:
  lines,image,size=compile_image(name,details.crop(box),w,h);output+=lines;images.append((name,image));total+=size
 directions=Image.open(ROOT/'art/reference/furniture-directions-v1.png').convert('RGBA')
 for row,(kind,sw,h) in enumerate((('Chair',22,26),('Armchair',28,32),('Cabinet',24,35))):
  for col,suffix in ((0,'North'),(1,'East'),(3,'West')):
   w=(20 if kind=='Chair' else 31 if kind=='Armchair' else 38) if col==0 else sw
   cell=directions.crop((col*directions.width//4,row*directions.height//3,(col+1)*directions.width//4,(row+1)*directions.height//3))
   name=kind+suffix;lines,image,size=compile_image(name,cell,w,h);output+=lines;images.append((name,image));total+=size
  output+=['inline constexpr const Image*'+kind+'Directions[]={&'+kind+'North,&'+kind+'East,&'+kind+',&'+kind+'West};']
 output+=['inline constexpr const Image*All[]={'+','.join('&'+name for name,_ in images)+'};','} // namespace furniture_sprites']
 (ROOT/'furniture_sprites.hpp').write_text('\n'.join(output)+'\n');print('Total compiled furniture bytes:',total)
 if args.preview:
  sheet=Image.new('RGBA',(6*82,5*64),(72,69,50,255))
  from PIL import ImageDraw
  draw=ImageDraw.Draw(sheet)
  for i,(name,im)in enumerate(images):
   x=(i%6)*82;y=(i//6)*64;sheet.alpha_composite(im,(x+(82-im.width)//2,y+51-im.height));draw.text((x+2,y+52),name,fill=(239,221,174))
  args.preview.parent.mkdir(parents=True,exist_ok=True);sheet.resize((1476,960),Image.Resampling.NEAREST).save(args.preview)
if __name__=='__main__':main()
