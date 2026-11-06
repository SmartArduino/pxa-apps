// SPDX-License-Identifier: MIT
#pragma once
#include "art.hpp"
namespace farm_ui {
enum class Icon { Backpack, Journal, Pause, Close, Back, Sprinkler, Use };
// Native 16px interface icons share the world's warm wood and sage palette.
template <class Canvas> void icon(Canvas &r, int x, int y, Icon value) {
  constexpr auto ink = art::Ink, paper = farm_palette::Highlight,
                 gold = farm_palette::Brass;
  constexpr auto sage = farm_palette::Sage, wood = farm_palette::Button;
  auto box = [&](int xx, int yy, int w, int h, uint16_t c) {
    r.rect(x + xx, y + yy, w, h, c);
  };
  switch (value) {
  case Icon::Use:
    box(6, 1, 3, 9, ink);
    box(3, 5, 3, 6, ink);
    box(9, 5, 3, 6, ink);
    box(12, 7, 2, 5, ink);
    box(4, 9, 9, 6, ink);
    box(7, 2, 1, 8, paper);
    box(4, 6, 1, 5, paper);
    box(10, 6, 1, 5, paper);
    box(5, 10, 7, 4, gold);
    break;
  case Icon::Backpack:
    box(5, 0, 6, 4, ink);
    box(6, 1, 4, 2, wood);
    box(1, 6, 3, 8, ink);
    box(12, 6, 3, 8, ink);
    box(3, 4, 10, 12, ink);
    box(4, 5, 8, 10, wood);
    box(3, 4, 10, 4, gold);
    box(4, 4, 8, 1, paper);
    box(5, 10, 6, 4, ink);
    box(6, 10, 4, 3, farm_palette::Selected);
    box(7, 7, 2, 3, paper);
    break;
  case Icon::Journal:
    box(2, 1, 12, 14, ink);
    box(3, 2, 10, 12, sage);
    box(4, 2, 1, 12, farm_palette::GrassLight);
    box(12, 3, 2, 10, paper);
    box(7, 5, 4, 2, art::Leaf);
    box(6, 7, 4, 2, art::Leaf);
    r.line(x + 7, y + 10, x + 10, y + 5, paper);
    box(8, 12, 2, 4, art::rgb(177, 94, 62));
    break;
  case Icon::Pause:
    box(4, 3, 3, 11, ink);
    box(10, 3, 3, 11, ink);
    box(4, 3, 2, 1, paper);
    box(10, 3, 2, 1, paper);
    break;
  case Icon::Close:
    for (int i = 0; i < 8; ++i) {
      box(4 + i, 4 + i, 2, 2, ink);
      box(11 - i, 4 + i, 2, 2, ink);
    }
    break;
  case Icon::Back:
    r.line(x + 10, y + 3, x + 4, y + 8, ink);
    r.line(x + 11, y + 3, x + 5, y + 8, ink);
    r.line(x + 4, y + 8, x + 10, y + 13, ink);
    r.line(x + 5, y + 8, x + 11, y + 13, ink);
    box(5, 7, 9, 2, ink);
    break;
  case Icon::Sprinkler:
    box(6, 2, 4, 12, ink);
    box(7, 3, 2, 10, art::rgb(154, 183, 172));
    box(2, 4, 12, 3, ink);
    box(2, 4, 12, 1, paper);
    box(5, 13, 6, 2, wood);
    box(7, 3, 2, 4, gold);
    break;
  }
}
} // namespace farm_ui
