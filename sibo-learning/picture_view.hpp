// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace sibo {
struct PictureRect {
  int x, y, width, height;
};
struct PictureView {
  static constexpr std::array<int, 6> scales{1, 2, 3, 4, 6, 8};
  int width = 1, height = 1, image_width = 1, image_height = 1, level = 0,
      pan_x = 0, pan_y = 0;
  void initialize(int iw, int ih, int w, int h) {
    image_width = std::max(1, iw);
    image_height = std::max(1, ih);
    width = std::max(1, w);
    height = std::max(1, h);
    level = pan_x = pan_y = 0;
  }
  PictureRect rectangle() const {
    int w = width,
        h = std::max(1, int(int64_t(image_height) * width / image_width));
    if (h > height) {
      h = height;
      w = std::max(1, int(int64_t(image_width) * height / image_height));
    }
    w *= scales[level];
    h *= scales[level];
    return {(width - w) / 2 + pan_x, (height - h) / 2 + pan_y, w, h};
  }
  void clamp() {
    auto r = rectangle();
    auto axis = [](int view, int image, int &pan) {
      if (image <= view) {
        pan = 0;
        return;
      }
      const int origin = (view - image) / 2;
      pan = std::clamp(pan, view - image - origin, -origin);
    };
    axis(width, r.width, pan_x);
    axis(height, r.height, pan_y);
  }
  void move(int x, int y) {
    pan_x += x;
    pan_y += y;
    clamp();
  }
  void zoom(int next, int w, int h, int anchor_x = -1, int anchor_y = -1) {
    auto old = rectangle();
    const bool anchored = anchor_x >= 0 && anchor_y >= 0;
    const int ox = anchored ? anchor_x : width / 2,
              oy = anchored ? anchor_y : height / 2;
    const int64_t sx =
        std::clamp<int64_t>(int64_t(ox - old.x) * 65536 / old.width, 0, 65536);
    const int64_t sy =
        std::clamp<int64_t>(int64_t(oy - old.y) * 65536 / old.height, 0, 65536);
    width = std::max(1, w);
    height = std::max(1, h);
    level = std::clamp(next, 0, int(scales.size() - 1));
    pan_x = pan_y = 0;
    if (level) {
      auto r = rectangle();
      int nx = anchored ? std::clamp(anchor_x, 0, width) : width / 2,
          ny = anchored ? std::clamp(anchor_y, 0, height) : height / 2;
      pan_x = nx - r.x - int(sx * r.width / 65536);
      pan_y = ny - r.y - int(sy * r.height / 65536);
    }
    clamp();
  }
};
} // namespace sibo
