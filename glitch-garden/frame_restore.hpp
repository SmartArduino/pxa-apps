// SPDX-License-Identifier: MIT
#pragma once
#include "filtered_art.hpp"
namespace garden {
struct DamageBounds {
 Rect rect{};
 void include(Rect r){if(r.w<=0||r.h<=0)return;if(rect.w<=0){rect=r;return;}int right=std::max(rect.x+rect.w,r.x+r.w),bottom=std::max(rect.y+rect.h,r.y+r.h);rect.x=std::min(rect.x,r.x);rect.y=std::min(rect.y,r.y);rect.w=right-rect.x;rect.h=bottom-rect.y;}
};
// Mapped buffers are Guest-owned storage. Each lease keeps its own previous
// damage: the compositor may return the buffers in any order or drop a frame.
struct FrameRestore {
 struct Buffer {uint64_t revision=0;Rect damage{};};
 std::array<Buffer,3> buffers{};
 void restore(unsigned index,uint16_t* dest,int stride,const uint16_t* background,int width,int height,uint64_t revision){
  bool known=index<buffers.size();Rect r=known&&buffers[index].revision==revision?buffers[index].damage:Rect{0,0,width,height};
  int left=std::clamp(r.x,0,width),right=std::clamp(r.x+r.w,0,width),top=std::clamp(r.y,0,height),bottom=std::clamp(r.y+r.h,0,height);
  if(right>left&&bottom>top){if(left==0&&right==width&&stride==width)std::copy_n(background+top*width,size_t(bottom-top)*width,dest+top*stride);
   else for(int y=top;y<bottom;++y)std::copy_n(background+y*width+left,right-left,dest+y*stride+left);}
  if(known)buffers[index].revision=revision;
 }
 void finish(unsigned index,Rect damage){if(index<buffers.size())buffers[index].damage=damage;}
};
}
