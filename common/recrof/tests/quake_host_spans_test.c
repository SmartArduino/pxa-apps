/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pxa/raster_spans.h"
#define SPAN_PRECISION 6
#define mipShift(m) (4 - (m))
#define mipMask(m) ((1 << mipShift(m)) - 1)
static struct {
    const uint32_t *blocklights;
    int r_lightwidth, r_texture_width, r_texture_height;
    const uint8_t *r_source, *colormap;
    uint32_t qcs, qct;
} sdd;
static int r_fullbright;
#include "quake_original_lit_span.h"
defineDrawLitSpan(0, 1) defineDrawLitSpan(1, 1)
defineDrawLitSpan(2, 1) defineDrawLitSpan(3, 1)
defineDrawLitSpan(0, 0) defineDrawLitSpan(1, 0)
defineDrawLitSpan(2, 0) defineDrawLitSpan(3, 0)
static union { uint32_t align; uint8_t bytes[300000]; } storage;
static uint32_t rng = 1731;
static uint32_t random32(void) {
    rng ^= rng << 13; rng ^= rng >> 17; return rng ^= rng << 5;
}
int main(void) {
    typedef void (*fn)(uint8_t *, int, int, int, int, int);
    fn refs[] = {drawLitSpan0_1, drawLitSpan1_1, drawLitSpan2_1, drawLitSpan3_1,
        drawLitSpan0_0, drawLitSpan1_0, drawLitSpan2_0, drawLitSpan3_0};
    uint8_t packet[72 + 48], expected[256], *m = storage.bytes;
    uint32_t *lights = (uint32_t *)(void *)m;
    pxa_game_render_spans_header_wire_t h = {0};
    h.magic = PXA_GAME_RENDER_SPANS_MAGIC; h.major = 1; h.count = 2;
    h.light_width = h.light_height = 192; h.light_bytes = 192 * 192 * 4;
    h.light_origin_s = h.light_origin_t = -64;
    h.texture_offset = h.light_bytes; h.colormap_offset = h.texture_offset + 16384;
    h.colormap_bytes = 65536; h.colormap_levels = 256;
    h.target_offset = h.colormap_offset + h.colormap_bytes; h.target_bytes = 256;
    for (unsigned i = 0; i < 192 * 192; ++i)
        lights[i] = (random32() & 4095) | ((random32() & 4095) << 16);
    for (unsigned i = h.texture_offset; i < h.target_offset; ++i) m[i] = random32();
    sdd.blocklights = lights + 64 * 192 + 64; sdd.r_lightwidth = 192;
    sdd.r_source = m + h.texture_offset; sdd.colormap = m + h.colormap_offset;
    unsigned cases = 0;
    for (unsigned kind = 0; kind < 8; ++kind) {
        h.texture_width = kind < 4 ? 64 : 48; h.texture_height = kind < 4 ? 64 : 80;
        h.texture_bytes = h.texture_width * h.texture_height;
        h.light_shift = 4 - (kind & 3);
        h.wrap_s = sdd.qcs = UINT32_MAX / h.texture_width + 1;
        h.wrap_t = sdd.qct = UINT32_MAX / h.texture_height + 1;
        sdd.r_texture_width = h.texture_width; sdd.r_texture_height = h.texture_height;
        for (unsigned j = 0; j < 25000; ++j) {
            pxa_game_render_span_wire_t s = {16, 1 + random32() % 32,
                (int32_t)(random32() % (64 * 65536)),
                (int32_t)(random32() % (64 * 65536)),
                ((int32_t)(random32() % 9) - 4) * 16384,
                ((int32_t)(random32() % 9) - 4) * 16384};
            if (j % 3 == 0) { s.s -= 96 * 65536; s.t -= 96 * 65536; }
            r_fullbright = j & 1;
            h.flags = (kind < 4 ? PXA_GAME_RENDER_SPANS_POW2 : 0) |
                (r_fullbright ? PXA_GAME_RENDER_SPANS_FULLBRIGHT : 0);
            pxa_game_render_spans_header_encode(packet, sizeof(packet), &h);
            pxa_game_render_span_encode(packet + 72, 24, &s);
            /* Ordered overlapping spans must also survive range splitting. */
            pxa_game_render_span_wire_t s2 = s; s2.destination += 3;
            pxa_game_render_span_encode(packet + 96, 24, &s2);
            memset(expected, 0xa5, sizeof(expected));
            refs[kind](expected + s.destination, s.count, s.s, s.t, s.ds, s.dt);
            refs[kind](expected + s2.destination, s2.count, s2.s, s2.t, s2.ds, s2.dt);
            pxa_raster_spans_t batch;
            assert(pxa_raster_spans_validate(packet, sizeof(packet), m,
                sizeof(storage.bytes), 2, &batch) == PXA_STATUS_OK);
            memset(m + h.target_offset, 0xa5, 256);
            unsigned split = 1 + random32() % 255;
            pxa_raster_spans_execute_range(&batch, 0, split);
            pxa_raster_spans_execute_range(&batch, split, 256);
            assert(!memcmp(expected, m + h.target_offset, 256));
            ++cases;
        }
    }
    pxa_raster_spans_t batch;
    memset(m + h.target_offset, 0xa5, 256);
    h.target_offset = 0; /* target aliases light source */
    pxa_game_render_spans_header_encode(packet, sizeof(packet), &h);
    assert(pxa_raster_spans_validate(packet, sizeof(packet), m,
        sizeof(storage.bytes), 2, &batch) == PXA_STATUS_INVALID_ARGUMENT);
    h.target_offset = 299999; h.target_bytes = 256;
    pxa_game_render_spans_header_encode(packet, sizeof(packet), &h);
    assert(pxa_raster_spans_validate(packet, sizeof(packet), m,
        sizeof(storage.bytes), 2, &batch) == PXA_STATUS_INVALID_ARGUMENT);
    puts("PASS: 200000 upstream comparisons, 400000 spans; split/ordered overlap, negative coordinates, 8 kernels and output guards");
    printf("validated_batch_bytes=%zu scratch_bytes=0 heap_allocations=0\n", sizeof(batch));
    return cases != 200000;
}
