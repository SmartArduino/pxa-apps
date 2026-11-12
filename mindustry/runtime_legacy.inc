// SPDX-License-Identifier: GPL-3.0-only
// App-local drawing adapter over the existing MIT/CC0 PXA game runtime.
// Common media codecs and lifecycle handling retain their MIT/CC0 notices.
#include "../common/milky-frog/media.hpp"
#include "audio.hpp"
#include <cmath>
#include "render.hpp"
#define PXA_POCKET_MEDIA 1
#include "platform_legacy.inc"
#include "audio_api.inc"
namespace milky {
// Adapted from common/milky-frog/runtime.cpp (MIT).
void configure_screen(int width, int height, uint32_t density,
                      const uint32_t *insets, unsigned shape,
                      const uint32_t *corners) {
  int edges[4]{}, radius = 0;
  for (int i = 0; i < 4; ++i) {
    if (insets)
      edges[i] = int(uint64_t(insets[i]) * (density ? density : 65536) / 65536);
    if (corners)
      radius = std::max(radius, int(uint64_t(corners[i]) *
                                    (density ? density : 65536) / 65536));
  }
  screen = fit_screen(width, height, edges, radius, shape == 2);
  pocket::WIDTH = screen.width;
  pocket::HEIGHT = screen.height;
}
void menu_draw(bool isPaused, int selected, bool saving) {
  auto b = menu_button();
  auto panelColor = pocket::rgb(27, 29, 34), gold = pocket::rgb(255, 211, 127),
       ink = pocket::rgb(230, 232, 235);
  pocket::rect(b.x, b.y, b.w, b.h, panelColor);
  gfx::icon(art::icon_menu,b.x+7,b.y+7,b.w-14,b.h-14,gold);
  if (!isPaused)
    return;
  auto p = pause_panel();
  pocket::rect(p.x, p.y, p.w, p.h, panelColor);
  pocket::outline(p.x, p.y, p.w, p.h, gold);
  system_text_add(p.x + 5, p.y + 8, p.w - 10,
                  saving ? pocket::tr("Saving...", "保存中…")
                         : pocket::tr("Paused", "已暂停"),
                  gold, 2, 1);
  const char *rows[] = {pocket::tr("Resume", "继续游戏"),
                        pocket::tr("Restart", "重新开始"),
                        pocket::tr("Exit", "退出游戏"),
                        pocket::tr("Language: English", "语言：简体中文")};
  for (unsigned i = 0; i < 4; ++i) {
    auto r = pause_row(i);
    pocket::outline(r.x, r.y, r.w, r.h, i == unsigned(selected) ? gold : ink);
    system_text_add(r.x + 3, r.y + (r.h - 12) / 2, r.w - 6, rows[i],
                    i == unsigned(selected) ? gold : ink, 1, 1);
  }
}
} // namespace milky
#include "render.hpp"
namespace gfx {
void toggle_language() { ::toggle_language(); }
void sprite(const art::Sprite &s, int x, int y, int w, int h, int rotation,
            int light, uint16_t tint, bool flipY) {
  if (!canvas || w <= 0 || h <= 0)
    return;
  int x0 = std::max(0, x), y0 = std::max(0, y),
      x1 = std::min(pocket::WIDTH, x + w), y1 = std::min(pocket::HEIGHT, y + h);
  for (int py = y0; py < y1; ++py)
    for (int px = x0; px < x1; ++px) {
      int u = (px - x) * s.w / w, v = (py - y) * s.h / h, a = u, b = v;
      if (rotation == 1) {
        a = s.w - 1 - v * s.w / s.h;
        b = u * s.h / s.w;
      } else if (rotation == 2) {
        a = s.w - 1 - u;
        b = s.h - 1 - v;
      } else if (rotation == 3) {
        a = v * s.w / s.h;
        b = s.h - 1 - u * s.h / s.w;
      }
      if (flipY) b = s.h - 1 - b;
      uint32_t p = s.p[b * s.w + a];
      if (!(p >> 16))
        continue;
      uint16_t c = uint16_t(p);
      if (tint)
        c = uint16_t(((((c >> 11) & 31) * ((tint >> 11) & 31) / 31) << 11) |
                     ((((c >> 5) & 63) * ((tint >> 5) & 63) / 63) << 5) |
                     ((c & 31) * (tint & 31) / 31));
      if (light != 255)
        c = uint16_t(((((c >> 11) & 31) * light / 255) << 11) |
                     ((((c >> 5) & 63) * light / 255) << 5) |
                     ((c & 31) * light / 255));
      canvas[py * pocket::WIDTH + px] = c;
    }
}
void label(int x, int y, int w, const char *text, uint16_t color, int font,
           int align) {
  system_text_add(x, y, w, text, color, 1, unsigned(align));
  if (system_text_count)
    system_texts[system_text_count - 1].font_size = font;
}
} // namespace gfx
