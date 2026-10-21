/* GPL-2.0-or-later: compare temporal reuse with an independent full renderer. */
#include "video_renderer.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static nes_video_renderer_t renderer;
static nes_controls_t controls;
static uint8_t history[NES_VIDEO_HISTORY_BYTES] __attribute__((aligned(8)));
static uint8_t video[240*272+5] __attribute__((aligned(8)));
static unsigned source_offset;
static uint16_t buffers[2][NES_DISPLAY_MAX_WIDTH*NES_DISPLAY_MAX_HEIGHT],reference[NES_DISPLAY_MAX_WIDTH*NES_DISPLAY_MAX_HEIGHT];
static unsigned resolve_calls;
static nes_display_rect_t safe_region;
static int resolve(uint16_t *out,unsigned pitch,unsigned w,unsigned h,const uint8_t *src,unsigned stride) {
    /* Simulate transient backpressure, then compare the real fallback too. */
    if(++resolve_calls%5==0)return 0;
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
        out[y*pitch+x]=controls.palette[src[(y*240/h)*stride+x*256/w]&63];
    return 1;
}
static unsigned random_state=0x12345678;
static unsigned rnd(void){random_state^=random_state<<13;random_state^=random_state>>17;return random_state^=random_state<<5;}
static void check(unsigned width,unsigned height,unsigned frame,unsigned buffer,uint8_t pressed) {
    unsigned visible=nes_controls_visible(&controls);
    if(visible)nes_controls_update(&controls,pressed);
    unsigned aw=safe_region.width?safe_region.width:width,ah=safe_region.height?safe_region.height:height;
    unsigned vw=aw,vh=ah;
    if(aw*240>ah*256)vw=ah*256/240;
    else vh=aw*240/256;
    if(vw>256){vw=256;vh=240;}
    unsigned ox=safe_region.x+(aw-vw)/2,oy=safe_region.y+(ah-vh)/2;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        unsigned style=visible?controls.mask[y*width+x]:0;
        if(x<ox || x>=ox+vw || y<oy || y>=oy+vh)
            reference[y*width+x]=controls.black_lut[style];
        else {
            unsigned index=video[source_offset+((y-oy)*240/vh)*272+(x-ox)*256/vw]&63;
            reference[y*width+x]=controls.lut[style][index];
        }
    }
    nes_video_render(&renderer,buffers[buffer],buffer,video+source_offset,272,&controls,pressed);
    for(unsigned n=0;n<width*height;++n)if(reference[n]!=buffers[buffer][n]) {
        fprintf(stderr,"Mismatch %ux%u frame=%u buffer=%u pixel=%u expected=%04x actual=%04x\n",width,height,frame,buffer,n,reference[n],buffers[buffer][n]);abort();
    }
}
int main(void) {
    const unsigned sizes[][2]={{296,240},{320,240},{200,160},{320,320},{400,240},{800,480}};
    uint16_t colors[64];for(unsigned i=0;i<64;++i)colors[i]=rnd();
    for(unsigned alignment=0;alignment<3;++alignment) {
    source_offset=alignment==2?4:alignment;
    for(unsigned size=0;size<sizeof(sizes)/sizeof(sizes[0]);++size)for(unsigned cached=0;cached<2;++cached) {
        unsigned w=sizes[size][0],h=sizes[size][1];
        nes_controls_init(&controls,w,h,colors);
        nes_video_init(&renderer,w,h,cached?history:NULL,cached?sizeof(history):0,&controls);
        if(w==400 && h==240) {
            assert(renderer.view_x==72 && renderer.view_y==0);
            assert(renderer.view_width==256 && renderer.view_height==240);
        }
        if(w==800 && h==480) {
            assert(renderer.view_x==272 && renderer.view_y==120);
            assert(renderer.view_width==256 && renderer.view_height==240);
        }
        assert(controls.black_lut[0]==0);
        for(unsigned i=0;i<sizeof(video);++i)video[i]=rnd();
        check(w,h,0,0,0);check(w,h,1,1,0);check(w,h,2,0,0);
        assert(!cached || renderer.painted_pixels==0);
        /* Opposite changes between visits to the same buffer must accumulate. */
        video[source_offset+80*272+100]^=1;check(w,h,3,1,0);
        video[source_offset+80*272+100]^=1;check(w,h,4,0,0);check(w,h,5,1,0);
        for(unsigned frame=6;frame<260;++frame) {
            renderer.resolve=(frame%11)?resolve:NULL;
            unsigned mode=frame%7;
            for(unsigned i=0;i<(mode==0?65536:mode==1?1000:mode==2?1:0);++i) {
                unsigned y=rnd()%240,x=rnd()%256;video[source_offset+y*272+x]=rnd();
            }
            if(frame%13==0)nes_controls_cycle(&controls);
            if(frame%19==0)nes_controls_use_controller(&controls);
            if(frame%23==0)nes_controls_use_touch(&controls);
            unsigned buffer=(rnd()>>3)&1;
            if(frame%17==0){memset(buffers[buffer],0,w*h*2);nes_video_invalidate(&renderer,buffer);}
            if(frame%59==0)nes_video_reset(&renderer);
            check(w,h,frame,buffer,rnd());
        }
    }
    }
    const nes_display_rect_t areas[]={{47,47,226,226},{32,24,260,184}};
    source_offset=0;
    for(unsigned area=0;area<2;++area)for(unsigned cached=0;cached<2;++cached) {
        safe_region=areas[area];unsigned w=320,h=area?240:320;
        nes_controls_init(&controls,w,h,colors);controls.safe=safe_region;
        nes_controls_set_labels(&controls,"SELECT","START","MENU");
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
            if(x<safe_region.x || x>=safe_region.x+safe_region.width ||
               y<safe_region.y || y>=safe_region.y+safe_region.height)assert(!controls.mask[y*w+x]);
        nes_video_init_in_rect(&renderer,w,h,safe_region,cached?history:NULL,cached?sizeof(history):0,&controls);
        for(unsigned frame=0;frame<64;++frame) {
            renderer.resolve=frame%3?resolve:NULL;
            if(frame%5==0)nes_controls_use_controller(&controls);
            if(frame%7==0)nes_controls_use_touch(&controls);
            video[(frame*7%240)*272+frame%256]^=31;
            check(w,h,frame,frame%2,frame%16);
        }
    }
    puts("PASS: 9360 full-screen and 256 safe-region frames; RGB565 reference equality, both buffers, alignment, native/fallback, controls and clipping");
}
