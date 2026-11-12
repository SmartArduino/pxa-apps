// SPDX-License-Identifier: GPL-3.0-only
#ifdef MINDUSTRY_LEGACY_RENDERER
#include "../common/pocket-game/runtime.hpp"
#include "render.hpp"
#include <algorithm>
namespace gfx {
void preload(const char *, int) {}
void icon(const art::Sprite &s, int x, int y, int w, int h, uint16_t color) {
  sprite(s, x, y, w, h, 0, 255, color);
}
void terrain(const md::State &s, int ox, int oy, int zoom, bool title) {
  for (int y = std::max(0, -oy / zoom - 1);
       y < std::min(md::H, (pocket::HEIGHT - oy) / zoom + 1); ++y)
    for (int x = std::max(0, -ox / zoom - 1);
         x < std::min(md::W, (pocket::WIDTH - ox) / zoom + 1); ++x) {
      auto &t = s.tiles[y * md::W + x];
      auto n = md::noise(s.seed, x, y);
      int px = ox + x * zoom, py = oy + y * zoom;
      auto &floor = t.ground == md::Sandy   ? art::sand1
                    : t.ground == md::Water ? art::water
                    : s.sector == 1         ? art::snow
                    : n % 3 == 0            ? art::stone1
                    : n % 3 == 1            ? art::stone2
                                            : art::stone3;
      sprite(floor, px, py, zoom, zoom, 0, title ? 110 : 205);
      if (t.ore < 3 || t.ore == md::Titanium)
        sprite(t.ore == md::Copper     ? art::copper
               : t.ore == md::Lead     ? art::lead
               : t.ore == md::Titanium ? art::titanium
                                       : art::coal,
               px, py, zoom, zoom, 0, title ? 110 : 235);
      if (t.ground == md::Rock)
        sprite(art::rock, px, py - zoom / 3, zoom, zoom * 4 / 3, 0,
               title ? 100 : 210);
    }
}
void minimap(const md::State &s, int x, int y, int scale) {
  for (int yy = 0; yy < md::H; ++yy)
    for (int xx = 0; xx < md::W; ++xx) {
      auto &t = s.tiles[yy * md::W + xx];
      auto c = t.ground == md::Water  ? pocket::rgb(55, 81, 119)
               : t.ground == md::Rock ? pocket::rgb(32, 33, 39)
               : t.ore == md::Copper  ? pocket::rgb(146, 104, 67)
                                      : pocket::rgb(83, 83, 90);
      pocket::rect(x + xx * scale, y + yy * scale, scale, scale, c);
    }
  for (auto &b : s.buildings)
    if (b.type)
      pocket::rect(x + b.x * scale, y + b.y * scale,
                   md::defs[b.type].size * scale, md::defs[b.type].size * scale,
                   pocket::rgb(255, 211, 127));
}
} // namespace gfx
#endif
