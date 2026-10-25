#include "loading.h"
#include <assert.h>
#include <stdlib.h>
static void check(int w,int h){
  size_t bytes=(size_t)w*h*sizeof(uint16_t);
  uint16_t *p=malloc(bytes+4),*copy=malloc(bytes);assert(p&&copy);
  p[0]=0x1234;p[w*h+1]=0xabcd;
  port_draw_loading(p+1,w,h,"Command & Conquer","Opening game files",0);
  memcpy(copy,p+1,bytes);
  port_draw_loading(p+1,w,h,"Command & Conquer","Opening game files",1000000);
  if(w>=100 && h>=100)assert(memcmp(copy,p+1,bytes));
  assert(p[0]==0x1234 && p[w*h+1]==0xabcd);
  port_draw_loading(p+1,w,h,"Red Alert","Preparing main menu",UINT64_C(9999999999999));
  assert(p[0]==0x1234 && p[w*h+1]==0xabcd);
  free(copy);free(p);
}
int main(int argc,char **argv){
  check(320,240);check(296,222);check(160,120);check(8,6);check(1,1);
  port_draw_loading(NULL,320,240,"game","stage",0);
  if(argc==2){
    uint16_t frame[320*240];port_draw_loading(frame,320,240,"Command & Conquer","Loading game artwork",12300000);
    FILE *f=fopen(argv[1],"wb");assert(f);fputs("P6\n320 240\n255\n",f);
    for(unsigned i=0;i<320*240;++i){unsigned c=frame[i];unsigned char rgb[]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}
    assert(!fclose(f));
  }
  puts("PASS: visible startup text, animated progress, time and scaled framebuffer bounds");
}
