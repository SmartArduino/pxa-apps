/* Dynamic console geometry and mapped RGB565 surfaces. MPL-2.0. */
#include "video_renderer.h"
#include <stdlib.h>
#include <string.h>
/* WASI's byte-wise memcmp repeatedly checks linear-memory bounds in AOT.
 * Compare aligned RGB565 rows in native words, with exact tail bounds. Keep
 * the portable byte path for odd strides and externally supplied buffers. */
typedef uint32_t emu_video_word_t __attribute__((__may_alias__));
static int row_equal(const uint16_t *a,const uint16_t *b,unsigned pixels){
    if(((uintptr_t)a|(uintptr_t)b)&3u)return !memcmp(a,b,pixels*2u);
    const emu_video_word_t *x=(const emu_video_word_t *)a,*y=(const emu_video_word_t *)b;
    while(pixels>=8){
        if((x[0]^y[0])|(x[1]^y[1])|(x[2]^y[2])|(x[3]^y[3]))return 0;
        x+=4;y+=4;pixels-=8;
    }
    while(pixels>=2){if(*x++!=*y++)return 0;pixels-=2;}
    return !pixels || *(const uint16_t *)x==*(const uint16_t *)y;
}
void emu_video_set_fit(emu_video_renderer_t *r,emu_display_rect_t a,unsigned fit){
    r->cache_valid=0;r->fit=!!fit;
    if(!r->width || !r->height || r->width>EMU_DISPLAY_MAX_WIDTH || r->height>EMU_DISPLAY_MAX_HEIGHT){r->view_width=r->view_height=0;return;}
    if(!a.width || !a.height || a.x>=r->width || a.y>=r->height || a.width>r->width-a.x || a.height>r->height-a.y)a=(emu_display_rect_t){0,0,r->width,r->height};
    r->area=a;
    unsigned sw=EMU_VIDEO_WIDTH,sh=EMU_VIDEO_HEIGHT;
    if(!sw || !sh || sw>EMU_MAX_SOURCE_WIDTH || sh>EMU_MAX_SOURCE_HEIGHT){r->view_width=r->view_height=0;return;}
    r->source_width=sw;r->source_height=sh;r->source_stride=emu_core_info.stride;
    r->aspect_num=emu_core_info.aspect_num?emu_core_info.aspect_num:sw;
    r->aspect_den=emu_core_info.aspect_den?emu_core_info.aspect_den:sh;
    unsigned vw=a.width,vh=a.height;
    if(!fit && sw<=vw && sh<=vh){vw=sw;vh=sh;}
    else if((uint64_t)vw*r->aspect_den>(uint64_t)vh*r->aspect_num)vw=(uint64_t)vh*r->aspect_num/r->aspect_den;
    else vh=(uint64_t)vw*r->aspect_den/r->aspect_num;
    if(!vw)vw=1;if(!vh)vh=1;
    r->view_x=a.x+(a.width-vw)/2;r->view_y=a.y+(a.height-vh)/2;r->view_width=vw;r->view_height=vh;
    for(unsigned x=0;x<vw;++x)r->sx[x]=(uint64_t)x*sw/vw;
    for(unsigned y=0;y<vh;++y)r->sy[y]=(uint64_t)y*sh/vh;
    size_t pixels=(size_t)sw*sh;
    if(pixels!=r->previous_pixels){
        free(r->previous[0]);free(r->previous[1]);r->previous[0]=r->previous[1]=NULL;r->previous_pixels=0;
        r->previous[0]=malloc(pixels*2u);r->previous[1]=malloc(pixels*2u);
        if(r->previous[0] && r->previous[1])r->previous_pixels=pixels;
        else {free(r->previous[0]);free(r->previous[1]);r->previous[0]=r->previous[1]=NULL;}
    }
}
void emu_video_sync(emu_video_renderer_t *r){
    if(r->source_width!=EMU_VIDEO_WIDTH || r->source_height!=EMU_VIDEO_HEIGHT ||
       r->source_stride!=emu_core_info.stride || r->aspect_num!=emu_core_info.aspect_num || r->aspect_den!=emu_core_info.aspect_den)
        emu_video_set_fit(r,r->area,r->fit);
}
unsigned emu_video_render_cached(emu_video_renderer_t *r,uint16_t *out,const uint16_t *src,unsigned buffer,unsigned force,uint8_t *dirty){
    emu_video_sync(r);
    if(!out || !src || !dirty || buffer>1 || !r->view_width || !r->view_height)return 0;
    force=force || !r->previous[buffer] || !(r->cache_valid&(1u<<buffer));
    memset(dirty,force?1:0,r->height);unsigned pixels=0;int old=-1,changed=0;uint16_t *last=NULL;
    for(unsigned y=0;y<r->view_height;++y){
        unsigned sy=r->sy[y];const uint16_t *line=src+(size_t)sy*r->source_stride;
        uint16_t *prior=r->previous[buffer]?r->previous[buffer]+(size_t)sy*r->source_width:NULL;
        uint16_t *dst=out+(size_t)(r->view_y+y)*r->stride+r->view_x;
        if((int)sy!=old){changed=force || !row_equal(prior,line,r->source_width);if(changed && prior)memcpy(prior,line,r->source_width*2u);}
        if(changed){dirty[r->view_y+y]=1;pixels+=r->view_width;
            if(last && (int)sy==old)memcpy(dst,last,r->view_width*2u);
            else if(r->view_width==r->source_width)memcpy(dst,line,r->view_width*2u);
            else for(unsigned x=0;x<r->view_width;++x)dst[x]=line[r->sx[x]];
        }
        old=sy;last=dst;
    }
    r->cache_valid|=1u<<buffer;return pixels;
}
void emu_video_init(emu_video_renderer_t *r,unsigned w,unsigned h,emu_display_rect_t a){
    memset(r,0,sizeof(*r));r->width=w;r->height=h;r->stride=w;emu_video_set_fit(r,a,1);
}
void emu_video_deinit(emu_video_renderer_t *r){free(r->previous[0]);free(r->previous[1]);r->previous[0]=r->previous[1]=NULL;r->previous_pixels=0;r->cache_valid=0;}
void emu_video_set_stride(emu_video_renderer_t *r,unsigned stride){if(stride>=r->width){if(r->stride!=stride)r->cache_valid=0;r->stride=stride;}}
void emu_video_render(emu_video_renderer_t *r,uint16_t *out,const uint16_t *src){uint8_t dirty[EMU_DISPLAY_MAX_HEIGHT];emu_video_render_cached(r,out,src,0,1,dirty);}
