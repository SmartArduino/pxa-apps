// SPDX-License-Identifier: CC0-1.0
#include "../../common/pocket-play/runtime.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
namespace test_runtime {
uint16_t pixels[pocket::WIDTH*pocket::HEIGHT];
bool drawing=false; uint32_t clock_ms=0; unsigned saves=0,redraws=0,tones=0;
void image(const char *path) { FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n288 224\n255\n");for(auto p:pixels){uint8_t c[3]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};assert(fwrite(c,1,3,f)==3);}assert(fclose(f)==0); }
}
namespace pocket {
// Use the same UTF-8 metrics and glyphs as the production runtime.
void pixel(int x,int y,uint16_t c) { assert(test_runtime::drawing);if(x>=0&&y>=0&&x<WIDTH&&y<HEIGHT)test_runtime::pixels[y*WIDTH+x]=c; }
void rect(int x,int y,int w,int h,uint16_t c) { assert(test_runtime::drawing);assert(w>=0&&h>=0);for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)pixel(xx,yy,c); }
void clear(uint16_t c) { rect(0,0,WIDTH,HEIGHT,c); }
void outline(int x,int y,int w,int h,uint16_t c) { rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c); }
void line(int x,int y,int xx,int yy,uint16_t c) { int dx=xx>x?xx-x:x-xx,dy=yy>y?y-yy:yy-y,sx=x<xx?1:-1,sy=y<yy?1:-1,e=dx+dy;for(;;){pixel(x,y,c);if(x==xx&&y==yy)break;int e2=2*e;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}} }
int text_width(const char *v,int z) { return pocket_ui::text_width(v,z); }
void text(int x,int y,const char *v,uint16_t c,int z) {
 assert(test_runtime::drawing);int height=7;
 for(const char *p=v;*p;){uint32_t cp=pocket_ui::next_utf8(p);assert(cp!=0xfffd);if(cp>=128)height=10;}
 if(x<0||y<0||x+text_width(v,z)>WIDTH||y+height*z>HEIGHT){fprintf(stderr,"TEXT BOUNDS %d %d %s\n",x,y,v);assert(false);}
 pocket_ui::draw_text(x,y,v,z,[&](int xx,int yy){pixel(xx,yy,c);});
}
void redraw() { ++test_runtime::redraws; } void save() { ++test_runtime::saves; }
uint32_t millis() { return test_runtime::clock_ms; } uint32_t random() { return 12345; }
void log(const char *m) { puts(m); } bool held(Key) { return false; }
uint16_t *framebuffer() { return test_runtime::drawing?test_runtime::pixels:nullptr; }
void tone(float f,float d,float w) { assert(f>=30&&f<=6000&&d>0&&d<=4&&w>=0);++test_runtime::tones; }void silence() {}
}
