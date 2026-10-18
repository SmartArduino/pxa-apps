#include "vflash.h"
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define PAGE 8192u
#define VIRTUAL_SIZE (1024u*1024u)
#define NVM_SIZE (4u*PAGE)
static char directory[]="/tmp/pxa-vflash-XXXXXX";
static jmp_buf errors;
static int expect_error;
static unsigned cooperative_yields;
uint64_t port_time_us(void){static uint64_t now;return ++now;}
int port_cooperate_due(uint64_t now,uint32_t budget){(void)now;assert(budget==8000);return 1;}
void port_sleep(unsigned ms){assert(ms==0);++cooperative_yields;}
const char *port_data_path(void){return directory;}
void port_error(const char *s){if(expect_error)longjmp(errors,1);fprintf(stderr,"%s\n",s);abort();}
static long size(const char *path){struct stat s;assert(!stat(path,&s));return s.st_size;}
int main(void){
  assert(mkdtemp(directory));
  char data_path[256],save_path[256];
  snprintf(data_path,sizeof(data_path),"%s/data.pak",directory);
  snprintf(save_path,sizeof(save_path),"%s/state.nvm",directory);
  uint8_t data[PAGE*2],expected[NVM_SIZE],actual[NVM_SIZE],write[12000];
  for(unsigned i=0;i<sizeof(data);++i)data[i]=i*13;
  FILE *f=fopen(data_path,"wb");assert(fwrite(data,1,sizeof(data),f)==sizeof(data));fclose(f);
  memset(expected,255,sizeof(expected));
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
  assert(size(save_path)==16+4*4);
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
  assert(!memcmp(expected,actual,NVM_SIZE));
  vflash_read(4,actual,sizeof(data));assert(!memcmp(data,actual,sizeof(data)));
  vflash_read(4,actual,1);assert(actual[0]==data[0]);
  vflash_read(4,actual,1);assert(actual[0]==data[0]);
  vflash_stats_t stats;vflash_stats(&stats,1);
  assert(stats.misses==2 && stats.hits==2 && stats.read_us>0);
  assert(stats.cache_bytes>=2*PAGE);
#ifdef PORT_VFLASH_COOPERATE
  for(unsigned i=0;i<1024;++i){
    vflash_read(4+i%sizeof(data),actual,1);
    assert(actual[0]==data[i%sizeof(data)]);
  }
  assert(cooperative_yields==4);
#endif
  vflash_write(VIRTUAL_SIZE-NVM_SIZE,NULL,NVM_SIZE,1);
  assert(size(save_path)==32); // Erasing unwritten NOR allocates no pages.
  for(unsigned round=0;round<12;++round){
    for(unsigned i=0;i<sizeof(write);++i)write[i]=(i*7+round*31)&255;
    unsigned offset=round*997%(NVM_SIZE-sizeof(write));
    vflash_write(VIRTUAL_SIZE-NVM_SIZE+offset,write,sizeof(write),0);
    for(unsigned i=0;i<sizeof(write);++i)expected[offset+i]&=write[i];
    vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
    assert(!memcmp(expected,actual,NVM_SIZE));
    vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
    vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
    assert(!memcmp(expected,actual,NVM_SIZE));
  }
  long allocated=size(save_path);assert(allocated<=32+NVM_SIZE);
  for(int round=0;round<10;++round){
    vflash_write(VIRTUAL_SIZE-NVM_SIZE+PAGE-9,NULL,72,1);
    memset(expected+PAGE-9,255,72);
    vflash_write(VIRTUAL_SIZE-NVM_SIZE+PAGE-9,write,72,0);
    memcpy(expected+PAGE-9,write,72);
  }
  assert(size(save_path)==allocated); // Repeated erase/write reuses its slots.
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
  assert(!memcmp(expected,actual,NVM_SIZE));
  f=fopen(save_path,"wb");memset(expected,255,NVM_SIZE);expected[PAGE+10]=0x55;
  assert(fwrite(expected,1,NVM_SIZE,f)==NVM_SIZE);fclose(f);
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
  assert(!memcmp(expected,actual,NVM_SIZE)); // Legacy raw saves stay readable.
  vflash_write(VIRTUAL_SIZE-NVM_SIZE+PAGE+10,write,1,0);
  expected[PAGE+10]&=write[0];
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
  assert(!memcmp(expected,actual,NVM_SIZE));assert(size(save_path)==NVM_SIZE);
  f=fopen(save_path,"wb");fputs("invalid",f);fclose(f);
  expect_error=1;
  if(!setjmp(errors)){vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);assert(!"corrupt save accepted");}
  unlink(data_path);unlink(save_path);rmdir(directory);
  puts("PASS: compact/legacy NOR, erased pages, cross-page AND, reopen, bounded storage and corrupt header");
}
