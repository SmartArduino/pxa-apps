// SPDX-License-Identifier: CC0-1.0
#include "../../common/pocket-play/runtime.hpp"
#include "../core.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
namespace test_runtime {
uint16_t pixels[pocket::WIDTH*pocket::HEIGHT];
bool drawing=false;unsigned saves=0,redraws=0,logs=0,tones=0,silences=0;
uint64_t monotonic=100000,wall=1792771200;
uint32_t foreground=0;
}
namespace grove {
uint64_t wall_seconds(){return test_runtime::wall;}
uint64_t monotonic_ms(){return test_runtime::monotonic;}
}
namespace pocket {
void pixel(int x,int y,uint16_t c){assert(test_runtime::drawing);if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT)test_runtime::pixels[y*WIDTH+x]=c;}
void rect(int x,int y,int w,int h,uint16_t c){assert(w>=0&&h>=0);assert(w<=WIDTH+32&&h<=HEIGHT+32);for(int yy=y;yy<y+h;++yy)for(int xx=x;xx<x+w;++xx)pixel(xx,yy,c);}
void clear(uint16_t c){rect(0,0,WIDTH,HEIGHT,c);}
void outline(int x,int y,int w,int h,uint16_t c){rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c);}
void line(int x,int y,int x1,int y1,uint16_t c){int dx=x<x1?x1-x:x-x1,dy=y<y1?y1-y:y-y1,sx=x<x1?1:-1,sy=y<y1?1:-1,e=dx-dy;for(;;){pixel(x,y,c);if(x==x1&&y==y1)break;int n=e*2;if(n>-dy){e-=dy;x+=sx;}if(n<dx){e+=dx;y+=sy;}}}
int text_width(const char *p,int scale){return pocket_ui::text_width(p,scale);}
void text(int x,int y,const char *p,uint16_t c,int scale){
    assert(test_runtime::drawing);int height=7;
    for(const char *q=p;*q;){uint32_t cp=pocket_ui::next_utf8(q);assert(cp!=0xfffd);if(cp>=128)height=10;}
    if(x<0||y<0||x+text_width(p,scale)>WIDTH||y+height*scale>HEIGHT){fprintf(stderr,"text bounds %d,%d %s\n",x,y,p);assert(false);}
    pocket_ui::draw_text(x,y,p,scale,[&](int xx,int yy){pixel(xx,yy,c);});
}
void redraw(){++test_runtime::redraws;}
void save(){++test_runtime::saves;}
uint32_t random(){return 0x92648211;}
uint32_t millis(){return test_runtime::foreground;}
void log(const char *p){++test_runtime::logs;puts(p);}
bool held(Key){return false;}
uint16_t *framebuffer(){return test_runtime::drawing?test_runtime::pixels:nullptr;}
void tone(float f,float d,float w){assert(f>=30&&f<=6000&&d>0&&d<=4&&w>=0);++test_runtime::tones;}
void silence(){++test_runtime::silences;}
}
