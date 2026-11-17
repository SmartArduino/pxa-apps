// SPDX-License-Identifier: MIT
#pragma once
#include "sprites.hpp"
namespace iron {
// Decode the tiny offline span atlas once. Interior strips are copied in runs;
// tiling keeps scratches and bevels on the same pixel grid at every size.
struct SkinTiles {
 static constexpr int width=48,height=32,count=8,border=6;
 static_assert(art::ui_skin_count==count&&[](){for(unsigned i=0;i<count;++i){auto f=art::frames[art::ui_skin_first+i];if(f.w!=width||f.h!=height)return false;}return true;}());
 struct Row {uint8_t left=width,right=0;bool solid=true;};
 struct Tile {std::array<uint16_t,width*height> pixels{};std::array<uint8_t,width*height/8> alpha{};std::array<Row,height> rows{};bool ready=false;};
 std::array<Tile,count> tiles{};
 static SkinTiles& cache(){static SkinTiles value;return value;}
 const Tile& get(unsigned id){
  auto& tile=tiles[id];if(tile.ready)return tile;auto& f=art::frames[art::ui_skin_first+id];auto src=art::bytes.data()+f.offset;auto& palette=art::palettes[f.palette][0];
  for(unsigned n=0;n<f.count;++n){int x=src[0],y=src[1],length=src[2];src+=3;for(int i=0;i<length;++i){int at=y*width+x+i;tile.pixels[at]=palette[src[i]];tile.alpha[at/8]|=1u<<(at%8);}src+=length;}
  for(int y=0;y<height;++y){auto& row=tile.rows[y];auto lit=[&](int x){int at=y*width+x;return tile.alpha[at/8]&(1u<<(at%8));};for(int x=0;x<width;++x)if(lit(x)){row.left=std::min(int(row.left),x);row.right=x+1;}for(int x=row.left;x<row.right;++x)if(!lit(x))row.solid=false;}tile.ready=true;return tile;
 }
 static int source(int at,int size,int native,int edge){if(at<edge)return at;if(at>=size-edge)return native-(size-at);return border+(at-edge)%(native-2*border);}
 static void draw(Raster& r,Rect area,unsigned id){
  if(id>=count||area.w<=0||area.h<=0)return;int left=std::max(area.x,r.clip.x),right=std::min(area.x+area.w,r.clip.x+r.clip.w),top=std::max(area.y,r.clip.y),bottom=std::min(area.y+area.h,r.clip.y+r.clip.h);if(left>=right||top>=bottom)return;
  const auto& tile=cache().get(id);int ex=std::min(border,(area.w-1)/2),ey=std::min(border,(area.h-1)/2);
  for(int y=top;y<bottom;++y){int sy=source(y-area.y,area.h,height,ey),base=sy*width;auto output=r.pixels+y*r.stride;int x=left;
   while(x<right){int dx=x-area.x,sx=source(dx,area.w,width,ex),run=dx<ex?ex-dx:dx>=area.w-ex?area.w-dx:std::min(width-border-sx,area.w-ex-dx);run=std::min(run,right-x);
    auto row=tile.rows[sy];if(row.solid){int first=std::max(sx,int(row.left)),last=std::min(sx+run,int(row.right));if(first<last)std::copy_n(tile.pixels.data()+base+first,last-first,output+x+first-sx);}else for(int n=0;n<run;++n){int at=base+sx+n;if(tile.alpha[at/8]&(1u<<(at%8)))output[x+n]=tile.pixels[at];}x+=run;
   }
  }
 }
};
}
