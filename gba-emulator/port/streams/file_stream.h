/* gpSP's cartridge-only VFS bridge. GPL-2.0-or-later. */
#pragma once
#include <stdint.h>
#include <stdio.h>
typedef struct { uint64_t offset; } RFILE;
RFILE *filestream_open(const char *path,unsigned mode,unsigned hints);
int filestream_close(RFILE *file);
int64_t filestream_get_size(RFILE *file);
int64_t filestream_seek(RFILE *file,int64_t offset,int whence);
int64_t filestream_read(RFILE *file,void *out,int64_t bytes);
