// SPDX-License-Identifier: MIT
#include "../ui_canvas.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
  constexpr uint16_t ink = 0xff11, background = 0x1234;
  std::vector<uint16_t> pixels(480 * 480, background);
  starsoil_ui::Canvas ui{pixels.data(), 480, 480, 288, 288};
  assert(ui.font_scale() == 2 && ui.text_height() == 15);
  assert(ui.line_height() == 17);
  assert(ui.font_scale(2) == 3 && ui.font_scale(3) == 5);
  // Test every embedded glyph at different projection phases. Each bit must
  // become the same exact square, even when its layout origin is fractional.
  for (const auto &g : font::glyphs)
    for (int offset = 0; offset < 3; ++offset) {
      std::fill(pixels.begin(), pixels.end(), background);
      char value[5]{};
      uint32_t cp = g.cp;
      if (cp < 128)
        value[0] = char(cp);
      else if (cp < 2048) {
        value[0] = char(0xc0 | (cp >> 6));
        value[1] = char(0x80 | (cp & 63));
      } else {
        value[0] = char(0xe0 | (cp >> 12));
        value[1] = char(0x80 | ((cp >> 6) & 63));
        value[2] = char(0x80 | (cp & 63));
      }
      ui.text(7 + offset, 9 + offset, value, ink);
      for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x) {
          bool bit = g.rows[y / 2] & (1 << (11 - x / 2));
          assert(pixels[(ui.y(9 + offset) + y) * 480 + ui.x(7 + offset) + x] ==
                 (bit ? ink : background));
        }
      // Advances use the native physical width; layout measurement rounds out.
      assert(ui.text_width(value) == (int(g.advance) * 2 * 288 + 479) / 480);
    }
  puts("PASS every native glyph at all three layout phases: exact 2x2 font "
       "pixels and matching advances");
  ui.rect(0, 0, 288, 288, background);
  ui.text(10, 10, "ABC 星壤", ink);
  ui.rect(0, 0, 288, 288, background);
  assert(std::all_of(pixels.begin(), pixels.end(),
                     [](uint16_t p) { return p == background; }));
  // Off-screen geometry must not form reversed spans or overrun the surface.
  for (int x : {-1000, -20, 0, 280, 1000})
    for (int y : {-1000, -20, 0, 280, 1000}) {
      ui.rect(x, y, 25, 25, ink);
      ui.text(x, y, "Hg星", ink);
    }
  puts("PASS painter order, modal occlusion, negative coordinates and all "
       "surface edges");
}
