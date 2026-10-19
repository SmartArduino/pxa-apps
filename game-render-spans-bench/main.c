#include "pxa_game_render_spans.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint8_t pixels[320 * 200], texture[64 * 64], colormap[64 * 256];
static uint8_t frame_storage[32768];
static uint16_t rgb_palette[256];
static uint16_t depth[320 * 200];
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
    /* The granularity values remain in telemetry; reserve bounded log slots
     * for the four functional paths below. */
    pxa_game_render_projection_wire_t projection = {0};
    projection.z_origin_bits = 0x3d800000; /* 1 / 16 */
    projection.s_du_bits = projection.t_dv_bits = 0x3d800000;
    projection.s_max = (320 << 16) - 1; projection.t_max = (200 << 16) - 1;
    projection.pitch = 320; projection.flags = PXA_GAME_RENDER_PROJECTED_WRITE_DEPTH;
    projection.depth_offset = (uint32_t)(uintptr_t)depth;
    projection.depth_bytes = sizeof(depth); projection.depth_pitch = 640;
    pxa_game_render_projected_batch_t projected;
    pxa_game_render_projected_batch_begin(&projected, packet, sizeof(packet), &h, &projection);
    for (unsigned y = 0; y < 200; ++y) {
        pxa_game_render_row_wire_t row = {0, y, 320, 0};
        if (!pxa_game_render_projected_append(&projected, &row)) return PXA_STATUS_INTERNAL;
    }
    const uint64_t projected_started = stamp();
    const uint64_t projected_validation_before = pxa_load_u64(stats + 32);
    const uint64_t projected_execution_before = pxa_load_u64(stats + 40);
    uint8_t *projected_pixels = grown_pixels;
    for (unsigned i = 0; i < 8; ++i) {
        if (i == 4) {
            size_t previous_pages = __builtin_wasm_memory_grow(0, 1);
            if (previous_pages == (size_t)-1) return PXA_STATUS_RESOURCE_LIMIT;
            projected_pixels = (uint8_t *)(uintptr_t)(previous_pages * 65536u);
            pxa_store_u32(packet + 16, (uint32_t)(uintptr_t)projected_pixels);
        }
        int r = pxa_game_render_execute_projected_spans(result.handle, &projected);
        if (r < 0) return r;
    }
    const uint64_t projected_elapsed = stamp() - projected_started;
    unsigned projected_mismatches = 0;
    for (unsigned y = 0; y < 200; ++y)
        for (unsigned x = 0; x < 320; ++x) {
            /* The upstream final segment floors its fixed reciprocal, then
             * truncates the 16.16 step to 10.6 before sampling. */
            unsigned sx = x < 288 ? x : (288 * 64 + (x - 288) * 63) / 64;
            uint8_t expected = (uint8_t)(texture[(y & 63) * 64 + (sx & 63)] ^ 63);
            projected_mismatches += grown_pixels[y * 320 + x] != expected;
            projected_mismatches += projected_pixels[y * 320 + x] != expected;
            projected_mismatches += depth[y * 320 + x] != 0x0800;
        }
    if (pxa_io(result.handle, 0x105, stats, sizeof(stats)) != 64) return PXA_STATUS_INTERNAL;
    snprintf(line, sizeof(line), "PXA PROJECTED SPANS pass=%u mismatches=%u batches=8 pixels=512000 elapsed_us=%llu validate_us=%llu native_us=%llu zero_scratch=1 grow_pages=1 depth=1",
        !projected_mismatches, projected_mismatches, (unsigned long long)projected_elapsed,
        (unsigned long long)(pxa_load_u64(stats + 32) - projected_validation_before),
        (unsigned long long)(pxa_load_u64(stats + 40) - projected_execution_before));
    pxa_log_write(2, line);
    if (projected_mismatches) return PXA_STATUS_INTERNAL;
    pxa_game_render_frame_batch_t frame;
    pxa_game_render_frame_begin(&frame, frame_storage, sizeof(frame_storage));
    h.target_offset = (uint32_t)(uintptr_t)projected_pixels;
    h.texture_offset = pxa_game_render_frame_copy_source(&frame, texture, sizeof(texture));
    h.light_offset = pxa_game_render_frame_copy_source(&frame, lights, sizeof(lights));
    h.colormap_offset = pxa_game_render_frame_copy_source(&frame, colormap, sizeof(colormap));
    for (unsigned group = 0; group < 32; ++group) {
        pxa_game_render_row_wire_t rows[7]; unsigned count = 0;
        for (unsigned y = group; y < 200; y += 32)
            rows[count++] = (pxa_game_render_row_wire_t){0, y, 320, 0};
        if (!pxa_game_render_frame_append(&frame, &h, &projection, rows, count)) return frame.status;
    }
    const uint64_t frame_started = stamp();
    const uint64_t frame_validation_before = pxa_load_u64(stats + 32);
    const uint64_t frame_execution_before = pxa_load_u64(stats + 40);
    const uint64_t frame_rows_before = pxa_load_u64(stats + 16);
    for (unsigned i = 0; i < 8; ++i) {
        if (i == 4) {
            size_t previous_pages = __builtin_wasm_memory_grow(0, 1);
            if (previous_pages == (size_t)-1) return PXA_STATUS_RESOURCE_LIMIT;
            uint8_t *moved_packet = (uint8_t *)(uintptr_t)(previous_pages * 65536u);
            memcpy(moved_packet, frame.bytes, frame.capacity);
            frame.bytes = moved_packet; /* Source offsets remain packet-relative. */
        }
        int r = pxa_game_render_execute_frame_spans(result.handle, &frame);
        if (r < 0) return r;
    }
    const uint64_t frame_elapsed = stamp() - frame_started;
    unsigned frame_mismatches = 0;
    for (unsigned y = 0; y < 200; ++y) for (unsigned x = 0; x < 320; ++x) {
        unsigned sx = x < 288 ? x : (288 * 64 + (x - 288) * 63) / 64;
        uint8_t expected = (uint8_t)(texture[(y & 63) * 64 + (sx & 63)] ^ 63);
        frame_mismatches += projected_pixels[y * 320 + x] != expected;
        frame_mismatches += depth[y * 320 + x] != 0x0800;
    }
    if (pxa_io(result.handle, 0x105, stats, sizeof(stats)) != 64) return PXA_STATUS_INTERNAL;
    uint64_t frame_rows = pxa_load_u64(stats + 16) - frame_rows_before;
    snprintf(line, sizeof(line), "PXA FRAME SPANS pass=%u mismatches=%u batches=8 materials=32 rows=%llu pixels=512000 elapsed_us=%llu validate_us=%llu native_us=%llu zero_scratch=1 packet_moved=1 depth=1",
        !frame_mismatches && frame_rows == 1600, frame_mismatches, (unsigned long long)frame_rows,
        (unsigned long long)frame_elapsed,
        (unsigned long long)(pxa_load_u64(stats + 32) - frame_validation_before),
        (unsigned long long)(pxa_load_u64(stats + 40) - frame_execution_before));
    pxa_log_write(2, line);
    if (frame_mismatches || frame_rows != 1600) return PXA_STATUS_INTERNAL;
    size_t output_pages = __builtin_wasm_memory_grow(0, 3);
    if (output_pages == (size_t)-1) return PXA_STATUS_RESOURCE_LIMIT;
    uint8_t *rgb_output = (uint8_t *)(uintptr_t)(output_pages * 65536u);
    for (unsigned i = 0; i < 256; ++i) rgb_palette[i] = (uint16_t)(i*257u ^ 0x3412u);
    pxa_game_render_blit_wire_t blit = {0};
    blit.source_offset = (uint32_t)(uintptr_t)projected_pixels; blit.source_bytes = 64000;
    blit.source_width = 320; blit.source_height = 200; blit.source_pitch = 320;
    blit.palette_offset = (uint32_t)(uintptr_t)rgb_palette; blit.palette_bytes = sizeof(rgb_palette);
    blit.target_offset = (uint32_t)(uintptr_t)rgb_output; blit.target_bytes = 320*240*2;
    blit.target_width = 320; blit.target_height = 240; blit.target_pitch = 640;
    blit.cursor_offset = (uint32_t)(uintptr_t)texture; blit.cursor_bytes = 7*64 + 8;
    blit.cursor_width = blit.cursor_height = 8; blit.cursor_pitch = 64;
    blit.cursor_x = 32; blit.cursor_y = 45;
    const uint64_t blit_validation_before = pxa_load_u64(stats + 32);
    const uint64_t blit_execution_before = pxa_load_u64(stats + 40);
    const uint64_t blit_started = stamp();
    for (unsigned i = 0; i < 8; ++i) {
        int r = pxa_game_render_blit_indexed_frame(result.handle, &blit);
        if (r < 0) return r;
    }
    const uint64_t blit_elapsed = stamp() - blit_started;
    unsigned blit_mismatches = 0;
    for (unsigned y = 0; y < 240; ++y) for (unsigned x = 0; x < 320; ++x) {
        unsigned sy = y*200/240;
        uint8_t index = projected_pixels[sy*320 + x];
        if (x >= 32 && x < 40 && sy >= 45 && sy < 53) {
            uint8_t cursor = texture[(sy-45)*64 + x-32];
            if (cursor) index = cursor;
        }
        uint16_t expected = rgb_palette[index];
        blit_mismatches += rgb_output[(y*320 + x)*2] != (uint8_t)expected;
        blit_mismatches += rgb_output[(y*320 + x)*2+1] != (uint8_t)(expected >> 8);
    }
    if (pxa_io(result.handle, 0x105, stats, sizeof(stats)) != 64) return PXA_STATUS_INTERNAL;
    snprintf(line, sizeof(line), "PXA INDEXED BLIT pass=%u mismatches=%u batches=8 pixels=614400 elapsed_us=%llu validate_us=%llu native_us=%llu grow_pages=3 cursor=1",
        !blit_mismatches, blit_mismatches, (unsigned long long)blit_elapsed,
        (unsigned long long)(pxa_load_u64(stats + 32) - blit_validation_before),
        (unsigned long long)(pxa_load_u64(stats + 40) - blit_execution_before));
    pxa_log_write(2, line);
    if (blit_mismatches) return PXA_STATUS_INTERNAL;
    return mismatches ? PXA_STATUS_INTERNAL : PXA_EVENT_HANDLED;
}
