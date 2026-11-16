// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"
#include "raster.hpp"
#include <vector>
namespace iron {
struct GroundCache {
 std::vector<uint16_t> pixels;std::array<uint8_t,N*N> terrain{},fog{},seen{},ore{};
 int width=0,top=0,bottom=0,chapter=-1,water=-1;float camx=0,camy=0,zoom=0;unsigned hits=0,misses=0;bool valid=false;
 template<class Draw> void draw(Raster& output,const World& world,int ww,int tt,int bb,float xx,float yy,float zz,int phase,Draw render){
  bool same=valid&&ww==width&&tt==top&&bb==bottom&&xx==camx&&yy==camy&&zz==zoom&&world.chapter==chapter&&phase==water&&terrain==world.terrain&&fog==world.fog&&seen==world.seen;
  if(same)for(int i=0;i<N*N;++i)if(ore[i]!=bool(world.ore[i])){same=false;break;}
  if(same)++hits;else{
   ++misses;width=ww;top=tt;bottom=bb;camx=xx;camy=yy;zoom=zz;chapter=world.chapter;water=phase;terrain=world.terrain;fog=world.fog;seen=world.seen;for(int i=0;i<N*N;++i)ore[i]=world.ore[i]>0;
   pixels.resize(size_t(width)*bottom);Raster backing(pixels.data(),width,bottom,width);backing.clip={0,top,width,bottom-top};render(backing);valid=true;
  }
  int left=std::max(0,output.clip.x),right=std::min(width,output.clip.x+output.clip.w),first=std::max(top,output.clip.y),last=std::min(bottom,output.clip.y+output.clip.h);
  if(left<right)for(int y=first;y<last;++y)std::copy_n(pixels.data()+y*width+left,right-left,output.pixels+y*output.stride+left);
 }
};
}
