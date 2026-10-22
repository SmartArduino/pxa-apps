/* GPL-2.0-or-later. Cached two-buffer composition versus fresh full rendering. */
#include "video_renderer.h"
#include "controls.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    const unsigned sizes[][2]={{320,240},{296,240},{240,320},{800,480},{151,130}};
    uint16_t source[160*144];uint8_t dirty[GB_DISPLAY_MAX_HEIGHT],restore[GB_DISPLAY_MAX_HEIGHT];
    for(unsigned n=0;n<sizeof(sizes)/sizeof(*sizes);++n) {
        unsigned w=sizes[n][0],h=sizes[n][1],stride=w+7;
        size_t bytes=(stride*h+32)*2;
        uint16_t *actual[2]={calloc(1,bytes),calloc(1,bytes)},*reference=calloc(1,bytes);
        gb_video_renderer_t *r=malloc(sizeof(*r));gb_controls_t *c=malloc(sizeof(*c));
        assert(actual[0] && actual[1] && reference && r && c);
        gb_video_init(r,w,h,(gb_display_rect_t){0,0,w,h});gb_video_set_stride(r,stride);
        gb_controls_init(c,w,h);c->stride=stride;
        gb_display_rect_t view={r->view_x,r->view_y,r->view_width,r->view_height};
        unsigned prior_pressed[2]={0};
        for(unsigned i=0;i<160*144;++i)source[i]=(uint16_t)(i*109+73);
        for(unsigned frame=0;frame<48;++frame) {
            unsigned index=frame&1,force=frame<2,pressed=0;
            if(frame>=8 && frame<18)source[(frame*13%144)*160+(frame*7%160)]^=0x5c73;
            if(frame>=22 && frame<30)pressed=GB_PAD_A|GB_PAD_RIGHT|((frame%3)?0:GB_PAD_B);
            if(frame==34 || frame==35 || frame==42 || frame==43) {
                c->controller_hidden=frame<40;memset(actual[index],0,bytes);force=1;
            }
            memset(restore,0,sizeof(restore));gb_controls_mark_rows(c,prior_pressed[index]^pressed,restore);
            unsigned pixels=gb_video_render_cached_rows(r,actual[index],source,index,force,restore,dirty);
            prior_pressed[index]=pressed;
            if(frame>=2 && frame<8)assert(!pixels);
            if(frame==22 || frame==23)assert(pixels>0 && pixels<view.width*view.height/2);
            gb_controls_clear_bars_rows(c,actual[index],&view,dirty);
            gb_controls_draw_rows(c,actual[index],pressed,dirty);
            memset(reference,0,bytes);gb_video_render(r,reference,source);
            gb_controls_draw(c,reference,pressed);
            assert(!memcmp(actual[index],reference,bytes));
        }
        /* A forced second copy of a scaled row must read pristine source,
         * not the preceding unchanged row containing a translucent button. */
        const gb_control_button_t *button=&c->buttons[GB_CONTROL_A];
        for(int y=button->y+1;y<button->y+button->h;++y){
            if(y<=(int)view.y || y>=(int)(view.y+view.height))continue;
            unsigned relative=(unsigned)y-view.y;
            if(r->sy[relative]!=r->sy[relative-1])continue;
            memset(restore,0,sizeof(restore));restore[y]=1;
            gb_video_render_cached_rows(r,actual[0],source,0,0,restore,dirty);
            gb_controls_clear_bars_rows(c,actual[0],&view,dirty);gb_controls_draw_rows(c,actual[0],0,dirty);
            assert(!memcmp(actual[0],reference,bytes));break;
        }
        free(c);free(r);free(reference);free(actual[0]);free(actual[1]);
    }
    puts("PASS: two-buffer dirty rows, changing pixels, controls, bars, padded strides and five resolutions");
}
