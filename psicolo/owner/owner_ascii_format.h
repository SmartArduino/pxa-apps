/* SPDX-License-Identifier: MIT. Exact bounded original scalar title formats. */
#pragma once
#include <stddef.h>
#include <stdint.h>

inline void obono_format_score(char *out, size_t capacity, uint16_t score) {
    if (capacity < 6) { if (capacity) out[0] = 0; return; }
    for (unsigned i = 0; i < 5; ++i) out[i] = ' ';
    unsigned pos = 5;
    do { out[--pos] = (char)('0' + score % 10); score /= 10; } while (score);
    out[5] = 0;
}

inline void obono_format_seconds(char *out, size_t capacity, uint32_t frames) {
    if (capacity < 5) { if (capacity) out[0] = 0; return; }
    const unsigned seconds = frames / 60 % 60;
    out[0] = '\'';
    out[1] = (char)('0' + seconds / 10);
    out[2] = (char)('0' + seconds % 10);
    out[3] = '"';
    out[4] = 0;
}
