// SPDX-License-Identifier: MIT
#pragma once
#include "raster.hpp"
#include "fx_data.hpp"
namespace iron {
static_assert([](){for(auto c:vfx::clips)if(c.width>84||c.height>70)return false;return true;}(),"Increase the fixed decoder edge table before adding larger clips");
// A bounded, allocation-free decoder. Each record skips transparent pixels or
// repeats one RGB565/alpha color; original terrain remains visible through glow.
inline void draw_fx(Raster& r,unsigned effect,int x,int y,float phase,float scale=1){
 if(effect>=vfx::clips.size()||phase<0||phase>=1||scale<=0)return;
 const auto& clip=vfx::clips[effect];int frame=std::min(clip.frames-1,int(phase*clip.frames));
 std::array<int,85> xs{};std::array<int,71> ys{};
 for(int i=0;i<=clip.width;++i)xs[i]=x+int(std::floor((i-clip.ox)*scale));
 for(int i=0;i<=clip.height;++i)ys[i]=y+int(std::floor((i-clip.oy)*scale));
 size_t at=vfx::offsets[clip.first+frame],end=vfx::offsets[clip.first+frame+1];int pixel=0;
 while(at<end&&pixel<clip.width*clip.height){unsigned token=vfx::bytes[at++],count=(token&127)+1;
  if(token&128){pixel+=count;continue;}if(end-at<3)return;
  uint16_t color=vfx::bytes[at]|(uint16_t(vfx::bytes[at+1])<<8);unsigned alpha=vfx::bytes[at+2];at+=3;
  while(count&&pixel<clip.width*clip.height){int col=pixel%clip.width,row=pixel/clip.width,n=std::min(int(count),clip.width-col);
   for(int yy=std::max(ys[row],r.clip.y);yy<std::min(ys[row+1],r.clip.y+r.clip.h);++yy)r.blend_span(xs[col],yy,xs[col+n]-xs[col],color,alpha);
   pixel+=n;count-=n;
  }
 }
}
}
