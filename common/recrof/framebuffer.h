/* Direct indexed-color to mapped RGB565 output. GPL-3.0-or-later. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  const uint8_t *pixels;
  int x, y, width, height;
} port_cursor_t;
void port_blit(uint16_t *dest, int dw, int dh, const uint8_t *pixels,
               int sw, int sh, int pitch, const uint16_t palette[256],
               const port_cursor_t *cursor);
void port_draw_controls(uint16_t *dest, int width, int height, int rts);
void port_draw_text(uint16_t *dest, int width, int height,
                    int x, int y, const char *text);
#ifdef __cplusplus
}
#endif
