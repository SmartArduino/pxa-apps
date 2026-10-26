/* SPDX-License-Identifier: GPL-3.0-or-later
 * Port-private uncompressed save stream. Compatible with this PXA port's
 * SaveFile.sav only, not desktop Meritous gzip saves.
 */
#ifndef MT_SAVE_STREAM_H
#define MT_SAVE_STREAM_H
#include <stdio.h>
typedef FILE *gzFile;
static inline gzFile gzopen(const char *name,const char *mode) {
    return fopen(name, mode[0]=='w' ? "wb" : "rb");
}
static inline int gzgetc(gzFile file) {return file ? fgetc(file) : EOF;}
static inline int gzputc(gzFile file,int c) {return file ? fputc(c,file) : EOF;}
static inline int gzclose(gzFile file) {return file ? fclose(file) : -1;}
#endif
