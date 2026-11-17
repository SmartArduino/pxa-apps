#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check actual prepared skin pixels for discontinuities and filter gutters."""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]/'resources/ui'
def load(name):
    tile=Image.open(root/name).convert('RGBA');w,h=tile.size
    core=tile.crop((2,2,w-2,h-2))
    for x in range(w):
        for y in range(h):
            if x<2 or y<2 or x>=w-2 or y>=h-2:
                assert tile.getpixel((x,y))==core.getpixel((min(core.width-1,max(0,x-2)),min(core.height-1,max(0,y-2)))),name
    return core,w*h*4
panels=[load(f'panel-{i}.png') for i in range(9)]
buttons=[load(f'button-{i}.png') for i in range(3)]
for row in range(3):
    for col in range(2):
        a,b=panels[row*3+col][0],panels[row*3+col+1][0]
        assert all(a.getpixel((a.width-1,y))==b.getpixel((0,y)) for y in range(a.height)),('panel horizontal',row,col)
for row in range(2):
    for col in range(3):
        a,b=panels[row*3+col][0],panels[(row+1)*3+col][0]
        assert all(a.getpixel((x,a.height-1))==b.getpixel((x,0)) for x in range(a.width)),('panel vertical',row,col)
for col in range(2):
    a,b=buttons[col][0],buttons[col+1][0]
    assert all(a.getpixel((a.width-1,y))==b.getpixel((0,y)) for y in range(a.height)),('button horizontal',col)
resident=sum(n for _,n in panels+buttons)
assert resident<=128*128*4+256*64*4
print('UI skins passed: all adjoining RGBA edges match, all gutters repeat edge pixels, resident bytes',resident)
