#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare matching skin edges and extruded texel gutters for native Canvas.
Included PNGs are offline build inputs; combined resident pixels stay below
those of the original 128-square panel plus 256x64 button.
"""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
def prepare(name,size):
    image=Image.open(root/'resources/brand'/name).convert('RGBA')
    bounds=image.getchannel('A').point(lambda a:255 if a>127 else 0).getbbox()
    return image.crop(bounds).resize(size,Image.Resampling.LANCZOS)
def mix(a,b,t):return tuple(round(x+(y-x)*t) for x,y in zip(a,b))
def save(core,name):
    w,h=core.size;tile=Image.new('RGBA',(w+4,h+4))
    for x in range(w+4):
        for y in range(h+4):tile.putpixel((x,y),core.getpixel((min(w-1,max(0,x-2)),min(h-1,max(0,y-2)))))
    tile.save(root/'resources/ui'/name,optimize=True)
    return tile.width*tile.height*4
image=prepare('panel-source.png',(256,256));spans=((0,48),(120,136),(208,256));resident=0
for row,(y0,y1) in enumerate(spans):
    for col,(x0,x1) in enumerate(spans):
        core=image.crop((x0,y0,x1,y1));w,h=core.size
        for x in range(w):
            for y in range(h):
                if col==1:
                    top=mix(image.getpixel((47,y0+y)),image.getpixel((208,y0+y)),x/(w-1))
                    if row==1:
                        top=mix(image.getpixel((47,47)),image.getpixel((208,47)),x/(w-1))
                        bottom=mix(image.getpixel((47,208)),image.getpixel((208,208)),x/(w-1))
                        top=mix(top,bottom,y/(h-1))
                    core.putpixel((x,y),top)
                elif row==1:core.putpixel((x,y),mix(image.getpixel((x0+x,47)),image.getpixel((x0+x,208)),y/(h-1)))
        resident+=save(core,f'panel-{row*3+col}.png')
image=prepare('button-source.png',(320,80))
for col,(x0,x1) in enumerate(((0,80),(152,168),(240,320))):
    core=image.crop((x0,0,x1,80))
    if col==1:
        for x in range(core.width):
            for y in range(80):core.putpixel((x,y),mix(image.getpixel((79,y)),image.getpixel((240,y)),x/(core.width-1)))
    resident+=save(core,f'button-{col}.png')
assert resident<=128*128*4+256*64*4
print('Panel and button pieces with continuous edges and two-pixel gutters:',resident,'resident bytes')
