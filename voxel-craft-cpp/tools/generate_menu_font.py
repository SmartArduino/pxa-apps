#!/usr/bin/env python3
"""Bake three grayscale coverage atlases. The Guest loads just one of them."""
import re
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1]
labels=[]
for name in ['voxel_session.inc','voxel_menu.inc','voxel_items.hpp']:
 for text in re.findall(r'"([^"\n]*)"',(root/name).read_text()):
  if re.search('[\u4e00-\u9fff]',text):labels.append(text)
chars=''.join(dict.fromkeys('VOXEL CRAFT0123456789'+''.join(labels)))
chars=''.join(c for c in chars if c.isalnum() or c==' ')
assert len(chars)<=121,len(chars)
# TTC face 0 is CL, not the mainland Simplified Chinese face. Do not rely
# on fontconfig's locale fallback or the collection's default face.
font_path='/usr/share/fonts/sarasa-gothic/Sarasa-Regular.ttc'
font_face=1
for size in [10,14,18]:
 cell=size;rows=(len(chars)+10)//11
 font=ImageFont.truetype(font_path,size,index=font_face)
 assert font.getname()==('Sarasa Gothic SC','Regular'),font.getname()
 im=Image.new('L',(cell*11,cell*rows));draw=ImageDraw.Draw(im)
 for i,c in enumerate(chars):
  if c==' ':continue
  box=draw.textbbox((0,0),c,font=font)
  x=(i%11)*cell+(cell-(box[2]-box[0]))//2-box[0];y=(i//11)*cell+(cell-(box[3]-box[1]))//2-box[1]
  draw.text((x,y),c,font=font,fill=255)
 (root/f'resources/menu-font-{size}.a8').write_bytes(im.tobytes())
 im.save(root/f'tools/menu-font-{size}-preview.png')
 print('Font',size,'glyphs',len(chars),'bytes',len(im.tobytes()))
(root/'voxel_font.hpp').write_text('''#pragma once
#include <string_view>
#include <cstdint>
namespace voxel {
inline constexpr std::u32string_view kFontCharacters=U"'''+chars+'''";
inline int select_font_size(float scale)noexcept {return scale>=1.65f?18:scale>=1.25f?14:10;}
inline int font_index(char32_t c)noexcept {auto p=kFontCharacters.find(c);return p==kFontCharacters.npos?-1:int(p);}
inline char32_t font_next(std::string_view s,std::size_t& at)noexcept {
    auto first=static_cast<unsigned char>(s[at++]);if(first<128)return first;
    unsigned n=first<224?1:first<240?2:3;char32_t c=first&((1u<<(6-n))-1);
    while(n--&&at<s.size())c=(c<<6)|(static_cast<unsigned char>(s[at++])&63);return c;
}
} // namespace voxel
''')
