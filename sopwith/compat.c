/* SPDX-License-Identifier: GPL-2.0-or-later
 * Port boundary only: no native file descriptor or process stdio imports.
 */
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include "pxa.h"
#include "vendor/timer.h"

clock_t clock(void) {
    return (clock_t)Timer_GetMS() * (CLOCKS_PER_SEC / 1000);
}
static int diagnostic(const char *format, va_list args) {
    char buffer[256];
    int count = vsnprintf(buffer, sizeof(buffer), format, args);
    (void)pxa_log_write(PXA_LOG_LEVEL_INFO, buffer);
    return count;
}
int sopwith_printf(const char *format, ...) {
    va_list args; va_start(args, format);
    int count = diagnostic(format, args); va_end(args); return count;
}
int sopwith_fprintf(FILE *file, const char *format, ...) {
    (void)file; va_list args; va_start(args, format);
    int count = diagnostic(format, args); va_end(args); return count;
}
int sopwith_puts(const char *text) {
    (void)pxa_log_write(PXA_LOG_LEVEL_INFO, text); return 0;
}
int sopwith_fputs(const char *text, FILE *file) {
    (void)file; (void)pxa_log_write(PXA_LOG_LEVEL_INFO, text); return 0;
}
void sopwith_perror(const char *text) {
    sopwith_printf("%s: %s", text, strerror(errno));
}
FILE *sopwith_fopen(const char *path, const char *mode) {
    (void)path; (void)mode; errno = ENOSYS; return NULL;
}
size_t sopwith_fread(void *ptr, size_t size, size_t count, FILE *file) {
    (void)ptr; (void)size; (void)count; (void)file; errno = ENOSYS; return 0;
}
size_t sopwith_fwrite(const void *ptr, size_t size, size_t count, FILE *file) {
    (void)ptr; (void)size; (void)count; (void)file; errno = ENOSYS; return 0;
}
char *sopwith_fgets(char *ptr, int count, FILE *file) {
    (void)ptr; (void)count; (void)file; errno = ENOSYS; return NULL;
}
int sopwith_fclose(FILE *file) { (void)file; errno = ENOSYS; return EOF; }
int sopwith_feof(FILE *file) { (void)file; return 1; }
int sopwith_fflush(FILE *file) { (void)file; return 0; }

/* Exact non-TCPIP branches of upstream tcpcomm.c. Its unconditional
 * PollSocket helper uses native select() even when TCPIP is disabled. */
void commconnect(char *host) { (void)host; }
void commlisten(void) {}
int commin(void) { return 0; }
void commout(unsigned char value) { (void)value; }
void commterm(void) {}
bool isNetworkGame(void) { return false; }
