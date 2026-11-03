/* SPDX-License-Identifier: MIT. Exercise the public FILE adapter with durable mock writes. */
#include "jy_fs.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#define TOTAL (JY_FS_MAX_WRITE_BYTES+37u)
static unsigned char disk[TOTAL],input[TOTAL],output[TOTAL];
static size_t bytes,max_request,calls;
static int exists,fail;
static int missing(void *c,const char *p,jy_fs_info *s){(void)c;(void)p;(void)s;return ENOENT;}
static int info(void *c,const char *p,jy_fs_info *s){(void)c;(void)p;if(!exists)return ENOENT;s->kind=JY_FS_FILE;s->size=bytes;return 0;}
static int resize_file(void *c,const char *p,uint64_t n){(void)c;(void)p;if(n>sizeof disk)return EFBIG;exists=1;bytes=(size_t)n;return 0;}
static int read_file(void *c,const char *p,uint64_t at,void *out,size_t n,size_t *got){(void)c;(void)p;*got=0;if(at>=bytes)return 0;if(n>bytes-at)n=bytes-(size_t)at;memcpy(out,disk+(size_t)at,n);*got=n;return 0;}
static int write_file(void *c,const char *p,uint64_t at,const void *in,size_t n,size_t *put){(void)c;(void)p;*put=0;calls++;if(n>max_request)max_request=n;if(fail)return ENOSPC;if(at>sizeof disk||n>sizeof disk-at)return EFBIG;memcpy(disk+(size_t)at,in,n);if(at+n>bytes)bytes=(size_t)(at+n);*put=n;return 0;}
int main(void){
 for(size_t i=0;i<TOTAL;i++)input[i]=(unsigned char)(i*29u+i/97u);
 for(unsigned wide=0;wide<2;wide++){
  jy_fs_ops base={.stat=missing,.read_at=read_file},overlay={.stat=info,.read_at=read_file,.write_at=write_file,.resize=resize_file,.max_write_bytes=wide?JY_FS_MAX_WRITE_BYTES:0};
  assert(!jy_fs_bind(&base,&overlay));max_request=calls=0;
  FILE *f=jy_fs_fopen("span.bin","wb");assert(f);assert(!setvbuf(f,NULL,_IONBF,0));
  assert(fwrite(input,1,TOTAL,f)==TOTAL&&!ferror(f));assert(!jy_fs_fclose(f));
  assert(bytes==TOTAL&&!memcmp(input,disk,TOTAL));assert(max_request==(wide?JY_FS_MAX_WRITE_BYTES:JY_FS_CHUNK_BYTES));
  printf("wide=%u callbacks=%zu maximum=%zu\n",wide,calls,max_request);
  f=jy_fs_fopen("span.bin","rb");assert(f);assert(fread(output,1,TOTAL,f)==TOTAL&&!memcmp(input,output,TOTAL));assert(!jy_fs_fclose(f));
  f=jy_fs_fopen("span.bin","r+b");assert(f);assert(!setvbuf(f,NULL,_IONBF,0));fail=1;size_t wrote=fwrite(input,1,TOTAL,f);assert(wrote!=TOTAL||ferror(f));fail=0;(void)jy_fs_fclose(f);jy_fs_reset();
 }
 puts("PASS default and 128KiB FILE spans, exact bytes, close/readback and write-error propagation");return 0;
}
