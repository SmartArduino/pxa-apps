// SPDX-License-Identifier: MIT
#pragma once
#include "filtered_art.hpp"
#include <cmath>
namespace garden {
// Mowing changes the light on one continuous painting. There are no repeated
// texture crops, seams or opaque rectangular patches over the garden.
inline float lawn_axis(float cell_position,int count){
 if(cell_position<=0||cell_position>=count)return 0;
 float edge=std::clamp(std::min(cell_position,float(count)-cell_position)/.2f,0.f,1.f);
 return std::clamp(std::sin(cell_position*3.14159265358979323846f)*3.f,-1.f,1.f)*edge*edge*(3-2*edge);
}
inline uint16_t lawn_tone(uint16_t color,int amount){
 unsigned q=256+amount;
 return (std::min(31u,((color>>11)*q+128)/256)<<11)|(std::min(63u,(((color>>5)&63)*q+128)/256)<<5)|std::min(31u,((color&31)*q+128)/256);
}
inline void shade_lawn(uint16_t* pixels,int width,int height,int stride,Rect field,int unit,float zoom){
 int l=std::clamp(int(std::lround(field.x*zoom)),0,width),r=std::clamp(int(std::lround((field.x+field.w)*zoom)),0,width);
 int t=std::clamp(int(std::lround(field.y*zoom)),0,height),b=std::clamp(int(std::lround((field.y+field.h)*zoom)),0,height);
 std::vector<float> columns(std::max(0,r-l));
 for(int x=l;x<r;++x)columns[x-l]=lawn_axis(((x+.5f)/zoom-field.x)/unit,9);
 for(int y=t;y<b;++y){float shade=lawn_axis(((y+.5f)/zoom-field.y)/unit,5);auto row=pixels+y*stride;
  for(int x=l;x<r;++x)row[x]=lawn_tone(row[x],int(std::lround(14*shade*columns[x-l])));}
}
}
