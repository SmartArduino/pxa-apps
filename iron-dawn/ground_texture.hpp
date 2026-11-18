// SPDX-License-Identifier: MIT
#pragma once
#include "raster.hpp"
#include "model.hpp"
#include "ground_texture_data.hpp"
#include <vector>
namespace iron {
struct GroundTexture {
 std::vector<uint16_t> columns,rows;
 static unsigned mirror(int value){unsigned i=unsigned(value)&511;return i<256?i:511-i;}
 void prepare(int width,int top,int bottom,float camx,float camy,float zoom){columns.resize(width);rows.resize(bottom);float cx=width*.5f-camx,cy=top+(bottom-top)*.5f-camy;for(int x=0;x<width;++x)columns[x]=mirror(int(std::floor((x-cx)/zoom)));for(int y=top;y<bottom;++y)rows[y]=mirror(int(std::floor((y-cy)/zoom)));}
 void span(Raster& r,int left,int right,int y,bool dim=false)const{left=std::max(left,r.clip.x);right=std::min(right,r.clip.x+r.clip.w);if(y<r.clip.y||y>=r.clip.y+r.clip.h||left>=right)return;auto src=art::ground_texels.data()+rows[y]*256;auto& palette=dim?art::ground_material_dim:art::ground_material;auto out=r.pixels+y*r.stride;for(int x=left;x<right;++x)out[x]=palette[src[columns[x]]];}
 void paint(Raster& r,int width,int top,int bottom,float camx,float camy,float zoom,int side=N)const{float cx=width*.5f-camx,cy=top+(bottom-top)*.5f-camy;for(int y=top;y<bottom;++y){float dy=(y+.5f-cy)/(10*zoom),lo=std::max(-dy,dy-2*side),hi=std::min(dy,2*side-dy);if(lo<hi)span(r,int(std::ceil(cx+20*zoom*lo)),int(std::ceil(cx+20*zoom*hi)),y);}}
 void dim(Raster& r,Pt p,float zoom)const{int first=std::max(r.clip.y,int(std::floor(p.y-10*zoom))),last=std::min(r.clip.y+r.clip.h,int(std::ceil(p.y+10*zoom)));for(int y=first;y<last;++y){float half=20*zoom*std::max(0.f,1-std::abs((y+.5f-p.y)/(10*zoom)));span(r,int(std::ceil(p.x-half)),int(std::ceil(p.x+half)),y,true);}}
};
}
