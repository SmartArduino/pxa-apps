#include "pxa_game_render_spans.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint8_t pixels[320 * 200], texture[64 * 64], colormap[64 * 256];
static uint32_t lights[34 * 34];
static uint8_t packet[72 + 200 * 24];
static uint64_t stamp(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
int32_t pxa_app_start(const uint8_t *config, uint32_t size) {
    (void)config; (void)size;
    return pxa_game_render_create_raster(1, 256);
}
void pxa_app_stop(uint32_t reason) { (void)reason; }
int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t size) {
    pxa_event_t event;
    pxa_game_render_raster_result_t result;
    if (!pxa_parse_event(bytes, size, &event) ||
        !pxa_game_render_parse_raster(&event, 1, &result)) return PXA_EVENT_UNHANDLED;
    if (result.status) return result.status;
    for (unsigned y = 0; y < 64; ++y)
        for (unsigned x = 0; x < 64; ++x) texture[y * 64 + x] = (x * 3 + y * 5) & 255;
    for (unsigned row = 0; row < 64; ++row)
        for (unsigned i = 0; i < 256; ++i) colormap[row * 256 + i] = i ^ row;
    for (unsigned i = 0; i < 34 * 34; ++i) lights[i] = 1023u | (1023u << 16);
    pxa_game_render_spans_header_wire_t h = {0};
    h.target_offset = (uint32_t)(uintptr_t)pixels; h.target_bytes = sizeof(pixels);
    h.texture_offset = (uint32_t)(uintptr_t)texture; h.texture_bytes = sizeof(texture);
    h.texture_width = h.texture_height = 64;
    h.light_offset = (uint32_t)(uintptr_t)lights; h.light_bytes = sizeof(lights);
    h.light_width = h.light_height = 34;
    h.colormap_offset = (uint32_t)(uintptr_t)colormap; h.colormap_bytes = sizeof(colormap);
    h.colormap_levels = 64; h.light_shift = 4; h.flags = PXA_GAME_RENDER_SPANS_POW2;
    pxa_game_render_span_batch_t batch;
    pxa_game_render_span_batch_begin(&batch, packet, sizeof(packet), &h);
    for (unsigned y = 0; y < 200; ++y) {
        pxa_game_render_span_wire_t span = {y * 320, 320, 0, (int32_t)y * 65536, 65536, 0};
        if (!pxa_game_render_span_append(&batch, &span)) return PXA_STATUS_INTERNAL;
    }
    const uint64_t started = stamp();
    uint8_t *grown_pixels = pixels;
    for (unsigned i = 0; i < 8; ++i) {
        /* This package deliberately uses unpinned memory. The next import
         * must resolve a fresh base and committed size after growth. */
        if (i == 4) {
            size_t previous_pages = __builtin_wasm_memory_grow(0, 1);
            if (previous_pages == (size_t)-1) return PXA_STATUS_RESOURCE_LIMIT;
            grown_pixels = (uint8_t *)(uintptr_t)(previous_pages * 65536u);
            /* Force validation and writes into the newly committed page. */
            pxa_store_u32(packet + 16, (uint32_t)(uintptr_t)grown_pixels);
        }
        int r = pxa_game_render_execute_spans(result.handle, &batch);
        if (r < 0) return r;
    }
    const uint64_t elapsed = stamp() - started;
    unsigned mismatches = 0;
    for (unsigned y = 0; y < 200; ++y)
        for (unsigned x = 0; x < 320; ++x) {
            uint8_t expected = (uint8_t)(texture[(y & 63) * 64 + (x & 63)] ^ 63);
            mismatches += pixels[y * 320 + x] != expected;
            mismatches += grown_pixels[y * 320 + x] != expected;
        }
    uint8_t stats[64] = {0};
    int status = pxa_io(result.handle, 0x105, stats, sizeof(stats));
    char line[300];
    snprintf(line, sizeof(line), "PXA NATIVE SPANS pass=%u mismatches=%u batches=%llu pixels=%llu elapsed_us=%llu validate_us=%llu native_us=%llu zero_scratch=1 grow_pages=1",
        !mismatches && status == 64, mismatches,
        (unsigned long long)pxa_load_u64(stats), (unsigned long long)pxa_load_u64(stats + 24),
        (unsigned long long)elapsed, (unsigned long long)pxa_load_u64(stats + 32),
        (unsigned long long)pxa_load_u64(stats + 40));
    pxa_log_write(2, line);
    if (mismatches || status != 64) return PXA_STATUS_INTERNAL;
    const uint64_t validation_before = pxa_load_u64(stats + 32);
    const uint64_t execution_before = pxa_load_u64(stats + 40);
    const uint64_t small_started = stamp();
    h.target_offset = (uint32_t)(uintptr_t)grown_pixels;
    for (unsigned y = 0; y < 200; ++y) {
        pxa_game_render_span_batch_begin(&batch, packet, sizeof(packet), &h);
        pxa_game_render_span_wire_t span = {y * 320, 320, 0, (int32_t)y * 65536, 65536, 0};
        if (!pxa_game_render_span_append(&batch, &span) ||
            pxa_game_render_execute_spans(result.handle, &batch) != 96)
            return PXA_STATUS_INTERNAL;
    }
    const uint64_t small_elapsed = stamp() - small_started;
    if (pxa_io(result.handle, 0x105, stats, sizeof(stats)) != 64) return PXA_STATUS_INTERNAL;
    snprintf(line, sizeof(line), "PXA SPAN GRANULARITY batches=200 pixels=64000 elapsed_us=%llu validate_us=%llu native_us=%llu",
        (unsigned long long)small_elapsed,
        (unsigned long long)(pxa_load_u64(stats + 32) - validation_before),
        (unsigned long long)(pxa_load_u64(stats + 40) - execution_before));
    pxa_log_write(2, line);
    return mismatches ? PXA_STATUS_INTERNAL : PXA_EVENT_HANDLED;
}
