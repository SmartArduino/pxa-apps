#include "pcx_stream.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t file[700000],raw[640*480],pixels[640*480+16];
static uint32_t length,fail_at;static unsigned calls,maximum;
static int read_bytes(void *ctx,uint32_t at,void *out,uint32_t n){
  (void)ctx;assert(at<=length && n<=length-at);++calls;if(n>maximum)maximum=n;
  if(fail_at && at<=fail_at && fail_at<at+n)return 0;
  memcpy(out,file+at,n);return 1;
}
static void little(uint8_t *p,unsigned n){p[0]=n;p[1]=n>>8;}
static void make(unsigned w,unsigned h,unsigned stride,unsigned seed){
  memset(file,0,128);file[0]=10;file[1]=5;file[2]=1;file[3]=8;file[65]=1;
  little(file+8,w-1);little(file+10,h-1);little(file+66,stride);
  unsigned n=stride*h,p=0;length=128;
  while(p<n){unsigned count=(p*19+seed)%63+1;if(count>n-p)count=n-p;
    uint8_t value=(p*71+seed*53);memset(raw+p,value,count);p+=count;
    if(count>1 || value>=192){file[length++]=192|count;file[length++]=value;}
    else file[length++]=value;
  }
  file[length++]=12;for(unsigned i=0;i<768;++i)file[length++]=(uint8_t)(i*29);
}
int main(void){
  for(unsigned seed=0;seed<80;++seed){
    unsigned w=seed%3==0?640:seed%3==1?37:1,h=seed%3==0?480:seed%3==1?19:1;
    make(w,h,w+(w%2),seed);fail_at=0;calls=maximum=0;
    port_pcx_source_t source={NULL,length,read_bytes};port_pcx_info_t info;
    assert(port_pcx_header(&source,&info));assert(info.width==w && info.height==h);
    memset(pixels,0x5a,sizeof(pixels));
    assert(port_pcx_pixels(&source,&info,pixels,w*h));
    for(unsigned y=0;y<h;++y)assert(!memcmp(pixels+y*w,raw+y*info.stride,w));
    for(unsigned i=0;i<16;++i)assert(pixels[w*h+i]==0x5a);
    uint8_t palette[768];assert(port_pcx_palette(&source,palette));
    for(unsigned i=0;i<768;++i)assert(palette[i]==(uint8_t)(i*29));
    assert(maximum<=4096);
    assert(!port_pcx_pixels(&source,&info,pixels,w*h-1));
    fail_at=128;assert(!port_pcx_pixels(&source,&info,pixels,w*h));fail_at=0;
    source.bytes=length-1;assert(!port_pcx_header(&source,&info));
  }
  make(1,1,1,0);port_pcx_source_t source={NULL,length,read_bytes};port_pcx_info_t info;
  assert(port_pcx_header(&source,&info));file[128]=192;assert(!port_pcx_pixels(&source,&info,pixels,sizeof(pixels)));
  file[128]=255;assert(!port_pcx_pixels(&source,&info,pixels,sizeof(pixels)));
  info.data_end=128;assert(!port_pcx_pixels(&source,&info,pixels,sizeof(pixels)));
  file[65]=3;assert(!port_pcx_header(&source,&info));
  puts("PASS: PCX pixels/padding/palette, 4096-byte boundaries, bounds, malformed RLE and I/O failure");
}
