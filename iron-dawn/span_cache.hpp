// SPDX-License-Identifier: MIT
#pragma once
#include "art_data.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace iron {
// Bounded RGB565 cache at the requested scale. Alpha stays in the span stream.
struct SpriteSpanCache {
 struct Entry {uint32_t offset=0,sx=0,sy=0;uint16_t count=0,left=0,top=0,stride=0,id=0;uint8_t variant=0;};
 static constexpr unsigned capacity=98304;
 std::array<uint16_t,capacity> pixels{};std::array<Entry,512> entries{};unsigned cursor=0,hits=0,misses=0;bool enabled=true;
 const Entry* get(unsigned id,unsigned variant,float sx=1,float sy=1){if(!enabled||(id>=art::mission_skin_first&&(id<art::construction_first||id>=art::story_portraits_first))||(id>=art::icons_first&&id<art::turret_heads_first))return nullptr;variant=std::min(4u,variant);uint32_t bx=std::bit_cast<uint32_t>(sx),by=std::bit_cast<uint32_t>(sy);unsigned key=(id*179+variant*31+(bx^(bx>>17))*7+(by^(by>>13)))%entries.size();Entry* slot=nullptr;
  for(unsigned n=0;n<8;++n){auto& e=entries[(key+n)%entries.size()];if(e.count&&e.id==id&&e.variant==variant&&e.sx==bx&&e.sy==by){++hits;return &e;}if(!e.count&&!slot)slot=&e;}
  if(!slot)slot=&entries[key];++misses;const auto& f=art::frames[id];std::array<int,433> xs;std::array<int,241> ys;for(unsigned x=0;x<=f.w;++x)xs[x]=int(std::floor(x*sx));for(unsigned y=0;y<=f.h;++y)ys[y]=int(std::floor(y*sy));
  int left=xs[f.w],top=ys[f.h],right=0,bottom=0;auto p=art::bytes.data()+f.offset;for(unsigned n=0;n<f.count;++n){unsigned x=p[0],y=p[1],length=p[2];p+=3+length;if(xs[x+length]<=xs[x]||ys[y+1]<=ys[y])continue;left=std::min(left,xs[x]);top=std::min(top,ys[y]);right=std::max(right,xs[x+length]);bottom=std::max(bottom,ys[y+1]);}if(right<=left||bottom<=top)return nullptr;
  unsigned stride=(right-left+1)&~1u,count=stride*(bottom-top);if(count>capacity||count>65535)return nullptr;if(cursor+count>capacity)cursor=0;
  for(auto& old:entries)if(old.count&&old.offset<cursor+count&&old.offset+old.count>cursor)old.count=0;
  *slot={cursor,bx,by,uint16_t(count),uint16_t(left),uint16_t(top),uint16_t(stride),uint16_t(id),uint8_t(variant)};const auto& palette=art::palettes[f.palette][variant];p=art::bytes.data()+f.offset;
  for(unsigned n=0;n<f.count;++n){unsigned x=p[0],y=p[1],length=p[2];p+=3;for(int row=ys[y];row<ys[y+1];++row){auto dest=pixels.data()+cursor+(row-top)*stride;unsigned source=0;for(int col=xs[x];col<xs[x+length];++col){while(source+1<length&&xs[x+source+1]<=col)++source;dest[col-left]=palette[p[source]];}}p+=length;}cursor+=count;return slot;
 }
};
}
