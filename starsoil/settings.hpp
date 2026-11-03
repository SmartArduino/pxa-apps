// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace starsoil_settings {
inline void encode(uint8_t out[8],bool english,bool smart) {
  const uint8_t value[]={'S','S',1,uint8_t(english),uint8_t(smart),0,0,0};
  for(int i=0;i<8;++i)out[i]=value[i];
}
inline bool decode(const uint8_t *data,size_t size,bool &english,bool &smart) {
  if(!data)return false;
  if(size==4 && data[0]=='P' && data[1]=='L' && data[2]==1 && data[3]<=1) {
    english=data[3]!=0;smart=true;return true;
  }
  if(size!=8 || data[0]!='S' || data[1]!='S' || data[2]!=1 || data[3]>1 || data[4]>1 || data[5] || data[6] || data[7])return false;
  english=data[3]!=0;smart=data[4]!=0;return true;
}
}
