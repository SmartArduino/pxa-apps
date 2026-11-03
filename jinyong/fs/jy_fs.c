/* SPDX-License-Identifier: MIT */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "jy_fs.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__GLIBC__)
typedef off64_t jy_cookie_offset;
#else
typedef off_t jy_cookie_offset;
#endif
typedef struct jy_cookie {
 struct jy_cookie *next;
 FILE *stream;
 char path[JY_FS_PATH_MAX+1];
 uint64_t position,cache_offset;
 size_t cache_length;
 unsigned char cache[JY_FS_CHUNK_BYTES];
 unsigned overlay:1,readable:1,writable:1,append:1,temporary:1;
} jy_cookie;
typedef struct jy_whiteout { struct jy_whiteout *next; char path[JY_FS_PATH_MAX+1]; } jy_whiteout;
static jy_fs_ops base_ops,write_ops;
static jy_cookie *files;
static jy_whiteout *whiteouts;
static FILE *standard_override[3];
static uint32_t temporary_counter;
static jy_fs_stats stats;
static int fail(int error){errno=error>0?error:EIO;return -1;}
static int error_code(int error){return error>0?error:EIO;}
static ssize_t write_failure(size_t done){
 if(done)return (ssize_t)done;
#if defined(__GLIBC__)
 /* glibc marks a short cookie write as F_ERR; negative results can produce
  * a misleading fwrite count. wasi-libc/musl instead requires a negative
  * cookie result to mark F_ERR. Preserve both real libc contracts. */
 return 0;
#else
 return -1;
#endif
}
int jy_fs_normalize(const char *input,char output[JY_FS_PATH_MAX+1]){
 if(!input||!output)return fail(EINVAL);
 if(*input=='/'||*input=='\\')return fail(EACCES);
 size_t used=0;
 while(*input){
  while(*input=='/'||*input=='\\')++input;
  if(!*input)break;
  const char *start=input;size_t n=0;
  while(*input&&*input!='/'&&*input!='\\'){
   unsigned char c=(unsigned char)*input;if(c<32||c==127||c==':')return fail(EACCES);++n;++input;
  }
  if(n==1&&start[0]=='.')continue;
  if(n==2&&start[0]=='.'&&start[1]=='.')return fail(EACCES);
  if(used+(used?1:0)+n>JY_FS_PATH_MAX)return fail(ENAMETOOLONG);
  if(used)output[used++]='/';
  memcpy(output+used,start,n);used+=n;
 }
 output[used]=0;return 0;
}
static int hidden(const char *p){for(jy_whiteout *w=whiteouts;w;w=w->next)if(!strcmp(w->path,p))return 1;return 0;}
static int hide(const char *p){
 if(hidden(p))return 0;
 jy_whiteout *w=malloc(sizeof *w);if(!w)return ENOMEM;
 strcpy(w->path,p);w->next=whiteouts;whiteouts=w;return 0;
}
static void reveal(const char *p){
 jy_whiteout **where=&whiteouts;
 while(*where){if(!strcmp((*where)->path,p)){jy_whiteout *w=*where;*where=w->next;free(w);return;}where=&(*where)->next;}
}
static int call_stat(const jy_fs_ops *ops,const char *p,jy_fs_info *info){
 if(!ops->stat)return ENOENT;
 memset(info,0,sizeof *info);int e=ops->stat(ops->context,p,info);
 if(e)return error_code(e);
 if(info->size>INT64_MAX||(info->kind!=JY_FS_FILE&&info->kind!=JY_FS_DIRECTORY))return EIO;
 return 0;
}
static int locate(const char *p,jy_fs_info *info,int *overlay){
 if(hidden(p))return ENOENT;
 int e=call_stat(&write_ops,p,info);
 if(!e){*overlay=1;return 0;}if(e!=ENOENT)return e;
 e=call_stat(&base_ops,p,info);if(!e)*overlay=0;return e;
}
int jy_fs_info_for(const char *path,jy_fs_info *info){char p[JY_FS_PATH_MAX+1];int layer;if(!info)return fail(EINVAL);if(jy_fs_normalize(path,p))return -1;int e=locate(p,info,&layer);return e?fail(e):0;}
static int read_at(const jy_fs_ops *ops,int overlay,const char *p,uint64_t off,void *dst,size_t n,size_t *got){
 *got=0;if(!ops->read_at)return ENOSYS;if(n>JY_FS_CHUNK_BYTES)return EINVAL;
 if(n>stats.max_callback_request)stats.max_callback_request=(uint32_t)n;
 int e=ops->read_at(ops->context,p,off,dst,n,got);if(e)return error_code(e);if(*got>n)return EIO;
 if(overlay){++stats.overlay_read_calls;stats.overlay_read_bytes+=*got;}else{++stats.base_read_calls;stats.base_read_bytes+=*got;}return 0;
}
static size_t write_limit(void){size_t n=write_ops.max_write_bytes?write_ops.max_write_bytes:JY_FS_CHUNK_BYTES;return n>JY_FS_MAX_WRITE_BYTES?JY_FS_MAX_WRITE_BYTES:n;}
static int write_at(const char *p,uint64_t off,const void *src,size_t n,size_t *put){
 *put=0;if(!write_ops.write_at)return EROFS;if(n>write_limit())return EINVAL;
 if(n>stats.max_callback_request)stats.max_callback_request=(uint32_t)n;
 int e=write_ops.write_at(write_ops.context,p,off,src,n,put);if(e)return error_code(e);if(*put>n)return EIO;
 ++stats.overlay_write_calls;stats.overlay_write_bytes+=*put;return 0;
}
static void invalidate(const char *p){for(jy_cookie *f=files;f;f=f->next)if(!strcmp(f->path,p))f->cache_length=0;}
static int copy_file(const jy_fs_ops *source,int source_overlay,const char *from,const char *to,uint64_t size){
 unsigned char buffer[JY_FS_CHUNK_BYTES];
 if(!write_ops.resize||!write_ops.write_at)return EROFS;
 int e=write_ops.resize(write_ops.context,to,0);if(e)return error_code(e);
 for(uint64_t off=0;off<size;){
  size_t want=(size-off>sizeof buffer)?sizeof buffer:(size_t)(size-off),got=0;
  e=read_at(source,source_overlay,from,off,buffer,want,&got);if(e||!got){e=e?e:EIO;break;}
  size_t done=0;
  while(done<got){size_t put=0;e=write_at(to,off+done,buffer+done,got-done,&put);if(e||!put){e=e?e:EIO;break;}done+=put;}
  if(e)break;
  off+=got;stats.copy_on_write_bytes+=got;
 }
 if(e){if(write_ops.remove)write_ops.remove(write_ops.context,to);return e;}return 0;
}
static int materialize(const char *p,int truncate){
 if(!write_ops.resize||!write_ops.write_at)return EROFS;
 jy_fs_info info;int e=call_stat(&write_ops,p,&info);
 if(!e&&!hidden(p)){if(info.kind!=JY_FS_FILE)return EISDIR;if(!truncate)return 0;}
 else if(e!=ENOENT&&e)return e;
 if(!truncate&&!hidden(p)){
  e=call_stat(&base_ops,p,&info);
  if(!e){if(info.kind!=JY_FS_FILE)return EISDIR;
   if(write_ops.clone_base){e=write_ops.clone_base(write_ops.context,p,p);if(e)return error_code(e);reveal(p);invalidate(p);return 0;}
   e=copy_file(&base_ops,0,p,p,info.size);if(e)return e;reveal(p);return 0;}
  if(e!=ENOENT)return e;
 }
 e=write_ops.resize(write_ops.context,p,0);if(e)return error_code(e);reveal(p);invalidate(p);return 0;
}
static ssize_t cookie_read(void *opaque,char *dst,size_t length){
 jy_cookie *f=opaque;if(!f->readable){errno=EBADF;return -1;}
 size_t done=0;const jy_fs_ops *ops=f->overlay?&write_ops:&base_ops;
 while(done<length){
  if(!(f->cache_length&&f->position>=f->cache_offset&&f->position-f->cache_offset<f->cache_length)){
   jy_fs_info info;int e=call_stat(ops,f->path,&info);if(e){errno=e;return done?(ssize_t)done:-1;}
   if(f->position>=info.size)break;
   f->cache_offset=f->position;
   size_t want=(info.size-f->cache_offset>JY_FS_CHUNK_BYTES)?JY_FS_CHUNK_BYTES:(size_t)(info.size-f->cache_offset);
   e=read_at(ops,f->overlay,f->path,f->cache_offset,f->cache,want,&f->cache_length);
   if(e||!f->cache_length){errno=e?e:EIO;return done?(ssize_t)done:-1;}
   if(f->position-f->cache_offset>=f->cache_length){errno=EIO;return done?(ssize_t)done:-1;}
  }else ++stats.cache_hits;
  size_t within=(size_t)(f->position-f->cache_offset),n=f->cache_length-within;
  if(n>length-done)n=length-done;
  memcpy(dst+done,f->cache+within,n);done+=n;f->position+=n;
 }
 return (ssize_t)done;
}
static ssize_t cookie_write(void *opaque,const char *src,size_t length){
 jy_cookie *f=opaque;if(!f->writable){errno=EBADF;return write_failure(0);}
 if(f->append){jy_fs_info info;int e=call_stat(&write_ops,f->path,&info);if(e){errno=e;return write_failure(0);}f->position=info.size;}
 if(length>(uint64_t)INT64_MAX-f->position){errno=EFBIG;return write_failure(0);}
 size_t done=0;invalidate(f->path);
 while(done<length){size_t n=length-done,limit=write_limit();if(n>limit)n=limit;size_t put=0;int e=write_at(f->path,f->position,src+done,n,&put);if(e||!put){errno=e?e:EIO;return write_failure(done);}done+=put;f->position+=put;}
 return (ssize_t)done;
}
static int cookie_seek(void *opaque,jy_cookie_offset *offset,int whence){
 jy_cookie *f=opaque;uint64_t origin=0;
 if(whence==SEEK_CUR)origin=f->position;
 else if(whence==SEEK_END){jy_fs_info info;int e=call_stat(f->overlay?&write_ops:&base_ops,f->path,&info);if(e)return fail(e);origin=info.size;}
 else if(whence!=SEEK_SET)return fail(EINVAL);
 int64_t delta=(int64_t)*offset;
 if((delta<0&&(uint64_t)(-(delta+1))+1>origin)||(delta>=0&&(uint64_t)delta>(uint64_t)INT64_MAX-origin))return fail(EINVAL);
 f->position=delta<0?origin-((uint64_t)(-(delta+1))+1):origin+(uint64_t)delta;*offset=(jy_cookie_offset)f->position;return 0;
}
static int cookie_close(void *opaque){
 jy_cookie *f=opaque;jy_cookie **where=&files;while(*where&&*where!=f)where=&(*where)->next;if(*where)*where=f->next;
 for(unsigned n=0;n<3;++n)if(standard_override[n]==f->stream)standard_override[n]=NULL;
 if(stats.open_files)--stats.open_files;
 int e=0;
 if(f->temporary&&write_ops.remove)e=write_ops.remove(write_ops.context,f->path);
 free(f);return e?fail(e):0;
}
static jy_cookie *owned(FILE *stream){for(jy_cookie *f=files;f;f=f->next)if(f->stream==stream)return f;return NULL;}
FILE *jy_fs_fopen(const char *path,const char *mode){
 char p[JY_FS_PATH_MAX+1];if(jy_fs_normalize(path,p))return NULL;
 if(!mode||!strchr("rwa",mode[0])){errno=EINVAL;return NULL;}
 int plus=0,exclusive=0;for(const char *m=mode+1;*m;++m){if(*m=='+')plus=1;else if(*m=='x')exclusive=1;else if(*m!='b'&&*m!='t'){errno=EINVAL;return NULL;}}
 if(!*p){errno=EISDIR;return NULL;}if(stats.open_files>=128){errno=EMFILE;return NULL;}
 jy_fs_info info;int layer=0,e=locate(p,&info,&layer);
 if(!e&&info.kind!=JY_FS_FILE){errno=EISDIR;return NULL;}
 if(e&&e!=ENOENT){errno=e;return NULL;}if(exclusive&&!e){errno=EEXIST;return NULL;}
 if(mode[0]=='r'&&e){errno=e;return NULL;}
 int writable=plus||mode[0]!='r';
 if(writable){e=materialize(p,mode[0]=='w');if(e){errno=e;return NULL;}layer=1;e=call_stat(&write_ops,p,&info);if(e){errno=e;return NULL;}}
 jy_cookie *f=calloc(1,sizeof *f);if(!f){errno=ENOMEM;return NULL;}
 strcpy(f->path,p);f->overlay=layer;f->readable=plus||mode[0]=='r';f->writable=writable;f->append=mode[0]=='a';f->position=f->append?info.size:0;
 cookie_io_functions_t callbacks={cookie_read,cookie_write,cookie_seek,cookie_close};
 char actual_mode[4]={mode[0],0,0,0};if(plus)actual_mode[1]='+';
 FILE *stream=fopencookie(f,actual_mode,callbacks);if(!stream){free(f);return NULL;}
 f->stream=stream;f->next=files;files=f;++stats.open_files;if(stats.open_files>stats.peak_open_files)stats.peak_open_files=stats.open_files;
 /* One source of buffering: the explicit bounded read cache. This preserves
  * visibility of writes across separately opened cookies and avoids allocating
  * libc's default 8KiB read buffer for every cached GRP/font handle. */
 if(setvbuf(stream,NULL,_IONBF,0)){fclose(stream);errno=EIO;return NULL;}return stream;
}
int jy_fs_fclose(FILE *stream){if(!stream)return fail(EINVAL);return fclose(stream);}
FILE *jy_fs_stdin(void){return standard_override[0]?standard_override[0]:stdin;}
FILE *jy_fs_stdout(void){return standard_override[1]?standard_override[1]:stdout;}
FILE *jy_fs_stderr(void){return standard_override[2]?standard_override[2]:stderr;}
FILE *jy_fs_freopen(const char *path,const char *mode,FILE *stream){
 if(!stream){errno=EINVAL;return NULL;}
 int standard=-1;if(stream==jy_fs_stdin())standard=0;else if(stream==jy_fs_stdout())standard=1;else if(stream==jy_fs_stderr())standard=2;
 jy_cookie *f=owned(stream);char previous[JY_FS_PATH_MAX+1];
 if(!path){if(!f){errno=ENOTSUP;return NULL;}strcpy(previous,f->path);path=previous;}
 if(!f&&standard<0){errno=ENOTSUP;return NULL;}
 if(f&&fclose(stream))return NULL;
 FILE *next=jy_fs_fopen(path,mode);if(next&&standard>=0)standard_override[standard]=next;return next;
}
static int busy(const char *path){for(jy_cookie *f=files;f;f=f->next)if(!strcmp(f->path,path))return 1;return 0;}
int jy_fs_remove(const char *path){
 char p[JY_FS_PATH_MAX+1];if(jy_fs_normalize(path,p))return -1;if(!*p)return fail(EBUSY);if(busy(p))return fail(EBUSY);
 jy_fs_info info;int layer,e=locate(p,&info,&layer);if(e)return fail(e);if(info.kind!=JY_FS_FILE)return fail(EISDIR);
 if(!write_ops.resize||!write_ops.remove)return fail(EROFS);
 e=hide(p);if(e)return fail(e);
 /* The overlay owns durable tombstones, including deletion of a base-only
  * asset. Do not skip it just because no overlay data file exists yet. */
 (void)layer;e=write_ops.remove(write_ops.context,p);if(e){reveal(p);return fail(e);}
 return 0;
}
int jy_fs_rename(const char *from,const char *to){
 char a[JY_FS_PATH_MAX+1],b[JY_FS_PATH_MAX+1];if(jy_fs_normalize(from,a)||jy_fs_normalize(to,b))return -1;
 jy_fs_info info;int layer,e=locate(a,&info,&layer);if(e)return fail(e);if(!strcmp(a,b))return 0;
 if(info.kind!=JY_FS_FILE)return fail(EISDIR);
 if(busy(a)||busy(b))return fail(EBUSY);
 if(!write_ops.rename)return fail(EROFS);
 jy_fs_info dest;int dlayer;e=locate(b,&dest,&dlayer);if(!e&&dest.kind!=JY_FS_FILE)return fail(EISDIR);if(e&&e!=ENOENT)return fail(e);
 /* Allocate the old-name whiteout before committing a real rename. */
 e=hide(a);if(e)return fail(e);
 /* The backend receives the original logical source, including base-only
  * assets, so it can commit destination and old-name tombstone together. */
 (void)layer;e=write_ops.rename(write_ops.context,a,b);
 if(e){reveal(a);return fail(e);}reveal(b);return 0;
}
char *jy_fs_tmpnam(char *output){
 static char internal[JY_FS_TMPNAM_BYTES];char *p=output?output:internal;
 for(unsigned attempt=0;attempt<1024;++attempt){
  char name[20];snprintf(name,sizeof name,".jy%08x",++temporary_counter);jy_fs_info info;int layer,e=locate(name,&info,&layer);
  if(e==ENOENT){if(strlen(name)+1>JY_FS_TMPNAM_BYTES){errno=ENAMETOOLONG;return NULL;}strcpy(p,name);return p;}if(e){errno=e;return NULL;}
 }
 errno=EEXIST;return NULL;
}
FILE *jy_fs_tmpfile(void){char name[JY_FS_TMPNAM_BYTES];if(!jy_fs_tmpnam(name))return NULL;FILE *f=jy_fs_fopen(name,"wx+");if(f)owned(f)->temporary=1;return f;}
int jy_fs_mkdir(const char *path,mode_t mode){
 (void)mode;char p[JY_FS_PATH_MAX+1];if(jy_fs_normalize(path,p))return -1;
 jy_fs_info info;int layer,e=locate(p,&info,&layer);if(!e)return fail(EEXIST);if(e!=ENOENT)return fail(e);if(!write_ops.mkdir)return fail(EROFS);
 e=write_ops.mkdir(write_ops.context,p);if(e)return fail(e);reveal(p);return 0;
}
int jy_fs_stat(const char *path,struct stat *output){
 if(!output)return fail(EINVAL);
 char p[JY_FS_PATH_MAX+1];if(jy_fs_normalize(path,p))return -1;
 jy_fs_info info;int layer,e=locate(p,&info,&layer);if(e)return fail(e);memset(output,0,sizeof *output);
 output->st_mode=(info.kind==JY_FS_DIRECTORY?S_IFDIR|0555:S_IFREG|0444)|(layer?0200:0);output->st_nlink=1;output->st_size=(off_t)info.size;return 0;
}
int jy_fs_access(const char *path,int mode){
 if(mode&~(R_OK|W_OK|X_OK))return fail(EINVAL);
 jy_fs_info info;if(jy_fs_info_for(path,&info))return -1;
 if(mode&X_OK)return fail(EACCES);
 if((mode&W_OK)&&(!write_ops.resize||!write_ops.write_at))return fail(EROFS);
 return 0;
}
int jy_fs_fileno(FILE *stream){(void)stream;return fail(EBADF);}
int jy_fs_fstat(int fd,struct stat *output){(void)fd;(void)output;return fail(EBADF);}
FILE *jy_fs_popen(const char *command,const char *mode){(void)command;(void)mode;errno=ENOSYS;return NULL;}
int jy_fs_pclose(FILE *stream){(void)stream;return fail(ENOSYS);}
int jy_fs_system(const char *command){if(!command)return 0;(void)command;return fail(ENOSYS);}
int jy_fs_bind(const jy_fs_ops *assets,const jy_fs_ops *overlay){
 if(files)return fail(EBUSY);
 if(!assets||!assets->stat||!assets->read_at)return fail(EINVAL);
 base_ops=*assets;if(overlay)write_ops=*overlay;else memset(&write_ops,0,sizeof write_ops);return 0;
}
void jy_fs_reset(void){
 while(files)fclose(files->stream);
 while(whiteouts){jy_whiteout *next=whiteouts->next;free(whiteouts);whiteouts=next;}
 memset(&base_ops,0,sizeof base_ops);memset(&write_ops,0,sizeof write_ops);memset(standard_override,0,sizeof standard_override);memset(&stats,0,sizeof stats);temporary_counter=0;
}
void jy_fs_get_stats(jy_fs_stats *output){if(output)*output=stats;}
