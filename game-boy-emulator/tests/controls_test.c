/* Check NES-style hit targets and RGB565 cache against arithmetic blending. */
#include "controls.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t blend(unsigned back,unsigned front,unsigned alpha) {
    unsigned r=(((back>>11)*(8-alpha)+(front>>11)*alpha+4)/8)<<11;
    unsigned g=(((((back>>5)&63)*(8-alpha)+((front>>5)&63)*alpha+4)/8)<<5);
    unsigned b=((back&31)*(8-alpha)+(front&31)*alpha+4)/8;
    return r|g|b;
}
int main(void) {
    const unsigned sizes[][2]={{200,160},{296,240},{320,240},{240,320},{480,480},{800,480}};
    gb_controls_t *c=malloc(sizeof(*c));assert(c);
    for(unsigned n=0;n<sizeof(sizes)/sizeof(*sizes);++n) {
        unsigned w=sizes[n][0],h=sizes[n][1],stride=w+7;
        gb_controls_init(c,w,h);c->stride=stride;
        c->safe=(gb_display_rect_t){8,4,w-16,h-8};
        gb_controls_set_labels(c,"SELECT","START","MENU");
        assert(c->mask_used && c->mask_used<=GB_CONTROLS_MASK_BYTES);
        for(unsigned i=0;i<GB_CONTROLS_COUNT;++i) {
            const gb_control_button_t *b=&c->buttons[i];
            assert(b->x>=(int)c->safe.x && b->x+b->w<=(int)(c->safe.x+c->safe.width));
            assert(b->y>=(int)c->safe.y && b->y+b->h<=(int)(c->safe.y+c->safe.height));
            int bit=gb_controls_hit(c,b->x+b->w/2,b->y+b->h/2);
            assert(bit==(b->bit?b->bit:-1));
        }
        assert(!gb_controls_hit(c,w/2,h/4));
        unsigned diagonal=gb_controls_hit(c,c->buttons[0].x+1,c->buttons[2].y+1);
        assert(diagonal==(GB_PAD_LEFT|GB_PAD_UP));
        size_t pixels=stride*h+64;
        uint16_t *a=malloc(pixels*2),*b=malloc(pixels*2);assert(a && b);
        gb_display_rect_t view={w/6,h/7,w*2/3,h*2/3};
        for(unsigned frame=0;frame<4;++frame) {
            unsigned pressed=frame&1?GB_PAD_A|GB_PAD_UP:0;
            for(size_t i=0;i<pixels;++i)a[i]=0xdead;
            for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x) {
                unsigned game=x>=view.x && x<view.x+view.width && y>=view.y && y<view.y+view.height;
                uint16_t color=game?(uint16_t)((x+y*w)*109+frame*193):0;
                a[y*stride+x]=color;
                if(!frame || game)b[y*stride+x]=color;
            }
            if(!frame)for(size_t i=0;i<pixels;++i)b[i]=a[i];
            gb_controls_clear_bars(c,b,&view);
            gb_controls_draw(c,a,pressed);gb_controls_draw(c,b,pressed);
            assert(!memcmp(a,b,pixels*2));
        }
        if(!n)for(unsigned i=0;i<GB_CONTROLS_COUNT;++i)for(unsigned part=1;part<=3;++part) {
            unsigned a=c->alpha+(c->buttons[i].bit&(GB_PAD_A|GB_PAD_UP)?2:0);
            a+=part==2?1:part==3?3:0;if(a>8)a=8;
            unsigned style=i*3+part,color=part==1?c->buttons[i].tint:0xffff;
            for(unsigned v=0;v<65536;++v)assert((c->red[style][v>>11]|c->green[style][(v>>5)&63]|c->blue[style][v&31])==blend(v,color,a));
        }
        c->controller_hidden=1;assert(!gb_controls_hit(c,w/2,h/2));
        free(a);free(b);
    }
    free(c);
    puts("PASS: six resolutions, diagonals, MENU, all RGB565 blends and repeated-frame bar restoration");
}
