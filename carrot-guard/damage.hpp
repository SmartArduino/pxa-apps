// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace carrot {
// Hash the actual clipped display list per small tile. A sprite moving or
// disappearing changes BOTH its old and new tiles; static scenery stays put.
// This also handles changing draw order and commands inserted in the middle.
class DamageTracker {
public:
  struct Rect {
    int x, y, w, h;
  };
  std::array<Rect, 4> regions{};
  unsigned count = 0, changedTiles = 0;
  bool full = true;

private:
  static constexpr int Tile = 16, Limit = 64;
  static constexpr uint64_t Seed = 1469598103934665603ULL;
  std::array<uint64_t, Limit * Limit> previous{}, current{};
  int width = 0, height = 0, columns = 0, rows = 0;
  bool valid = false, prepared = false;
  static uint32_t u32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
           uint32_t(p[3]) << 24;
  }
  static int s32(const uint8_t *p) { return int32_t(u32(p)); }
  static Rect intersect(Rect a, Rect b) {
    int x = std::max(a.x, b.x), y = std::max(a.y, b.y);
    return {x, y, std::max(0, std::min(a.x + a.w, b.x + b.w) - x),
            std::max(0, std::min(a.y + a.h, b.y + b.h) - y)};
  }
  static Rect unite(Rect a, Rect b) {
    int x = std::min(a.x, b.x), y = std::min(a.y, b.y);
    return {x, y, std::max(a.x + a.w, b.x + b.w) - x,
            std::max(a.y + a.h, b.y + b.h) - y};
  }
  static int64_t area(Rect a) { return int64_t(a.w) * a.h; }
  void add(Rect r) {
    if (count < regions.size()) {
      regions[count++] = r;
      return;
    }
    // Keep four spatial clusters, merging the pair with least extra area.
    std::array<Rect, 5> all{regions[0], regions[1], regions[2], regions[3], r};
    unsigned first = 0, second = 1;
    int64_t best = INT64_MAX;
    for (unsigned i = 0; i < 5; ++i)
      for (unsigned j = i + 1; j < 5; ++j) {
        int64_t cost =
            area(unite(all[i], all[j])) - area(all[i]) - area(all[j]);
        if (cost < best) {
          best = cost;
          first = i;
          second = j;
        }
      }
    all[first] = unite(all[first], all[second]);
    unsigned n = 0;
    for (unsigned i = 0; i < 5; ++i)
      if (i != second)
        regions[n++] = all[i];
  }

public:
  void invalidate() { valid = false; }
  bool analyse(const uint8_t *data, size_t bytes, int w, int h) {
    count = changedTiles = 0;
    full = true;
    prepared = false;
    if (w != width || h != height)
      valid = false;
    width = w;
    height = h;
    columns = (w + Tile - 1) / Tile;
    rows = (h + Tile - 1) / Tile;
    if (w <= 0 || h <= 0 || columns > Limit || rows > Limit)
      return true;
    std::fill_n(current.begin(), columns * rows, Seed);
    std::array<Rect, 16> clips;
    Rect clip{0, 0, w, h};
    unsigned depth = 0;
    size_t offset = 0;
    while (offset + 4 <= bytes) {
      const uint8_t *record = data + offset;
      unsigned type = record[0],
               length = unsigned(record[2]) | (unsigned(record[3]) << 8);
      offset += 4;
      if (length > bytes - offset)
        return true;
      const uint8_t *p = data + offset;
      offset += length;
      if (type == 7) {
        if (length != 16 || depth == clips.size())
          return true;
        clips[depth++] = clip;
        clip = intersect(clip, {s32(p), s32(p + 4), s32(p + 8), s32(p + 12)});
        continue;
      }
      if (type == 8) {
        if (!depth)
          return true;
        clip = clips[--depth];
        continue;
      }
      if (length < 16)
        return true;
      Rect bounds{s32(p), s32(p + 4), s32(p + 8), s32(p + 12)};
      if (type == 3 && length == 22) {
        int pad = (int(p[20]) | (int(p[21]) << 8)) + 2;
        bounds = {std::min(s32(p), s32(p + 8)) - pad,
                  std::min(s32(p + 4), s32(p + 12)) - pad,
                  std::abs(s32(p + 8) - s32(p)) + pad * 2 + 1,
                  std::abs(s32(p + 12) - s32(p + 4)) + pad * 2 + 1};
      } else if (type == 4 && length == 22) {
        int radius = s32(p + 8) + 2;
        bounds = {s32(p) - radius, s32(p + 4) - radius, radius * 2 + 1,
                  radius * 2 + 1};
      } else if (type != 1 && type != 2 && type != 6 && type != 9 &&
                 type != 10 && type != 11) {
        return true; // Unknown primitive: conservative full redraw.
      }
      // Include antialias fringes and density rounding at primitive edges.
      bounds = {bounds.x - 2, bounds.y - 2, bounds.w + 4, bounds.h + 4};
      bounds = intersect(bounds, clip);
      if (bounds.w <= 0 || bounds.h <= 0)
        continue;
      uint64_t hash = Seed;
      for (unsigned i = 0; i < length + 4; ++i)
        hash = (hash ^ record[i]) * 1099511628211ULL;
      for (int value : {clip.x, clip.y, clip.w, clip.h})
        hash = (hash ^ uint32_t(value)) * 1099511628211ULL;
      int x1 = bounds.x / Tile, x2 = (bounds.x + bounds.w - 1) / Tile;
      int y1 = bounds.y / Tile, y2 = (bounds.y + bounds.h - 1) / Tile;
      for (int y = y1; y <= y2; ++y)
        for (int x = x1; x <= x2; ++x) {
          auto &tile = current[y * columns + x];
          tile = (tile ^ hash) * 1099511628211ULL;
        }
    }
    if (offset != bytes || depth)
      return true;
    prepared = true;
    if (!valid)
      return true;
    full = false;
    for (int y = 0; y < rows; ++y) {
      int x = 0;
      while (x < columns) {
        if (current[y * columns + x] == previous[y * columns + x]) {
          ++x;
          continue;
        }
        int start = x;
        while (x < columns &&
               current[y * columns + x] != previous[y * columns + x]) {
          ++x;
          ++changedTiles;
        }
        add({start * Tile, y * Tile, std::min(w, x * Tile) - start * Tile,
             std::min(Tile, h - y * Tile)});
      }
    }
    int64_t dirty = 0;
    for (unsigned i = 0; i < count; ++i)
      dirty += area(regions[i]);
    full = dirty >= int64_t(w) * h * 4 / 5;
    return count != 0;
  }
  void commit() {
    if (prepared) {
      std::copy_n(current.begin(), columns * rows, previous.begin());
      valid = true;
    } else
      valid = false;
  }
};
} // namespace carrot
