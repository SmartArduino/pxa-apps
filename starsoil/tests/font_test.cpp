// SPDX-License-Identifier: MIT
#include "../ui_canvas.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
  {
    uint16_t source[256];
    for (int i = 0; i < 256; ++i)
      source[i] = i % 7 ? uint16_t(i * 251) : 0;
    for (auto size :
         {std::pair{320, 240}, std::pair{480, 480}, std::pair{240, 284}})
      for (int x : {-9, 7, 281})
        for (int y : {-9, 47, 220})
          for (bool flip : {false, true})
            for (int light : {28, 112, 255}) {
              std::vector<uint16_t> actual(size.first * size.second, 0x1357),
                  expected = actual;
              starsoil_ui::Canvas fast{actual.data(), size.first, size.second,
                                       288, 224};
              auto slow = fast;
              slow.pixels = expected.data();
              if (size.first == 240) {
                fast.origin_x = slow.origin_x = 16;
                fast.origin_y = slow.origin_y = 14;
                fast.region_width = slow.region_width = 208;
                fast.region_height = slow.region_height = 250;
              }
              assert(fast.sprite(x, y, source, 16, 16, flip, light, 0, 224));
              for (int yy = 0; yy < 16; ++yy)
                for (int xx = 0; xx < 16; ++xx) {
                  uint16_t c = source[yy * 16 + (flip ? 15 - xx : xx)];
                  if (!c)
                    continue;
                  c = uint16_t(((((c >> 11) & 31) * light / 255) << 11) |
                               ((((c >> 5) & 63) * light / 255) << 5) |
                               ((c & 31) * light / 255));
                  slow.rect(x + xx, y + yy, 1, 1, c);
                }
              assert(actual == expected);
            }
    puts("PASS mapped sprite edges equal per-pixel UI at fractional scales, "
         "safe areas, clipping, flip and lighting");
  }
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
  for (const char *value : {"AB 星\nHg", " \xf4\x8f\xbf\xbf"}) {
    std::vector<uint16_t> expected(64 * 64, background), actual = expected;
    starsoil_text::draw(0, 0, value, 1, [&](int x, int y) {
      assert(x >= 0 && x < 64 && y >= 0 && y < 64);
      expected[y * 64 + x] = ink;
    });
    starsoil_text::spans(0, 0, value, [&](int x, int y, int w, int h) {
      for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) {
          assert(xx >= 0 && xx < 64 && yy >= 0 && yy < 64);
          actual[yy * 64 + xx] = ink;
        }
    });
    assert(actual == expected);
  }
  puts("PASS text spans preserve spaces, multi-line baselines and unknown-glyph outlines");
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
  {
    std::vector<uint16_t> round(412 * 412, background);
    starsoil_ui::Canvas safe{round.data(), 412, 412, 288, 288};
    safe.origin_x = 76;
    safe.origin_y = 74;
    safe.region_width = 260;
    safe.region_height = 278;
    assert(safe.font_scale() == 1);
    assert(safe.y(safe.line_height()) - safe.y(0) >= 14);
    safe.text(12, 12, "星壤", ink);
    const auto &glyph =
        *std::find_if(std::begin(font::glyphs), std::end(font::glyphs),
                      [](const auto &g) { return g.cp == 0x661f; });
    for (int y = 0; y < 12; ++y)
      for (int x = 0; x < 12; ++x)
        assert((round[(safe.y(12) + y) * 412 + safe.x(12) + x] == ink) ==
               bool(glyph.rows[y] & (1 << (11 - x))));
    safe.rect(-200, -200, 1000, 1000, ink);
    for (int y = 0; y < 412; ++y)
      for (int x = 0; x < 412; ++x)
        assert(round[y * 412 + x] ==
               (x >= 76 && x < 336 && y >= 74 && y < 352 ? ink : background));
    puts("PASS circular safe-area font pixels, line spacing, clipping and "
         "framebuffer stride");
  }
}
