/* GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#define NES_DISPLAY_MAX_WIDTH 800u
#define NES_DISPLAY_MAX_HEIGHT 480u
typedef struct {unsigned x,y,width,height;} nes_display_rect_t;

/* Insets/radii are native pixels here. Conservatively inscribe a rectangle;
 * all square roots/clipping are avoided in the frame loop. */
static inline nes_display_rect_t nes_display_safe_rect(unsigned w,unsigned h,
    unsigned shape,const unsigned inset[4],const unsigned radius[4]) {
    unsigned edge[4];for(unsigned i=0;i<4;++i)edge[i]=inset[i];
    if(shape==2) {
        unsigned side=(w<h?w:h)*707/1000;
        unsigned x=(w-side+1)/2,y=(h-side+1)/2;
        if(edge[0]<y)edge[0]=y;if(edge[2]<y)edge[2]=y;
        if(edge[1]<x)edge[1]=x;if(edge[3]<x)edge[3]=x;
    } else if(shape==1) {
        for(unsigned i=0;i<4;++i) {
            unsigned r=radius[i],limit=(w<h?w:h)/2;if(r>limit)r=limit;
            unsigned cut=(r*293+999)/1000;
            unsigned vertical=i<2?0:2,horizontal=(i==0 || i==3)?3:1;
            if(edge[vertical]<cut)edge[vertical]=cut;
            if(edge[horizontal]<cut)edge[horizontal]=cut;
        }
    }
    if((uint64_t)edge[1]+edge[3]>=w || (uint64_t)edge[0]+edge[2]>=h)
        return (nes_display_rect_t){0,0,0,0};
    return (nes_display_rect_t){edge[3],edge[0],w-edge[1]-edge[3],h-edge[0]-edge[2]};
}

/* Match the physical panel so the Host can copy without scaling/interpolation.
 * The game renderer paints at most 256x240; static bars stay in each buffer. */
static inline unsigned nes_display_canvas(unsigned *width,unsigned *height) {
    if(*width<200 || *height<160 || *width>NES_DISPLAY_MAX_WIDTH ||
       *height>NES_DISPLAY_MAX_HEIGHT)return 0;
    return 1;
}
static inline unsigned nes_display_ui_scale(unsigned width,unsigned height) {
    return width>=600 && height>=480 && !(width&1) && !(height&1)?2:1;
}
