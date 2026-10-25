#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "framebuffer.h"
#ifdef PXA_QUAKE_RENDER_PROBE
/* Private comparison builds only; changed between completed frames. */
extern unsigned port_render_probe_mode;
#define PORT_RENDER_PROBE_NATIVE_BLIT 1u
#define PORT_RENDER_PROBE_FRAME_SPANS 2u
#endif
#ifndef PORT_COOPERATE_BUDGET_US
#define PORT_COOPERATE_BUDGET_US 8000u
#endif
#ifndef PORT_TICK_MS
#define PORT_TICK_MS 16u
#endif
#ifndef PXA_RECROF_BULK_READ_COPY
#define PXA_RECROF_BULK_READ_COPY 0
#endif
/* Keep small metadata copies under the ordinary compiler settings. Only large
 * read-only file/cache transfers opt into the WASM bulk instruction. */
#if PXA_RECROF_BULK_READ_COPY && defined(__wasm__) && defined(__clang__)
__attribute__((target("bulk-memory"), noinline))
static void port_copy_read_bulk(void *destination, const void *source, size_t bytes) {
    __builtin_memcpy(destination, source, bytes);
}
#endif
static inline void port_copy_read(void *destination, const void *source, size_t bytes) {
#if PXA_RECROF_BULK_READ_COPY && defined(__wasm__) && defined(__clang__)
    if (bytes >= 128) { port_copy_read_bulk(destination, source, bytes); return; }
#endif
    memcpy(destination, source, bytes);
}
#ifdef __cplusplus
extern "C" {
#endif
void port_game_main(void);
void port_game_key(int key, int down);
void port_game_motion(int dx, int dy);
void port_game_reset_motion(void);
void port_sample_controller(uint32_t elapsed_us, int *dx, int *dy);
int port_game_in_menu(void);
#ifdef PORT_RTS
void port_game_scroll(int dx, int dy);
void port_game_move_cursor(int dx, int dy, int *x, int *y);
#endif
void port_game_audio(void);
void port_yield(void);
/* Read pending input immediately before the engine consumes its key queue. */
void port_poll_input(void);
void port_sleep(unsigned ms);
uint64_t port_time_us(void);
int port_cooperate_due(uint64_t now, uint32_t budget_us);
void port_palette(const uint8_t *rgb, int sixbit);
void port_palette_rgb565(const uint16_t *rgb, int swapped);
/* Exclusive temporary bytes in an acquired display buffer. Complete all
 * borrowing before port_frame overwrites it; no pointer survives present. */
void *port_frame_scratch(size_t *bytes);
void port_frame(const uint8_t *pixels, int width, int height, int pitch);
void port_frame_cursor(const uint8_t *pixels, int width, int height, int pitch,
                        const port_cursor_t *cursor);
void port_pcm(const int16_t *samples, unsigned frames, unsigned rate,
              unsigned channels);
/* Borrow the existing playback session only for synchronous Audio operations. */
uint64_t port_audio_session(void);
void port_error(const char *message);
/* RTS startup feedback ends when the engine presents its next real frame. */
void port_loading_stage(const char *stage);
/* RTS absent-file results remain bounded by startup or the next mutation. */
void port_filesystem_loading_cache(int active);
void port_log(const char *fmt, ...);
const char *port_data_path(void);
/* Atomically publish a closed private file, replacing an existing file. */
int port_replace(const char *source, const char *destination);
int port_mouse_x(void);
int port_mouse_y(void);
int port_key_down(int key);
#ifdef __cplusplus
}
#endif
