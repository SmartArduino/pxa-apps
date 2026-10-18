#pragma once
#include <stdint.h>

// RGB565 span with the original 16.16 UV/light interpolation and index-zero
// transparency. The calling engine owns clipping and every input array.
void ol_classic_span(uint16_t *output,int count,int32_t u,int32_t v,int32_t l,
    int32_t du,int32_t dv,int32_t dl,const uint8_t *tile,
    const uint8_t *lightmap,const uint16_t *palette);
