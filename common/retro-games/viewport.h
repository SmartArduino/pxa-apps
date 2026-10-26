/* SPDX-License-Identifier: MIT. Integer aspect-fit and inverse input mapping. */
#ifndef PXA_RETRO_VIEWPORT_H
#define PXA_RETRO_VIEWPORT_H
#include <stdint.h>
typedef struct { uint32_t x,y,width,height; } retro_viewport_t;
static inline retro_viewport_t retro_viewport(uint32_t width,uint32_t height,
    uint32_t sw,uint32_t sh,const uint32_t insets[4]) {
    retro_viewport_t v={0,0,0,0};if(!width||!height||!sw||!sh)return v;
    uint32_t left=insets[3]<width?insets[3]:width-1,top=insets[0]<height?insets[0]:height-1;
    uint32_t right=insets[1]<width-left?insets[1]:width-left-1,bottom=insets[2]<height-top?insets[2]:height-top-1;
    uint32_t aw=width-left-right,ah=height-top-bottom;
    uint64_t scaled=(uint64_t)aw*sh/sw;
    v.width=aw;v.height=scaled>ah?ah:(uint32_t)scaled;
    if(scaled>ah)v.width=(uint64_t)ah*sw/sh;
    if(!v.width)v.width=1;
    if(!v.height)v.height=1;
    v.x=left+(aw-v.width)/2;v.y=top+(ah-v.height)/2;return v;
}
static inline int retro_viewport_point(retro_viewport_t v,int32_t x,int32_t y,
    uint32_t sw,uint32_t sh,int32_t *ox,int32_t *oy) {
    if(!v.width||!v.height||x<(int32_t)v.x||y<(int32_t)v.y||
       (uint32_t)x-v.x>=v.width||(uint32_t)y-v.y>=v.height)return 0;
    *ox=(uint64_t)((uint32_t)x-v.x)*sw/v.width;*oy=(uint64_t)((uint32_t)y-v.y)*sh/v.height;return 1;
}
#endif
