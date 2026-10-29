#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bake one game's used glyphs, its original vector icon and complete notices."""
from pathlib import Path
import argparse,hashlib,json
from PIL import Image,ImageDraw,ImageFont
from fontTools.ttLib import TTFont
import cairosvg
p=argparse.ArgumentParser();p.add_argument('--app',type=Path,required=True);a=p.parse_args()
common=Path(__file__).resolve().parents[1];workspace=common.parents[3];app=a.app.resolve()
source='\n'.join(path.read_text() for path in [app/'main.cpp',common/'runtime.hpp'])
chars=sorted(set(' 0123456789+-=:/().%?')|{c for c in source if ord(c)>127})
font_path=workspace/'factory/base/system/fonts/noto_sans_cjk_common.ttf';face=ImageFont.truetype(str(font_path),16);cmap=TTFont(font_path).getBestCmap()
missing=''.join(c for c in chars if ord(c) not in cmap);assert not missing,('Missing glyphs',missing)
cols,cw,ch=14,18,24;capacity=cols*(256//ch);entries=[]
for start in range(0,len(chars),capacity):
 page=start//capacity;items=chars[start:start+capacity];w=cols*cw;h=((len(items)+cols-1)//cols)*ch;im=Image.new('L',(w,h));d=ImageDraw.Draw(im)
 for i,c in enumerate(items):d.text(((i%cols)*cw,(i//cols)*ch),c,font=face,fill=255)
 name=f'font-{page}';(app/'resources'/f'{name}.index8').write_bytes(im.tobytes());entries.append(dict(path=f'assets/{name}.pxr',kind='index8',source=f'resources/{name}.index8',width=w,height=h))
header='// Generated glyph metrics; artwork is SIL OFL 1.1.\n// Source font SHA-256: '+hashlib.sha256(font_path.read_bytes()).hexdigest()+'\n#pragma once\n#include <array>\n#include <cstdint>\nnamespace motion {\n'
header+=f'inline constexpr unsigned font_pages={len(entries)},font_columns={cols},font_cell_w={cw},font_cell_h={ch},font_per_page={capacity};\n'
header+=f'inline constexpr std::array<uint32_t,{len(chars)}> font_codes={{'+','.join(str(ord(c)) for c in chars)+'};\n'
header+=f'inline constexpr std::array<uint8_t,{len(chars)}> font_advance={{'+','.join(str(max(4,min(cw,round(face.getlength(c))))) for c in chars)+'};\n}\n'
(app/'font.hpp').write_text(header);(app/'resources.json').write_text(json.dumps({'assets':entries},indent=2)+'\n')
notice='Application code and original icon: MIT. The PXA SDK MIT notice and source-font notices and SIL OFL license texts follow in full. Source font license filenames refer to the named sections below.\n\n'
for name in ['PXA-SDK-LICENSE.txt','FONT-NOTICE','FONT-OFL.txt','FONT-OFL-Latin.txt']:notice+='=== '+name+' ===\n\n'+(common/'licenses'/name).read_text()+'\n\n'
(app/'assets/NOTICE.txt').write_text(notice)
cairosvg.svg2png(url=str(app/'art/icon.svg'),write_to=str(app/'assets/icon.png'),output_width=96,output_height=96)
print(app.name,len(chars),'glyphs',len(entries),'font pages')
