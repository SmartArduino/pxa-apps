/* Compare every pixel against the actual upstream kernel. GPL-2.0-or-later. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define SPAN_PRECISION 6
#define mipShift(m) (4 - (m))
#define mipMask(m) ((1 << mipShift(m)) - 1)
static struct {
    const uint32_t *blocklights;
    int r_lightwidth, textureType;
    const uint8_t *r_source, *colormap;
    int r_texture_width, r_texture_height;
    uint32_t qcs, qct;
} sdd;
static int r_fullbright;
static struct {
    int surfmip;
} r_drawsurf;
#include "quake_original_lit_span.h"
defineDrawLitSpan(0, 1) defineDrawLitSpan(1, 1) defineDrawLitSpan(2, 1) defineDrawLitSpan(3, 1)
    defineDrawLitSpan(0, 0) defineDrawLitSpan(1, 0) defineDrawLitSpan(2, 0) defineDrawLitSpan(3, 0)
#include "lit_span.h"
        typedef void (*span_fn)(uint8_t *, int, int, int, int, int);
static span_fn refs[] = {drawLitSpan0_1, drawLitSpan1_1, drawLitSpan2_1, drawLitSpan3_1,
                         drawLitSpan0_0, drawLitSpan1_0, drawLitSpan2_0, drawLitSpan3_0};
static span_fn opts[] = {pxaDrawLitSpan0_1, pxaDrawLitSpan1_1, pxaDrawLitSpan2_1,
                         pxaDrawLitSpan3_1, pxaDrawLitSpan0_0, pxaDrawLitSpan1_0,
                         pxaDrawLitSpan2_0, pxaDrawLitSpan3_0};
static uint32_t rng = 1234567;
static uint32_t random_u32(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    return rng ^= rng << 5;
}
static uint32_t lights[520 * 520];
static uint8_t pixels[128 * 128], map[65536];
static uint64_t ticks(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}
int main(int argc, char **argv) {
    for (unsigned i = 0; i < sizeof(pixels); i++)
        pixels[i] = random_u32();
    for (unsigned i = 0; i < sizeof(map); i++)
        map[i] = random_u32();
    for (unsigned i = 0; i < sizeof(lights) / sizeof(lights[0]); i++)
        lights[i] = (random_u32() & 4095) | ((random_u32() & 4095) << 16);
    sdd.blocklights = lights + 260 * 520 + 260;
    sdd.r_lightwidth = 520;
    sdd.r_source = pixels;
    sdd.colormap = map;
    unsigned cases = 0;
    for (unsigned kind = 0; kind < 8; kind++) {
        sdd.textureType = kind >= 4;
        r_drawsurf.surfmip = kind & 3;
        sdd.r_texture_width = kind < 4 ? 64 : 48;
        sdd.r_texture_height = kind < 4 ? 64 : 80;
        sdd.qcs = UINT32_MAX / sdd.r_texture_width + 1;
        sdd.qct = UINT32_MAX / sdd.r_texture_height + 1;
        for (unsigned j = 0; j < 25000; j++) {
            int count = (random_u32() % 32) + 1;
            int s = ((32 + (random_u32() % 32)) << 16) + (random_u32() & 65535),
                t = ((32 + (random_u32() % 32)) << 16) + (random_u32() & 65535);
            int ds = ((int)(random_u32() % 9) - 4) * 16384,
                dt = ((int)(random_u32() % 9) - 4) * 16384;
            if ((j % 4) == 0)
                dt = 0;
            if ((j % 8) == 0)
                ds = 0;
            if (kind < 4 && (j % 3) == 0) {
                s -= 96 * 65536;
                t -= 96 * 65536;
            }
            /* Signed Q10.6 wraps from +511 to -512, in either direction.
             * A repeated-texel hit must not survive a row or wrap boundary. */
            if ((j % 16) == 0) { s = 511 * 65536 + 49152; ds = 16384; dt = 0; }
            if ((j % 16) == 1) { s = -512 * 65536 + 16384; ds = -16384; dt = 0; }
            if ((j % 16) == 2) { t = 511 * 65536 + 49152; dt = 16384; ds = 0; }
            if ((j % 16) == 3) { t = -512 * 65536 + 16384; dt = -16384; ds = 0; }
            uint8_t a[64], b[64];
            memset(a, 0xa5, sizeof(a));
            memset(b, 0xa5, sizeof(b));
            r_fullbright = j & 1;
            refs[kind](a + 16, count, s, t, ds, dt);
            opts[kind](b + 16, count, s, t, ds, dt);
            if (memcmp(a, b, sizeof(a))) {
                fprintf(stderr, "mismatch kernel=%u case=%u\n", kind, j);
                return 1;
            }
            memset(b, 0xa5, sizeof(b));
            int cached = Pxa_TryCachedLitSpan(b + 16, count, s, t, ds, dt);
            if (!cached)
                refs[kind](b + 16, count, s, t, ds, dt);
            assert(!memcmp(a, b, sizeof(a)));
            cases++;
        }
    }
    printf("PASS: %u exact pixel comparisons, 8 kernels, fractional/negative steps, fullbright, "
           "row/cell/signed-wrap boundaries and output guards\n",
           cases);
    if (argc > 1) {
        volatile uint32_t checksum = 0;
        unsigned repetitions = 500000;
        int ds = 16384;
        r_fullbright = 0;
        sdd.r_texture_width = sdd.r_texture_height = 64;
        sdd.textureType = 0;
        r_drawsurf.surfmip = 0;
        for (int moving_t = 0; moving_t < 2; moving_t++) {
            uint64_t values[2];
            for (unsigned which = 0; which < 2; which++) {
                uint8_t out[32];
                uint64_t before = ticks();
                for (unsigned i = 0; i < repetitions; i++) {
                    if (!which || !Pxa_TryCachedLitSpan(out, 32, 32 << 16, 32 << 16, ds,
                                                        moving_t ? 65536 : 0))
                        refs[0](out, 32, 32 << 16, 32 << 16, ds, moving_t ? 65536 : 0);
                    checksum += out[i & 31];
                }
                values[which] = ticks() - before;
            }
            printf("HOST_MICROBENCH moving_t=%d original_ns=%llu cached_ns=%llu speedup=%.3f "
                   "checksum=%u (not device FPS)\n",
                   moving_t, (unsigned long long)values[0], (unsigned long long)values[1],
                   (double)values[0] / values[1], checksum);
        }
    }
    return 0;
}
