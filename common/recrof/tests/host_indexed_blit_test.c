/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pxa/raster_compute.h"
#include "framebuffer.h"
static uint32_t rng = 1731;
static uint32_t random32(void) { rng ^= rng << 13; rng ^= rng >> 17; return rng ^= rng << 5; }
int main(void) {
    uint8_t *m = malloc(1000000), *expected = malloc(320*240*2);
    assert(m && expected);
    uint16_t *palette = (uint16_t *)(void *)(m + 200000);
    for (unsigned i = 0; i < 256; ++i) palette[i] = random32();
    for (unsigned i = 210000; i < 602000; ++i) m[i] = random32();
    unsigned cases = 0;
    for (unsigned i = 0; i < 500; ++i) {
        unsigned dw = (i % 4 >= 2) ? 1 + random32()%320 : 320;
        unsigned dh = (i % 4 >= 2) ? 1 + random32()%240 : 240;
        unsigned sw = (i % 4 >= 2) ? 1 + random32()%700 : 320;
        unsigned sh = (i % 4 >= 2) ? 1 + random32()%500 : 200;
        if (i % 8 == 2 || i % 8 == 3) { dw = 296; dh = 222; sw = 320; sh = 200; }
        /* Original RTS Windows buffers, with and without their mouse cursor. */
        if (i % 16 == 4 || i % 16 == 5) { dw = 320; dh = 240; sw = 640; sh = 400; }
        if (i % 16 == 6 || i % 16 == 7) { dw = 320; dh = 240; sw = 640; sh = 480; }
        unsigned sp = sw + random32()%4;
        port_cursor_t cursor = {.pixels = m + 600000, .width = 8 + random32()%24,
            .height = 8 + random32()%24, .x = (int)(random32()%(sw + 24)) - 12,
            .y = (int)(random32()%(sh + 24)) - 12};
        const port_cursor_t *overlay = i & 1 ? &cursor : NULL;
        pxa_game_render_blit_wire_t h = {0};
        h.source_offset = 210000; h.source_bytes = sp*sh; h.source_width = sw;
        h.source_height = sh; h.source_pitch = sp;
        h.palette_offset = 200000; h.palette_bytes = 512;
        h.target_offset = 64; h.target_bytes = dw*dh*2 + 16;
        h.target_width = dw; h.target_height = dh; h.target_pitch = dw*2;
        if (overlay) {
            h.cursor_offset = 600000; h.cursor_bytes = cursor.width*cursor.height;
            h.cursor_width = cursor.width; h.cursor_height = cursor.height;
            h.cursor_pitch = cursor.width; h.cursor_x = cursor.x; h.cursor_y = cursor.y;
        }
        memset(m, 0xa5, 160000); memset(expected, 0xa5, 320*240*2);
        port_blit((uint16_t *)(void *)expected, dw, dh, m + 210000, sw, sh, sp, palette, overlay);
        pxa_game_render_blit_encode(m + 700000, 64, &h);
        pxa_raster_compute_t batch;
        assert(pxa_raster_blit_validate(m + 700000, 64, m, 1000000, 1, &batch) == 0);
        unsigned bytes = dw*dh*2, split = random32()%(bytes + 1);
        pxa_raster_compute_execute_range(&batch, split, bytes);
        pxa_raster_compute_execute_range(&batch, 0, split);
        assert(!memcmp(m + 64, expected, bytes));
        for (unsigned j = 0; j < 64; ++j) assert(m[j] == 0xa5);
        for (unsigned j = 0; j < 16; ++j) assert(m[64 + bytes + j] == 0xa5);
        ++cases;
    }
    pxa_game_render_blit_wire_t h = {0};
    h.source_offset = 210000; h.source_bytes = 16; h.source_width = h.source_height = h.source_pitch = 4;
    h.palette_offset = 200000; h.palette_bytes = 512; h.target_offset = 64; h.target_bytes = 32;
    h.target_width = h.target_height = 4; h.target_pitch = 8;
    pxa_raster_compute_t batch;
    const unsigned offset[] = {0,8,16,40,44,32,36};
    const uint32_t value[] = {999999,65,700000,600000,2,0,7};
    pxa_game_render_blit_encode(m + 700000, 64, &h);
    for (unsigned i = 0; i < sizeof(offset)/sizeof(*offset); ++i) {
        uint32_t old = pxa_wire_generated_load_u32(m + 700000 + offset[i]);
        pxa_wire_generated_store_u32(m + 700000 + offset[i], value[i]);
        memset(m + 64, 0xa5, 32);
        assert(pxa_raster_blit_validate(m + 700000, 64, m, 1000000, 1, &batch) < 0);
        for (unsigned j = 0; j < 32; ++j) assert(m[64+j] == 0xa5);
        pxa_wire_generated_store_u32(m + 700000 + offset[i], old);
    }
    h.target_pitch = 16; h.target_bytes = 64; h.source_offset = 72; h.source_bytes = 4;
    h.source_height = 1;
    for (unsigned i = 0; i < 4; ++i) m[72 + i] = i + 1;
    pxa_game_render_blit_encode(m + 700000, 64, &h);
    assert(pxa_raster_blit_validate(m + 700000, 64, m, 1000000, 1, &batch) == 0);
    pxa_raster_compute_execute_range(&batch, 0, 64);
    for (unsigned y = 0; y < 4; ++y) for (unsigned x = 0; x < 4; ++x)
        assert(pxa_wire_generated_load_u16(m + 64 + y*16 + x*2) == palette[x + 1]);
    for (unsigned i = 0; i < 4; ++i) assert(m[72+i] == i+1);
    h.source_offset = 210000; h.source_bytes = 16; h.source_height = 4;
    h.target_pitch = 200000; h.target_height = 2; h.target_bytes = 200008;
    pxa_game_render_blit_encode(m + 700000, 64, &h);
    assert(pxa_raster_blit_validate(m + 700000, 64, m, 1000000, 1, &batch) == PXA_STATUS_INVALID_ARGUMENT);
    printf("PASS: %u legacy indexed frame/cursor comparisons, 320x200/240 and arbitrary scale, odd byte splits, guards and rejection\n", cases);
    free(expected); free(m); return 0;
}
