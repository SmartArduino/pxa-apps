/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef PXA_QUAKE_SKY_CACHE_H
#define PXA_QUAKE_SKY_CACHE_H
#include <stdint.h>

/* Raw Quake sky: a masked 128x128 foreground beside a 128x128 background.
 * Keep 48 KiB in the application instead of streaming a 2 MiB animation
 * through the shared resource cache. Palette indices and timing are unchanged. */
typedef struct {
    uint8_t layers[128 * 256];
    uint8_t pixels[128 * 128];
    int phase;
} pxa_quake_sky_cache_t;

static uint8_t *Pxa_QuakeSkyFrame(pxa_quake_sky_cache_t *cache, unsigned phase)
{
    phase &= 127u;
    if (cache->phase != (int)phase) {
        for (unsigned y = 0; y < 128; ++y) {
            const uint8_t *front = cache->layers + ((y + phase) & 127u) * 256;
            const uint8_t *back = cache->layers + y * 256 + 128;
            for (unsigned x = 0; x < 128; ++x) {
                uint8_t pixel = front[(x + phase) & 127u];
                cache->pixels[y * 128 + x] = pixel ? pixel : back[x];
            }
        }
        cache->phase = (int)phase;
    }
    return cache->pixels;
}
#endif
