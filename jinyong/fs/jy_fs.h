/* SPDX-License-Identifier: MIT
 * Jinyong PXA port: streaming read-only assets plus an independent write layer.
 */
#ifndef JY_FS_H
#define JY_FS_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __cplusplus
extern "C" {
#endif
#define JY_FS_PATH_MAX 255u
#define JY_FS_CHUNK_BYTES 4064u
#define JY_FS_MAX_WRITE_BYTES 131072u
#define JY_FS_TMPNAM_BYTES 20
enum { JY_FS_FILE=1, JY_FS_DIRECTORY=2 };
typedef struct { uint64_t size; uint32_t kind; } jy_fs_info;
/* Every callback returns 0 or a positive errno value. read/write_at must set
 * transferred <= requested. Reads are bounded by 4064 bytes; a backend may
 * opt into larger writes up to JY_FS_MAX_WRITE_BYTES. An
 * asynchronous implementation may suspend and resume its own C stack inside
 * these callbacks, but must finish before returning. No FILE/Guest pointer is
 * retained by the asset service. resize(0) creates/truncates an overlay file.
 * Completed write_at must preserve the actual bytes, not report queued success.
 */
typedef struct {
 void *context;
 int (*stat)(void *,const char *,jy_fs_info *);
 int (*read_at)(void *,const char *,uint64_t,void *,size_t,size_t *);
 int (*write_at)(void *,const char *,uint64_t,const void *,size_t,size_t *);
 int (*resize)(void *,const char *,uint64_t);
 /* Also called for base-only files. A persistent backend must record a
  * tombstone and have its asset stat wrapper hide it after cold rebind. */
 int (*remove)(void *,const char *);
 /* May receive a base-only source. Commit the resolved destination and
  * persistent old-name tombstone together. After a durable intent, backend
  * recovery may roll forward despite a reported error; rollback is not implied. */
 int (*rename)(void *,const char *,const char *);
 int (*mkdir)(void *,const char *);
 /* Optional atomic copy-on-write from immutable asset bytes. On failure the
  * destination view must remain unchanged. The PXA persistent backend supplies
  * this callback so a failed r+/append never creates a half-file/tombstone. */
 int (*clone_base)(void *,const char *,const char *);
 /* Zero keeps 4064-byte writes. Completed larger writes retain the same
  * error/visibility semantics; this is not a queued write buffer. */
 uint32_t max_write_bytes;
} jy_fs_ops;
typedef struct {
 uint64_t base_read_calls,base_read_bytes,overlay_read_calls,overlay_read_bytes;
 uint64_t overlay_write_calls,overlay_write_bytes,cache_hits,copy_on_write_bytes;
 uint32_t open_files,peak_open_files,max_callback_request;
} jy_fs_stats;
int jy_fs_bind(const jy_fs_ops *assets,const jy_fs_ops *overlay);
void jy_fs_reset(void);
void jy_fs_get_stats(jy_fs_stats *);
int jy_fs_normalize(const char *,char[JY_FS_PATH_MAX+1]);
int jy_fs_info_for(const char *,jy_fs_info *);
FILE *jy_fs_fopen(const char *,const char *);
FILE *jy_fs_freopen(const char *,const char *,FILE *);
int jy_fs_fclose(FILE *);
FILE *jy_fs_tmpfile(void);
char *jy_fs_tmpnam(char *);
int jy_fs_remove(const char *);
int jy_fs_rename(const char *,const char *);
int jy_fs_mkdir(const char *,mode_t);
int jy_fs_stat(const char *,struct stat *);
int jy_fs_access(const char *,int);
int jy_fs_fileno(FILE *);
int jy_fs_fstat(int,struct stat *);
FILE *jy_fs_stdin(void);
FILE *jy_fs_stdout(void);
FILE *jy_fs_stderr(void);
FILE *jy_fs_popen(const char *,const char *);
int jy_fs_pclose(FILE *);
int jy_fs_system(const char *);
#ifdef __cplusplus
}
#endif
#endif
