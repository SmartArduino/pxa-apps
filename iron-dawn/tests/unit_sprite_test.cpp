// SPDX-License-Identifier: MIT
#include "units.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(int argc,char** argv){
 constexpr int w=256,h=240;std::vector<uint16_t> direct(w*h+2),cached(w*h+2);unsigned cases=0;
 for(float zoom:{.7f,1.f,1.3f})for(int kind=rifle;kind<=harvester;++kind)for(int facing=0;facing<16;++facing){
  int team=facing&1,pose=facing%3;bool load=facing%2,flash=facing%7==0;
  for(auto& p:direct)p=navy;for(auto& p:cached)p=navy;direct.front()=direct.back()=cached.front()=cached.back()=0xa55a;
  Raster a(direct.data()+1,w,h,w),b(cached.data()+1,w,h,w);int x=128,y=120;
  UnitPainter{a,x,y,zoom,unit_directions[facing].x,unit_directions[facing].y,team?red:cyan,flash,pose}.draw(kind,load);
  unit_sprites.draw(b,x,y,kind,team,facing,zoom,pose,load,flash);assert(direct==cached);unsigned hits=unit_sprites.hits;unit_sprites.draw(b,x,y,kind,team,facing,zoom,pose,load,flash);assert(unit_sprites.hits==hits+1);assert(direct==cached);
  // Compare partially visible poses at both screen edges and the top border.
  for(auto edge:std::array<Pt,4>{{{5,7},{20,19},{-6,25},{250,236}}}){std::fill(direct.begin()+1,direct.end()-1,navy);std::fill(cached.begin()+1,cached.end()-1,navy);x=edge.x;y=edge.y;
   UnitPainter{a,x,y,zoom,unit_directions[facing].x,unit_directions[facing].y,team?red:cyan,flash,pose}.draw(kind,load);unit_sprites.draw(b,x,y,kind,team,facing,zoom,pose,load,flash);if(direct!=cached){std::fprintf(stderr,"Clipped mismatch: zoom=%g kind=%d facing=%d pose=%d x=%d y=%d\n",zoom,kind,facing,pose,x,y);return 1;}}
  ++cases;
 }
 if(argc>1){constexpr int sw=640,sh=400;std::vector<uint16_t> p(sw*sh,navy);Raster r(p.data(),sw,sh,sw);for(int kind=0;kind<5;++kind){int x=kind*128;r.box({x+3,3,122,394},panel);r.label({x+4,12,120,24},specs[kind].name,cream,12);for(int row=0;row<4;++row){int facing=row*4;unit_sprites.draw(r,x+64,103+row*85,kind,row==3,facing,1.3f,row%3,kind==harvester&&row%2,false);}}FILE* f=fopen(argv[1],"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",sw,sh);for(auto c:p){unsigned char rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
 std::printf("%u unit directions/poses/zooms: direct, cached, clipped and hit reuse passed; cache=%zu bytes\n",cases,sizeof(unit_sprites));
}
