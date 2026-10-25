// SPDX-License-Identifier: MIT
// Native-only runtime: a clipped framebuffer, deterministic input and call counters.
#include "../../common/pocket-play/runtime.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
namespace test_runtime {
uint16_t pixels[pocket::WIDTH*pocket::HEIGHT];
bool keys[12] = {};
uint32_t clock_ms = 0;
unsigned saves = 0, redraws = 0, logs = 0, music_tones = 0;
bool drawing = false;
void image(const char *path) {
    FILE *f = fopen(path,"wb"); assert(f);
    fprintf(f,"P6\n%d %d\n255\n",pocket::WIDTH,pocket::HEIGHT);
    for(uint16_t p:pixels) { unsigned char b[3]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)}; assert(fwrite(b,1,3,f)==3); }
    fclose(f);
}
}
namespace pocket {
// Original small glyphs for readable native visual evidence; production uses the
// shared runtime's 5x7 font. Each entry is a row-major 5-bit bitmap.
static const uint8_t glyphs[][7]={
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},
{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{15,16,16,16,16,16,15},{30,17,17,17,17,17,30},
{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{15,16,16,19,17,17,15},{17,17,17,31,17,17,17},
{14,4,4,4,4,4,14},{1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
void pixel(int x,int y,uint16_t c) { assert(test_runtime::drawing); if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT) test_runtime::pixels[y*WIDTH+x]=c; }
void rect(int x,int y,int w,int h,uint16_t c) { assert(test_runtime::drawing); for(int yy=y;yy<y+h;++yy) for(int xx=x;xx<x+w;++xx) pixel(xx,yy,c); }
void clear(uint16_t c) { rect(0,0,WIDTH,HEIGHT,c); }
void outline(int x,int y,int w,int h,uint16_t c) { rect(x,y,w,1,c); rect(x,y+h-1,w,1,c); rect(x,y,1,h,c); rect(x+w-1,y,1,h,c); }
void line(int x0,int y0,int x1,int y1,uint16_t c) {
    int dx=x1>x0?x1-x0:x0-x1, dy=y1>y0?y0-y1:y1-y0, sx=x0<x1?1:-1, sy=y0<y1?1:-1, err=dx+dy;
    for(;;) { pixel(x0,y0,c); if(x0==x1&&y0==y1) break; int e=2*err; if(e>=dy) { err+=dy; x0+=sx; } if(e<=dx) { err+=dx; y0+=sy; } }
}
int text_width(const char *v,int scale) { return pocket_ui::text_width(v,scale); }
void text(int x,int y,const char *v,uint16_t color,int scale) {
    assert(test_runtime::drawing);
    if(!v)return;
    const int w=text_width(v,scale);
    const char *scan=v;bool cjk=false;while(*scan)if(pocket_ui::next_utf8(scan)>=0x80)cjk=true;
    if(x<0||y<0||x+w>WIDTH||y+(cjk?10:7)*scale>HEIGHT) {
        fprintf(stderr,"text out of bounds: %s at %d,%d width=%d scale=%d\n",v,x,y,w,scale);assert(false);
    }
    assert(!(y<20&&x+w>264));
    pocket_ui::draw_text(x,y,v,scale,[=](int px,int py){pixel(px,py,color);});
}
void redraw() { ++test_runtime::redraws; }
void save() { ++test_runtime::saves; }
uint32_t random() { return 0x12345678; }
uint32_t millis() { return test_runtime::clock_ms; }
void log(const char *s) { ++test_runtime::logs; puts(s); }
bool held(Key k) { return test_runtime::keys[unsigned(k)]; }
uint16_t *framebuffer() { return test_runtime::drawing?test_runtime::pixels:nullptr; }
void tone(float f,float d,float when) {
    assert(f>0&&d>0&&d<=4&&when>=0);
    if(when>float(millis())/1000.f+.035f) ++test_runtime::music_tones;
}
void silence() {}
}
