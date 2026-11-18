// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
namespace iron {
// Precompute source coordinates once. Packed RGB565 stores and reuse of
// repeated source rows avoid redoing the same work on a high-DPI display.
struct FrameScaler {
 int width=0;std::vector<uint32_t> pairs;std::vector<uint16_t> rows;
 void reset(int sw,int sh,int dw,int dh){width=dw;pairs.resize((dw+1)/2);rows.resize(dh);for(int x=0;x<dw;x+=2)pairs[x/2]=uint32_t(int64_t(x)*sw/dw)|(uint32_t(int64_t(std::min(x+1,dw-1))*sw/dw)<<16);for(int y=0;y<dh;++y)rows[y]=int64_t(y)*sh/dh;}
 void draw(uint16_t* dest,int stride,const uint16_t* source,int source_width)const{
  int previous=-1;for(unsigned y=0;y<rows.size();++y){auto out=dest+y*stride;int sy=rows[y];if(sy==previous){std::memcpy(out,out-stride,size_t(width)*2);continue;}previous=sy;auto in=source+sy*source_width;
   if(width==source_width){std::memcpy(out,in,size_t(width)*2);continue;}
   if((reinterpret_cast<uintptr_t>(out)&3)==0){auto words=reinterpret_cast<uint32_t*>(out);for(int x=0;x<width/2;++x){uint32_t p=pairs[x];uint16_t a=in[p&65535],b=(p&65535)==(p>>16)?a:in[p>>16];uint32_t value=uint32_t(a)|(uint32_t(b)<<16);std::memcpy(words+x,&value,4);}}
   else for(int x=0;x<width/2;++x){uint32_t p=pairs[x];out[x*2]=in[p&65535];out[x*2+1]=in[p>>16];}
   if(width&1)out[width-1]=in[pairs.back()&65535];
  }
 }
};
}
