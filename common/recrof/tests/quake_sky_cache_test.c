/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sky_cache.h"
typedef uint8_t byte;
#include "converter_sky.h"
static pxa_quake_sky_cache_t cache;
int main(void)
{
    uint32_t rng = 0xb71d9023;
    for (unsigned scene = 0; scene < 4; ++scene) {
        for (unsigned i = 0; i < sizeof(cache.layers); ++i) {
            rng = rng * 1664525u + 1013904223u;
            cache.layers[i] = (byte)(rng >> 24);
            if ((i % 256) < 128) {
                if (scene == 0 || (scene == 2 && (i % 3) == 0)) cache.layers[i] = 0;
                if (scene == 1) cache.layers[i] = 255;
            }
        }
        cache.phase = -1; /* map change must invalidate even at the same phase */
        R_InitSky(cache.layers);
        for (unsigned frame = 0; frame < 256; ++frame) {
            unsigned phase = (frame * 67u) & 127u;
            R_MakeSky(phase);
            byte *actual = Pxa_QuakeSkyFrame(&cache, phase);
            for (unsigned y = 0; y < 128; ++y)
                assert(!memcmp(actual + y * 128, newsky + y * 256, 128));
            assert(Pxa_QuakeSkyFrame(&cache, phase + 128) == actual);
        }
    }
    puts("sky cache: 1024 frames match actual PAK converter, all palette indices preserved");
}
