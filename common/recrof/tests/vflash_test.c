#include "vflash.h"
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef PORT_TEST_QUAKE_BYTES
#include "extMemory.h"
#endif
#define PAGE 8192u
#define VIRTUAL_SIZE (1024u*1024u)
#define NVM_SIZE (4u*PAGE)
static char directory[]="/tmp/pxa-vflash-XXXXXX";
static jmp_buf errors;
static int expect_error;
static unsigned cooperative_yields;
static int fail_replace;
static unsigned replace_count;
int port_replace(const char *a,const char *b){++replace_count;return fail_replace ? -1 : rename(a,b);}
uint64_t port_time_us(void){static uint64_t now;return ++now;}
int port_cooperate_due(uint64_t now,uint32_t budget){(void)now;assert(budget==8000);return 1;}
void port_sleep(unsigned ms){assert(ms==0);++cooperative_yields;}
const char *port_data_path(void){return directory;}
void port_error(const char *s){if(expect_error)longjmp(errors,1);fprintf(stderr,"%s\n",s);abort();}
static long size(const char *path){struct stat s;assert(!stat(path,&s));return s.st_size;}
#ifdef PORT_VFLASH_PAGED_NVM
static void clear_pages(void) {
  char path[320];
  for(unsigned i=0;i<4;++i) {
    snprintf(path,sizeof(path),"%s/nvm-%u/%03u",directory,NVM_SIZE,i);
    unlink(path);
  }
  snprintf(path,sizeof(path),"%s/nvm-%u/pending",directory,NVM_SIZE);unlink(path);
  snprintf(path,sizeof(path),"%s/nvm-%u",directory,NVM_SIZE);assert(!rmdir(path));
}
#endif
int main(void){
  assert(mkdtemp(directory));
  char data_path[256],save_path[256];
  snprintf(data_path,sizeof(data_path),"%s/data.pak",directory);
  snprintf(save_path,sizeof(save_path),"%s/state.nvm",directory);
  uint8_t data[PAGE*2],expected[NVM_SIZE],actual[NVM_SIZE],write[12000];
  for(unsigned i=0;i<sizeof(data);++i)data[i]=i*13+(i/PAGE)*31;
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
#ifdef PORT_VFLASH_PAGED_NVM
  unsigned before_batch=replace_count;
  vflash_begin_batch();vflash_begin_batch();
  for(unsigned i=0;i<200;++i) {
    uint32_t value=0x12340000u+i;
    vflash_write(VIRTUAL_SIZE-NVM_SIZE+i*4,&value,4,0);
    memcpy(expected+i*4,&value,4);
  }
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
  assert(!memcmp(expected,actual,NVM_SIZE));
  vflash_end_batch();assert(replace_count==before_batch);
  vflash_end_batch();assert(replace_count==before_batch+1);
#endif
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
#ifdef PORT_VFLASH_PAGED_NVM
  assert(size(save_path)==32); // Migration source never receives random writes.
  unsigned published=replace_count;
  vflash_write(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE,0);
  assert(replace_count==published); // Programming unchanged bits performs no writes.
  for(unsigned i=0;i<3;++i){
    char path[320];snprintf(path,sizeof(path),"%s/nvm-%u/%03u",directory,NVM_SIZE,i);
    assert(size(path)==PAGE+16);
  }
  /* A failed publication leaves the complete preceding page readable, even
   * after reopen. An abandoned pending file is never interpreted as a page. */
  expect_error=1;fail_replace=1;
  if(!setjmp(errors)) {vflash_write(VIRTUAL_SIZE-NVM_SIZE,NULL,PAGE,1);assert(!"failed publish accepted");}
  expect_error=0;fail_replace=0;
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);assert(!memcmp(expected,actual,NVM_SIZE));
  clear_pages();
#endif
  f=fopen(save_path,"wb");memset(expected,255,NVM_SIZE);expected[PAGE+10]=0x55;
  assert(fwrite(expected,1,NVM_SIZE,f)==NVM_SIZE);fclose(f);
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
  assert(!memcmp(expected,actual,NVM_SIZE)); // Legacy raw saves stay readable.
  vflash_write(VIRTUAL_SIZE-NVM_SIZE+PAGE+10,write,1,0);
  expected[PAGE+10]&=write[0];
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);
  assert(!memcmp(expected,actual,NVM_SIZE));assert(size(save_path)==NVM_SIZE);
#ifdef PORT_VFLASH_PAGED_NVM
  /* New page overlays survive restart while the legacy source stays intact. */
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);assert(!memcmp(expected,actual,NVM_SIZE));
  clear_pages();
  /* Construct a nonempty NVM2 source and migrate a changed page from it. */
  f=fopen(save_path,"wb");uint32_t header[4],offsets[4]={32,0,0,0};
  memcpy(header,"PXA-NVM2",8);header[2]=NVM_SIZE;header[3]=PAGE;
  memset(expected,255,NVM_SIZE);expected[20]=0x77;
  assert(fwrite(header,1,16,f)==16 && fwrite(offsets,4,4,f)==4);
  assert(fwrite(expected,1,PAGE,f)==PAGE);fclose(f);
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);assert(!memcmp(expected,actual,NVM_SIZE));
  vflash_write(VIRTUAL_SIZE-NVM_SIZE+20,write,1,0);expected[20]&=write[0];
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);
  vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,NVM_SIZE);assert(!memcmp(expected,actual,NVM_SIZE));
  assert(size(save_path)==32+PAGE);
  char bad[320];snprintf(bad,sizeof(bad),"%s/nvm-%u/000",directory,NVM_SIZE);
  f=fopen(bad,"wb");fputs("invalid",f);fclose(f);
  vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);expect_error=1;
  if(!setjmp(errors)){vflash_read(VIRTUAL_SIZE-NVM_SIZE,actual,1);assert(!"corrupt page accepted");}
  expect_error=0;clear_pages();
#endif
  f=fopen(save_path,"wb");fputs("invalid",f);fclose(f);
  expect_error=1;
  if(!setjmp(errors)){vflash_init(data_path,VIRTUAL_SIZE,NVM_SIZE);assert(!"corrupt save accepted");}
#ifdef PORT_TEST_QUAKE_BYTES
  /* Sequential bytes, page changes and intervening bulk reads must agree.
   * In particular, bulk cache eviction must never leave a stale byte view. */
  for(unsigned i=0;i<sizeof(data);++i)
    assert(extMemGetByteFromAddress((const void *)(uintptr_t)(4+i))==data[i]);
  for(unsigned i=0;i<32;++i) {
    unsigned at=i*37;
    assert(extMemGetByteFromAddress((const void *)(uintptr_t)(4+at))==data[at]);
    vflash_read(4+PAGE,actual,PAGE);
    assert(extMemGetByteFromAddress((const void *)(uintptr_t)(5+at))==data[at+1]);
  }
  expect_error=1;
  if(!setjmp(errors)) {
    extMemGetByteFromAddress((const void *)(uintptr_t)(4+sizeof(data)));
    assert(!"byte read outside archive accepted");
  }
  expect_error=0;
#endif
  unlink(data_path);unlink(save_path);rmdir(directory);
  puts("PASS: compact/legacy NOR, erased pages, cross-page AND, reopen, bounded storage and corrupt header");
}
