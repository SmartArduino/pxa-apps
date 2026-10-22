#include "controls.h"
#include <assert.h>
#include <stdio.h>
static nes_controls_t c;
int main(void) {
    uint16_t palette[64];for(unsigned i=0;i<64;++i)palette[i]=i&1?0xffff:0;
    nes_controls_init(&c,296,240,palette);
    for(unsigned i=0;i<NES_CONTROLS_COUNT;++i){
        const nes_control_button_t *b=&c.buttons[i];
        assert(nes_controls_hit(&c,b->x+b->w/2,b->y+b->h/2)==(b->bit?b->bit:-1));
    }
    int cx=c.buttons[2].x+c.buttons[2].w/2,cy=c.buttons[0].y+c.buttons[0].h/2;
    assert(nes_controls_hit(&c,cx-25,cy-25)==(64|16));
    assert(nes_controls_hit(&c,cx+25,cy+25)==(128|32));
    assert(!nes_controls_hit(&c,cx,cy));
    assert(!nes_controls_hit(&c,70,228));
    assert(!nes_controls_hit(&c,145,100));
    /* Fixed RGB565 reference values, including per-channel rounding. */
    assert(c.lut[1][1]==0x9cf3); /* 37.5% black over white */
    assert(c.lut[2][0]==0x8410); /* 50% white outline over black */
    assert(c.lut[3][0]==0xbdf7); /* 75% white glyph over black */
    nes_controls_update(&c,64);assert(c.lut[1][1]==0x630c);
    nes_controls_update(&c,0);assert(c.lut[1][1]==0x9cf3);
    for(unsigned i=0;i<296*240;++i)assert(c.mask[i]<NES_CONTROL_STYLES);
    for(unsigned y=0;y<240;++y)if(y<120 || y>=216)assert(!c.row_used[y]);
    assert(nes_controls_visible(&c));
    assert(nes_controls_use_controller(&c));assert(!nes_controls_visible(&c));
    assert(!nes_controls_hit(&c,275,171));assert(!nes_controls_use_controller(&c));
    assert(nes_controls_use_touch(&c));assert(nes_controls_visible(&c));
    assert(c.alpha==3); /* Restores the user's selected opacity. */
    nes_controls_cycle(&c);assert(c.alpha==2);
    nes_controls_cycle(&c);assert(c.alpha==5);
    nes_controls_cycle(&c);assert(!c.alpha && !nes_controls_hit(&c,275,171));
    nes_controls_use_controller(&c);nes_controls_use_touch(&c);assert(!nes_controls_visible(&c));
    nes_controls_cycle(&c);assert(c.alpha==3);
    const unsigned dimensions[][2]={{200,160},{320,320},{400,240},{800,480}};
    for(unsigned d=0;d<sizeof(dimensions)/sizeof(dimensions[0]);++d) {
        unsigned w=dimensions[d][0],h=dimensions[d][1];nes_controls_init(&c,w,h,palette);
        for(unsigned i=0;i<w*h;++i)assert(c.mask[i]<NES_CONTROL_STYLES);
        for(unsigned i=0;i<NES_CONTROLS_COUNT;++i){
            const nes_control_button_t *b=&c.buttons[i];
            assert(b->x>=0 && b->x+b->w<=(int)w && b->y>=0 && b->y+b->h<=(int)h-24);
            assert(nes_controls_hit(&c,b->x+b->w/2,b->y+b->h/2)==(b->bit?b->bit:-1));
        }

    }
    puts("PASS: controls, RGB565 alpha, pressed feedback, gesture exclusion and opacity cycle");
}
