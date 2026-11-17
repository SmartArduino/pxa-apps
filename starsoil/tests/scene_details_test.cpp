// SPDX-License-Identifier: MIT
#include "../scene_details.hpp"
#include <array>
#include <cassert>
#include <cstdio>
using namespace starsoil;
int main() {
  for(int material:{Dirt,Stone,Sand,Snow,Ice,Plank,Brick,CopperOre,IronOre,GoldOre,CoalOre,CrystalOre,RubyOre,Obsidian,Deepstone,Moss})
  for(int column=0;column<25;++column)for(int mask=0;mask<16;++mask)for(int light:{40,112,190,255}) {
    std::array<uint16_t,256> pixels{};
    auto sample=[&](int x,int y) {
      x-=column;
      if(!x && !y)return material;
      return (y==-1 && !x && (mask&1)) || (x==1 && !y && (mask&2)) ||
             (y==1 && !x && (mask&4)) || (x==-1 && !y && (mask&8)) ? material:int(Air);
    };
    auto plot=[&](int x,int y,uint16_t c) {assert(x>=0 && x<16 && y>=0 && y<16);pixels[y*16+x]=c;};
    starsoil_terrain::draw_terrain(column,0,0,0,light,sample,plot,Desert);
    const auto silhouette=pixels;
    starsoil_details::tile(material,column,0,0,0,light,sample,[&](int x,int y,int w,int h,uint16_t c) {
      for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)plot(xx,yy,c);
    },material==Stone);
    for(size_t i=0;i<pixels.size();++i) {
      // Cosmetic highlights must not create floating specks outside the tile.
      assert(bool(pixels[i])==bool(silhouette[i]));
      if(pixels[i] && (material==Dirt || material==Sand || material==Plank || material==Stone))
        assert((pixels[i]>>11)>(pixels[i]&31));
    }
    if(material!=Dirt && material!=Sand && material!=Stone)assert(pixels==silhouette);
  }
  puts("PASS every terrain detail across 25 positions, 16 joins and four lights: no unowned cap, floating rim or cold highlight on soil/timber");
  for(int t:{Water,Lava})for(int mask=0;mask<16;++mask) {
    const auto *image=art::tiles[t*16+mask];
    for(int y=0;y<16;++y)for(int x=0;x<16;++x)
      assert(bool(image[y*16+x])==bool((mask&1) || y>=2));
    if(mask&1)for(int x=0;x<16;++x)assert(image[x]==image[16+x]);
    if(mask==15)for(int i=0;i<256;++i)assert(image[i]==image[0]);
  }
  puts("PASS connected water/lava have no internal air gaps, repeated crests or seams; only the exposed surface has a crest");
}
