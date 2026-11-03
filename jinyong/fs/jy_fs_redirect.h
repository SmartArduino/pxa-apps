/* SPDX-License-Identifier: MIT
 * Force-include into game/Lua/FILE-using decoder translation units only.
 * Do not force-include into jy_fs.c or a backend implementing native host I/O.
 */
#ifndef JY_FS_REDIRECT_H
#define JY_FS_REDIRECT_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "jy_fs.h"
#ifndef L_tmpnam
/* WASI has no host temporary directory; this size belongs to our private
 * virtual temporary-name implementation, also consumed by Lua's luaconf. */
#define L_tmpnam JY_FS_TMPNAM_BYTES
#endif
#define fopen(path,mode) jy_fs_fopen((path),(mode))
#define freopen(path,mode,stream) jy_fs_freopen((path),(mode),(stream))
#define fclose(stream) jy_fs_fclose((stream))
#define tmpfile() jy_fs_tmpfile()
#define tmpnam(buffer) jy_fs_tmpnam((buffer))
#define remove(path) jy_fs_remove((path))
#define rename(from,to) jy_fs_rename((from),(to))
#define mkdir(path,mode) jy_fs_mkdir((path),(mode))
#define stat(path,output) jy_fs_stat((path),(output))
#define access(path,mode) jy_fs_access((path),(mode))
#define fileno(stream) jy_fs_fileno((stream))
#define fstat(fd,output) jy_fs_fstat((fd),(output))
#define popen(command,mode) jy_fs_popen((command),(mode))
#define pclose(stream) jy_fs_pclose((stream))
#define system(command) jy_fs_system((command))
/* main ignores freopen's return value; these getters still route subsequent
 * stderr writes to the new real cookie FILE. Standard library headers precede
 * these macros so their declarations are not accidentally rewritten. */
#undef stdin
#undef stdout
#undef stderr
#define stdin (jy_fs_stdin())
#define stdout (jy_fs_stdout())
#define stderr (jy_fs_stderr())
#endif
