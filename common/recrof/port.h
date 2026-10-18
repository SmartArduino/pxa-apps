#pragma once
#include <stddef.h>
#include <stdint.h>
#include "framebuffer.h"
#ifndef PORT_COOPERATE_BUDGET_US
#define PORT_COOPERATE_BUDGET_US 8000u
#endif
#ifndef PORT_TICK_MS
#define PORT_TICK_MS 16u
#endif
#ifdef __cplusplus
extern "C" {
#endif
void port_game_main(void);
void port_game_key(int key, int down);
void port_game_motion(int dx, int dy);
void port_game_audio(void);
void port_yield(void);
void port_sleep(unsigned ms);
uint64_t port_time_us(void);
int port_cooperate_due(uint64_t now, uint32_t budget_us);
void port_palette(const uint8_t *rgb, int sixbit);
void port_palette_rgb565(const uint16_t *rgb, int swapped);
void port_frame(const uint8_t *pixels, int width, int height, int pitch);
void port_frame_cursor(const uint8_t *pixels, int width, int height, int pitch,
                        const port_cursor_t *cursor);
void port_pcm(const int16_t *samples, unsigned frames, unsigned rate,
              unsigned channels);
void port_error(const char *message);
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
