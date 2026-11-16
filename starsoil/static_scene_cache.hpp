// SPDX-License-Identifier: MIT
#pragma once
#include "background_cache.hpp"

namespace starsoil_scene {
// Reuses the former background snapshot: no additional framebuffer. Actors,
// controls, grass motion, liquids and light motes are always painted afterward.
class StaticSceneCache {
  BackgroundCache picture;
  BackgroundKey current;
  std::array<int, 7> previous_view{};
  uint64_t previous_geometry = 0;
  bool have_view = false, have_geometry = false;
  static uint64_t geometry(const starsoil::State &s, const BackgroundKey &key) {
    using namespace starsoil;
    uint32_t a = 2166136261u, b = 0x9e3779b9u;
    auto add = [&](uint32_t v) {
      a = (a ^ v) * 16777619u;
      b ^= v + 0x9e3779b9u + (b << 6) + (b >> 2);
    };
    const int left = std::max(0, key.view[3] / 16 - 8);
    const int right = std::min(s.width - 1, (key.view[3] + key.view[0] - 1) / 16 + 8);
    const int top = std::max(0, (key.view[4] + key.view[1]) / 16 - 20);
    const int bottom = std::min(s.height - 1, (key.view[4] + key.view[2] - 1) / 16 + 20);
    // Includes tree crown/trunk inspection beyond the viewport and nearby
    // emitters. World edits need no separate mutation counter or save change.
    for (int y = top; y <= bottom; ++y)
      for (int x = left; x <= right; ++x) {
        const int at = y * WorldW + x;
        add(uint32_t(s.tiles[at]) | (uint32_t(s.walls[at]) << 8));
      }
    for (int x = left; x <= right; ++x) add(s.heights[x]);
    add(s.width); add(s.height);
    add(uint32_t(int(s.player.x / 16))); add(uint32_t(int(s.player.y / 16)));
    add(s.equip[7] == GlowCharm);
    add(items[s.bag[s.selected].item].tile == Torch);
    add(s.clock < 25000 || s.clock > 240000);
    return (uint64_t(a) << 32) | b;
  }
public:
  void invalidate() { picture.invalidate(); have_geometry = false; }
  bool restore(const starsoil::State &state, const BackgroundKey &key, uint16_t *frame) {
    current = key;
    const bool stationary = have_view && previous_view == key.view;
    previous_view = key.view; have_view = true;
    if (!stationary || !key.reuse_allowed) {
      current.reuse_allowed = false;
      invalidate();
    } else {
      const auto signature = geometry(state, key);
      if (!have_geometry || signature != previous_geometry) picture.invalidate();
      previous_geometry = signature; have_geometry = true;
    }
    return picture.restore(current, frame);
  }
  void store(const uint16_t *frame) { picture.store(current, frame); }
};
} // namespace starsoil_scene
