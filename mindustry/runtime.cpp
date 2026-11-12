// SPDX-License-Identifier: GPL-3.0-only
#ifdef MINDUSTRY_LEGACY_RENDERER
#include "runtime_legacy.inc"
#else
// App-local drawing adapter over the existing MIT/CC0 PXA game runtime.
// Common media codecs and lifecycle handling retain their MIT/CC0 notices.
#include "../common/milky-frog/media.hpp"
#include "audio.hpp"
#include <cmath>
#define PXA_POCKET_MEDIA 1
#include "pxa_game_render.h"
#include "render_backend.hpp"

#include "platform.inc"

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
  gfx::label(p.x + 5, p.y + 8, p.w - 10,
             saving ? pocket::tr("Saving...", "保存中…")
                    : pocket::tr("Paused", "已暂停"),
             gold, 22, 1);
  const char *rows[] = {pocket::tr("Resume", "继续游戏"),
                        pocket::tr("Restart", "重新开始"),
                        pocket::tr("Exit", "退出游戏"),
                        pocket::tr("Language: English", "语言：简体中文")};
  for (unsigned i = 0; i < 4; ++i) {
    auto r = pause_row(i);
    pocket::outline(r.x, r.y, r.w, r.h, i == unsigned(selected) ? gold : ink);
    gfx::label(r.x + 3, r.y + (r.h - 12) / 2, r.w - 6, rows[i],
               i == unsigned(selected) ? gold : ink, 12, 1);
  }
}
} // namespace milky

namespace gfx {
void toggle_language() { ::toggle_language(); }
} // namespace gfx

#endif
