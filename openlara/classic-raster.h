#pragma once
#include <stdint.h>

// RGB565 span with the original 16.16 UV/light interpolation and index-zero
// transparency. The calling engine owns clipping and every input array.
void ol_classic_span(uint16_t *output,int count,int32_t u,int32_t v,int32_t l,
    int32_t du,int32_t dv,int32_t dl,const uint8_t *tile,
    const uint8_t *lightmap,const uint16_t *palette);

// Depth is projected screen-space Z in 15.16 fixed point. Transparent texels
// leave both buffers untouched; UI and translucent passes can disable writes.
void ol_classic_depth_span(uint16_t *output,uint16_t *depth,int count,int32_t z,
    int32_t dz,int32_t u,int32_t v,int32_t l,int32_t du,int32_t dv,int32_t dl,
    const uint8_t *tile,const uint8_t *lightmap,const uint16_t *palette,bool write);
