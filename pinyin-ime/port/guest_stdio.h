#pragma once
// Force-included only for the decoder translation units, after the libc
// declarations. The decoder sees a read-only capability, never an ambient path.
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stddef.h>
struct ImeFile;
ImeFile* ime_fopen(const char*, const char*);
ImeFile* ime_fdopen(int, const char*);
int ime_fclose(ImeFile*);
size_t ime_fread(void*, size_t, size_t, ImeFile*);
size_t ime_fwrite(const void*, size_t, size_t, ImeFile*);
int ime_fseek(ImeFile*, long, int);
long ime_ftell(ImeFile*);
int ime_ferror(ImeFile*);
int ime_feof(ImeFile*);
int ime_no_open(const char*, int, ...);
off_t ime_no_lseek(int, off_t, int);
ssize_t ime_no_write(int, const void*, size_t);
int ime_no_truncate(int, off_t);
int ime_no_close(int);
int ime_no_unlink(const char*);
#define FILE ImeFile
#define fopen ime_fopen
#define fdopen ime_fdopen
#define fclose ime_fclose
#define fread ime_fread
#define fwrite ime_fwrite
#define fseek ime_fseek
#define ftell ime_ftell
#define ferror ime_ferror
#define feof ime_feof
