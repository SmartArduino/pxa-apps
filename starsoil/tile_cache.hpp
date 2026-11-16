// SPDX-License-Identifier: MIT
#pragma once
#include "scene_blit.hpp"
#include <array>
#include <cstring>

namespace starsoil_scene {
// Only immutable 16x16 art belongs here. Exact light is part of the key;
// transparency is read from the original so an opaque pixel may become black.
class TileCache {
  struct Entry {
    const uint16_t *source = nullptr;
    int light = 0;
    bool opaque = false;
    uint8_t uses = 0;
    uint16_t pixels[256]{};
  };
  std::array<Entry, 32> entries{};
  std::array<uint8_t, 16> next{};
  unsigned hits = 0, misses = 0, cooldown = 0;
  bool enabled = true;
public:
  bool active() const { return enabled; }
  void begin_frame(bool stationary_input) {
    if (cooldown) --cooldown;
    // A large/changing working set should pay only the original drawing
    // cost. Retry occasionally, and preserve useful entries while bypassed.
    if (enabled && misses > 16 && misses > hits + 8) cooldown = 60;
    enabled = stationary_input && !cooldown;
    hits = misses = 0;
  }
  __attribute__((noinline)) void draw(uint16_t *dst, int width, int height, int x, int y,
            const uint16_t *src, bool flip, int light, int clip_top, int clip_bottom) {
    if (!enabled || light >= 255) {
      blit(dst, width, height, x, y, src, 16, 16, flip, light, clip_top, clip_bottom);
      return;
    }
    const int left = std::max(0, -x), right = std::min(16, width - x);
    const int top = std::max({0, -y, clip_top - y});
    const int bottom = std::min({16, height - y, clip_bottom - y});
    if (left >= right || top >= bottom) return;
    // Two ways keep nearby light levels from evicting each other on every tile.
    const auto address = uintptr_t(src) >> 9;
    const unsigned bucket = unsigned((address ^ (address >> 5) ^ unsigned(light)) & 15);
    auto *entry = &entries[bucket * 2];
    if (entry->source != src || entry->light != light) {
      ++entry;
      if (entry->source != src || entry->light != light) {
        ++misses;
        // Edge tiles may expose only a few pixels. Don't shade a whole tile
        // to populate the cache when a clipped software draw is cheaper.
        if (left || right != 16 || top || bottom != 16) {
          blit(dst, width, height, x, y, src, 16, 16, flip, light, clip_top, clip_bottom);
          return;
        }
        entry = &entries[bucket * 2 + next[bucket]];
        next[bucket] ^= 1;
        entry->source = src; entry->light = light; entry->uses = 1;
        blit(dst, width, height, x, y, src, 16, 16, flip, light, clip_top, clip_bottom);
        return;
      }
    }
    ++hits;
    // Admit only repeatedly reused art/light pairs. Scrolling or an oversized
    // working set uses the original blitter without extra per-pixel work.
    if (entry->uses < 4) {
      if (++entry->uses < 4) {
        blit(dst, width, height, x, y, src, 16, 16, flip, light, clip_top, clip_bottom);
        return;
      }
      unsigned transparent = 0;
      for (int i = 0; i < 256; ++i) {
        transparent |= src[i] == 0;
        entry->pixels[i] = starsoil_color::dim(src[i], unsigned(light));
      }
      entry->opaque = !transparent;
    }
    if (entry->opaque && !flip) {
      for (int yy = top; yy < bottom; ++yy)
        std::memcpy(dst + (y + yy) * width + x + left,
                    entry->pixels + yy * 16 + left, size_t(right - left) * sizeof(uint16_t));
      return;
    }
    for (int yy = top; yy < bottom; ++yy) {
      auto *row = dst + (y + yy) * width + x + left;
      for (int xx = left; xx < right; ++xx) {
        const int at = yy * 16 + (flip ? 15 - xx : xx);
        if (src[at]) row[xx - left] = entry->pixels[at];
      }
    }
  }
};
} // namespace starsoil_scene
