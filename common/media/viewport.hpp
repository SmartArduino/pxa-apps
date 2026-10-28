// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_ui.h"
#include "pxa_window.h"
#include <algorithm>
#include <cmath>

namespace media {
struct ContentRect {
  int x = 0, y = 0, w = 320, h = 240;
};

// Geometry and Window insets are Surface pixels. Canvas lengths are dp.
// Keep the background edge-to-edge; round safe edges inward so fractional
// density cannot put controls back under a cutout. The left Back gesture
// strip is a separate hit boundary so page padding is not added twice.
struct Viewport {
  pxa_ui_environment_t display{};
  pxa_window_snapshot_t window{};
  ContentRect content;
  int width = 320, height = 240;
  int touch_left = 0;
  Viewport() {
    display.width = 320;
    display.height = 240;
    display.density_q16 = 65536;
  }
  uint32_t density() const {
    return display.density_q16 ? display.density_q16 : 65536;
  }
  bool update() {
    const auto old = content;
    const int old_width = width, old_height = height;
    const int old_touch_left = touch_left;
    const uint32_t d = density();
    const auto dp = [d](uint64_t px, bool up) {
      return int(std::min<uint64_t>(32760,
          (px * 65536 + (up ? d - 1 : 0)) / d));
    };
    const uint32_t w = std::min(display.width, 65535u);
    const uint32_t h = std::min(display.height, 65535u);
    width = dp(w, true);
    height = dp(h, true);
    uint32_t edge[4]{}; // top, right, bottom, left
    for (unsigned i = 0; i < 4; ++i) {
      const double radius = display.display_shape == 2
          ? std::min(w, h) / 2.0
          : std::min<double>(display.corner_radii[i], std::min(w, h) / 2.0);
      const auto margin = uint32_t(std::ceil(radius * (1 - std::sqrt(.5))));
      edge[i] = std::max(edge[i], margin);
      edge[(i + 3) % 4] = std::max(edge[(i + 3) % 4], margin);
    }
    if (display.display_shape == 2) {
      const auto side = std::min(w, h);
      edge[0] += (h - side + 1) / 2;
      edge[2] += (h - side + 1) / 2;
      edge[1] += (w - side + 1) / 2;
      edge[3] += (w - side + 1) / 2;
    }
    for (unsigned i = 0; i < 4; ++i)
      edge[i] = std::max(edge[i], display.safe_insets[i]);
    uint32_t left_reserve = display.safe_insets[3];
    uint32_t left_gesture = 0;
    // A Window snapshot can arrive before or after the corresponding UI
    // environment. Never apply insets from a different panel size.
    if (window.revision && window.pixel_width == display.width &&
        window.pixel_height == display.height) {
      const uint32_t safe[] = {window.safe_insets.top, window.safe_insets.right,
                              window.safe_insets.bottom, window.safe_insets.left};
      const uint32_t bars[] = {window.system_bar_insets.top,
                              window.system_bar_insets.right,
                              window.system_bar_insets.bottom,
                              window.system_bar_insets.left};
      for (unsigned i = 0; i < 4; ++i)
        edge[i] = std::max({edge[i], safe[i], i == 3 ? 0u : bars[i]});
      left_reserve = std::max(left_reserve, safe[3]);
      left_gesture = bars[3];
    }
    if (display.display_shape != 2) {
      // Headers and playback controls already have at least 4 dp of inner
      // padding. Use their vertical distance from the rounded corners rather
      // than reserving a second full corner inset down the entire left edge.
      // On pai-touch this keeps the published 10 px left inset, instead of 17.
      const double padding = uint64_t(4) * d / 65536;
      uint32_t corner_left = 0;
      for (unsigned corner : {0u, 3u}) {
        const double radius = std::min<double>(display.corner_radii[corner],
                                               std::min(w, h) / 2.0);
        const double dy = std::max(0., radius -
            std::min<double>(radius, edge[corner == 0 ? 0 : 2] + padding));
        const double required = radius - std::sqrt(radius * radius - dy * dy);
        corner_left = std::max(corner_left,
            uint32_t(std::ceil(std::max(0., required - padding))));
      }
      edge[3] = std::max(left_reserve, corner_left);
    }
    const auto left = std::min(edge[3], w), top = std::min(edge[0], h);
    const auto right = w - std::min(edge[1], w);
    const auto bottom = h - std::min(edge[2], h);
    content.x = dp(left, true);
    content.y = dp(top, true);
    content.w = std::max(0, dp(right, false) - content.x);
    content.h = std::max(0, dp(bottom, false) - content.y);
    // Left Back gestures coexist with visual content. Reserve their strip for
    // hit testing without adding the page margin again outside the strip.
    touch_left = std::clamp(dp(std::min(left_gesture, w), true) - content.x,
                            0, content.w);
    return old.x != content.x || old.y != content.y || old.w != content.w ||
           old.h != content.h || old_width != width || old_height != height ||
           old_touch_left != touch_left;
  }
};
} // namespace media
