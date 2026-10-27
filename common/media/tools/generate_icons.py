#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Render original media UI vectors to alpha PNGs at native screen densities.

Requires CairoSVG and Pillow on the development computer only. Neither library
is linked into the PXA system, Host or applications.
"""
from pathlib import Path
from io import BytesIO
import cairosvg
from PIL import Image
ROOT=Path(__file__).resolve().parents[3]
HEART='M12 20 C10.5 18.6 3 13.8 3 8.8 C3 4.2 8.8 2.8 12 7.2 C15.2 2.8 21 4.2 21 8.8 C21 13.8 13.5 18.6 12 20 Z'
PLAY='M8.7 5.6 C8.1 5.2 7.3 5.7 7.3 6.4 V17.6 C7.3 18.3 8.1 18.8 8.7 18.4 L18.1 12.8 C18.7 12.4 18.7 11.6 18.1 11.2 Z'
NEXT='<path d="M6.9 5.9 C6.3 5.5 5.6 5.9 5.6 6.6 V17.4 C5.6 18.1 6.3 18.5 6.9 18.1 L15.8 12.7 C16.3 12.4 16.3 11.6 15.8 11.3 Z"/><rect x="17.5" y="5.5" width="2.3" height="13" rx="1.15"/>'
SHAPES={
 'play':f'<path d="{PLAY}"/>',
 'pause':'<rect x="6.3" y="5.1" width="3.8" height="13.8" rx="1.35"/><rect x="13.9" y="5.1" width="3.8" height="13.8" rx="1.35"/>',
 'next':NEXT,'previous':f'<g transform="translate(24 0) scale(-1 1)">{NEXT}</g>',
 'heart':f'<path d="{HEART}" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linejoin="round"/>',
 'heart-filled':f'<path d="{HEART}"/>',
 'back':'<path d="M14.7 5.7 L8.4 12 L14.7 18.3" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round"/>',
 'search':'<g fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round"><circle cx="10.5" cy="10.5" r="5.8"/><path d="M14.8 14.8 L19.5 19.5"/></g>',
 'order':'<g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4 6 H19 M15 2 L19 6 L15 10 M4 13 H11 M4 19 H11"/></g>',
 'repeat':'<g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4 9 V7 A2 2 0 0 1 6 5 H20 M16 1 L20 5 L16 9 M20 15 V17 A2 2 0 0 1 18 19 H4 M8 15 L4 19 L8 23"/></g>',
 'repeat-one':'<g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M3 9 V7 A2 2 0 0 1 5 5 H20 M17 2 L20 5 L17 8 M21 15 V17 A2 2 0 0 1 19 19 H4 M7 16 L4 19 L7 22 M10 11 L12 10 V15 M10 15 H14"/></g>',
 'volume':'<g fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"><path d="M4 9 H7 L12 5 V19 L7 15 H4 Z M16 8 A6 6 0 0 1 16 16 M19 5 A10 10 0 0 1 19 19"/></g>',
 'refresh':'<g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M18.2 7.3 A7.1 7.1 0 1 0 18.8 15.8"/><path d="M18.3 3.7 V7.8 H14.2"/></g>',
}
PALETTES={
 'music':{'play':'222139','pause':'222139','next':'f5f1ea','previous':'f5f1ea','heart':'d5b7fa','heart-filled':'d5b7fa','back':'f5f1ea','search':'f5f1ea','refresh':'91a2b6','order':'91a2b6','repeat':'91a2b6','repeat-one':'91a2b6','volume':'91a2b6'},
 'internet-radio':{'play':'ffffff','pause':'ffffff','next':'242a2a','previous':'242a2a','heart':'bd572f','heart-filled':'bd572f','back':'242a2a','search':'242a2a','volume':'858980'},
}
for app,palette in PALETTES.items():
 dest=ROOT/app/'assets/ui';dest.mkdir(parents=True,exist_ok=True)
 for name,color in palette.items():
  svg=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" color="#{color}" fill="currentColor">{SHAPES[name]}</svg>'
  for size in (18,24,30,48):
   png=cairosvg.svg2png(bytestring=svg.encode(),output_width=size*4,output_height=size*4)
   image=Image.open(BytesIO(png)).convert('RGBA').resize((size,size),Image.Resampling.LANCZOS)
   image.save(dest/f'{name}-{color}-{size}.png',optimize=True)
 print(app,len(palette)*4,'original antialiased icons')

# A quiet static placeholder, displayed only until the original cover is ready.
placeholder = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 160 160"><defs><linearGradient id="bg" x2="1" y2="1"><stop stop-color="#3b3b56"/><stop offset="1" stop-color="#252c40"/></linearGradient></defs><rect width="160" height="160" rx="12" fill="url(#bg)"/><circle cx="80" cy="80" r="50" fill="none" stroke="#79748d" stroke-width="1.4"/><circle cx="80" cy="80" r="37" fill="none" stroke="#56556b"/><path d="M87 58 V94 C87 104 65 107 65 97 C65 88 78 87 82 89 V64 L103 59 V89 C103 99 82 101 82 92" fill="none" stroke="#d5b7fa" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"/></svg>'
for size in (24,48,160):
 png=cairosvg.svg2png(bytestring=placeholder.encode(),output_width=size*4,output_height=size*4)
 Image.open(BytesIO(png)).convert('RGBA').resize((size,size),Image.Resampling.LANCZOS).save(ROOT/'music/assets/ui'/f'cover-placeholder-{size}.png',optimize=True)
