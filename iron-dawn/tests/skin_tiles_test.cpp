// SPDX-License-Identifier: MIT
#include "ui.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace iron;
int main(){
 constexpr uint16_t blank=0xf81f,guard=0xa55a;std::vector<uint16_t> native(48*32),sliced(48*32);
 for(unsigned id=0;id<8;++id){std::fill(native.begin(),native.end(),blank);std::fill(sliced.begin(),sliced.end(),blank);Raster a(native.data(),48,32,48),b(sliced.data(),48,32,48);Sprites::blit(a,art::ui_skin_first+id,0,0);SkinTiles::draw(b,{0,0,48,32},id);assert(native==sliced);
  for(auto size:{std::pair{1,1},std::pair{19,17},std::pair{88,28},std::pair{240,64},std::pair{624,400}}){std::vector<uint16_t> output(640*480+2,blank);output.front()=output.back()=guard;Raster r(output.data()+1,640,480,640);r.clip={3,5,620,450};SkinTiles::draw(r,{-2,-3,size.first,size.second},id);assert(output.front()==guard&&output.back()==guard);for(int y=0;y<480;++y)for(int x=0;x<640;++x)if(!r.clip.has(x,y))assert(output[1+y*640+x]==blank);}
  std::vector<uint16_t> expanded(240*100,blank);Raster big(expanded.data(),240,100,240);SkinTiles::draw(big,{0,0,240,100},id);for(int y=0;y<6;++y)for(int x=0;x<6;++x){assert(expanded[y*240+x]==native[y*48+x]);assert(expanded[(99-y)*240+239-x]==native[(31-y)*48+47-x]);}assert(expanded[12*240+12]==expanded[12*240+48]);
 }
 std::puts("Eight image skins match source pixels; nine-slice corners, tiled interiors, clipping and buffer canaries passed");
}
