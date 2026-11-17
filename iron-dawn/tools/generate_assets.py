#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bake the pinned OFL pixel font at its designed 12px grid and refresh notices."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw,ImageFont

app=Path(__file__).resolve().parents[1]
font=app/'tools/fonts/ZLabsPixel_12px_M_CN.ttf'
upstream=json.loads((app/'tools/fonts/upstream.json').read_text())
assert hashlib.sha256(font.read_bytes()).hexdigest()==upstream['font_sha256']
source=''.join(p.read_text() for pattern in ('*.hpp','*.inc') for p in app.glob(pattern) if p.name!='font.hpp')+(app/'main.cpp').read_text()
chars=sorted(set(chr(i) for i in range(32,127))|{c for c in source if ord(c)>127})
data=[];adv=[];faces=[]
for size in (12,):
 face=ImageFont.truetype(str(font),size);cw,ch=size,size;stride=(cw*ch+7)//8;offset=len(data);widths=[]
 for c in chars:
  im=Image.new('1',(cw,ch));d=ImageDraw.Draw(im);d.fontmode='1';d.text((0,0),c,font=face,fill=1,anchor='la')
  pixels=list(im.getdata());packed=bytearray(stride)
  for i,v in enumerate(pixels):
   if v:packed[i//8]|=1<<(i%8)
  data.extend(packed);widths.append(max(1,round(face.getlength(c))))
 adv.append(widths);faces.append((size,cw,ch,stride,offset))
header='// Generated Iron Dawn UI Pixel subset of Z Labs Pixel 12px M CN.\n// Font data: SIL OFL 1.1; see assets/third-party/ZLabs-Pixel-OFL.txt.\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace iron {\n'
header+=f'inline constexpr std::array<uint32_t,{len(chars)}> glyph_codes={{'+','.join(str(ord(c)) for c in chars)+'};\n'
header+='struct FontStrike {int size,width,height,stride,offset;};\ninline constexpr std::array<FontStrike,1> font_strikes={{'+','.join('{'+','.join(map(str,f))+'}' for f in faces)+'}};\n'
header+=f'inline constexpr std::array<std::array<uint8_t,{len(chars)}>,1> glyph_advances={{{{'+','.join('{{'+','.join(map(str,row))+'}}' for row in adv)+'}};\n'
header+=f'inline constexpr std::array<uint8_t,{len(data)}> glyph_bits={{'+','.join(map(str,data))+'};\n}\n'
(app/'font.hpp').write_text(header)
# The app icon is imported by generate_art.py from the generated PNG master.
common=app.parent/'common/motion-games/licenses'
notice='Iron Dawn: original application code, game rules, procedural and generated artwork: MIT.\nGenerated image masters: built-in ImageGen; prompts and input lineage in tools/art-prompts.json.\nFont subset: Iron Dawn UI Pixel, derived from Z Labs Pixel 12px M CN, SIL OFL 1.1.\nFont SHA256: '+hashlib.sha256(font.read_bytes()).hexdigest()+'\n\n'
notice=notice.replace('Font subset:', 'Environment and large-screen UI reference: built-in ImageGen; prompts and input lineage in tools/environment-prompts.json; original MIT artwork.\nNine-slice UI skins: built-in ImageGen; prompts and input lineage in tools/ui-skin-prompts.json; original MIT artwork.\nFont subset:')
notice+='Generated music and effects: original project outputs (MIT), using ACE-Step 1.5 and Stable Audio 3 Small SFX. Models are not included; generation provenance in tools/audio-prompts.json.\n\n'
notice+='Unit voices: original synthetic cast and dialogue (MIT), designed with Qwen3-TTS VoiceDesign and generated from fixed synthetic references with Qwen3-TTS Base (Apache-2.0 models, not included). No third-party voice recordings used; provenance in tools/voice-prompts.json.\n\n'
for name in ('PXA-SDK-LICENSE.txt',):
 notice+='=== '+name+' ===\n'+(common/name).read_text()+'\n'
for name in ('ZLabs-Pixel-OFL.txt','pixelfx-LICENSE','pixelfx-NOTICE.md'):
 path=app/'assets/third-party'/name
 if path.exists():notice+='=== '+name+' ===\n'+path.read_text()+'\n'
(app/'assets/NOTICE.txt').write_text('\n'.join(line.rstrip() for line in notice.splitlines())+'\n')
print(len(chars),'glyphs;',len(data),'font bytes')
