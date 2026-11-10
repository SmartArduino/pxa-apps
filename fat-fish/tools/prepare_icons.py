#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Render original SVG icons offline and encode a transparent indexed atlas.

Requires Pillow and CairoSVG. Runtime rendering uses the generated atlas only.
"""
from io import BytesIO
import math
from pathlib import Path

import cairosvg
from PIL import Image

APP = Path(__file__).resolve().parents[1]
INK = '#40516d'
icons = [
    ('heart', '#ffd0d9', '#e78ea9', '''
      <path d="M12 21C9 18.6 2.5 14 2.5 8.1C2.5 3.1 9.1 1.5 12 6C14.9 1.5 21.5 3.1 21.5 8.1C21.5 14 15 18.6 12 21Z" fill="url(#paint)"/>
      <path d="M5.4 8.4C5.3 6.2 7.4 5.2 9.1 6.4" fill="none" stroke="white" stroke-width="1.8" opacity=".8"/>
    '''),
    ('food', '#bfe8d6', '#69b5ac', '''
      <path d="M5 10C4 6.8 7.5 5.4 9.3 6.5C10.4 3.6 14.5 3.6 15.7 6.5C18 5.8 20.4 7.4 19 10" fill="#fff1cf"/>
      <path d="M3 10H21L18.1 17.3C17.7 18.4 16.7 19 15.5 19H8.5C7.3 19 6.3 18.4 5.9 17.3Z" fill="url(#paint)"/>
      <path d="M7 20.5H17M6 12.8L7.4 16" fill="none"/>
      <path d="M8 2.5C6 3.4 8.4 4.2 7.3 5M16 1.6C14.3 2.6 16.4 3.4 15.5 4.4" fill="none" stroke="#d3a570" stroke-width="1.2"/>
      <path d="M6 12.3H9.2" stroke="#f0fff8" stroke-width="1.5"/>
    '''),
    ('ball', '#c1e6fc', '#6ea8d6', '''
      <circle cx="12" cy="12" r="9.4" fill="url(#paint)"/>
      <path d="M6.1 4.8C13 7.2 16.8 12.2 16.8 19.6M3.2 14C10.5 10.3 16 10.5 21 13" fill="none" stroke="#f4fbff" stroke-width="1.7"/>
      <path d="M15.3 3.4C12.8 9.3 9 13.6 4.9 17.4" fill="none" opacity=".75"/>
      <path d="M6.1 8.1L7.6 6.6" stroke="white" stroke-width="1.9"/>
    '''),
    ('moon', '#ffe9ad', '#edbd70', '''
      <path d="M15.2 2.9C9.5 1 3 5.2 3 11.8C3 17.8 8.4 22 14.1 20.7C17.5 20 20 17.8 21 14.5C15.6 17.1 10.9 13.1 11.4 8.5C11.7 6.1 13 4.2 15.2 2.9Z" fill="url(#paint)"/>
      <path d="M6.4 7.3C4.6 10 4.8 13.2 6.2 15.2" fill="none" stroke="#fff8d5" stroke-width="1.7"/>
      <path d="M19.1 3.3L19.8 5.4L21.9 6.1L19.8 6.8L19.1 8.9L18.4 6.8L16.3 6.1L18.4 5.4Z" fill="#c7b9ef" stroke-width=".8"/>
    '''),
    ('film', '#d2cef6', '#a59bce', '''
      <rect x="3.1" y="7.6" width="17.8" height="13" rx="2.3" fill="url(#paint)"/>
      <path d="M3 7.6L2.3 3.7L19.9 1.7L20.6 5.6Z" fill="#90b9d5"/>
      <path d="M6.7 3.2L9.1 6.4M12.5 2.6L14.9 5.7M3.3 10H20.6" fill="none" stroke="#f7f8ff" stroke-width="1.4"/>
      <path d="M10.1 12.1L15.4 15.1L10.1 18.1Z" fill="#fff7d9" stroke-width=".9"/>
    '''),
    ('gear', '#dcd3fa', '#a99acb', ''),
    ('more', '#d5e4f3', '#a1bfd7', '''
      <circle cx="5" cy="12" r="1.7" fill="#6987a7" stroke="none"/>
      <circle cx="12" cy="12" r="1.7" fill="#6987a7" stroke="none"/>
      <circle cx="19" cy="12" r="1.7" fill="#6987a7" stroke="none"/>
    '''),
    ('joy', '#fff0b9', '#efc779', '''
      <circle cx="12" cy="12" r="9.1" fill="url(#paint)"/>
      <path d="M7.1 9.1L8 8.5L8.9 9.1M15.1 9.1L16 8.5L16.9 9.1M7.7 13.1C9.8 17.2 14.2 17.2 16.3 13.1" fill="none"/>
      <ellipse cx="6.5" cy="12.4" rx="1.6" ry=".8" fill="#f3ac9d" stroke="none"/>
      <ellipse cx="17.5" cy="12.4" rx="1.6" ry=".8" fill="#f3ac9d" stroke="none"/>
    '''),
    ('energy', '#fff2b7', '#edbb68', '''
      <path d="M13.9 1.9L4.2 13.1H10.4L9.8 22.1L20 10.4H13.4Z" fill="url(#paint)"/>
      <path d="M11.4 7L8.3 10.9" stroke="#fffbe8" stroke-width="1.8"/>
    '''),
    ('sun', '#fff1b3', '#edba69', '''
      <path d="M12 1.2V3M12 21V22.8M1.2 12H3M21 12H22.8M4.2 4.2L5.6 5.6M18.4 18.4L19.8 19.8M4.2 19.8L5.6 18.4M18.4 5.6L19.8 4.2" stroke="#dbad70" stroke-width="1.5"/>
      <circle cx="12" cy="12" r="6.8" fill="url(#paint)"/>
      <path d="M8 10.4L8.7 9.8L9.4 10.4M14.6 10.4L15.3 9.8L16 10.4M9.3 14C10.7 15.6 13.3 15.6 14.7 14" fill="none" stroke-width="1.1"/>
    '''),
]
points = []
for i in range(48):
    radius = 10 if i % 6 in (1, 2, 3, 4) else 8
    angle = i * math.tau / 48
    points.append(f'{12+radius*math.cos(angle):.3f},{12+radius*math.sin(angle):.3f}')
name, first, last, _ = icons[5]
icons[5] = (name, first, last, '<polygon points="'+' '.join(points)+'" fill="url(#paint)"/>'
            '<circle cx="12" cy="12" r="4.4" fill="#fff3d4"/>'
            '<circle cx="12" cy="12" r="1.7" fill="#e2b977" stroke="none"/>')

source = APP / 'art/icons'
source.mkdir(parents=True, exist_ok=True)
atlas = Image.new('RGBA', (320, 128))
for index, (name, first, last, body) in enumerate(icons):
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 24 24">'
           f'<defs><linearGradient id="paint" x1="0" y1="0" x2="0" y2="1">'
           f'<stop stop-color="{first}"/><stop offset="1" stop-color="{last}"/></linearGradient></defs>'
           f'<g stroke="{INK}" stroke-width="1.25" stroke-linecap="round" stroke-linejoin="round">'
           + body + '</g></svg>')
    (source / (name+'.svg')).write_text(svg+'\n')
    image = Image.open(BytesIO(cairosvg.svg2png(bytestring=svg.encode()))).convert('RGBA')
    atlas.alpha_composite(image, (index % 5 * 64, index // 5 * 64))
atlas.save(APP / 'assets/controls.png')
quantized = atlas.convert('RGB').quantize(colors=255, dither=Image.Dither.NONE)
palette = quantized.getpalette()
words = [0]+[((palette[i]&248)<<8)|((palette[i+1]&252)<<3)|(palette[i+2]>>3) for i in range(0,765,3)]
alpha = atlas.getchannel('A').tobytes()
pixels = bytes(v+1 if alpha[i]>=96 else 0 for i,v in enumerate(quantized.tobytes()))
lines = ['// Original MIT icon artwork. Generated by tools/prepare_icons.py.',
         '#include "../common/pocket-studio/studio.hpp"', 'namespace {',
         'const uint16_t controls_palette[256]={'+','.join(map(str,words))+'};',
         'const uint8_t controls_pixels[]={']
lines.extend(','.join(map(str,pixels[at:at+96]))+',' for at in range(0,len(pixels),96))
lines.extend(['};','const studio::Image controls_image{320,128,controls_palette,controls_pixels};','}'])
(APP / 'controls.inc').write_text('\n'.join(lines)+'\n')
