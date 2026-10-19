#include "classic-raster.h"

#if OL_RENDER_PROFILE
#include <time.h>
OlRenderProfile ol_render_profile;
uint64_t ol_render_clock_us() {
    timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);
    return uint64_t(t.tv_sec)*1000000u+t.tv_nsec/1000u;
}
#endif

void ol_classic_upscale(uint16_t *pixels,unsigned width) {
    if(width!=240 && width!=160)return;
    // Every source index is <= its destination index, so backwards traversal
    // preserves unread pixels without another frame buffer or heap allocation.
    for(int y=239;y>=0;--y) {
        auto *dest=pixels+y*320;
        const auto *source=pixels+(width==240?((y*3)>>2)*240:(y>>1)*160);
        for(int x=319;x>=0;--x)dest[x]=source[width==240?(x*3)>>2:x>>1];
    }
}

void ol_classic_span(uint16_t *output,int count,int32_t u,int32_t v,int32_t l,
    int32_t du,int32_t dv,int32_t dl,const uint8_t *tile,
    const uint8_t *lightmap,const uint16_t *palette) {
    auto pixel=[&]() {
        unsigned index=tile[((uint32_t(v)>>16)<<8)+(uint32_t(u)>>16)];
        if(index) *output=palette[lightmap[((l>>19)<<8)+index]];
        ++output;u+=du;v+=dv;l+=dl;
    };
    while(count>=4) {pixel();pixel();pixel();pixel();count-=4;}
    while(count-->0)pixel();
}

template<bool Write>
static void depth_span(uint16_t *output,uint16_t *depth,int count,int32_t z,
    int32_t dz,int32_t u,int32_t v,int32_t l,int32_t du,int32_t dv,int32_t dl,
    const uint8_t *tile,const uint8_t *lightmap,const uint16_t *palette) {
    auto pixel=[&]() {
        const uint16_t value=uint32_t(z)>>15;
        if(value<=*depth) {
            unsigned index=tile[((uint32_t(v)>>16)<<8)+(uint32_t(u)>>16)];
            if(index) {
                *output=palette[lightmap[((l>>19)<<8)+index]];
                if(Write)*depth=value;
            }
        }
        ++output;++depth;z+=dz;u+=du;v+=dv;l+=dl;
    };
    while(count>=4) {pixel();pixel();pixel();pixel();count-=4;}
    while(count-->0)pixel();
}

void ol_classic_depth_span(uint16_t *output,uint16_t *depth,int count,int32_t z,
    int32_t dz,int32_t u,int32_t v,int32_t l,int32_t du,int32_t dv,int32_t dl,
    const uint8_t *tile,const uint8_t *lightmap,const uint16_t *palette,bool write) {
    if(write)depth_span<true>(output,depth,count,z,dz,u,v,l,du,dv,dl,tile,lightmap,palette);
    else depth_span<false>(output,depth,count,z,dz,u,v,l,du,dv,dl,tile,lightmap,palette);
}
