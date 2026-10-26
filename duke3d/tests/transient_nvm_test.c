#include "transient_nvm.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t disk[1024],model[1024],a[32],b[64],c[32],d[32];
static const duke_transient_region_t regions[]={
  {128,sizeof(a),a},{256,sizeof(b),b},{640,sizeof(c),c},{800,sizeof(d),d}};
static jmp_buf failure;
static int expecting_failure;
static unsigned reads,writes;
void port_error(const char *s) {
  if(expecting_failure)longjmp(failure,1);
  fprintf(stderr,"%s\n",s);abort();
}
void vflash_read(uint32_t at,void *out,uint32_t n) {
  assert(at<=sizeof(disk) && n<=sizeof(disk)-at);++reads;memcpy(out,disk+at,n);
}
void vflash_write(uint32_t at,const void *in,uint32_t n,int erase) {
  assert(at<=sizeof(disk) && n<=sizeof(disk)-at);++writes;
  for(unsigned i=0;i<n;++i)disk[at+i]=erase?255:disk[at+i]&((const uint8_t*)in)[i];
}
static uint32_t rng=314159;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;return rng^=rng<<5;}
int main(void) {
  memset(disk,255,sizeof(disk));memset(model,255,sizeof(model));
  duke_transient_init(sizeof(disk),regions,4);
  uint8_t data[1024],actual[1024];
  for(unsigned test=0;test<3000;++test) {
    uint32_t at=random32()%1024,n=random32()%(1025-at);int erase=random32()%3==0;
    for(unsigned i=0;i<n;++i)data[i]=(uint8_t)random32();
    duke_transient_write(at|(test&1?0x90000000u:0),data,n,erase);
    for(unsigned i=0;i<n;++i)model[at+i]=erase?255:model[at+i]&data[i];
    duke_transient_read(0,actual,sizeof(actual));assert(!memcmp(actual,model,sizeof(model)));
  }
  unsigned before=writes;
  duke_transient_write(640,NULL,sizeof(c),1);
  memset(data,0x35,sizeof(c));duke_transient_write(640,data,sizeof(c),0);
  assert(writes==before);before=reads;duke_transient_read(640,actual,sizeof(c));
  assert(reads==before && !memcmp(actual,data,sizeof(c)));
  uint8_t saved_disk[1024];memcpy(saved_disk,disk,sizeof(disk));
  expecting_failure=1;
  if(!setjmp(failure)){duke_transient_write(1020,data,8,0);assert(!"overflow accepted");}
  expecting_failure=0;assert(!memcmp(saved_disk,disk,sizeof(disk)));
  duke_transient_init(sizeof(disk),regions,4);
  duke_transient_read(0,actual,sizeof(actual));
  for(unsigned i=0;i<sizeof(actual);++i) {
    int transient=0;
    for(unsigned j=0;j<4;++j)transient|=i>=regions[j].offset && i-regions[j].offset<regions[j].size;
    assert(actual[i]==(transient?255:disk[i]));
  }
  assert(!memcmp(saved_disk,disk,sizeof(disk)));
  puts("PASS: 3000 mixed NOR operations, tagged addresses, range crossings, no transient disk I/O, overflow rejection and durable data after restart");
}
