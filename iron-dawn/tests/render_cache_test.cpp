// SPDX-License-Identifier: MIT
#include "sprites.hpp"
#include "minimap.hpp"
#include "combat_art.hpp"
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace iron;
int main(){
 constexpr int w=150,h=130;std::vector<uint16_t> a(w*h+2),b(w*h+2);unsigned cases=0;
 for(unsigned round=0;round<3;++round)for(unsigned id=0;id<art::frames.size();++id)for(float zoom:{.45f,.72f,1.f,1.3f}){
  std::fill(a.begin(),a.end(),navy);b=a;a.front()=a.back()=b.front()=b.back()=0xa55a;Raster ra(a.data()+1,w,h,w),rb(b.data()+1,w,h,w);ra.clip=rb.clip={3,4,130,110};unsigned variant=(id+round)%5;Sprites::cache().enabled=false;Sprites::blit(ra,id,-9,7,zoom,zoom,variant);Sprites::cache().enabled=true;Sprites::blit(rb,id,-9,7,zoom,zoom,variant);Sprites::blit(rb,id,-9,7,zoom,zoom,variant);assert(a==b);++cases;
 }
 assert(Sprites::cache().hits>0&&Sprites::cache().misses>0);
 for(int kind=0;kind<2;++kind)for(unsigned d=0;d<8;++d)for(int phase=0;phase<8;++phase)for(float weight:{0.f,.4f,1.f}){auto x=CombatArt::uncached_gait(kind,d,phase/8.f,weight),y=CombatArt::gait(kind,d,phase/8.f,weight);std::array<float,24> xx,yy;std::memcpy(xx.data(),&x,sizeof x);std::memcpy(yy.data(),&y,sizeof y);assert(xx==yy);}
 World world;world.init(0);MinimapCache cache;for(int iteration=0;iteration<7;++iteration){std::fill(a.begin(),a.end(),navy);b=a;Raster ra(a.data(),w,h,w),rb(b.data(),w,h,w);Rect m{11,13,80,80};
  for(int y=0;y<N;++y)for(int x=0;x<N;++x){int c=y*N+x;auto col=!world.seen[c]?navy:world.terrain[c]==2?rgb(38,72,80):world.ore[c]?gold:world.fog[c]?rgb(81,106,82):rgb(45,62,56);ra.rect(m.x+x*m.w/N,m.y+y*m.h/N,(x+1)*m.w/N-x*m.w/N,(y+1)*m.h/N-y*m.h/N,col);}cache.draw(rb,world,m);assert(a==b);
  if(iteration==1)world.seen[10]^=1;if(iteration==2)world.fog[10]^=1;if(iteration==3)world.ore[10]=world.ore[10]?0:10;if(iteration==4)world.ore[10]=world.ore[10]?0:10;if(iteration==5)world.terrain[10]=(world.terrain[10]+1)%3;
 }
 assert(cache.hits&&cache.misses>=6);std::printf("%u cache/uncached scaled/clipped/variant/eviction comparisons, 384 exact gait cases and minimap invalidation passed\n",cases);
}
