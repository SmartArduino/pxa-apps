#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Small game-heading images, Noto Serif CJK Bold (SIL OFL).
Body text stays native and scalable. P4 loads only the 48-pixel variant;
larger screens use 96 pixels. Normal builds consume the included PNGs.
"""
from pathlib import Path
from PIL import Image,ImageFont,ImageDraw,ImageFilter
root=Path(__file__).resolve().parents[1]
labels=['暂停旅程','操作指南','旅程结束','新纪录']
for size in (48,96):
    factor=size//48;cw=256*factor;ch=64*factor
    atlas=Image.new('RGBA',(cw*2,ch*2))
    font=ImageFont.truetype('/usr/share/fonts/noto-cjk/NotoSerifCJK-Bold.ttc',size)
    for i,text in enumerate(labels):
        mask=Image.new('L',(cw,ch));draw=ImageDraw.Draw(mask);box=draw.textbbox((0,0),text,font=font)
        draw.text(((cw-box[2]+box[0])//2-box[0],(ch-box[3]+box[1])//2-box[1]),text,font=font,fill=255)
        tile=Image.new('RGBA',(cw,ch));edge=mask.filter(ImageFilter.MaxFilter(3))
        tile.paste((35,47,29,255),(0,0,cw,ch),edge)
        gold=Image.new('RGBA',(cw,ch));gd=ImageDraw.Draw(gold)
        for y in range(ch):
            t=y/max(1,ch-1);gd.line((0,y,cw,y),fill=(255-round(43*t),241-round(78*t),185-round(104*t),255))
        tile.paste(gold,(0,0),mask);atlas.alpha_composite(tile,((i%2)*cw,(i//2)*ch))
    for i in range(4):
        tile=atlas.crop(((i%2)*cw,(i//2)*ch,(i%2+1)*cw,(i//2+1)*ch))
        bounds=tile.getbbox();tile=tile.crop(bounds)
        target=root/'resources/ui'/f'heading-{size}-{i}.png';tile.save(target,optimize=True);print(target)
