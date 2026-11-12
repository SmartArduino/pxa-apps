// SPDX-License-Identifier: MIT
#include "font_cache.hpp"
#include "../common/pocket-play/runtime.hpp"
#include <stdio.h>
#include <string.h>
namespace {
constexpr unsigned COUNT = 512, ARENA = 192 * 1024;
constexpr uint64_t BASE = 0x55440000;
struct Entry {
  uint32_t codepoint = 0;
  uint16_t pixels = 0;
  uint8_t state = 0;
  pxa_font_glyph_t glyph{};
};
Entry entries[COUNT];
uint8_t coverage[ARENA];
unsigned count = 0, used = 0, inflight = 0, ready = 0;
bool supported = true;
} // namespace
namespace font_cache {
const pxa_font_glyph_t *get(uint32_t cp, uint16_t size) {
  if (!supported)
    return nullptr;
  for (unsigned i = 0; i < count; i++)
    if (entries[i].codepoint == cp && entries[i].pixels == size)
      return entries[i].state == 3 ? &entries[i].glyph : nullptr;
  if (count == COUNT)
    return nullptr;
  auto &e = entries[count++];
  e.codepoint = cp;
  e.pixels = size;
  e.state = 1;
  return nullptr;
}
void update(bool fg) {
  if (!fg || !supported)
    return;
  for (unsigned i = 0; i < count && inflight < 4; i++) {
    auto &e = entries[i];
    if (e.state != 1)
      continue;
    int status = pxa_font_request_glyph(BASE + i, e.codepoint, e.pixels);
    if (!status) {
      e.state = 2;
      inflight++;
    } else if (status != PXA_STATUS_WOULD_BLOCK)
      e.state = 4;
  }
}
bool event(const pxa_event_t &e) {
  if (e.service != PXA_FONT_SERVICE || e.opcode != PXA_FONT_GLYPH ||
      e.token < BASE || e.token >= BASE + COUNT)
    return false;
  unsigned i = e.token - BASE;
  if (i >= count || entries[i].state != 2)
    return true;
  inflight--;
  auto &entry = entries[i];
  pxa_font_glyph_t glyph{};
  if (!pxa_font_parse_glyph(&e, e.token, &glyph) || glyph.status) {
    entry.state = 4;
    if (glyph.status == PXA_STATUS_UNSUPPORTED) {
      supported = false;
      pocket::log("Unciv: system glyphs unavailable; Canvas fallback");
    }
    return true;
  }
  unsigned bytes = glyph.width * glyph.height;
  if (bytes > ARENA - used) {
    entry.state = 4;
    return true;
  }
  if (bytes)
    memcpy(coverage + used, glyph.coverage, bytes);
  glyph.coverage = coverage + used;
  used += bytes;
  entry.glyph = glyph;
  entry.state = 3;
  ready++;
  if (ready % 64 == 0) {
    char b[96];
    snprintf(b, sizeof(b), "Unciv: font cache glyphs=%u bytes=%u", ready, used);
    pocket::log(b);
  }
  pocket::redraw();
  return true;
}
} // namespace font_cache
