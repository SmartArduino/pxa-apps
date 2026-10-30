#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bake one game's used glyphs, its original vector icon and complete notices."""
from pathlib import Path
import argparse,hashlib,json
from PIL import Image,ImageDraw,ImageFont
from fontTools.ttLib import TTFont
import cairosvg
p=argparse.ArgumentParser();p.add_argument('--app',type=Path,required=True);p.add_argument('--embedded-fonts',action='store_true');a=p.parse_args()
common=Path(__file__).resolve().parents[1];workspace=common.parents[3];app=a.app.resolve()
config=app/'art/font.json'
if config.exists():
 settings=json.loads(config.read_text());assert settings in ({'storage':'embedded'},{'storage':'system'}),'Unsupported font configuration'
 a.embedded_fonts=settings['storage']=='embedded'
else:settings={'storage':'embedded' if a.embedded_fonts else 'resource'}
system_fonts=settings['storage']=='system'
if not system_fonts:(app/'resources').mkdir(exist_ok=True)
inputs=sorted(path for path in app.iterdir() if path.suffix in ('.cpp','.hpp') and path.name!='font.hpp')+[common/'runtime.hpp',common/'sound.hpp']
source='\n'.join(path.read_text() for path in inputs)
chars=sorted(set(' 0123456789+-=:/().%?Dkmh')|{c for c in source if ord(c)>127})
font_path=workspace/'factory/base/system/fonts/noto_sans_cjk_common.ttf';face=ImageFont.truetype(str(font_path),16);cmap=TTFont(font_path).getBestCmap()
missing=''.join(c for c in chars if ord(c) not in cmap);assert not missing,('Missing glyphs',missing)
cols,cw,ch=14,18,24;capacity=cols*(4 if a.embedded_fonts else 256//ch);entries=[];packed_pages=[];filters=[];heights=[]
for start in ([] if system_fonts else range(0,len(chars),capacity)):
 page=start//capacity;items=chars[start:start+capacity];w=cols*cw;h=((len(items)+cols-1)//cols)*ch;im=Image.new('L',(w,h));d=ImageDraw.Draw(im)
 for i,c in enumerate(items):d.text(((i%cols)*cw,(i//cols)*ch),c,font=face,fill=255)
 name=f'font-{page}';(app/'resources'/f'{name}.index8').write_bytes(im.tobytes());entries.append(dict(path=f'assets/{name}.pxr',kind='index8',source=f'resources/{name}.index8',width=w,height=h))
 if a.embedded_fonts:
  from font_pack import pack
  kind,data=pack(im.tobytes(),w);packed_pages.append(data);filters.append(kind);heights.append(h)
header='// Generated glyph metrics; artwork is SIL OFL 1.1.\n// Source font SHA-256: '+hashlib.sha256(font_path.read_bytes()).hexdigest()+'\n#pragma once\n#include <array>\n#include <cstdint>\n#include <span>\nnamespace motion {\n'
if system_fonts:
 header+='#define MOTION_SYSTEM_FONT 1\n'
 for old in (app/'resources').glob('font-*.index8'):old.unlink()
header+=f'inline constexpr unsigned font_pages={len(entries)},font_columns={cols},font_cell_w={cw},font_cell_h={ch},font_per_page={capacity};\n'
header+=f'inline constexpr std::array<uint32_t,{len(chars)}> font_codes={{'+','.join(str(ord(c)) for c in chars)+'};\n'
header+=f'inline constexpr std::array<uint8_t,{len(chars)}> font_advance={{'+','.join(str(max(4,min(cw,round(face.getlength(c))))) for c in chars)+'};\n'
if a.embedded_fonts:
 header+=f'inline constexpr uint16_t font_page_width={cols*cw};\n'
 for i,data in enumerate(packed_pages):header+=f'inline constexpr std::array<uint8_t,{len(data)}> font_packed_{i}={{'+','.join(map(str,data))+'};\n'
 header+=f'inline constexpr std::array<std::span<const uint8_t>,{len(packed_pages)}> font_packed_pages={{'+','.join('font_packed_'+str(i) for i in range(len(packed_pages)))+'};\n'
 header+=f'inline constexpr std::array<uint16_t,{len(heights)}> font_page_heights={{'+','.join(map(str,heights))+'};\n'
 header+=f'inline constexpr std::array<uint8_t,{len(filters)}> font_page_filters={{'+','.join(map(str,filters))+'};\n'
header+='}\n'
(app/'font.hpp').write_text(header);(app/'resources.json').write_text(json.dumps({'assets':[] if a.embedded_fonts or system_fonts else entries},indent=2)+'\n')
notice='Application code and original icon: MIT. The PXA SDK MIT notice and source-font notices and SIL OFL license texts follow in full. Source font license filenames refer to the named sections below.\n\n'
for name in ['PXA-SDK-LICENSE.txt','FONT-NOTICE','FONT-OFL.txt','FONT-OFL-Latin.txt']:notice+='=== '+name+' ===\n\n'+(common/'licenses'/name).read_text()+'\n\n'
(app/'assets/NOTICE.txt').write_text(notice)
cairosvg.svg2png(url=str(app/'art/icon.svg'),write_to=str(app/'assets/icon.png'),output_width=96,output_height=96)
print(app.name,len(chars),'glyphs',len(entries),'font pages')
if a.embedded_fonts:print('Embedded font bytes:',sum(map(len,packed_pages)))
