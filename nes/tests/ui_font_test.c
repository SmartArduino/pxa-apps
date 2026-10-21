/* GPL-2.0-or-later: bounded UTF-8 decoding and clipped menu drawing. */
#include "ui_font.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    const char *s="A重装机兵";assert(nes_ui_next(&s)=='A');
    const unsigned expected[]={0x91cd,0x88c5,0x673a,0x5175};
    for(unsigned i=0;i<4;++i)assert(nes_ui_next(&s)==expected[i]);assert(!nes_ui_next(&s));
    assert(nes_ui_width("重装机兵")==56 && nes_ui_width("A B")==24);
    uint16_t one[16];assert(nes_ui_glyph(0x4e00,one)==14);
    /* 一 belongs near the middle of the em square, not at the top edge. */
    unsigned top=0,bottom=15;while(top<16 && !one[top])++top;
    while(bottom && !one[bottom])--bottom;
    assert(top>=5 && top<=9 && bottom<=11);
    const char bad[][6]={{(char)0xe0,0},{(char)0xe0,(char)0xa0,0},{(char)0xf0,0},{(char)0xc0,(char)0xaf,0},{(char)0xed,(char)0xa0,(char)0x80,0},{(char)0xf4,(char)0x90,(char)0x80,(char)0x80,0}};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i){s=bad[i];assert(nes_ui_next(&s)=='?');while(nes_ui_next(&s));}
    uint16_t pixels[32*24+2];for(unsigned i=0;i<sizeof(pixels)/2;++i)pixels[i]=0x1234;
    nes_ui_draw(pixels+1,32,24,-9,-4,"重装机兵",0xffff,56);
    nes_ui_draw(pixels+1,32,24,28,20,"开始",0xffff,56);
    assert(pixels[0]==0x1234 && pixels[32*24+1]==0x1234);
    memset(pixels+1,0,32*24*2);nes_ui_draw(pixels+1,32,24,0,0,"重",0xffff,13);
    for(unsigned i=1;i<=32*24;++i)assert(!pixels[i]);
    puts("PASS: Chinese decoding/width, truncated/invalid UTF-8, clipping, canaries and whole-glyph limits");
}
