#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Original Lantern Grove icon, drawn from primitive pixels; no source assets."""
from pathlib import Path
from PIL import Image, ImageDraw
im=Image.new('RGB',(128,128),(23,39,66));d=ImageDraw.Draw(im)
d.rectangle((0,91,127,127),fill=(35,70,72))
d.rectangle((92,14,105,27),fill=(249,238,204));d.rectangle((98,10,110,22),fill=(23,39,66))
for x,y in [(13,19),(42,12),(70,27),(115,45),(12,59)]:d.point((x,y),fill=(249,238,204))
for x,y,w,h in [(44,104,50,65),(77,102,34,48)]:
    d.rectangle((x-3,y-h//2,x+3,y+3),fill=(192,135,91))
    for n in range(4):
        ww=w-16+n*5;yy=y-h+n*9
        d.rectangle((x-ww//2,yy+4,x+ww//2,yy+16),fill=(30,73,75))
        d.rectangle((x-ww//2+3,yy,x+ww//2-3,yy+12),fill=(50,112,104))
        d.rectangle((x-ww//2+6,yy+3,x-ww//2+12,yy+4),fill=(74,147,124))
    for xx,yy in [(x-10,y-h+16),(x+8,y-h+27),(x-4,y-h+40)]:
        d.rectangle((xx-3,yy-3,xx+3,yy+4),fill=(131,137,85));d.rectangle((xx-1,yy-1,xx+1,yy+2),fill=(246,209,111))
d.rectangle((99,68,102,111),fill=(103,89,90));d.rectangle((88,67,102,69),fill=(103,89,90))
d.rectangle((86,72,95,87),fill=(246,209,111));d.rectangle((89,74,91,84),fill=(249,238,204))
for x,y in [(13,90),(26,78),(62,92),(111,99)]:d.rectangle((x-1,y-1,x+1,y+1),fill=(246,209,111))
im.save(Path(__file__).with_name('icon.png'))
