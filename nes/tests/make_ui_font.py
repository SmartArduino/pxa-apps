#!/usr/bin/env python3
"""Bake only menu glyphs; generated data is reproducible from the named font."""
from pathlib import Path
import argparse,hashlib,json
from PIL import Image,ImageFont,ImageDraw
p=argparse.ArgumentParser();p.add_argument('--font',type=Path,default=Path('/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc'));p.add_argument('--index',type=int,default=2);a=p.parse_args()
app=Path(__file__).resolve().parents[1]
chars=sorted(set(c for c in '\n'.join(p.read_text() for p in sorted((app/'i18n').glob('*.yaml'))) if ord(c)>127))
font=ImageFont.truetype(str(a.font),14,index=a.index)
lines=['/* Generated bitmap subset; Noto Sans CJK SC, SIL OFL-1.1. See licenses/ui-font-OFL.txt. */',f'/* Source SHA256: {hashlib.sha256(a.font.read_bytes()).hexdigest()} */','static const struct { uint32_t codepoint; uint16_t rows[16]; } ui_glyphs[] = {']
for c in chars:
 # One baseline for the entire font. Per-glyph 'lt' alignment incorrectly
 # raises short glyphs such as 一 to the top of the line.
 im=Image.new('L',(14,16));ImageDraw.Draw(im).text((0,13),c,font=font,fill=255,anchor='ls')
 rows=[sum((1<<x) for x in range(14) if im.getpixel((x,y))>=96) for y in range(16)]
 lines.append('    {0x%x,{%s}}, /* %s */'%(ord(c),','.join('0x%04x'%r for r in rows),c))
lines.append('};\n')
(app/'ui_font_data.inc').write_text('\n'.join(lines));print('Generated',len(chars),'glyphs;',len(chars)*36,'bitmap/table bytes before alignment')
