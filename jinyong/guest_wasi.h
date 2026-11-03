/* SPDX-License-Identifier: MIT */
#ifndef JY_GUEST_WASI_H
#define JY_GUEST_WASI_H
#include <wasi/api.h>
void jy_guest_wasi_reset(void);
__wasi_errno_t __wrap___wasi_environ_get(uint8_t **,uint8_t *);
__wasi_errno_t __wrap___wasi_environ_sizes_get(__wasi_size_t *,__wasi_size_t *);
__wasi_errno_t __wrap___wasi_fd_close(__wasi_fd_t);
__wasi_errno_t __wrap___wasi_fd_fdstat_get(__wasi_fd_t,__wasi_fdstat_t *);
__wasi_errno_t __wrap___wasi_fd_read(__wasi_fd_t,const __wasi_iovec_t *,__wasi_size_t,__wasi_size_t *);
__wasi_errno_t __wrap___wasi_fd_seek(__wasi_fd_t,__wasi_filedelta_t,__wasi_whence_t,__wasi_filesize_t *);
__wasi_errno_t __wrap___wasi_fd_write(__wasi_fd_t,const __wasi_ciovec_t *,__wasi_size_t,__wasi_size_t *);
#endif
