#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Draw the original vector-style icon using only Pillow geometric primitives."""
from pathlib import Path
from PIL import Image, ImageDraw
im=Image.new('RGB',(256,256),'#101c2a')
d=ImageDraw.Draw(im)
def rect(box,color):d.rectangle(tuple(v*2 for v in box),fill=color)
d.rounded_rectangle((26,36,230,220),radius=24,fill='#1c2e3b',outline='#7fe8bd',width=6)
rect((22,90,106,102),'#738791')
for box in ((24,83,49,90),(30,76,49,83),(36,69,49,76),(43,62,49,69)):rect(box,'#eaba62')
for box in ((53,79,77,90),(59,72,77,79)):rect(box,'#3f9ed8')
rect((88,49,95,90),'#a36f44')
for box in ((76,49,107,61),(82,39,101,49),(88,31,95,39)):rect(box,'#56bf75')
rect((30,38,37,45),'#ffd370');rect((40,50,46,56),'#eaba62');rect((62,56,68,62),'#7fe8bd')
im.save(Path(__file__).with_name('icon.png'))
