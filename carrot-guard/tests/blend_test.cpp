// SPDX-License-Identifier: MIT
#include "../pixel_renderer.hpp"
#include <cassert>
#include <cstdio>
extern "C" int32_t pxa_call(uint32_t,uint64_t,const uint8_t*,uint32_t){return 0;}
extern "C" int32_t pxa_log_write(uint32_t,const char*){return 0;}
namespace carrot {
struct PixelRendererTest {
  static void verify(){
    PixelRenderer painter;
    for(unsigned alpha=0;alpha<=255;++alpha){
      const auto& fade=painter.opacityBlend(alpha);
      for(unsigned shift:{0u,5u,11u}){
        unsigned maximum=shift==5?63:31;
        for(unsigned s=0;s<=maximum;++s)for(unsigned d=0;d<=maximum;++d){
          uint16_t source=s<<shift,dest=d<<shift;
          uint16_t scaled=((s*alpha+127)/255)<<shift;
          assert(fade.scale(source)==scaled);
          assert(fade.opaque(source,dest)==PixelRenderer::over(scaled,alpha,dest));
        }
      }
      for(unsigned a=0;a<=255;++a)assert(fade.coverage[a]==(a*alpha+127)/255);
    }
  }
};
}
int main(){carrot::PixelRendererTest::verify();std::puts("All opacity values and RGB565 channel combinations match the original blend exactly");}
