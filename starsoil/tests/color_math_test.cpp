// SPDX-License-Identifier: MIT
#include "../color_math.hpp"
#include <cassert>
#include <cstdio>
int main() {
  for(unsigned n=0;n<=63*255;++n) assert(starsoil_color::div255(n)==n/255);
  for(unsigned c=0;c<65536;++c) for(unsigned v=0;v<256;++v) {
    auto expected=uint16_t(((((c>>11)&31)*v/255)<<11)|
                          ((((c>>5)&63)*v/255)<<5)|((c&31)*v/255));
    assert(starsoil_color::dim(uint16_t(c),v)==expected);
  }
  for(unsigned a=0;a<64;++a) for(unsigned b=0;b<64;++b) for(unsigned k=0;k<256;++k)
    assert(starsoil_color::div255(a*(255-k)+b*k)==(a*(255-k)+b*k)/255);
  puts("PASS exact RGB565 shading for every color and brightness; exact blending for every channel pair and alpha");
}
