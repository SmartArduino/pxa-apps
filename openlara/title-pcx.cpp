#include "title-pcx.h"

static unsigned le16(const uint8_t *p) {return unsigned(p[0])|(unsigned(p[1])<<8);}

bool ol_decode_title_pcx(const uint8_t *source, size_t bytes,
                         uint8_t *indices, unsigned width, unsigned height,
                         uint8_t palette_rgb565[512]) {
    if(!source || !indices || !palette_rgb565 || !width || !height ||
       bytes<128+769 || source[0]!=0x0a || source[2]!=1 || source[3]!=8 ||
       source[65]!=1 || source[bytes-769]!=12)return false;
    const unsigned xmin=le16(source+4),ymin=le16(source+6);
    const unsigned xmax=le16(source+8),ymax=le16(source+10);
    if(xmax<xmin || ymax<ymin)return false;
    const unsigned w=xmax-xmin+1,h=ymax-ymin+1,stride=le16(source+66);
    if(w>2048 || h>2048 || stride<w || stride>4096 || width>w || height>h)return false;
    size_t offset=128,end=bytes-769;
    unsigned run=0,value=0,out_y=0;
    for(unsigned y=0;y<h;++y) {
        const unsigned selected_y=(2*out_y+1)*h/(2*height);
        unsigned out_x=0;
        for(unsigned x=0;x<stride;++x) {
            if(!run) {
                if(offset>=end)return false;
                value=source[offset++];
                if((value&0xc0)==0xc0) {
                    run=value&63;
                    if(!run || offset>=end)return false;
                    value=source[offset++];
                } else run=1;
            }
            --run;
            if(out_y<height && y==selected_y && out_x<width &&
               x==(2*out_x+1)*w/(2*width)) {
                indices[out_y*width+out_x++]=uint8_t(value);
            }
        }
        if(y==selected_y) {
            if(out_x!=width)return false;
            ++out_y;
        }
    }
    if(out_y!=height || run)return false;
    const uint8_t *palette=source+bytes-768;
    for(unsigned i=0;i<256;++i) {
        const unsigned color=((palette[i*3]>>3)<<11)|((palette[i*3+1]>>2)<<5)|(palette[i*3+2]>>3);
        palette_rgb565[i*2]=uint8_t(color);
        palette_rgb565[i*2+1]=uint8_t(color>>8);
    }
    return true;
}
