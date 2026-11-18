// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <algorithm>
#include <span>
namespace garden {
// Low tags contain 1..128 literals. High tags copy 4..131 bytes from a
// previous 16-bit distance; overlapping copies deliberately grow runs.
inline bool unpack(std::span<const uint8_t> input,std::span<uint8_t> output){
 size_t at=0,end=0;
 while(at<input.size()){
  unsigned tag=input[at++],count=(tag&127)+((tag&128)?4:1);
  if(count>output.size()-end)return false;
  if(tag&128){if(input.size()-at<2)return false;unsigned distance=input[at]|unsigned(input[at+1])<<8;at+=2;if(!distance||distance>end)return false;
   if(distance==1)std::fill_n(output.data()+end,count,output[end-1]);
   else if(distance>=count)std::copy_n(output.data()+end-distance,count,output.data()+end);
   else for(unsigned i=0;i<count;++i)output[end+i]=output[end+i-distance];
  }else{if(count>input.size()-at)return false;std::copy_n(input.data()+at,count,output.data()+end);at+=count;}
  end+=count;
 }
 return end==output.size();
}
}
