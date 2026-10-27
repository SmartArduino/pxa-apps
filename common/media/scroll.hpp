// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace media {
// Pixel scrolling shared by touch and controller navigation. No row snapping.
struct SmoothList {
  float offset = 0, target = 0, velocity = 0, maximum = 0;
  int top = 0, bottom = 0, row = 48, down_y = 0, last_y = 0;
  uint64_t last = 0, moved_at = 0, sample_at = 0;
  bool dragging = false, moved = false, easing = false;
  void reset() {
    offset = target = velocity = 0;
    dragging = moved = easing = false;
  }
  void layout(int t, int b, int r, unsigned count) {
    top = t;
    bottom = b;
    row = r;
    maximum = std::max(0, int(count) * row - (bottom - top));
    offset = std::clamp(offset, 0.f, maximum);
    target = std::clamp(target, 0.f, maximum);
  }
  void ensure(unsigned i) {
    float y = i * row;
    target =
        std::clamp(y < target                        ? y
                   : y + row > target + bottom - top ? y + row - (bottom - top)
                                                     : target,
                   0.f, maximum);
    easing = true;
    velocity = 0;
  }
  bool down(int y, uint64_t time) {
    if (y < top || y >= bottom)
      return false;
    dragging = true;
    moved = false;
    easing = false;
    velocity = 0;
    down_y = last_y = y;
    sample_at = time;
    return true;
  }
  void move(int y, uint64_t time) {
    if (!dragging)
      return;
    if (std::abs(y - down_y) > 5)
      moved = true;
    if (moved) {
      float delta = last_y - y;
      offset = std::clamp(offset + delta, 0.f, maximum);
      float dt = std::max<uint64_t>(1, time - sample_at);
      velocity = std::clamp(velocity * .35f + delta / dt * .65f, -2.5f, 2.5f);
      target = offset;
      moved_at = time;
    }
    last_y = y;
    sample_at = time;
  }
  bool up(int y, uint64_t time, bool cancel = false) {
    if (!dragging)
      return false;
    if (y != last_y)
      move(y, time);
    bool scrolled = moved;
    if (cancel || time - sample_at > 100)
      velocity = 0;
    dragging = false;
    return scrolled;
  }
  bool tick(uint64_t time) {
    float dt = std::min<uint64_t>(40, last ? time - last : 20);
    last = time;
    float before = offset;
    if (!dragging) {
      if (easing) {
        offset += (target - offset) * (1 - std::exp(-dt / 65.f));
        if (std::abs(offset - target) < .25f) {
          offset = target;
          easing = false;
        }
      } else if (std::abs(velocity) > .015f) {
        offset = std::clamp(offset + velocity * dt, 0.f, maximum);
        velocity *= std::exp(-dt / 180.f);
        if (offset == 0 || offset == maximum)
          velocity = 0;
        target = offset;
      }
    }
    if (offset != before)
      moved_at = time;
    return std::abs(offset - before) > .05f;
  }
  int first() const { return int(offset) / row; }
  int y(unsigned i) const { return top + int(i) * row - int(offset); }
  int thumb_size() const {
    return std::max(12.f,
                    float(bottom - top) * (bottom - top) /
                        std::max(float(bottom - top), maximum + bottom - top));
  }
  int thumb_y() const {
    return top +
           (maximum ? offset / maximum * (bottom - top - thumb_size()) : 0);
  }
};
} // namespace media
