/* Regression for scaling, RGB ordering and the former index-array overflow. */
#include "video_renderer.h"
#include "vendor/gnuboy/lcd.h"
#include "vendor/gnuboy/pixels.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    for(unsigned planes=0;planes<65536;++planes)for(unsigned flip=0;flip<2;++flip){
        uint8_t row[10];memset(row,0xa5,sizeof(row));
        gnuboy_decode_row(row+1,planes&255,planes>>8,flip);
        assert(row[0]==0xa5 && row[9]==0xa5);
        for(unsigned i=0;i<8;++i){unsigned bit=flip?i:7-i;assert(row[i+1]==(((planes>>bit)&1)|(((planes>>(bit+8))&1)<<1)));}
    }

    assert(gnuboy_rgb565(0x001f)==0xf800);
    assert(gnuboy_rgb565(0x03e0)==0x07e0);
    assert(gnuboy_rgb565(0x7c00)==0x001f);
    assert(gnuboy_rgb565(0x7fff)==0xffff);
    for(unsigned c=0;c<32768;++c) {
        unsigned r=c&31,g=(c>>5)&31,b=c>>10;
        assert(gnuboy_rgb565(c)==((r<<11)|((g*2+g/16)<<5)|b));
    }
    uint16_t source[160*144];
    for(unsigned i=0;i<160*144;++i)source[i]=(uint16_t)(i*109+0x9e37);
    const unsigned sizes[][2]={{320,240},{296,240},{240,320},{400,240},{480,480},{800,480},{160,144},{151,130}};
    for(unsigned n=0;n<sizeof(sizes)/sizeof(*sizes);++n)for(unsigned fit=0;fit<2;++fit) {
        unsigned w=sizes[n][0],h=sizes[n][1],stride=w+7;
        size_t bytes=(stride*h+32)*2;
        uint16_t *actual=malloc(bytes),*expected=malloc(bytes);assert(actual&&expected);
        for(size_t i=0;i<bytes/2;++i)actual[i]=expected[i]=0x54a9;
        gb_video_renderer_t renderer;
        gb_display_rect_t area={7,5,w-14,h-10};
        gb_video_init(&renderer,w,h,area);assert(renderer.fit);gb_video_set_stride(&renderer,stride);
        gb_video_set_fit(&renderer,area,fit);
        gb_video_render(&renderer,actual,source);
        for(unsigned y=0;y<renderer.view_height;++y)for(unsigned x=0;x<renderer.view_width;++x)
            expected[(renderer.view_y+y)*stride+renderer.view_x+x]=source[(y*144/renderer.view_height)*160+x*160/renderer.view_width];
        assert(!memcmp(actual,expected,bytes));
        if(!fit && area.width>=160 && area.height>=144)
            assert(renderer.view_width==160 && renderer.view_height==144);
        free(actual);free(expected);
    }
    puts("PASS: all 32768 colors, native/fit scaling, eight resolutions, padded stride and guards");
}
