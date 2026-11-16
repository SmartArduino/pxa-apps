// SPDX-License-Identifier: MIT
#pragma once
#include "raster.hpp"
#include "fx_data.hpp"
namespace iron {
// A bounded, allocation-free decoder. Each record skips transparent pixels or
// repeats one RGB565/alpha color; original terrain remains visible through glow.
inline void draw_fx(Raster& r,unsigned effect,int x,int y,float phase,float scale=1){
 if(effect>=vfx::clips.size()||phase<0||phase>=1||scale<=0)return;
 const auto& clip=vfx::clips[effect];int frame=std::min(clip.frames-1,int(phase*clip.frames));
 size_t at=vfx::offsets[clip.first+frame],end=vfx::offsets[clip.first+frame+1];int pixel=0;
 while(at<end&&pixel<clip.width*clip.height){unsigned token=vfx::bytes[at++],count=(token&127)+1;
  if(token&128){pixel+=count;continue;}if(end-at<3)return;
  uint16_t color=vfx::bytes[at]|(uint16_t(vfx::bytes[at+1])<<8);unsigned alpha=vfx::bytes[at+2];at+=3;
  for(unsigned i=0;i<count;++i,++pixel){
   int sx=pixel%clip.width-clip.ox,sy=pixel/clip.width-clip.oy;
   int x0=x+int(std::floor(sx*scale)),x1=x+int(std::floor((sx+1)*scale));int y0=y+int(std::floor(sy*scale)),y1=y+int(std::floor((sy+1)*scale));
   if(alpha>=16)r.rect(x0,y0,x1-x0,y1-y0,color);else for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx)r.blend(xx,yy,color,alpha);
  }
 }
}
}
