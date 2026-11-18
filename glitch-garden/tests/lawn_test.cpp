// SPDX-License-Identifier: MIT
#include "../lawn.hpp"
#include <cassert>
#include <iostream>
int main(){using namespace garden;
 for(float zoom:{1.f,480.f/296,2.f}){
  constexpr int width=640,height=480;std::vector<uint16_t> pixels(width*height,rgb(136,184,64));auto original=pixels;
  Rect field{24,50,9*24,5*24};int l=std::lround(field.x*zoom),r=std::lround((field.x+field.w)*zoom),t=std::lround(field.y*zoom),b=std::lround((field.y+field.h)*zoom);
  // A painted detail inside the lawn must survive across cells rather than
  // being covered by an opaque, repeated grass rectangle.
  int detail_x=std::lround((field.x+36)*zoom),detail_y=std::lround((field.y+36)*zoom);pixels[detail_y*width+detail_x]=rgb(240,80,48);
  original=pixels;shade_lawn(pixels.data(),width,height,width,field,24,zoom);
  assert((pixels[detail_y*width+detail_x]>>11)>25);
  unsigned changed=0;
  for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto at=y*width+x;
   if(x<=l||x>=r-1||y<=t||y>=b-1)assert(pixels[at]==original[at]);
   changed+=pixels[at]!=original[at];}
  assert(changed>1000);
 }
 for(int edge=0;edge<=9;++edge)assert(std::abs(lawn_axis(edge,9))<.0001f);
 assert(lawn_axis(.5f,9)>.99f&&lawn_axis(1.5f,9)<-.99f);
 assert(lawn_axis(.001f,9)<.0001f&&lawn_axis(8.999f,9)<.0001f);
 std::cout<<"continuous painted detail, soft field perimeter, grid alignment and bounded mowing contrast passed\n";
}
