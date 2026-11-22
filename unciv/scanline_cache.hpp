// SPDX-License-Identifier: MIT
#pragma once
#include "overlay_raster.hpp"
#include <string.h>
namespace scanline_cache {
// Native-size pixels and opaque row intervals. The fixed arena bounds memory;
// compaction is safe because callers copy immediately and retain no pointers.
template <unsigned Words = 131072, unsigned Slots = 32> class Cache {
  struct Entry {
    const uint16_t *rgb = nullptr;
    const uint8_t *alpha = nullptr;
    int sw, sh, w, h, dark;
    unsigned offset, words, stamp;
  } entries_[Slots];
  alignas(4) uint16_t pixels_[Words];
  unsigned used_ = 0, clock_ = 0;
  void evict(Entry &entry) {
    unsigned end = entry.offset + entry.words;
    memmove(pixels_ + entry.offset, pixels_ + end, (used_ - end) * 2);
    for (auto &e : entries_)
      if (e.rgb && e.offset > entry.offset)
        e.offset -= entry.words;
    used_ -= entry.words;
    entry.rgb = nullptr;
  }
  Entry *get(const uint16_t *rgb, const uint8_t *alpha, int sw, int sh, int w,
             int h, int dark) {
    for (auto &e : entries_)
      if (e.rgb == rgb && e.alpha == alpha && e.sw == sw && e.sh == sh &&
          e.w == w && e.h == h && e.dark == dark) {
        e.stamp = ++clock_;
        ++hits;
        return &e;
      }
    ++misses;
    unsigned runs = 0;
    for (int y = 0; y < h; ++y) {
      bool before = false;
      for (int x = 0; x < w; ++x) {
        bool opaque = alpha[(y * sh / h) * sw + x * sw / w] >= 60;
        runs += opaque && !before;
        before = opaque;
      }
    }
    unsigned required = w * h + h + runs * 2;
    if (required > Words)
      return nullptr;
    Entry *slot = nullptr;
    for (;;) {
      Entry *oldest = nullptr;
      slot = nullptr;
      for (auto &e : entries_) {
        if (!e.rgb)
          slot = &e;
        else if (!oldest || e.stamp < oldest->stamp)
          oldest = &e;
      }
      if (slot && used_ + required <= Words)
        break;
      if (!oldest)
        return nullptr;
      evict(*oldest);
      ++evictions;
    }
    *slot = {rgb, alpha, sw, sh, w, h, dark, used_, required, ++clock_};
    used_ += required;
    uint16_t *image = pixels_ + slot->offset, *stream = image + w * h;
    for (int y = 0; y < h; ++y) {
      uint16_t *count = stream++;
      *count = 0;
      bool before = false;
      int start = 0;
      for (int x = 0; x <= w; ++x) {
        int k = (y * sh / h) * sw + (x < w ? x * sw / w : 0);
        bool opaque = x < w && alpha[k] >= 60;
        if (opaque) {
          uint16_t c =
              dark ? overlay_raster::blend(rgb[k], 0x08c5, dark) : rgb[k];
          image[y * w + x] = c ? c : 1;
          if (!before)
            start = x;
        } else if (before) {
          *stream++ = start;
          *stream++ = x - start;
          ++*count;
        }
        before = opaque;
      }
    }
    return slot;
  }

public:
  unsigned hits = 0, misses = 0, evictions = 0;
  unsigned bytes() const { return used_ * 2; }
  bool blit(overlay_raster::Surface target, int x, int y, int w, int h,
            const uint16_t *rgb, const uint8_t *alpha, int sw, int sh,
            int dark) {
    if (!target.pixels || !rgb || !alpha || w <= 0 || h <= 0 || w > 128 ||
        h > 128 || sw <= 0 || sh <= 0 || dark < 0 || dark > 256)
      return false;
    Entry *e = get(rgb, alpha, sw, sh, w, h, dark);
    if (!e)
      return false;
    const uint16_t *image = pixels_ + e->offset, *stream = image + w * h;
    for (int row = 0; row < h; ++row) {
      unsigned count = *stream++;
      int yy = y + row;
      for (unsigned n = 0; n < count; ++n) {
        int start = *stream++, length = *stream++;
        int left = std::max(0, x + start),
            right = std::min(target.width, x + start + length);
        if (yy >= std::max(0, target.top) &&
            yy < std::min(target.height, target.bottom) && left < right)
          memcpy(target.pixels + yy * target.width + left,
                 image + row * w + left - x, (right - left) * 2);
      }
    }
    return true;
  }
};
} // namespace scanline_cache
