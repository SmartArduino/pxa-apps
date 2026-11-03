/* SPDX-License-Identifier: MIT
 * Real block-store code against a host-file transport with short I/O, back
 * pressure and failure injection. This does not emulate the whole game. */
#define _GNU_SOURCE 1
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../pxa_fs_backend.c"
static int fail_write,fail_replace,block_once,yields;
void jy_suspend(uint64_t us){(void)us;yields++;}
static int32_t mapped_error(void){return errno==ENOENT?-5:errno==EEXIST?-2:-14;}
int32_t pxa_call(uint32_t route,uint64_t token,const uint8_t *data,uint32_t length){
 unsigned service=route&65535,op=route>>16;
 if(service==PXA_CORE_SERVICE){assert(op==PXA_CORE_CLOSE_HANDLE);return close((int)(pxa_load_u64(data)&0xffffffffu))?mapped_error():0;}
 assert(service==PXA_FS_SERVICE);
 char from[256]={0},to[256]={0};unsigned flags=0;size_t cursor=0;
 while(cursor<length){pxa_wire_record_view_t r;size_t n;assert(pxa_wire_record_decode(data+cursor,length-cursor,&r,&n));cursor+=n;
  if(r.tag==1){assert(r.payload_size<sizeof from);memcpy(from,r.payload,r.payload_size);}
  else if(r.tag==2){assert(r.payload_size<sizeof to);memcpy(to,r.payload,r.payload_size);}
  else if(r.tag==3)flags=pxa_load_u32(r.payload);
  else assert(0);
 }
 int32_t status=0;uint8_t reply[12]={0};uint32_t reply_n=4;
 if(op==PXA_FS_OPEN){
  int mode=(flags&PXA_FS_OPEN_WRITE)?((flags&PXA_FS_OPEN_READ)?O_RDWR:O_WRONLY):O_RDONLY;
  if(flags&PXA_FS_OPEN_CREATE)mode|=O_CREAT;
  if(flags&PXA_FS_OPEN_TRUNCATE)mode|=O_TRUNC;
  if(flags&PXA_FS_OPEN_DIRECTORY)mode|=O_DIRECTORY;
  int fd=open(from,mode,0600);if(fd<0)status=mapped_error();else{reply_n=12;pxa_store_u64(reply+4,UINT64_C(0x100000000)|(unsigned)fd);}
 }else if(op==PXA_FS_MAKE_DIRECTORY){if(mkdir(from,0700))status=mapped_error();}
 else if(op==PXA_FS_REMOVE){if(unlink(from))status=mapped_error();}
 else if(op==PXA_FS_REPLACE){if(fail_replace)status=-14;else if(rename(from,to))status=mapped_error();}
 else assert(0);
 pxa_store_u32(reply,(uint32_t)status);pxa_event_t event={PXA_FS_SERVICE,(uint16_t)op,token,reply,reply_n};assert(jy_pxa_fs_handle_event(&event));return 0;
}
int32_t pxa_io(uint64_t handle,uint32_t operation,uint8_t *data,uint32_t length){
 if(block_once){block_once=0;return PXA_STATUS_WOULD_BLOCK;}
 int fd=(int)(handle&0xffffffffu);ssize_t n;
 if(operation==PXA_FS_IO_WRITE){if(fail_write)return PXA_STATUS_QUOTA_EXCEEDED;if(length>113)length=113;n=write(fd,data,length);}
 else{assert(operation==PXA_FS_IO_READ);if(length>97)length=97;n=read(fd,data,length);}
 return n<0?mapped_error():(int32_t)n;
}
int main(int argc,char **argv){
 assert(argc==2&&!chdir(argv[1]));uint8_t original[2048],other[2048],out[4096];size_t n;
 /* Exercise the production low-memory compressor with structured and worst
  * case incompressible pages, then decode using the ordinary zlib reader. */
 static uint8_t decoded[GROUP];uint32_t rng=1;
 for(unsigned mode=0;mode<4;mode++){
  for(unsigned i=0;i<GROUP;i++){
   rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
   page.bytes[i]=mode==0?0:mode==1?(uint8_t)(i/257):mode==2?(uint8_t)rng:(i%97?0:(uint8_t)rng);
  }
  uLongf packed=0,decoded_bytes=GROUP;assert(compress_group(&packed)==Z_OK);
  assert(packed<=COMP_CAP&&deflate_used<=sizeof deflate_arena);
  assert(uncompress(decoded,&decoded_bytes,compressed,packed)==Z_OK);
  assert(decoded_bytes==GROUP&&!memcmp(decoded,page.bytes,GROUP));
  printf("compression pattern=%u arena=%zu packed=%lu\n",mode,deflate_used,(unsigned long)packed);
 }
 for(unsigned i=0;i<sizeof original;i++){original[i]=(uint8_t)(i*37+i/11);other[i]=(uint8_t)~original[i];}
 assert(get("absent",out,sizeof out,&n)==ENOENT);
 block_once=1;assert(!set("sample",original,sizeof original)&&yields==1);
 assert(!get("sample",out,sizeof out,&n)&&n==sizeof original&&!memcmp(out,original,n));
 fail_write=1;assert(set("sample",other,sizeof other)==ENOSPC);fail_write=0;
 assert(!get("sample",out,sizeof out,&n)&&n==sizeof original&&!memcmp(out,original,n));
 fail_replace=1;assert(set("sample",other,sizeof other)==EIO);fail_replace=0;
 assert(!get("sample",out,sizeof out,&n)&&n==sizeof original&&!memcmp(out,original,n));
 assert(!set("sample",other,sizeof other));files_ready=0;
 assert(!get("sample",out,sizeof out,&n)&&n==sizeof other&&!memcmp(out,other,n));
 for(unsigned i=0;i<40;i++){char key[32];snprintf(key,sizeof key,"data-%u",i);assert(!set(key,original,sizeof original));}
 for(unsigned i=0;i<40;i++){char key[32];snprintf(key,sizeof key,"data-%u",i);assert(!get(key,out,sizeof out,&n)&&n==2048&&!memcmp(out,original,n));}
 assert(!set("../escape",original,17));assert(access("../escape",F_OK));
 assert(!get("../escape",out,sizeof out,&n)&&n==17);
 assert(get("sample",out,7,&n)==EFBIG);assert(!del("sample"));assert(!del("sample"));
 assert(get("sample",out,sizeof out,&n)==ENOENT);
 /* Larger logical spans still use the real 2KiB object transport and must
  * survive a complete rebind of both metadata and compression caches. */
 assert(!jy_pxa_fs_bind());
 FILE *span=jy_fs_fopen("span-sample","wb");assert(span);assert(!setvbuf(span,NULL,_IONBF,0));
 assert(fwrite(decoded,1,sizeof decoded,span)==sizeof decoded&&!ferror(span));assert(!jy_fs_fclose(span));
 assert(!jy_pxa_fs_bind());span=jy_fs_fopen("span-sample","rb");assert(span);
 static uint8_t restored[GROUP];assert(fread(restored,1,sizeof restored,span)==sizeof restored&&!memcmp(decoded,restored,sizeof decoded));assert(!jy_fs_fclose(span));
 puts("PASS 2048-byte values, 40 keys/80 KiB, short I/O, WOULD_BLOCK, cold reopen, failed write/replace preservation, bounded paths and deletion");return 0;
}
