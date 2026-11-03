/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "layout.h"
int nano_clamp(int n,int low,int high) {return n<low?low:n>high?high:n;}
int nano_inside(nano_rect r,int x,int y) {return x>=r.x && y>=r.y && x-r.x<r.w && y-r.y<r.h;}
int nano_canvas_size(int display_w,int display_h,int *w,int *h) {
    if(display_w<96 || display_h<96 || display_w>8192 || display_h>8192)return 0;
    int scale=1;
    while((uint64_t)(display_w/scale)*(display_h/scale)>153600u ||
          display_w/scale>1024 || display_h/scale>1024)++scale;
    *w=(display_w/scale)&~7;*h=(display_h/scale)&~1;
    /* Both contiguous mapped buffers require 64-byte aligned starts. */
    if(((*w)*(*h)*2)%64)*h-=2;
    return *w>=48 && *h>=48 ? scale : 0;
}
nano_rect nano_safe_rect(int w,int h,unsigned shape,const unsigned inset[4],const unsigned radius[4]) {
    unsigned edge[4];for(unsigned i=0;i<4;++i)edge[i]=inset[i];
    if(shape==2) {
        unsigned side=(unsigned)(w<h?w:h)*707/1000;
        unsigned x=((unsigned)w-side+1)/2,y=((unsigned)h-side+1)/2;
        if(edge[0]<y)edge[0]=y;if(edge[2]<y)edge[2]=y;
        if(edge[1]<x)edge[1]=x;if(edge[3]<x)edge[3]=x;
    } else if(shape==1) {
        for(unsigned i=0;i<4;++i) {
            unsigned r=radius[i],limit=(unsigned)(w<h?w:h)/2;if(r>limit)r=limit;
            unsigned cut=(r*293+999)/1000,vertical=i<2?0:2,horizontal=(i==0 || i==3)?3:1;
            if(edge[vertical]<cut)edge[vertical]=cut;if(edge[horizontal]<cut)edge[horizontal]=cut;
        }
    }
    if((uint64_t)edge[1]+edge[3]>=(unsigned)w || (uint64_t)edge[0]+edge[2]>=(unsigned)h)
        return (nano_rect){0,0,w,h};
    return (nano_rect){edge[3],edge[0],w-edge[1]-edge[3],h-edge[0]-edge[2]};
}
nano_rect nano_fit(nano_rect area,int w,int h) {
    if(w<=0 || h<=0 || area.w<=0 || area.h<=0)return (nano_rect){0};
    int rw=area.w,rh=(int)((int64_t)h*rw/w);
    if(rh>area.h){rh=area.h;rw=(int)((int64_t)w*rh/h);}
    if(rw<1)rw=1;if(rh<1)rh=1;
    return (nano_rect){area.x+(area.w-rw)/2,area.y+(area.h-rh)/2,rw,rh};
}
