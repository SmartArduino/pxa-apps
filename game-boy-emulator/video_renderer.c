/* PXA Game Boy video renderer. MPL-2.0. */
#include "video_renderer.h"
#include <string.h>

void gb_video_set_fit(gb_video_renderer_t *r, gb_display_rect_t area, unsigned fit) {
    r->cache_valid=0;
    unsigned width=r->width, height=r->height;
    if (!width || !height || width>GB_DISPLAY_MAX_WIDTH || height>GB_DISPLAY_MAX_HEIGHT) {
        r->view_width=r->view_height=0; return;
    }
    if (!area.width || !area.height || area.x>=width || area.y>=height ||
        area.width>width-area.x || area.height>height-area.y)
        area=(gb_display_rect_t){0,0,width,height};
    unsigned vw=area.width,vh=area.height;
    if (!fit && vw>=GB_VIDEO_WIDTH && vh>=GB_VIDEO_HEIGHT) {
        vw=GB_VIDEO_WIDTH; vh=GB_VIDEO_HEIGHT;
    } else if ((uint64_t)vw*GB_VIDEO_HEIGHT>(uint64_t)vh*GB_VIDEO_WIDTH)
        vw=(unsigned)((uint64_t)vh*GB_VIDEO_WIDTH/GB_VIDEO_HEIGHT);
    else vh=(unsigned)((uint64_t)vw*GB_VIDEO_HEIGHT/GB_VIDEO_WIDTH);
    if (!vw)vw=1;
    if (!vh)vh=1;
    r->fit=!!fit;r->view_width=vw;r->view_height=vh;
    r->view_x=area.x+(area.width-vw)/2;r->view_y=area.y+(area.height-vh)/2;
    for(unsigned x=0;x<vw;++x)r->sx[x]=(uint16_t)((uint64_t)x*GB_VIDEO_WIDTH/vw);
    for(unsigned y=0;y<vh;++y)r->sy[y]=(uint16_t)((uint64_t)y*GB_VIDEO_HEIGHT/vh);
}

unsigned gb_video_render_cached(gb_video_renderer_t *r,uint16_t *out,const uint16_t *rgb565,
                                unsigned buffer,unsigned force,uint8_t *dirty_rows) {
    return gb_video_render_cached_rows(r,out,rgb565,buffer,force,NULL,dirty_rows);
}

unsigned gb_video_render_cached_rows(gb_video_renderer_t *r,uint16_t *out,const uint16_t *rgb565,
                                     unsigned buffer,unsigned force,const uint8_t *restore_rows,
                                     uint8_t *dirty_rows) {
    if(!out || !rgb565 || !dirty_rows || buffer>1 || !r->view_width || !r->view_height)return 0;
    force=force || !(r->cache_valid&(1u<<buffer));
    if(!force && restore_rows)memcpy(dirty_rows,restore_rows,r->height);
    else memset(dirty_rows,force?1:0,r->height);
    unsigned stride=r->stride?r->stride:r->width,pixels=0;
    int old_y=-1,changed=0,last_changed=0;
    uint16_t *last=NULL;
    for(unsigned y=0;y<r->view_height;++y) {
        unsigned sy=r->sy[y];
        const uint16_t *src=rgb565+sy*GB_VIDEO_WIDTH;
        uint16_t *prior=r->previous[buffer]+sy*GB_VIDEO_WIDTH;
        uint16_t *dst=out+(r->view_y+y)*stride+r->view_x;
        if((int)sy!=old_y) {
            changed=force || memcmp(prior,src,GB_VIDEO_WIDTH*2u);
            if(changed)memcpy(prior,src,GB_VIDEO_WIDTH*2u);
        }
        unsigned repaint=changed || dirty_rows[r->view_y+y];
        if(repaint) {
            dirty_rows[r->view_y+y]=1;pixels+=r->view_width;
            if(last && (int)sy==old_y && last_changed)memcpy(dst,last,r->view_width*2u);
            else if(r->view_width==GB_VIDEO_WIDTH)memcpy(dst,src,GB_VIDEO_WIDTH*2u);
            else for(unsigned x=0;x<r->view_width;++x)dst[x]=src[r->sx[x]];
        }
        old_y=sy;last=dst;last_changed=repaint;
    }
    r->cache_valid|=1u<<buffer;
    return pixels;
}

void gb_video_init(gb_video_renderer_t *r,unsigned width,unsigned height,gb_display_rect_t area) {
    memset(r,0,sizeof(*r));r->width=width;r->height=height;r->stride=width;
    gb_video_set_fit(r,area,1);
}

void gb_video_set_stride(gb_video_renderer_t *r,unsigned stride) {
    if(stride>=r->width){if(r->stride!=stride)r->cache_valid=0;r->stride=stride;}
}

void gb_video_render(gb_video_renderer_t *r,uint16_t *out,const uint16_t *rgb565) {
    if(!out || !rgb565 || !r->view_width || !r->view_height)return;
    unsigned stride=r->stride?r->stride:r->width;
    uint16_t *last=NULL;
    for(unsigned y=0;y<r->view_height;++y) {
        const uint16_t *src=rgb565+(unsigned)r->sy[y]*GB_VIDEO_WIDTH;
        uint16_t *dst=out+(r->view_y+y)*stride+r->view_x;
        if(last && r->sy[y]==r->sy[y-1])memcpy(dst,last,r->view_width*2u);
        else if(r->view_width==GB_VIDEO_WIDTH)memcpy(dst,src,GB_VIDEO_WIDTH*2u);
        else for(unsigned x=0;x<r->view_width;++x)dst[x]=src[r->sx[x]];
        last=dst;
    }
}
