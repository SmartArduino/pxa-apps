/* Direct indexed-color to mapped RGB565 output. GPL-3.0-or-later. */
#include "framebuffer.h"
#include "font8x8.h"
#include <string.h>

static uint16_t source_x[320], source_y[240];
static int old_dw, old_dh, old_sw, old_sh;

void port_blit(uint16_t *dest, int dw, int dh, const uint8_t *pixels,
               int sw, int sh, int pitch, const uint16_t palette[256],
               const port_cursor_t *cursor) {
  if (dw != old_dw || dh != old_dh || sw != old_sw || sh != old_sh) {
    /* Keep the original 320x240 logical sampling grid, including on displays
     * such as 296x222. This avoids a change in the cursor or pixel positions. */
    for (int x = 0; x < dw; ++x) source_x[x] = (x * 320 / dw) * sw / 320;
    for (int y = 0; y < dh; ++y) source_y[y] = (y * 240 / dh) * sh / 240;
    old_dw = dw; old_dh = dh; old_sw = sw; old_sh = sh;
  }
  for (int y = 0; y < dh; ++y) {
    const uint8_t *row = pixels + (size_t)source_y[y] * pitch;
    uint16_t *out = dest + (size_t)y * dw;
    if (dw == 320 && sw == 320)
      for (int x = 0; x < dw; ++x) out[x] = palette[row[x]];
    else
      for (int x = 0; x < dw; ++x) out[x] = palette[row[source_x[x]]];
  }
  if (!cursor || !cursor->pixels || cursor->width <= 0 || cursor->height <= 0)
    return;
  int x0 = 0, x1 = dw;
  while (x0 < x1 && source_x[x0] < cursor->x) ++x0;
  while (x1 > x0 && source_x[x1-1] >= cursor->x + cursor->width) --x1;
  for (int y = 0; y < dh; ++y) {
    int cy = source_y[y] - cursor->y;
    if (cy < 0 || cy >= cursor->height) continue;
    const uint8_t *row = cursor->pixels + (size_t)cy * cursor->width;
    for (int x = x0; x < x1; ++x) {
      uint8_t color = row[source_x[x] - cursor->x];
      if (color) dest[y * dw + x] = palette[color];
    }
  }
}

static int scaled(int value, int size, int logical) {
  return (value * size + logical - 1) / logical;
}
static void pixel(uint16_t *dest, int w, int h, int x, int y, uint16_t c) {
  if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
  for (int dy = scaled(y,h,240); dy < scaled(y+1,h,240); ++dy)
    for (int dx = scaled(x,w,320); dx < scaled(x+1,w,320); ++dx)
      dest[dy*w+dx] = c;
}
void port_draw_text(uint16_t *dest, int w, int h, int x, int y, const char *s) {
  for (; *s; ++s, x += 8) {
    if (*s == '\n') {x = 8; y += 10; continue;}
    if (x > 312) {x = 8; y += 10;}
    if (y > 230) break;
    const uint8_t *glyph = font8x8_basic[(unsigned char)*s & 127];
    for (int j = 0; j < 8; ++j)
      for (int i = 0; i < 8; ++i)
        if (glyph[j] & (1 << i)) pixel(dest,w,h,x+i,y+j,0xffff);
  }
}
void port_draw_controls(uint16_t *dest, int w, int h, int rts) {
  static const struct {int x,y,w,h; const char *label;} buttons[] = {
    {0,0,38,23,"MENU"}, {280,0,40,24,"WPN"}, {276,116,44,26,"USE"},
    {276,148,44,28,"JMP"}, {268,192,52,48,"FIRE"}, {280,0,40,24,"RMB"}};
  for (unsigned i = rts ? 5 : 0; i < (rts ? 6 : 5); ++i) {
    int bx=buttons[i].x, by=buttons[i].y, bw=buttons[i].w, bh=buttons[i].h;
    for (int y=scaled(by,h,240); y<scaled(by+bh,h,240); ++y)
      for (int x=scaled(bx,w,320); x<scaled(bx+bw,w,320); ++x) {
        int lx=x*320/w, ly=y*240/h;
        uint16_t *p=&dest[y*w+x];
        *p=lx==bx || lx==bx+bw-1 || ly==by || ly==by+bh-1
               ? 0xffff : rts ? 0 : (*p & 0xf7de) >> 1;
      }
    port_draw_text(dest,w,h,bx+(bw-(int)strlen(buttons[i].label)*8)/2,
                    by+(bh-8)/2,buttons[i].label);
  }
}
