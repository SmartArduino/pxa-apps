/* GPL-2.0-or-later; generated font subset remains SIL OFL-1.1. */
#include "ui_font.h"
#include "font8x8.h"
#include <string.h>
#include "ui_font_data.inc"
uint32_t gb_ui_next(const char **text) {
    const unsigned char *s=(const unsigned char*)*text;
    if(!*s)return 0;
    unsigned n;uint32_t value,minimum;
    if(*s<128){*text+=1;return *s;}
    if(*s>=0xc2 && *s<=0xdf){n=2;value=*s&31;minimum=128;}
    else if(*s>=0xe0 && *s<=0xef){n=3;value=*s&15;minimum=2048;}
    else if(*s>=0xf0 && *s<=0xf4){n=4;value=*s&7;minimum=65536;}
    else {*text+=1;return '?';}
    for(unsigned i=1;i<n;++i) {
        if(!s[i] || (s[i]&0xc0)!=0x80){*text+=1;return '?';}
        value=(value<<6)|(s[i]&63);
    }
    *text+=n;
    return value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)?'?':value;
}
unsigned gb_ui_glyph(uint32_t cp,uint16_t rows[16]) {
    memset(rows,0,16*sizeof(*rows));
    if(cp<128){for(unsigned y=0;y<8;++y)rows[y+4]=font8x8_basic[cp][y];return 8;}
    unsigned first=0,end=sizeof(ui_glyphs)/sizeof(ui_glyphs[0]);
    while(first<end){unsigned m=(first+end)/2;if(ui_glyphs[m].codepoint<cp)first=m+1;else end=m;}
    if(first<sizeof(ui_glyphs)/sizeof(ui_glyphs[0]) && ui_glyphs[first].codepoint==cp)memcpy(rows,ui_glyphs[first].rows,sizeof(ui_glyphs[first].rows));
    else {rows[1]=rows[14]=0x1fff;for(unsigned y=2;y<14;++y)rows[y]=0x1001;}
    return 14;
}
unsigned gb_ui_width(const char *text) {
    unsigned width=0;uint32_t cp;
    while((cp=gb_ui_next(&text)))width+=cp<128?8:14;
    return width;
}
void gb_ui_draw(uint16_t *out,unsigned width,unsigned height,int x,int y,
                 const char *text,uint16_t color,unsigned max_width) {
    unsigned used=0;uint32_t cp;uint16_t rows[16];
    while((cp=gb_ui_next(&text))) {
        unsigned advance=gb_ui_glyph(cp,rows);if(used+advance>max_width)break;
        for(unsigned yy=0;yy<16;++yy)if(y+(int)yy>=0 && y+yy<height)
            for(unsigned xx=0;xx<advance;++xx)if((rows[yy]&(1u<<xx)) && x+(int)xx>=0 && x+xx<width)
                out[(y+yy)*width+x+xx]=color;
        x+=advance;used+=advance;
    }
}
