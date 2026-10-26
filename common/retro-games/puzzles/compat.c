/* SPDX-License-Identifier: MIT. Only the platform error/environment boundary. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wasi/api.h>
#include "pxa.h"
char *__wrap_getenv(const char *name) { (void)name; return NULL; }
__attribute__((noreturn)) void fatal(const char *format, ...) {
    char buffer[240]; va_list args; va_start(args,format);
    vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);
    (void)pxa_log_write(4,buffer);__builtin_trap();
}
__attribute__((noreturn)) void __wrap___assert_fail(const char *expr,const char *file,int line,const char *function) {
    fatal("PUZZLE assertion %s:%d %s: %s",file,line,function,expr);
}
__attribute__((noreturn)) void __wrap_abort(void) { fatal("PUZZLE abort"); }
__attribute__((noreturn)) void __wrap_exit(int status) { fatal("PUZZLE unexpected libc exit %d",status); }

/* sscanf/vsnprintf retain libc FILE helpers even when only memory streams are
 * used. Keep those optional fd paths entirely within the Guest boundary. */
__wasi_errno_t __wrap___wasi_fd_close(__wasi_fd_t fd) {
    return fd<=2?__WASI_ERRNO_SUCCESS:__WASI_ERRNO_BADF;
}
__wasi_errno_t __wrap___wasi_fd_seek(__wasi_fd_t fd,__wasi_filedelta_t offset,
                                  __wasi_whence_t whence,__wasi_filesize_t *result) {
    (void)offset;(void)whence;if(result)*result=0;
    return fd<=2?__WASI_ERRNO_SPIPE:__WASI_ERRNO_BADF;
}
__wasi_errno_t __wrap___wasi_fd_write(__wasi_fd_t fd,const __wasi_ciovec_t *iov,
                                   size_t count,__wasi_size_t *written) {
    if(fd!=1&&fd!=2)return __WASI_ERRNO_BADF;
    char message[240];unsigned used=0;__wasi_size_t total=0;
    for(size_t i=0;i<count;i++){const uint8_t *bytes=iov[i].buf;total+=iov[i].buf_len;
        for(size_t j=0;j<iov[i].buf_len;j++){
            if(bytes[j]=='\n'||used==sizeof(message)-1){message[used]=0;if(used)pxa_log_write(2,message);used=0;}
            if(bytes[j]!='\n')message[used++]=(char)bytes[j];}}
    message[used]=0;if(used)pxa_log_write(2,message);if(written)*written=total;
    return __WASI_ERRNO_SUCCESS;
}
