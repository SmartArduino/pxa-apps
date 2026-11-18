// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
namespace garden {
struct HudUnderlay {
 std::vector<uint16_t> pixels;int width=0,height=0,top=0,bottom=0;
 void configure(int w,int h,int upper,int lower){width=w;height=h;top=std::clamp(upper,0,h);bottom=std::clamp(lower,top,h);pixels.resize(size_t(w)*(top+h-bottom));}
 void capture(const uint16_t* source){std::copy_n(source,size_t(width)*top,pixels.begin());std::copy_n(source+size_t(width)*bottom,size_t(width)*(height-bottom),pixels.begin()+size_t(width)*top);}
 void restore(uint16_t* dest)const{std::copy_n(pixels.begin(),size_t(width)*top,dest);std::copy(pixels.begin()+size_t(width)*top,pixels.end(),dest+size_t(width)*bottom);}
};
struct HudPatch {
 std::vector<uint16_t> pixels;int x=0,y=0,width=0,height=0;
 void capture(const uint16_t* source,int stride,int left,int top,int w,int h){x=left;y=top;width=w;height=h;pixels.resize(size_t(w)*h);for(int row=0;row<h;++row)std::copy_n(source+(y+row)*stride+x,w,pixels.data()+row*w);}
 void restore(uint16_t* dest,int stride)const{for(int row=0;row<height;++row)std::copy_n(pixels.data()+row*width,width,dest+(y+row)*stride+x);}
};
}
