// SPDX-License-Identifier: MIT
#pragma once
#include "sprites.hpp"
namespace iron {
// Cap widths and border heights are independent; long metal strips retain
// their end pieces while the quiet middle and horizontal rims repeat.
struct MissionSkin {
 static constexpr int width=144,height=20,count=4,bx=8,by=3;
 struct Row{uint8_t left=width,right=0;bool solid=true;};
 struct Tile{std::array<uint16_t,width*height> pixels{};std::array<uint8_t,width*height/8> alpha{};std::array<Row,height> rows{};bool ready=false;};
 std::array<Tile,count> tiles{};
 static MissionSkin& cache(){static MissionSkin value;return value;}
 const Tile& get(unsigned id){auto& t=tiles[id];if(t.ready)return t;auto& f=art::frames[art::mission_skin_first+id];auto src=art::bytes.data()+f.offset;for(unsigned i=0;i<f.count;++i){int x=src[0],y=src[1],n=src[2];src+=3;auto& row=t.rows[y];row.left=std::min(int(row.left),x);row.right=std::max(int(row.right),x+n);for(int k=0;k<n;++k){int at=y*width+x+k;t.pixels[at]=art::palettes[f.palette][0][src[k]];t.alpha[at/8]|=1u<<(at%8);}src+=n;}for(int y=0;y<height;++y)for(int x=t.rows[y].left;x<t.rows[y].right;++x){int at=y*width+x;if(!(t.alpha[at/8]&(1u<<(at%8))))t.rows[y].solid=false;}t.ready=true;return t;}
 static int source(int at,int size,int native,int border,int edge){if(at<edge)return at;if(at>=size-edge)return native-size+at;return border+(at-edge)%(native-2*border);}
 static void draw(Raster& r,Rect a,unsigned id=0){if(id>=count||a.w<=0||a.h<=0)return;int left=std::max(a.x,r.clip.x),right=std::min(a.x+a.w,r.clip.x+r.clip.w),top=std::max(a.y,r.clip.y),bottom=std::min(a.y+a.h,r.clip.y+r.clip.h);if(left>=right||top>=bottom)return;auto& t=cache().get(id);int ex=std::min(bx,(a.w-1)/2),ey=std::min(by,(a.h-1)/2);
  for(int y=top;y<bottom;++y){int sy=source(y-a.y,a.h,height,by,ey);auto out=r.pixels+y*r.stride;for(int x=left;x<right;){int dx=x-a.x,sx=source(dx,a.w,width,bx,ex),n=dx<ex?ex-dx:dx>=a.w-ex?a.w-dx:std::min(width-bx-sx,a.w-ex-dx);n=std::min(n,right-x);int first=std::max(sx,int(t.rows[sy].left)),last=std::min(sx+n,int(t.rows[sy].right));if(first<last){if(t.rows[sy].solid)std::copy_n(t.pixels.data()+sy*width+first,last-first,out+x+first-sx);else for(int k=first;k<last;++k){int at=sy*width+k;if(t.alpha[at/8]&(1u<<(at%8)))out[x+k-sx]=t.pixels[at];}}x+=n;}}
 }
};
}
