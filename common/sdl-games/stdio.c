/* Formatted input over the guest filesystem. GPL-3.0-or-later.
 * Use libc's scanner with a real cookie FILE: an engine FILE is a PXA handle
 * and must never be passed directly to libc's private FILE implementation. */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "port.h"
#include <wasi/api.h>
#undef lseek
extern off_t __wrap_lseek(int fd,off_t offset,int origin);
off_t __wrap___wasilibc_tell(int fd) { return __wrap_lseek(fd,0,SEEK_CUR); }

/* No ambient stdin. Cookie streams obtain bytes through their PXA callback. */
__wasi_errno_t __wrap___wasi_fd_read(__wasi_fd_t fd,const __wasi_iovec_t *iov,
    size_t count,size_t *read_bytes) {
  (void)fd;(void)iov;(void)count;if(read_bytes)*read_bytes=0;
  return __WASI_ERRNO_BADF;
}

extern long __real_ftell(FILE *file);
extern int __real_fclose(FILE *file);
extern int __real_vfprintf(FILE *file,const char *format,va_list args);
static ssize_t cookie_read(void *file,char *bytes,size_t n) { return fread(bytes,1,n,file); }
static int cookie_seek(void *file,off_t *offset,int origin) {
  if (*offset>LONG_MAX || *offset<LONG_MIN) {errno=EOVERFLOW;return -1;}
  if (fseek(file,(long)*offset,origin))return -1;
  *offset=ftell(file);return *offset<0?-1:0;
}
int __wrap_vfscanf(FILE *file,const char *format,va_list args) {
  cookie_io_functions_t io={.read=cookie_read,.seek=cookie_seek};
  FILE *stream=fopencookie(file,"r",io);
  if (!stream)return EOF;
  int result=vfscanf(stream,format,args);
  /* libc can read ahead and retain one ungetc byte. Restore the logical
   * cursor so a following fgetc/fread/fscanf sees exactly the same stream. */
  long position=__real_ftell(stream);
  __real_fclose(stream);
  if (position>=0)fseek(file,position,SEEK_SET);
  return result;
}
int __wrap_fscanf(FILE *file,const char *format,...) {
  va_list args;va_start(args,format);
  int result=__wrap_vfscanf(file,format,args);
  va_end(args);return result;
}
int __wrap_fstat(int fd,struct stat *st) {
  off_t position=lseek(fd,0,SEEK_CUR);
  if (position<0)return -1;
  off_t size=lseek(fd,0,SEEK_END);
  if (lseek(fd,position,SEEK_SET)<0 || size<0)return -1;
  memset(st,0,sizeof(*st));st->st_size=size;st->st_mode=S_IFREG|0600;
  return 0;
}
int __wrap_vfprintf(FILE *file,const char *format,va_list args) {
  /* vsnprintf itself uses vfprintf with a genuine libc memory FILE. Passing
   * that stream back through our formatter would recurse. */
  if(file!=stdout && file!=stderr && fileno(file)<0)
    return __real_vfprintf(file,format,args);
  va_list copy;va_copy(copy,args);
  int n=vsnprintf(NULL,0,format,copy);va_end(copy);
  if(n<0)return n;
  char *text=malloc((size_t)n+1);
  if(!text){errno=ENOMEM;return -1;}
  vsnprintf(text,(size_t)n+1,format,args);
  if(file==stdout || file==stderr)port_log("%s",text);
  else if(fwrite(text,1,n,file)!=(size_t)n)n=-1;
  free(text);return n;
}

/* The PXA private namespace contains no symlinks. */
int __wrap_lstat(const char *path,struct stat *st) { return stat(path,st); }

_Noreturn void __wrap___assert_fail(const char *expression,const char *file,int line,const char *function) {
  port_log("PXA SDL assertion: %s (%s:%d, %s)",expression,file,line,function);
  port_error("The game stopped after an assertion; see the runtime log.");
  __builtin_unreachable();
}
