// SPDX-License-Identifier: MIT
#pragma once
#include "background.hpp"
#include <array>
#include <cstring>

namespace starsoil_scene {
struct BackgroundKey {
  std::array<int, 16> picture{};
  std::array<int, 7> view{};
  bool reuse_allowed = true;
  int ground_y() const { return view[5]; }

  static BackgroundKey make(const starsoil::State &state, int width, int bottom,
                            int camera_x, int camera_y, uint32_t animation,
                            int first_y) {
    const int ground = surface_level(state, camera_x + width / 2);
    const int base = bottom - 47;
    const int horizon = base + floor_div(ground - camera_y - base, 6);
    const int biome = starsoil::biome_at((camera_x + width / 2) / 16, 0, state.width);
    const bool surface = cave_opacity(camera_y + first_y, ground) < 255;
    const bool cave = cave_opacity(camera_y + bottom - 1, ground) > 0;
    const bool night = state.clock < 25000 || state.clock > 240000;
    const int palette = night ? 1 : state.clock > 180000 && state.clock < 240000 ? 2 : 0;
    const int sun_x = width - 31 - int((state.clock % 240000) * (width - 50) / 240000);
    BackgroundKey key;
    // Match the discrete inputs in draw_world()/draw(). Cave sparkles are
    // applied after restoring the picture; night backgrounds draw directly.
    key.picture = {width, first_y, bottom, surface ? horizon : 0,
      cave ? ground : 0, cave ? camera_y : 0, surface ? biome : 0,
      surface ? palette : 0, surface ? sun_x : 0,
      surface ? int((animation / 330 + camera_x / 18) % 97) : 0,
      surface ? camera_x / 12 : 0, surface ? camera_x / 9 : 0,
      camera_x / 6, camera_x / 3, cave ? camera_x / 4 : 0,
      int(surface) | (int(cave) << 1)};
    key.view = {width, first_y, bottom, camera_x, camera_y, ground, biome};
    // Night sky effects change at several independent rates. Draw those
    // frames directly to avoid copying snapshots with little reuse time.
    key.reuse_allowed = !surface || !night;
    return key;
  }
};

class BackgroundCache {
  // Fits the application's existing 3 MiB memory limit. Covers the square
  // 288-pixel scene and common wide/tall layouts. Larger scenes draw directly.
  static constexpr unsigned Capacity = 96 * 1024;
  uint16_t pixels[Capacity];
  std::array<int, 16> cached_picture{};
  std::array<int, 7> previous_view{};
  bool valid = false, have_view = false, capture = false;

  static unsigned pixel_count(const BackgroundKey &key) {
    const int width = key.picture[0], first = key.picture[1], bottom = key.picture[2];
    if (width < 1 || width > 480 || first < 0 || bottom <= first || bottom > 480)
      return 0;
    const unsigned count = unsigned(width * (bottom - first));
    return count <= Capacity ? count : 0;
  }

public:
  bool restore(const BackgroundKey &key, uint16_t *frame) {
    const unsigned count = pixel_count(key);
    capture = frame && count && key.reuse_allowed && have_view && key.view == previous_view;
    previous_view = key.view;
    have_view = true;
    if (!frame || !count || !key.reuse_allowed || !valid || key.picture != cached_picture)
      return false;
    std::memcpy(frame + key.picture[0] * key.picture[1], pixels, count * sizeof(uint16_t));
    return true;
  }

  void store(const BackgroundKey &key, const uint16_t *frame) {
    // A moving view skips the extra snapshot copy. A stationary view can
    // still refresh cloud motion, sunlight and cave sparkles independently.
    const unsigned count = pixel_count(key);
    if (!capture || !frame || !count) return;
    std::memcpy(pixels, frame + key.picture[0] * key.picture[1], count * sizeof(uint16_t));
    cached_picture = key.picture;
    valid = true;
  }
};
}
