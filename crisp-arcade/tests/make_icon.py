#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Recreate the original 128px four-game app icon with no image dependencies."""
from pathlib import Path
import struct
import zlib

size = 128
pixels = [[(12,17,34) for _ in range(size)] for _ in range(size)]
def rect(x,y,w,h,c):
    for py in range(max(0,y),min(size,y+h)):
        for px in range(max(0,x),min(size,x+w)):
            pixels[py][px]=c
def line(x,y,ex,ey,c,width=2):
    dx,dy=abs(ex-x),-abs(ey-y)
    sx,sy=1 if x<ex else -1,1 if y<ey else -1
    err=dx+dy
    while True:
        rect(x,y,width,width,c)
        if x==ex and y==ey:break
        twice=2*err
        if twice>=dy:err+=dy;x+=sx
        if twice<=dx:err+=dx;y+=sy
cyan=(61,210,221);gold=(250,182,69);purple=(173,137,255);pink=(252,109,147)
for x,y,c in [(7,7,cyan),(67,7,gold),(7,67,purple),(67,67,pink)]:
    rect(x,y,54,54,(25,33,53));rect(x,y,54,3,c);rect(x,y,3,54,c)
# Fishing rod, hook, and fish.
line(17,23,43,17,cyan);line(43,17,48,40,cyan);line(48,40,43,44,cyan)
rect(16,43,20,9,cyan);line(34,47,41,40,cyan);line(34,47,41,54,cyan);rect(19,45,3,2,(12,17,34))
# Grappling anchor and swinging player.
rect(104,16,8,8,gold);rect(106,18,4,4,(25,33,53));line(107,25,85,43,gold)
rect(78,40,14,14,gold);rect(81,43,8,8,(25,33,53));line(90,52,101,57,gold)
# Reflector craft and returning projectiles.
rect(19,83,28,7,purple);rect(26,77,14,7,purple);line(17,99,48,99,purple)
line(13,117,25,105,gold);line(42,105,54,117,gold);rect(14,77,3,3,(246,247,255))
# Four rolling color lanes with a shot.
colors=[pink,(74,222,128),(56,189,248),gold]
for i,c in enumerate(colors):
    rect(75,76+i*7,17,5,c);rect(94,76+i*7,23,5,colors[(i+1)%4])
line(96,118,96,95,(246,247,255))
# Compact frame keeps the artwork crisp on light launcher backgrounds.
for x,y,w,h in [(0,0,128,2),(0,126,128,2),(0,0,2,128),(126,0,2,128)]:rect(x,y,w,h,(46,59,84))
def chunk(kind,data):
    return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data)&0xffffffff)
raw=b''.join(b'\0'+bytes(v for pixel in row for v in pixel) for row in pixels)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',size,size,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')
target=Path(__file__).resolve().parents[1]/'assets/icon.png'
target.write_bytes(png)
print(target)
