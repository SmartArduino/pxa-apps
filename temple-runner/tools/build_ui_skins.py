#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare sharp corner tiles with extruded texel gutters for native Canvas.
Small center strips keep all nine pieces below the old 128-square texture's
resident byte count. The included PNGs are the normal offline build inputs.
"""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
image=Image.open(root/'resources/brand/panel-source.png').convert('RGBA')
bounds=image.getchannel('A').point(lambda a:255 if a>127 else 0).getbbox()
image=image.crop(bounds).resize((256,256),Image.Resampling.LANCZOS)
spans=((0,48),(120,136),(208,256));gutter=2;resident=0
for row,(y0,y1) in enumerate(spans):
    for col,(x0,x1) in enumerate(spans):
        core=image.crop((x0,y0,x1,y1));w,h=core.size
        tile=Image.new('RGBA',(w+4,h+4));tile.paste(core,(gutter,gutter))
        for x in range(w+4):
            for y in range(h+4):
                if x<2 or y<2 or x>=w+2 or y>=h+2:
                    tile.putpixel((x,y),core.getpixel((min(w-1,max(0,x-2)),min(h-1,max(0,y-2)))))
        tile.save(root/'resources/ui'/f'panel-{row*3+col}.png',optimize=True);resident+=tile.width*tile.height*4
assert resident<=128*128*4
print('Nine panel pieces with two-pixel gutters:',resident,'resident bytes')
