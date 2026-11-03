/* SPDX-License-Identifier: MIT
 * Session-only WASI facade. No host files, network, credentials or global env.
 * Original stdio/INI/high-score/record logic remains in the engine.
 */
#ifdef __wasi__
#include <wasi/api.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "digger-host.h"
#define FILES 16
#define HANDLES 48
#define CAPACITY 262144u
static struct {char name[160];unsigned char *data;uint32_t size;} files[FILES];
static struct {int file;uint32_t pos;int append,used;} handles[HANDLES];
static int path_name(const char *p,char out[160]){
 while(p[0]=='.'&&p[1]=='/')p+=2;
 size_t n=strlen(p);if(!n||n>=160||strstr(p,"..")||strchr(p,'/')||strchr(p,'\\'))return 0;
 memcpy(out,p,n+1);return 1;
}
static int lookup(const char *p){for(int i=0;i<FILES;i++)if(files[i].name[0]&&!strcmp(files[i].name,p))return i;return -1;}
static int handle(__wasi_fd_t fd){int i=(int)fd-4;return i>=0&&i<HANDLES&&handles[i].used?i:-1;}
static void filestat(int f,__wasi_filestat_t *s){memset(s,0,sizeof *s);s->filetype=__WASI_FILETYPE_REGULAR_FILE;s->nlink=1;s->size=files[f].size;s->ino=(uint64_t)f+1;}
void digger_sessionfs_reset(void){for(int i=0;i<FILES;i++)free(files[i].data);memset(files,0,sizeof files);memset(handles,0,sizeof handles);}
clock_t __wrap_clock(void){return(clock_t)(digger_port_time_us()*CLOCKS_PER_SEC/1000000);}
__wasi_errno_t __wrap___wasi_fd_prestat_get(__wasi_fd_t fd,__wasi_prestat_t *s){
 if(fd!=3)return __WASI_ERRNO_BADF;memset(s,0,sizeof *s);s->tag=__WASI_PREOPENTYPE_DIR;s->u.dir.pr_name_len=1;return 0;
}
__wasi_errno_t __wrap___wasi_fd_prestat_dir_name(__wasi_fd_t fd,uint8_t *p,__wasi_size_t n){if(fd!=3||n<1)return __WASI_ERRNO_BADF;p[0]='.';return 0;}
__wasi_errno_t __wrap___wasi_path_open(__wasi_fd_t fd,__wasi_lookupflags_t l,const char *p,__wasi_oflags_t flags,__wasi_rights_t r,__wasi_rights_t ri,__wasi_fdflags_t ff,__wasi_fd_t *result){
 (void)l;(void)r;(void)ri;if(fd!=3)return __WASI_ERRNO_BADF;char name[160];if(!path_name(p,name))return __WASI_ERRNO_NOTCAPABLE;
 int f=lookup(name);if(f>=0&&(flags&__WASI_OFLAGS_EXCL))return __WASI_ERRNO_EXIST;
 if(f<0){if(!(flags&__WASI_OFLAGS_CREAT))return __WASI_ERRNO_NOENT;for(f=0;f<FILES&&files[f].name[0];f++);if(f==FILES)return __WASI_ERRNO_NOSPC;
  files[f].data=calloc(CAPACITY,1);if(!files[f].data)return __WASI_ERRNO_NOMEM;strcpy(files[f].name,name);}
 if(flags&__WASI_OFLAGS_TRUNC)files[f].size=0;
 int h;for(h=0;h<HANDLES&&handles[h].used;h++);if(h==HANDLES)return __WASI_ERRNO_MFILE;
 handles[h].used=1;handles[h].file=f;handles[h].append=!!(ff&__WASI_FDFLAGS_APPEND);handles[h].pos=handles[h].append?files[f].size:0;*result=(__wasi_fd_t)(h+4);return 0;
}
__wasi_errno_t __wrap___wasi_fd_close(__wasi_fd_t fd){if(fd<=3)return 0;int h=handle(fd);if(h<0)return __WASI_ERRNO_BADF;memset(handles+h,0,sizeof handles[h]);return 0;}
__wasi_errno_t __wrap___wasi_fd_fdstat_get(__wasi_fd_t fd,__wasi_fdstat_t *s){
 memset(s,0,sizeof *s);if(fd>3&&handle(fd)<0)return __WASI_ERRNO_BADF;s->fs_filetype=fd==3?__WASI_FILETYPE_DIRECTORY:fd<3?__WASI_FILETYPE_CHARACTER_DEVICE:__WASI_FILETYPE_REGULAR_FILE;s->fs_rights_base=s->fs_rights_inheriting=~(__wasi_rights_t)0;return 0;
}
__wasi_errno_t __wrap___wasi_fd_fdstat_set_flags(__wasi_fd_t fd,__wasi_fdflags_t f){int h=handle(fd);if(h<0)return fd<=3?0:__WASI_ERRNO_BADF;handles[h].append=!!(f&__WASI_FDFLAGS_APPEND);return 0;}
__wasi_errno_t __wrap___wasi_fd_read(__wasi_fd_t fd,const __wasi_iovec_t *iov,size_t n,__wasi_size_t *readn){
 *readn=0;if(fd==0)return 0;int h=handle(fd);if(h<0)return __WASI_ERRNO_BADF;int f=handles[h].file;
 for(size_t i=0;i<n;i++){uint32_t pos=handles[h].pos,available=pos<files[f].size?files[f].size-pos:0;size_t take=iov[i].buf_len;if(take>available)take=available;memcpy(iov[i].buf,files[f].data+pos,take);handles[h].pos+=(uint32_t)take;*readn+=(__wasi_size_t)take;if(take<iov[i].buf_len)break;}return 0;
}
__wasi_errno_t __wrap___wasi_fd_write(__wasi_fd_t fd,const __wasi_ciovec_t *iov,size_t n,__wasi_size_t *written){
 *written=0;if(fd==1||fd==2){for(size_t i=0;i<n;i++)*written+=(__wasi_size_t)iov[i].buf_len;return 0;}
 int h=handle(fd);if(h<0)return __WASI_ERRNO_BADF;int f=handles[h].file;
 if(handles[h].append)handles[h].pos=files[f].size;
 for(size_t i=0;i<n;i++){uint32_t pos=handles[h].pos;size_t take=iov[i].buf_len;if(pos>CAPACITY)return __WASI_ERRNO_NOSPC;if(take>CAPACITY-pos)take=CAPACITY-pos;memcpy(files[f].data+pos,iov[i].buf,take);handles[h].pos+=(uint32_t)take;if(handles[h].pos>files[f].size)files[f].size=handles[h].pos;*written+=(__wasi_size_t)take;if(take<iov[i].buf_len)return *written?0:__WASI_ERRNO_NOSPC;}return 0;
}
__wasi_errno_t __wrap___wasi_fd_seek(__wasi_fd_t fd,__wasi_filedelta_t offset,__wasi_whence_t whence,__wasi_filesize_t *pos){
 int h=handle(fd);if(h<0)return __WASI_ERRNO_BADF;int64_t base=whence==__WASI_WHENCE_SET?0:whence==__WASI_WHENCE_CUR?handles[h].pos:files[handles[h].file].size;int64_t p=base+offset;if(p<0||p>CAPACITY)return __WASI_ERRNO_INVAL;handles[h].pos=(uint32_t)p;*pos=(__wasi_filesize_t)p;return 0;
}
__wasi_errno_t __wrap___wasi_fd_tell(__wasi_fd_t fd,__wasi_filesize_t *p){int h=handle(fd);if(h<0)return __WASI_ERRNO_BADF;*p=handles[h].pos;return 0;}
__wasi_errno_t __wrap___wasi_fd_filestat_get(__wasi_fd_t fd,__wasi_filestat_t *s){int h=handle(fd);if(h<0)return __WASI_ERRNO_BADF;filestat(handles[h].file,s);return 0;}
__wasi_errno_t __wrap___wasi_path_filestat_get(__wasi_fd_t fd,__wasi_lookupflags_t l,const char *p,__wasi_filestat_t *s){(void)l;if(fd!=3)return __WASI_ERRNO_BADF;char name[160];if(!path_name(p,name))return __WASI_ERRNO_NOTCAPABLE;int f=lookup(name);if(f<0)return __WASI_ERRNO_NOENT;filestat(f,s);return 0;}
__wasi_errno_t __wrap___wasi_fd_sync(__wasi_fd_t fd){return fd<=3||handle(fd)>=0?0:__WASI_ERRNO_BADF;}
__wasi_errno_t __wrap___wasi_fd_datasync(__wasi_fd_t fd){return __wrap___wasi_fd_sync(fd);}
__wasi_errno_t __wrap___wasi_path_unlink_file(__wasi_fd_t fd,const char*p){if(fd!=3)return __WASI_ERRNO_BADF;char name[160];if(!path_name(p,name))return __WASI_ERRNO_NOTCAPABLE;int f=lookup(name);if(f<0)return __WASI_ERRNO_NOENT;free(files[f].data);memset(files+f,0,sizeof files[f]);for(int h=0;h<HANDLES;h++)if(handles[h].used&&handles[h].file==f)memset(handles+h,0,sizeof handles[h]);return 0;}
__wasi_errno_t __wrap___wasi_path_remove_directory(__wasi_fd_t fd,const char*p){if(fd!=3)return __WASI_ERRNO_BADF;char name[160];if(!path_name(p,name))return __WASI_ERRNO_NOTCAPABLE;return lookup(name)>=0?__WASI_ERRNO_NOTDIR:__WASI_ERRNO_NOENT;}
void __wrap___wasi_proc_exit(__wasi_exitcode_t code){(void)code;__builtin_trap();}
__wasi_errno_t __wrap___wasi_path_rename(__wasi_fd_t fd,const char*old,__wasi_fd_t newfd,const char*newp){
 if(fd!=3||newfd!=3)return __WASI_ERRNO_BADF;char a[160],b[160];if(!path_name(old,a)||!path_name(newp,b))return __WASI_ERRNO_NOTCAPABLE;int f=lookup(a);if(f<0)return __WASI_ERRNO_NOENT;int t=lookup(b);if(t>=0&&t!=f){free(files[t].data);memset(files+t,0,sizeof files[t]);}strcpy(files[f].name,b);return 0;
}
__wasi_errno_t __wrap___wasi_clock_time_get(__wasi_clockid_t id,__wasi_timestamp_t precision,__wasi_timestamp_t *t){(void)precision;*t=digger_port_time_us()*1000+(id==__WASI_CLOCKID_REALTIME?UINT64_C(1793491200000000000):0);return 0;}
__wasi_errno_t __wrap___wasi_clock_res_get(__wasi_clockid_t id,__wasi_timestamp_t *t){(void)id;*t=1000000;return 0;}
__wasi_errno_t __wrap___wasi_environ_sizes_get(__wasi_size_t *n,__wasi_size_t *size){*n=*size=0;return 0;}
__wasi_errno_t __wrap___wasi_environ_get(uint8_t **p,uint8_t*b){(void)p;(void)b;return 0;}
#else
void digger_sessionfs_reset(void){}
#endif

