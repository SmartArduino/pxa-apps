/* SPDX-License-Identifier: MIT. Bounded diagnostics without WASI stdio. */
#ifndef PXA_PORT_LOG_H
#define PXA_PORT_LOG_H
#include "pxa.h"
typedef struct { char bytes[256]; unsigned length; } pxlog_t;
static inline void pxlog_init(pxlog_t *log) { log->length = 0; log->bytes[0] = 0; }
static inline void pxlog_text(pxlog_t *log, const char *text) {
    while (*text && log->length < sizeof(log->bytes) - 1)
        log->bytes[log->length++] = *text++;
    log->bytes[log->length] = 0;
}
static inline void pxlog_uint(pxlog_t *log, uint32_t number) {
    char digits[10]; unsigned count = 0;
    do { digits[count++] = (char)('0' + number % 10); number /= 10; } while (number);
    while (count && log->length < sizeof(log->bytes) - 1)
        log->bytes[log->length++] = digits[--count];
    log->bytes[log->length] = 0;
}
static inline void pxlog_field(pxlog_t *log, const char *key, int32_t number) {
    pxlog_text(log, " "); pxlog_text(log, key); pxlog_text(log, "=");
    if (number < 0) { pxlog_text(log, "-"); pxlog_uint(log, 0u - (uint32_t)number); }
    else pxlog_uint(log, (uint32_t)number);
}
static inline void pxlog_send(pxlog_t *log) { (void)pxa_log_write(2, log->bytes); }
#endif
