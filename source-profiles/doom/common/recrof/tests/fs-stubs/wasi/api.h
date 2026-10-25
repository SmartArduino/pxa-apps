/* Native-test types for unused WASI stdio/environment adapter entry points. */
#pragma once
#include <stdint.h>
typedef uint16_t __wasi_errno_t;
typedef uint32_t __wasi_fd_t;
typedef uint32_t __wasi_size_t;
typedef struct {uint8_t opaque[24];} __wasi_fdstat_t;
#define __WASI_ERRNO_BADF 8
