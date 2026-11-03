/* SPDX-License-Identifier: MIT.
 * Guest-local empty environment and null standard streams. This matches the
 * retained Host's configured empty environment and /dev/null stdio, without
 * requesting unavailable Host features. There are no path operations, preopens
 * or Host descriptors. Real game/Lua/font FILE I/O uses jy_fs cookie callbacks
 * backed by authenticated Assets and bounded Storage, including redirected
 * debug/error streams. Unexpected descriptors fail rather than fabricate data.
 */
#include "guest_wasi.h"
#include <stdint.h>
#include <stddef.h>
static unsigned closed;
void jy_guest_wasi_reset(void){closed=0;}
static int valid(__wasi_fd_t fd){return fd<3&&!(closed&(1u<<fd));}
__wasi_errno_t __wrap___wasi_environ_get(uint8_t **environment,uint8_t *buffer){
 (void)environment;(void)buffer;return __WASI_ERRNO_SUCCESS;
}
__wasi_errno_t __wrap___wasi_environ_sizes_get(__wasi_size_t *count,__wasi_size_t *bytes){
 if(!count||!bytes)return __WASI_ERRNO_FAULT;*count=0;*bytes=0;return __WASI_ERRNO_SUCCESS;
}
__wasi_errno_t __wrap___wasi_fd_close(__wasi_fd_t fd){
 if(!valid(fd))return __WASI_ERRNO_BADF;closed|=1u<<fd;return __WASI_ERRNO_SUCCESS;
}
__wasi_errno_t __wrap___wasi_fd_fdstat_get(__wasi_fd_t fd,__wasi_fdstat_t *out){
 if(!valid(fd))return __WASI_ERRNO_BADF;if(!out)return __WASI_ERRNO_FAULT;
 *out=(__wasi_fdstat_t){0};out->fs_filetype=__WASI_FILETYPE_CHARACTER_DEVICE;
 out->fs_rights_base=fd?__WASI_RIGHTS_FD_WRITE:__WASI_RIGHTS_FD_READ;return __WASI_ERRNO_SUCCESS;
}
__wasi_errno_t __wrap___wasi_fd_read(__wasi_fd_t fd,const __wasi_iovec_t *iov,__wasi_size_t count,__wasi_size_t *read){
 if(!valid(fd)||fd!=0)return __WASI_ERRNO_BADF;if(!read||(!iov&&count))return __WASI_ERRNO_FAULT;
 for(__wasi_size_t i=0;i<count;i++)if(!iov[i].buf&&iov[i].buf_len)return __WASI_ERRNO_FAULT;
 *read=0;return __WASI_ERRNO_SUCCESS;
}
__wasi_errno_t __wrap___wasi_fd_seek(__wasi_fd_t fd,__wasi_filedelta_t delta,__wasi_whence_t whence,__wasi_filesize_t *position){
 (void)delta;(void)whence;(void)position;return valid(fd)?__WASI_ERRNO_SPIPE:__WASI_ERRNO_BADF;
}
__wasi_errno_t __wrap___wasi_fd_write(__wasi_fd_t fd,const __wasi_ciovec_t *iov,__wasi_size_t count,__wasi_size_t *written){
 if(!valid(fd)||fd==0)return __WASI_ERRNO_BADF;if(!written||(!iov&&count))return __WASI_ERRNO_FAULT;
 uint64_t total=0;for(__wasi_size_t i=0;i<count;i++){
  if(!iov[i].buf&&iov[i].buf_len)return __WASI_ERRNO_FAULT;
  total+=iov[i].buf_len;if(total>UINT32_MAX)return __WASI_ERRNO_OVERFLOW;
 }*written=(__wasi_size_t)total;return __WASI_ERRNO_SUCCESS;
}
