/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "pxa/raster_spans.h"
#define SPAN_PRECISION 6
#define mipShift(m) (4 - (m))
#define mipMask(m) ((1 << mipShift(m)) - 1)
#define ESP_PLATFORM 1
#define ESPANS_HAVE_V 0
#define ESP_LIT_SPAN 1
#define MAX_SPAN_COUNT 32
#define D_FIXED_Z(zi) (65536.0f * (1.0f / (zi)))
#define QDFLOAT float
#define WIN32 0
#define WIDTH 320
#define HEIGHT 8
#define TARGET_BYTES (WIDTH * HEIGHT)
static struct {
    const uint32_t *blocklights;
    int r_lightwidth, r_texture_width, r_texture_height;
    int r_surf_soffs, r_surf_toffs;
    const uint8_t *r_source, *colormap;
    uint32_t qcs, qct;
} sdd;
static int r_fullbright;
#include "quake_original_lit_span.h"
defineDrawLitSpan(0, 1) defineDrawLitSpan(1, 1)
defineDrawLitSpan(2, 1) defineDrawLitSpan(3, 1)
defineDrawLitSpan(0, 0) defineDrawLitSpan(1, 0)
defineDrawLitSpan(2, 0) defineDrawLitSpan(3, 0)
static void (*drawLitSpan)(uint8_t *, int, int, int, int, int);
typedef int32_t fixed16_t;
typedef uint8_t byte;
typedef struct { unsigned u, count, next; } espan_t;
static struct {
    float d_sdivzorigin, d_sdivzstepu, d_sdivzstepv;
    float d_tdivzorigin, d_tdivzstepu, d_tdivzstepv;
    float d_ziorigin, d_zistepu, d_zistepv;
    int sadjust, tadjust, bbextents, bbextentt;
    byte *d_viewbuffer;
    short *d_pzbuffer;
    espan_t *basespan_p;
    uint8_t *basespanV_p;
} globals, *_g = &globals;
static const int screenwidth = WIDTH, d_zwidth = WIDTH;
static espan_t *getNextSpan(espan_t *s, int null_end) {
    return !s->next && null_end ? NULL : _g->basespan_p + s->next;
}
#include "quake_reciprocal.h"
#include "quake_original_projected.h"
static union { uint32_t align; uint8_t bytes[300000]; } storage;
static uint32_t rng = 1731;
static uint32_t random32(void) {
    rng ^= rng << 13; rng ^= rng >> 17; return rng ^= rng << 5;
}
static uint32_t bits(float v) { uint32_t b; memcpy(&b, &v, 4); return b; }
int main(void) {
    typedef void (*fn)(uint8_t *, int, int, int, int, int);
    fn refs[] = {drawLitSpan0_1, drawLitSpan1_1, drawLitSpan2_1, drawLitSpan3_1,
        drawLitSpan0_0, drawLitSpan1_0, drawLitSpan2_0, drawLitSpan3_0};
    uint8_t packet[152 + 4 * 8], expected[TARGET_BYTES];
    union { max_align_t align; uint8_t bytes[4096]; } prepared;
    union { uint64_t align; uint8_t bytes[2 * TARGET_BYTES]; } depth;
    uint8_t *m = storage.bytes;
    uint32_t *lights = (uint32_t *)(void *)(m + 65536);
    pxa_game_render_spans_header_wire_t h = {0};
    h.magic = PXA_GAME_RENDER_PROJECTED_MAGIC; h.major = 1; h.count = 4;
    h.light_offset = 65536;
    h.light_width = h.light_height = 128; h.light_bytes = 128 * 128 * 4;
    h.texture_offset = h.light_offset + h.light_bytes; h.colormap_offset = h.texture_offset + 16384;
    h.colormap_bytes = 65536; h.colormap_levels = 256;
    h.target_offset = h.colormap_offset + h.colormap_bytes; h.target_bytes = TARGET_BYTES;
    for (unsigned i = 0; i < 128 * 128; ++i)
        lights[i] = (random32() & 4095) | ((random32() & 4095) << 16);
    for (unsigned i = h.texture_offset; i < h.target_offset; ++i) m[i] = random32();
    sdd.r_lightwidth = 128; sdd.r_source = m + h.texture_offset;
    sdd.colormap = m + h.colormap_offset;
    espan_t spans[5] = {0}; uint8_t ys[5] = {0};
    _g->basespan_p = spans; _g->basespanV_p = ys;
    _g->d_viewbuffer = expected; _g->d_pzbuffer = (short *)(void *)depth.bytes;
    unsigned cases = 0;
    for (unsigned kind = 0; kind < 8; ++kind) {
        h.texture_width = kind < 4 ? 64 : 48; h.texture_height = kind < 4 ? 64 : 80;
        h.texture_bytes = h.texture_width * h.texture_height; h.light_shift = 4 - (kind & 3);
        h.wrap_s = sdd.qcs = UINT32_MAX / h.texture_width + 1;
        h.wrap_t = sdd.qct = UINT32_MAX / h.texture_height + 1;
        sdd.r_texture_width = h.texture_width; sdd.r_texture_height = h.texture_height;
        drawLitSpan = refs[kind];
        for (unsigned j = 0; j < 5000; ++j) {
            pxa_game_render_projection_wire_t p = {0};
            _g->d_ziorigin = (5 + random32() % 90) / 1000.0f;
            _g->d_zistepu = ((int)(random32() % 21) - 10) / 1000000.0f;
            _g->d_zistepv = ((int)(random32() % 21) - 10) / 1000000.0f;
            _g->d_sdivzorigin = (8 + random32() % 32) * _g->d_ziorigin;
            _g->d_tdivzorigin = (8 + random32() % 32) * _g->d_ziorigin;
            _g->d_sdivzstepu = ((int)(random32() % 25) - 12) / 100.0f * _g->d_ziorigin;
            _g->d_tdivzstepu = ((int)(random32() % 25) - 12) / 100.0f * _g->d_ziorigin;
            _g->d_sdivzstepv = 0.1f * _g->d_ziorigin;
            _g->d_tdivzstepv = -0.1f * _g->d_ziorigin;
#define COPY(dst, src) p.dst##_bits = bits(_g->src)
            COPY(s_origin, d_sdivzorigin); COPY(s_du, d_sdivzstepu); COPY(s_dv, d_sdivzstepv);
            COPY(t_origin, d_tdivzorigin); COPY(t_du, d_tdivzstepu); COPY(t_dv, d_tdivzstepv);
            COPY(z_origin, d_ziorigin); COPY(z_du, d_zistepu); COPY(z_dv, d_zistepv);
#undef COPY
            p.s_adjust = _g->sadjust = (int)(random32() % 65536) - 32768;
            p.t_adjust = _g->tadjust = (int)(random32() % 65536) - 32768;
            p.s_max = _g->bbextents = (64 << 16) - 1;
            p.t_max = _g->bbextentt = (64 << 16) - 1;
            p.s_offset = sdd.r_surf_soffs = ((int)(j % 3) - 1) * 16;
            p.t_offset = sdd.r_surf_toffs = ((int)(j % 5) - 2) * 16;
            h.light_origin_s = sdd.r_surf_soffs >> h.light_shift;
            h.light_origin_t = sdd.r_surf_toffs >> h.light_shift;
            sdd.blocklights = lights - h.light_origin_s - h.light_origin_t * 128;
            p.pitch = WIDTH;
            p.flags = j & 1 ? PXA_GAME_RENDER_PROJECTED_WRITE_DEPTH : 0;
            if (p.flags) { p.depth_offset = h.target_offset + TARGET_BYTES;
                p.depth_bytes = TARGET_BYTES * 2; p.depth_pitch = WIDTH * 2; }
            r_fullbright = (j >> 1) & 1;
            h.flags = (kind < 4 ? PXA_GAME_RENDER_SPANS_POW2 : 0) |
                (r_fullbright ? PXA_GAME_RENDER_SPANS_FULLBRIGHT : 0);
            pxa_game_render_spans_header_encode(packet, sizeof(packet), &h);
            pxa_game_render_projection_encode(packet + 72, 80, &p);
            for (unsigned k = 0; k < 4; ++k) {
                pxa_game_render_row_wire_t row = {random32() % 20,
                    random32() % HEIGHT, 1 + random32() % 288, 0};
                if (j % 7 == 0) row.count = (j % 65) + 1;
                spans[k + 1] = (espan_t){row.x, row.count, k == 3 ? 0 : k + 2};
                ys[k + 1] = row.y;
                pxa_game_render_row_encode(packet + 152 + k * 8, 8, &row);
            }
            memset(expected, 0xa5, sizeof(expected)); memset(depth.bytes, 0x5a, sizeof(depth.bytes));
            D_DrawSpans8Rows(spans + 1, -1);
            if (p.flags) D_DrawZSpansRows(spans + 1, -1);
            pxa_raster_spans_t batch;
            assert(pxa_raster_projected_spans_validate(packet, sizeof(packet), m,
                sizeof(storage.bytes), 4, &batch) == PXA_STATUS_OK);
            memset(m + h.target_offset, 0xa5, TARGET_BYTES);
            memset(m + h.target_offset + TARGET_BYTES, 0x5a, TARGET_BYTES * 2);
            unsigned split = 1 + random32() % (TARGET_BYTES - 1);
            pxa_raster_spans_execute_range(&batch, split, TARGET_BYTES);
            pxa_raster_spans_execute_range(&batch, 0, split);
            if (memcmp(expected, m + h.target_offset, TARGET_BYTES) ||
                memcmp(depth.bytes, m + h.target_offset + TARGET_BYTES, TARGET_BYTES * 2)) {
                fprintf(stderr, "mismatch kind=%u case=%u depth=%u\n", kind, j, p.flags); return 1;
            }
            ++cases;
            /* The retained projection and bounded-arena fallback must both
             * match the original functions, including split depth writes. */
            for (unsigned capacity = 128; capacity <= sizeof(prepared.bytes); capacity *= 32) {
                memset(prepared.bytes, 0xc7, sizeof(prepared.bytes));
                assert(pxa_raster_projected_spans_prepare(packet, sizeof(packet), m,
                    sizeof(storage.bytes), 4, prepared.bytes, capacity, &batch) == PXA_STATUS_OK);
                assert((batch.prepared != NULL) == (capacity == sizeof(prepared.bytes)));
                if (j & 1) {
                    split = pxa_raster_spans_split_hint(&batch, (uint8_t)(1u + j % 255u));
                    assert(split <= TARGET_BYTES);
                }
                for (unsigned guard = capacity; guard < sizeof(prepared.bytes); ++guard)
                    assert(prepared.bytes[guard] == 0xc7);
                memset(m + h.target_offset, 0xa5, TARGET_BYTES);
                memset(m + h.target_offset + TARGET_BYTES, 0x5a, TARGET_BYTES * 2);
                pxa_raster_spans_execute_range(&batch, split, TARGET_BYTES);
                pxa_raster_spans_execute_range(&batch, 0, split);
                assert(!memcmp(expected, m + h.target_offset, TARGET_BYTES));
                assert(!memcmp(depth.bytes, m + h.target_offset + TARGET_BYTES, TARGET_BYTES * 2));
            }
        }
    }
    printf("PASS: %u original projection/light/depth cases in ordinary, prepared and exhausted-arena paths; split ranges, overlap, negative offsets, eight kernels\n", cases);
    return cases != 40000;
}
