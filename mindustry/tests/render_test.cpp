// SPDX-License-Identifier: GPL-3.0-only
#include "../render_backend.hpp"
#include "pxa_font.h"
#include "pxa_raster.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using Bytes = std::vector<uint8_t>;
struct Pending {
  uint64_t token;
  uint32_t cp;
  uint16_t pixels;
};
std::vector<Pending> pending;
std::array<Bytes, 16> textures;
Bytes frame;
int checks = 0;
unsigned glyphRequests = 0, textureUploads = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)
extern "C" int32_t pxa_call(uint32_t route, uint64_t token, const uint8_t *p,
                            uint32_t n) {
  if ((route & 65535) == PXA_FONT_SERVICE && (route >> 16) == PXA_FONT_GLYPH) {
    CHECK(n == 8);
    if (pending.size() >= 2)
      return PXA_STATUS_RESOURCE_LIMIT;
    pending.push_back({token, pxa_load_u32(p), pxa_load_u16(p + 4)});
    ++glyphRequests;
  }
  return 0;
}
extern "C" int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *p,
                          uint32_t n) {
  CHECK(handle == 1);
  if (operation == PXA_GAME_RENDER_IO_UPLOAD) {
    CHECK(n >= 20);
    unsigned kind = p[8], slot = p[9];
    if (kind == PXA_RASTER_UPLOAD_TEXTURE_INDEX8) {
      CHECK(slot < 16);
      auto w = pxa_load_u16(p + 12), h = pxa_load_u16(p + 14);
      CHECK(n == 20 + unsigned(w) * h);
      textures[slot] = Bytes(p + 20, p + n);
      ++textureUploads;
    }
    return int32_t(n);
  }
  if (operation == PXA_GAME_RENDER_IO_SUBMIT) {
    frame = Bytes(p, p + n);
    CHECK(n <= 49152);
    CHECK(pxa_load_u32(p + 16) <= 1024);
    return int32_t(n);
  }
  return -3;
}
void deliver() {
  auto batch = pending;
  pending.clear();
  for (auto &r : batch) {
    std::array<uint8_t, 20 + 64> payload{};
    pxa_store_u16(payload.data() + 4, 12);
    pxa_store_u16(payload.data() + 6, 2);
    pxa_store_u16(payload.data() + 8, 8);
    pxa_store_u16(payload.data() + 10, 6);
    pxa_store_u16(payload.data() + 12, 8);
    std::fill(payload.begin() + 20, payload.begin() + 68, uint8_t(128));
    pxa_event_t e{PXA_FONT_SERVICE, PXA_FONT_GLYPH, r.token, payload.data(),
                  68};
    CHECK(gfx::font_event(e));
    gfx::service_fonts(1);
  }
}
void warmFonts() {
  for (int i = 0; i < 300; ++i) {
    gfx::service_fonts();
    if (pending.empty())
      return;
    deliver();
  }
  CHECK(false);
}
void drawText() {
  CHECK(gfx::begin(1, 320, 240));
  gfx::rect(0, 0, 320, 240, 0);
  gfx::label(20, 20, 200, "Copper 0123 中文", 0xffff, 12);
  CHECK(gfx::finish() > 0);
}
#ifndef MINDUSTRY_RENDER_SUPPORT_ONLY
int main() {
  constexpr auto caps =
      PXA_RASTER_CAP_TEXTURED_QUAD | PXA_RASTER_CAP_PAINTER_POLYGON |
      PXA_RASTER_CAP_AFFINE_UV | PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA;
  CHECK(gfx::attach(1, caps));
  CHECK(textures[0].size() == 65536);
  drawText();
  gfx::service_fonts();
  CHECK(pending.size() == 2);
  CHECK(gfx::fonts_pending());
  warmFonts();
  CHECK(!gfx::fonts_pending());
  drawText();
  auto count = glyphRequests, uploads = textureUploads;
  for (int i = 0; i < 20; ++i) {
    drawText();
    gfx::service_fonts();
  }
  CHECK(glyphRequests == count);
  CHECK(textureUploads == uploads);
  CHECK(std::any_of(textures[11].begin(), textures[11].end(),
                    [](auto x) { return x == 128; }));
  // Small label styles share one system size. Visible glyphs outrank warmup.
  CHECK(gfx::begin(1, 320, 240));
  gfx::label(0, 0, 200, "Copper 0123 中文", 0xffff, 9);
  CHECK(gfx::finish() > 0);
  gfx::service_fonts();
  CHECK(glyphRequests == count);
  gfx::preload("", 12);
  CHECK(gfx::begin(1, 320, 240));
  gfx::clear(0);
  gfx::label(0, 0, 100, "", 0xffff, 12);
  gfx::service_fonts(1);
  CHECK(pending.size() == 1 && pending[0].cp == 0xe012);
  CHECK(gfx::finish() > 0);
  warmFonts();
  // A short label stays bounded and correctly clips an overhanging glyph.
  CHECK(gfx::begin(2, 320, 240));
  gfx::label(-4, -4, 10, "中文", 0xffff, 12);
  CHECK(gfx::finish() > 0);
  // Off-screen world endpoints must clip before their signed Q4 conversion.
  CHECK(gfx::begin(2, 320, 240));
  gfx::clear(0);
  gfx::line(-9000, -9000, 9000, 9000, 0xffff);
  gfx::line(-9000, -2, -8000, -40, 0xffff);
  CHECK(gfx::finish() > 0);
  CHECK(pxa_load_u32(frame.data() + 16) == 2);
  for (unsigned at = 32; at < frame.size();) {
    if (frame[at] == PXA_RASTER_RECORD_FLAT_QUAD)
      for (unsigned i = 0; i < 8; ++i) {
        int value = int16_t(pxa_load_u16(frame.data() + at + 8 + i * 2));
        CHECK(value >= -16 && value <= (i % 2 ? 241 : 321) * 16);
      }
    at += pxa_load_u16(frame.data() + at + 2);
  }
  // Map + minimap textures are reused after warm-up, including maximum
  // zoom-out.
  md::State state;
  state.generate(154, 0, true);
  CHECK(gfx::begin(3, 480, 320));
  gfx::terrain(state, 0, 0, 8, false);
  gfx::minimap(state, 400, 20);
  CHECK(gfx::finish() > 0);
  uploads = textureUploads;
  CHECK(gfx::begin(4, 480, 320));
  gfx::terrain(state, 0, 0, 8, false);
  gfx::minimap(state, 400, 20);
  CHECK(gfx::finish() > 0);
  CHECK(textureUploads == uploads);
  CHECK(pxa_load_u32(frame.data() + 16) < 30);
  // A changed structure only invalidates the minimap, while terrain remains
  // resident.
  CHECK(state.build(md::Wall, 24, 31, 0) >= 0);
  CHECK(gfx::begin(5, 480, 320));
  gfx::terrain(state, 0, 0, 8, false);
  gfx::minimap(state, 400, 20);
  CHECK(gfx::finish() > 0);
  CHECK(textureUploads == uploads + 1);
  // Fill and overflow the bounded glyph working set, without corrupting
  // commands.
  for (int pass = 0; pass < 6; ++pass) {
    CHECK(gfx::begin(6 + pass, 320, 240));
    gfx::rect(0, 0, 320, 240, 0);
    char text[4]{};
    for (int i = 0; i < 60; ++i) {
      unsigned cp = 0x4e00 + pass * 60 + i;
      text[0] = char(0xe0 | (cp >> 12));
      text[1] = char(0x80 | ((cp >> 6) & 63));
      text[2] = char(0x80 | (cp & 63));
      gfx::label(i % 20 * 14, i / 20 * 16, 14, text, 0xffff, 12);
    }
    CHECK(gfx::finish() > 0);
    warmFonts();
  }
  gfx::detach();
  CHECK(!gfx::begin(100, 320, 240));
  std::printf("%d rendering checks passed: glyph cache, clipping, persistent "
              "maps and command bounds\n",
              checks);
}
#endif
