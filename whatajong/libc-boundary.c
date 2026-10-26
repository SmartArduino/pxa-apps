/* SPDX-License-Identifier: MIT. QuickJS diagnostic stdio stays guest-local.
 * The engine does not load modules, access files, or spawn processes. */
#include <wasi/api.h>
#include <string.h>
#include "pxa.h"
__wasi_errno_t __wrap___wasi_fd_write(__wasi_fd_t fd,const __wasi_ciovec_t *vec,size_t count,__wasi_size_t *written) {
    (void)fd;*written=0;for(size_t i=0;i<count;i++)*written+=vec[i].buf_len;
    return __WASI_ERRNO_SUCCESS;
}
__wasi_errno_t __wrap___wasi_fd_close(__wasi_fd_t fd){(void)fd;return __WASI_ERRNO_BADF;}
__wasi_errno_t __wrap___wasi_fd_seek(__wasi_fd_t fd,__wasi_filedelta_t off,__wasi_whence_t whence,__wasi_filesize_t *position){(void)fd;(void)off;(void)whence;(void)position;return __WASI_ERRNO_BADF;}
__wasi_errno_t __wrap___wasi_fd_fdstat_get(__wasi_fd_t fd,__wasi_fdstat_t *stat){(void)fd;memset(stat,0,sizeof(*stat));return __WASI_ERRNO_BADF;}
