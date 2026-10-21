#include "controls.h"
#include <assert.h>
#include <stdio.h>
static nes_controls_t c;
int main(void) {
    uint16_t palette[64];for(unsigned i=0;i<64;++i)palette[i]=i&1?0xffff:0;
    nes_controls_init(&c,296,240,palette);
    assert(nes_controls_hit(&c,95,179)==128);
    assert(nes_controls_hit(&c,62,203)==32);
    assert(nes_controls_hit(&c,62,155)==16);
    assert(nes_controls_hit(&c,29,179)==64);
    assert(nes_controls_hit(&c,95,155)==(128|16));
    assert(nes_controls_hit(&c,29,203)==(64|32));
    assert(!nes_controls_hit(&c,62,179));
    assert(nes_controls_hit(&c,275,171)==1);
    assert(nes_controls_hit(&c,200,199)==8);
    assert(nes_controls_hit(&c,164,163)==-1);
    assert(!nes_controls_hit(&c,70,228));
    assert(!nes_controls_hit(&c,145,100));
    /* Fixed RGB565 reference values, including per-channel rounding. */
    assert(c.lut[1][1]==0x9cf3); /* 37.5% black over white */
    assert(c.lut[2][0]==0x8410); /* 50% white outline over black */
    assert(c.lut[3][0]==0xbdf7); /* 75% white glyph over black */
    nes_controls_update(&c,64);assert(c.lut[1][1]==0x630c);
    nes_controls_update(&c,0);assert(c.lut[1][1]==0x9cf3);
    for(unsigned i=0;i<296*240;++i)assert(c.mask[i]<NES_CONTROL_STYLES);
    for(unsigned y=0;y<240;++y)if(y<144 || y>=216)assert(!c.row_used[y]);
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
        unsigned scale=nes_display_ui_scale(w,h);
        for(unsigned y=0;y<h;++y)if(y<h-96*scale || y>=h-24*scale)assert(!c.row_used[y]);
        if(w==800) {
            assert(nes_controls_hit(&c,256,358)==128);
            assert(nes_controls_hit(&c,742,342)==1);
            assert(nes_controls_hit(&c,442,326)==-1);
        }
    }
    puts("PASS: controls, RGB565 alpha, pressed feedback, gesture exclusion and opacity cycle");
}
