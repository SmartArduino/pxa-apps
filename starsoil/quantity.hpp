// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace starsoil_quantity {
// Original 4x7 pixel digits. Only ink and its one-pixel shadow are written;
// icon pixels and slot colours remain visible through the background.
inline constexpr uint8_t Digits[10][7]={
 {6,9,9,9,9,9,6},{2,6,2,2,2,2,7},{6,9,1,2,4,8,15},
 {14,1,1,6,1,1,14},{2,6,10,10,15,2,2},{15,8,8,14,1,1,14},
 {6,8,8,14,9,9,6},{15,1,1,2,2,4,4},{6,9,9,6,9,9,6},{6,9,9,7,1,1,6}};
template<class Plot>
void draw(int right,int bottom,unsigned count,uint16_t ink,uint16_t shadow,Plot plot) {
  if(count<=1)return;
  if(count>999)count=999;
  const int digits=count>=100?3:count>=10?2:1;
  const int left=right-(digits*5-1)-1,top=bottom-8;
  for(int pass=1;pass>=0;--pass) {
    unsigned divisor=digits==3?100:digits==2?10:1;
    for(int i=0;i<digits;++i,divisor/=10) {
      unsigned digit=count/divisor%10;
      for(int y=0;y<7;++y)for(int x=0;x<4;++x)
        if(Digits[digit][y]&(8u>>x))plot(left+i*5+x+pass,top+y+pass,pass?shadow:ink);
    }
  }
}
} // namespace starsoil_quantity
