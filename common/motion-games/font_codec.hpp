// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace motion {
// Private generated-font format: literals (1..128 bytes), or overlapping
// back-references (4..131 bytes, LE16 distance). No third-party decoder.
inline bool unpack_font(std::span<const uint8_t> input,std::span<std::byte> output,unsigned width,unsigned filter){
 if(!width||output.size()%width||filter>4)return false;size_t in=0,out=0;
 while(in<input.size()){
  unsigned token=input[in++],count=(token&127)+(token&128?4:1);
  if(count>output.size()-out)return false;
  if(token&128){if(input.size()-in<2)return false;unsigned offset=unsigned(input[in])|(unsigned(input[in+1])<<8);in+=2;if(!offset||offset>out)return false;
   for(unsigned i=0;i<count;++i){output[out]=output[out-offset];++out;}}
  else{if(input.size()-in<count)return false;for(unsigned i=0;i<count;++i)output[out++]=std::byte(input[in++]);}
 }
 if(out!=output.size())return false;
 if(filter)for(size_t i=0;i<output.size();++i){size_t distance=filter==1||filter==3?width:1;bool predicted=filter==1||filter==3?i>=width:i%width!=0;
  if(predicted){unsigned a=unsigned(output[i]),b=unsigned(output[i-distance]);output[i]=std::byte(filter<=2?(a+b)&255:a^b);}}
 return true;
}
}
