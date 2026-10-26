/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SOPWITH_PXA_STDIO_H
#define SOPWITH_PXA_STDIO_H
#include_next <stdio.h>
#undef stderr
#undef stdout
#undef stdin
#define stderr ((FILE *)2)
#define stdout ((FILE *)1)
#define stdin ((FILE *)0)
/* All optional file operations have explicit unavailable semantics. The
 * actual embedded game never needs a file. Diagnostic output uses PXA log. */
#define printf sopwith_printf
#define fprintf sopwith_fprintf
#define puts sopwith_puts
#define fputs sopwith_fputs
#define perror sopwith_perror
#define fopen sopwith_fopen
#define fread sopwith_fread
#define fwrite sopwith_fwrite
#define fgets sopwith_fgets
#define fclose sopwith_fclose
#define feof sopwith_feof
#define fflush sopwith_fflush
int sopwith_printf(const char *, ...);
int sopwith_fprintf(FILE *, const char *, ...);
int sopwith_puts(const char *);
int sopwith_fputs(const char *, FILE *);
void sopwith_perror(const char *);
FILE *sopwith_fopen(const char *, const char *);
size_t sopwith_fread(void *, size_t, size_t, FILE *);
size_t sopwith_fwrite(const void *, size_t, size_t, FILE *);
char *sopwith_fgets(char *, int, FILE *);
int sopwith_fclose(FILE *);
int sopwith_feof(FILE *);
int sopwith_fflush(FILE *);
#endif
