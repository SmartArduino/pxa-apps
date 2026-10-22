/* GPL-2.0-or-later. Cold layout/input helpers; raster caches stay in each renderer. */
#pragma once
#include <stdint.h>
typedef struct { int x,y,w,h; } emu_touch_rect_t;
enum { EMU_TOUCH_COUNT=13 };
static inline unsigned emu_touch_scale(unsigned width,unsigned height) {
    unsigned x=(width<<16)/296,y=(height<<16)/240;return x<y?x:y;
}
static inline int emu_touch_gesture_margin(unsigned scale) {
    int margin=(int)((24u*scale+32768)>>16);return margin<24?24:margin;
}
static inline int emu_touch_px(int value,unsigned scale) {
    return (int)(((int64_t)value*scale+32768)>>16);
}
/* Two thumb clusters retain their proportions on wide and portrait screens.
 * The middle controls move with the centre, leaving a gap around the D-pad. */
static inline unsigned emu_touch_layout(emu_touch_rect_t *out,unsigned width,unsigned height,unsigned style,int ox,int oy) {
    unsigned scale=emu_touch_scale(width,height);
    int cy=240-79;
    emu_touch_rect_t b[EMU_TOUCH_COUNT]={
        {8,cy-13,32,26},{64,cy-13,32,26},{36,cy-41,32,26},{36,cy+15,32,26},
        {103,cy+30,40,20},{151,cy+30,36,20},{106,cy-36,72,24},
        {211,cy+9,36,34},{249,cy-21,36,34},
        {223,cy-44,32,32},{195,cy-16,32,32},{10,cy-76,54,24},{232,cy-76,54,24}};
    if(style==2)for(unsigned i=9;i<EMU_TOUCH_COUNT;++i)b[i].w=b[i].h=0;
    if(style==4) {b[7]=(emu_touch_rect_t){223,cy+12,32,32};b[8]=(emu_touch_rect_t){251,cy-16,32,32};}
    if(style==6) {
        const unsigned slots[]={8,7,9,10,11,12};
        for(unsigned n=0;n<6;++n)b[slots[n]]=(emu_touch_rect_t){187+(int)(n%3)*36,cy+(n<3?12:-24),32,32};
    }
    int gap=(int)width-emu_touch_px(296,scale),bottom=(int)height-emu_touch_px(240,scale);
    bottom-=emu_touch_gesture_margin(scale)-emu_touch_px(24,scale);
    for(unsigned i=0;i<EMU_TOUCH_COUNT;++i) {
        int anchor=(i<4 || (style==4 && i==11))?0:(i>=7?gap:gap/2);
        out[i]=(emu_touch_rect_t){ox+anchor+emu_touch_px(b[i].x,scale),oy+bottom+emu_touch_px(b[i].y,scale),
            emu_touch_px(b[i].w,scale),emu_touch_px(b[i].h,scale)};
    }
    return scale;
}
/* Returns a button index, -1 for empty space, -2 for the neutral centre, or
 * 16 + directional index bits. A finger selects one action button; separate
 * contacts can still hold several buttons. The pause target is never enlarged. */
static inline int emu_touch_target(const emu_touch_rect_t *b,unsigned count,int x,int y,unsigned scale) {
    int cx=b[2].x+b[2].w/2,cy=b[0].y+b[0].h/2;
    int pad=emu_touch_px(5,scale);
    if(x>=b[0].x-pad && x<b[1].x+b[1].w+pad && y>=b[2].y-pad && y<b[3].y+b[3].h+pad) {
        int dx=x-cx,dy=y-cy,ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
        int dead=emu_touch_px(7,scale);if(dead<2)dead=2;
        if(dx*dx+dy*dy<=dead*dead)return -2;
        unsigned bits=0;
        if(2*ax>ay)bits|=dx<0?1u:2u;
        if(2*ay>ax)bits|=dy<0?4u:8u;
        return 16+bits;
    }
    int best=-1;int64_t distance=INT64_MAX;
    for(unsigned i=4;i<count;++i) {
        if(!b[i].w || !b[i].h)continue;
        int grow=i==6?0:emu_touch_px(i<6?3:6,scale);
        if(x<b[i].x-grow || x>=b[i].x+b[i].w+grow || y<b[i].y-grow || y>=b[i].y+b[i].h+grow)continue;
        int dx=2*x-(2*b[i].x+b[i].w),dy=2*y-(2*b[i].y+b[i].h);
        int64_t score=(int64_t)dx*dx+(int64_t)dy*dy;
        if(score<distance){best=i;distance=score;}
    }
    return best;
}
