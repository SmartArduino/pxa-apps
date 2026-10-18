#include "classic-raster.h"

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
