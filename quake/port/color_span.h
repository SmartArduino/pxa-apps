/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "color_cache.h"
#include "lightmap_cache.h"
#define PXA_DEFINE_COLOR_SPAN(miplevel,power2) \
static void pxaColorSpan##miplevel##_##power2(uint8_t *out,int count, \
    int s,int t,int sstep,int tstep,uint32_t token) { \
    const int shift=mipShift(miplevel),mask=mipMask(miplevel); \
    const int *lights=(const int *)sdd.blocklights; \
    const int lw=sdd.r_lightwidth,width=sdd.r_texture_width,height=sdd.r_texture_height; \
    const uint8_t *source=sdd.r_source,*map=sdd.colormap; \
    const uintptr_t texture=(uintptr_t)r_drawsurf.texture; \
    const uint32_t qcs=sdd.qcs,qct=sdd.qct; \
    int s6=s>>(16-SPAN_PRECISION),t6=t>>(16-SPAN_PRECISION); \
    const int ds=sstep>>(16-SPAN_PRECISION),dt=tstep>>(16-SPAN_PRECISION); \
    pxa_quake_color_tile_t *tile=0; \
    do { \
        int ss=(int16_t)s6,tt=(int16_t)t6; \
        int si=ss>>SPAN_PRECISION,ti=tt>>SPAN_PRECISION; \
        int tx=si>>3,ty=ti>>3; \
        if(!tile || tile->x!=tx || tile->y!=ty)tile=Pxa_QuakeColorTile(token,texture,tx,ty); \
        unsigned row=(unsigned)ti&7u,col=(unsigned)si&7u,bit=1u<<col,index=row*8u+col; \
        uint8_t pixel; \
        if(tile->valid[row]&bit) {pixel=tile->pixels[index];++pxa_quake_color_hits;} \
        else { \
            const int *p=lights+(si>>shift)+lw*(ti>>shift); \
            int l0=p[0],l1=p[1],delta=ti&mask,a0=(int16_t)l0,a1=(int16_t)l1; \
            int left=(a0<<shift)+((l0>>16)-a0)*delta; \
            int right=(a1<<shift)+((l1>>16)-a1)*delta; \
            int light=(left<<shift)+(right-left)*(si&mask),x,y; \
            if(power2) {x=si&(width-1);y=ti&(height-1);} \
            else { \
                x=((uint64_t)(qcs*(uint32_t)((uint32_t)ss<<(16-SPAN_PRECISION)))*width)>>32; \
                y=((uint64_t)(qct*(uint32_t)((uint32_t)tt<<(16-SPAN_PRECISION)))*height)>>32; \
            } \
            pixel=map[((light>>shift)&0xFF00)+source[y*width+x]]; \
            tile->pixels[index]=pixel;tile->valid[row]|=bit;++pxa_quake_color_misses; \
        } \
        *out++=pixel;s6+=ds;t6+=dt; \
    } while(--count); \
}
PXA_DEFINE_COLOR_SPAN(0,1) PXA_DEFINE_COLOR_SPAN(1,1)
PXA_DEFINE_COLOR_SPAN(2,1) PXA_DEFINE_COLOR_SPAN(3,1)
#undef PXA_DEFINE_COLOR_SPAN
static int Pxa_TryColorLitSpan(uint8_t *out,int count,int s,int t,int ds,int dt) {
    uint32_t token=Pxa_QuakeLightToken();
    /* The general reciprocal sampler can distinguish fractional positions
     * within one texel. Keep that path in the original shader. */
    if(!token || r_fullbright || sdd.textureType || count<4)return 0;
    if(!sdd.textureType) {
        switch(r_drawsurf.surfmip&3) {
        case 0:pxaColorSpan0_1(out,count,s,t,ds,dt,token);break;
        case 1:pxaColorSpan1_1(out,count,s,t,ds,dt,token);break;
        case 2:pxaColorSpan2_1(out,count,s,t,ds,dt,token);break;
        default:pxaColorSpan3_1(out,count,s,t,ds,dt,token);break;
        }
    }
    return 1;
}
