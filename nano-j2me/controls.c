/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cached masks and component lookup tables follow the approach used in
 * game-boy-emulator/controls.c (GPL-2.0-or-later). */
#include "controls.h"
#include <stdlib.h>
#include <string.h>
static int rounded(int x,int y,int w,int h,int r) {
    if(x<0 || y<0 || x>=w || y>=h)return 0;
    int dx=x<r?r-x-1:x>=w-r?x-(w-r):0;
    int dy=y<r?r-y-1:y>=h-r?y-(h-r):0;
    return dx*dx+dy*dy<r*r;
}
static void add(nano_controls *c,nano_rect r,unsigned key,const char *text,nano_control_label label) {
    if(r.x<c->bounds.x){r.w-=c->bounds.x-r.x;r.x=c->bounds.x;}
    if(r.y<c->bounds.y){r.h-=c->bounds.y-r.y;r.y=c->bounds.y;}
    if(r.x+r.w>c->bounds.x+c->bounds.w)r.w=c->bounds.x+c->bounds.w-r.x;
    if(r.y+r.h>c->bounds.y+c->bounds.h)r.h=c->bounds.y+c->bounds.h-r.y;
    if(c->count>=NANO_CONTROL_COUNT || r.w<1 || r.h<1 || r.w>64 || r.h>64 ||
       (unsigned)(r.w*r.h)>NANO_CONTROL_MASK_BYTES-c->used)return;
    nano_button *b=&c->buttons[c->count++];
    memset(b,0,sizeof(*b));b->rect=r;b->key=key;b->offset=c->used;b->pressed=255;
    uint8_t *mask=c->mask+c->used;c->used+=(unsigned)r.w*r.h;
    int radius=nano_clamp((r.w<r.h?r.w:r.h)/4,3,9);
    for(int y=0;y<r.h;++y)for(int x=0;x<r.w;++x)
        mask[y*r.w+x]=rounded(x,y,r.w,r.h,radius)?rounded(x-1,y-1,r.w-2,r.h-2,radius-1)?1:2:0;
    if(label) {
        uint16_t scratch[64*64]={0};label(scratch,r.w,r.h,text);
        for(int i=0;i<r.w*r.h;++i)if(mask[i] && scratch[i]) {
            unsigned alpha=((scratch[i]>>11)*8+15)/31;
            if(alpha)mask[i]=(uint8_t)(2+alpha);
        }
    }
}
void nano_controls_build(nano_controls *c,nano_rect s,const nano_settings *settings,int visible,nano_control_label label) {
    c->count=c->used=0;c->alpha=settings->alpha;c->bounds=s;c->dpad=(nano_rect){0};++c->generation;++c->cache_rebuilds;
    int side=s.w<s.h?s.w:s.h;
    int k=nano_clamp(side*(settings->button_size==0?22:settings->button_size==2?32:27)/240,18,40);
    int top_h=nano_clamp(k,22,30),top_w=nano_clamp(s.w/4,34,58);
    add(c,(nano_rect){s.x+s.w-top_w-3,s.y+3,top_w,top_h},NANO_ACTION_MENU,"菜单",label);
    if(!visible)return;
    add(c,(nano_rect){s.x+3,s.y+3,top_w,top_h},NANO_ACTION_KEYBOARD,settings->numeric?"方向":"123",label);
    int key_w=nano_clamp(k+5,22,40),key_h=nano_clamp(k,20,36),gap=3;
    int numeric_w=key_w*3+gap*2,numeric_h=key_h*4+gap*3;
    int soft_y;
    if(settings->numeric) {
        int x=s.x+s.w-numeric_w-4,y=s.y+s.h-numeric_h-4;
        if(y<s.y+top_h+7)y=s.y+top_h+7;
        if(x<s.x+3)x=s.x+3;
        for(unsigned i=0;i<12;++i) {
            static const char *const names[]={"1","2","3","4","5","6","7","8","9","*","0","#"};
            unsigned key=i<9?NJ_KEY_1+i:i==9?NJ_KEY_STAR:i==10?NJ_KEY_0:NJ_KEY_POUND;
            add(c,(nano_rect){x+(int)(i%3)*(key_w+gap),y+(int)(i/3)*(key_h+gap),key_w,key_h},key,names[i],label);
        }
        soft_y=y-top_h-4;
    } else soft_y=s.y+s.h-3*k-top_h-9;
    if(soft_y<s.y+top_h+8)soft_y=s.y+top_h+8;
    int soft_w=nano_clamp(s.w/3,38,60);
    add(c,(nano_rect){s.x+4,soft_y,soft_w,top_h},NJ_KEY_SOFT_LEFT,"左软",label);
    add(c,(nano_rect){s.x+s.w-soft_w-4,soft_y,soft_w,top_h},NJ_KEY_SOFT_RIGHT,"右软",label);
    if(!settings->numeric || s.w>=300) {
        nano_rect d={s.x+4,s.y+s.h-k*3-4,k*3,k*3};c->dpad=d;
        add(c,(nano_rect){d.x+k,d.y,k,k},NJ_KEY_UP,"^",label);
        add(c,(nano_rect){d.x+k,d.y+2*k,k,k},NJ_KEY_DOWN,"v",label);
        add(c,(nano_rect){d.x,d.y+k,k,k},NJ_KEY_LEFT,"<",label);
        add(c,(nano_rect){d.x+2*k,d.y+k,k,k},NJ_KEY_RIGHT,">",label);
    }
    if(!settings->numeric) {
        int fire=nano_clamp(k*3/2,28,56);
        add(c,(nano_rect){s.x+s.w-fire-6,s.y+s.h-fire-8,fire,fire},NJ_KEY_FIRE,"OK",label);
    }
}
uint64_t nano_controls_hit(const nano_controls *c,int x,int y,unsigned *action,int *button) {
    *action=0;*button=-1;
    if(!nano_inside(c->bounds,x,y))return 0;
    for(unsigned i=0;i<c->count;++i) {
        const nano_button *b=&c->buttons[i];nano_rect r=b->rect;
        if(nano_inside(r,x,y) && c->mask[b->offset+(y-r.y)*r.w+x-r.x]) {
            *button=(int)i;
            if(b->key>=NJ_KEY_COUNT){*action=b->key;return 0;}
            if(b->key<NJ_KEY_UP || b->key>NJ_KEY_RIGHT || !nano_inside(c->dpad,x,y))return UINT64_C(1)<<b->key;
            break;
        }
    }
    if(nano_inside(c->dpad,x,y)) {
        nano_rect d=c->dpad;int k=d.w/3,dx=x-d.x-d.w/2,dy=y-d.y-d.h/2;
        uint64_t keys=0;
        if(abs(dx)>k/3)keys|=UINT64_C(1)<<(dx<0?NJ_KEY_LEFT:NJ_KEY_RIGHT);
        if(abs(dy)>k/3)keys|=UINT64_C(1)<<(dy<0?NJ_KEY_UP:NJ_KEY_DOWN);
        return keys;
    }
    return 0;
}
static void style(nano_controls *c,nano_button *b,int pressed) {
    if(b->pressed==pressed)return;b->pressed=pressed;
    for(unsigned part=0;part<2;++part) {
        unsigned a=c->alpha+(pressed?2:0)+(part?2:0);if(a>8)a=8;
        uint16_t tint=part?0xffff:pressed?0x2473:0x18e4;
        for(unsigned i=0;i<32;++i){b->red[part][i]=((i*(8-a)+(tint>>11)*a+4)>>3)<<11;b->blue[part][i]=(i*(8-a)+(tint&31)*a+4)>>3;}
        for(unsigned i=0;i<64;++i)b->green[part][i]=((i*(8-a)+((tint>>5)&63)*a+4)>>3)<<5;
    }
}
static uint16_t white(uint16_t p,unsigned a) {
    unsigned r=((p>>11)*(8-a)+31*a+4)>>3,g=(((p>>5)&63)*(8-a)+63*a+4)>>3,b=((p&31)*(8-a)+31*a+4)>>3;
    return (uint16_t)((r<<11)|(g<<5)|b);
}
void nano_controls_draw(nano_controls *c,uint16_t *pixels,unsigned stride,uint64_t keys,uint32_t buttons) {
    for(unsigned i=0;i<c->count;++i) {
        nano_button *b=&c->buttons[i];nano_rect r=b->rect;
        style(c,b,((buttons>>i)&1) || (b->key<NJ_KEY_COUNT && (keys&(UINT64_C(1)<<b->key))));
        const uint8_t *mask=c->mask+b->offset;
        for(int y=0;y<r.h;++y) {
            uint16_t *row=pixels+(size_t)(r.y+y)*stride+r.x;
            for(int x=0;x<r.w;++x) {
                unsigned m=mask[y*r.w+x];if(!m)continue;
                unsigned part=m==2;uint16_t p=row[x];
                p=b->red[part][p>>11]|b->green[part][(p>>5)&63]|b->blue[part][p&31];
                row[x]=m>2?white(p,m-2):p;++c->blended_pixels;
            }
        }
    }
}
