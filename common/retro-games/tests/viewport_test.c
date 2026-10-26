/* SPDX-License-Identifier: MIT. Viewport bounds and exact raster/input pairing. */
#include "../viewport.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    const uint32_t sizes[][2]={{296,240},{320,240},{412,412},{800,480},{480,800},{1,1}};
    const uint32_t sources[][2]={{400,416},{128,64},{320,240},{800,552},{UINT32_MAX,1},{1,UINT32_MAX}};
    const uint32_t insets[][4]={{0,0,0,0},{17,13,21,11},{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}};
    for(unsigned a=0;a<sizeof(sizes)/sizeof(*sizes);++a)
    for(unsigned b=0;b<sizeof(sources)/sizeof(*sources);++b)
    for(unsigned c=0;c<sizeof(insets)/sizeof(*insets);++c) {
        uint32_t w=sizes[a][0],h=sizes[a][1],sw=sources[b][0],sh=sources[b][1];
        retro_viewport_t v=retro_viewport(w,h,sw,sh,insets[c]);
        assert(v.width&&v.height&&v.x+v.width<=w&&v.y+v.height<=h);
        for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x) {
            int32_t gx=-1,gy=-1;
            int inside=retro_viewport_point(v,x,y,sw,sh,&gx,&gy);
            assert(inside==(x>=v.x&&x-v.x<v.width&&y>=v.y&&y-v.y<v.height));
            if(inside){assert((uint32_t)gx==(uint64_t)(x-v.x)*sw/v.width);assert((uint32_t)gy==(uint64_t)(y-v.y)*sh/v.height);}
        }
        int32_t x,y;assert(!retro_viewport_point(v,-1,-1,sw,sh,&x,&y));
    }
    puts("PASS: aspect fit, safe areas, extreme ratios and inverse pointer coordinates");
}
