/* Compare direct output with the original two-stage sampling and controls. */
#include "framebuffer.h"
#include "font8x8.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint8_t source[672*480], cursor_pixels[33*25];
static uint16_t palette[256], logical[320*240], expected[320*240], actual[320*240+1];
static void reference(int dw,int dh,int sw,int sh,int pitch,const port_cursor_t *cursor,int controls) {
  for (int y=0;y<240;++y) for(int x=0;x<320;++x) {
    int sx=x*sw/320, sy=y*sh/240;
    uint8_t c=source[sy*pitch+sx];
    if(cursor && sx>=cursor->x && sx<cursor->x+cursor->width &&
       sy>=cursor->y && sy<cursor->y+cursor->height) {
      uint8_t over=cursor->pixels[(sy-cursor->y)*cursor->width+sx-cursor->x];
      if(over)c=over;
    }
    logical[y*320+x]=palette[c];
  }
  if (controls) {
    static const struct{int x,y,w,h;const char*label;} b[]={
      {0,0,38,23,"MENU"},{280,0,40,24,"WPN"},{276,116,44,26,"USE"},
      {276,148,44,28,"JMP"},{268,192,52,48,"FIRE"}};
    for(unsigned n=0;n<5;++n){
      for(int y=b[n].y;y<b[n].y+b[n].h;++y)
        for(int x=b[n].x;x<b[n].x+b[n].w;++x){
          uint16_t *c=&logical[y*320+x];
          *c=x==b[n].x||x==b[n].x+b[n].w-1||y==b[n].y||y==b[n].y+b[n].h-1?0xffff:(*c&0xf7de)>>1;
        }
      int tx=b[n].x+(b[n].w-(int)strlen(b[n].label)*8)/2,ty=b[n].y+(b[n].h-8)/2;
      for(const char *s=b[n].label;*s;++s,tx+=8)
        for(int y=0;y<8;++y)for(int x=0;x<8;++x)
          if(font8x8_basic[(unsigned)*s][y]&(1<<x))logical[(ty+y)*320+tx+x]=0xffff;
    }
  }
  for(int y=0;y<dh;++y)for(int x=0;x<dw;++x)
    expected[y*dw+x]=logical[(y*240/dh)*320+x*320/dw];
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void){
  for(unsigned i=0;i<sizeof(source);++i)source[i]=(i*31+i/313)%256;
  for(unsigned i=0;i<256;++i)palette[i]=(i*257)^0x5143;
  for(unsigned i=0;i<sizeof(cursor_pixels);++i)cursor_pixels[i]=i%7?i%255+1:0;
  const int cases[][5]={{320,240,320,240,320},{320,240,320,200,320},
    {296,222,320,240,336},{296,222,320,200,320},{296,222,640,400,672},
    {160,120,640,480,640},{1,1,320,240,320}};
  const int positions[][2]={{0,0},{-25,-20},{319,239},{100,73},{700,500}};
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);++i)
    for(unsigned j=0;j<sizeof(positions)/sizeof(*positions);++j)
      for(int controls=0;controls<2;++controls){
        int dw=cases[i][0],dh=cases[i][1],sw=cases[i][2],sh=cases[i][3],pitch=cases[i][4];
        port_cursor_t cursor={cursor_pixels,positions[j][0],positions[j][1],33,25};
        reference(dw,dh,sw,sh,pitch,&cursor,controls);
        actual[dw*dh]=0x1234;
        port_blit(actual,dw,dh,source,sw,sh,pitch,palette,&cursor);
        if(controls)port_draw_controls(actual,dw,dh,0);
        assert(actual[dw*dh]==0x1234);
        assert(!memcmp(expected,actual,(size_t)dw*dh*2));
      }
  puts("PASS: 70 sampling/cursor/control cases match original pixels");
  reference(320,240,320,240,320,NULL,1);
  actual[0]=0x5678;
  port_blit(actual+1,320,240,source,320,240,320,palette,NULL);
  port_draw_controls(actual+1,320,240,0);
  assert(actual[0]==0x5678 && !memcmp(expected,actual+1,320*240*2));
  puts("PASS: unaligned RGB565 destination retains identical output");
  const int iterations=500;
  double before=now();
  for(int i=0;i<iterations;++i){source[0]=i;reference(296,222,640,400,672,NULL,0);}
  double middle=now();
  for(int i=0;i<iterations;++i){source[0]=i;port_blit(actual,296,222,source,640,400,672,palette,NULL);}
  double after=now();
  assert(!memcmp(expected,actual,296*222*2));
  printf("{\"iterations\":%d,\"reference_us\":%.1f,\"direct_us\":%.1f,\"scope\":\"native CPU blit only\"}\n",
           iterations,(middle-before)*1e6/iterations,(after-middle)*1e6/iterations);
}
