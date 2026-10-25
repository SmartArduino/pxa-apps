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
/* Insets are in the 320x240 logical control space. Updates happen on layout
 * changes; drawing and hit-testing share the same translated rectangles. */
void port_controls_safe_area(int top, int right, int bottom, int left);
/* Visibility gates both drawing and hit-testing; returns whether it changed. */
int port_controls_set_visible(int visible);
int port_controls_visible(void);
int port_control_key_at(int x, int y, int rts);
void port_draw_controls(uint16_t *dest, int width, int height, int rts);
void port_draw_text(uint16_t *dest, int width, int height,
                    int x, int y, const char *text);
#ifdef __cplusplus
}
#endif
