// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <stdint.h>
namespace pinch_tracker {
enum class Phase { Down, Move, Up, Cancel };
enum class Action { Pass, Begin, Update, End, Consume };
struct Result {
  Action action = Action::Pass;
  int x = 0, y = 0;
  uint32_t scale = 65536;
};
class Tracker {
  struct Contact {
    int id = -1, x = 0, y = 0;
    bool down = false, eligible = false;
  } fingers[2];
  bool pinching = false, locked = false;
  unsigned start_span = 12;
  unsigned span() const {
    int dx = fingers[0].x - fingers[1].x, dy = fingers[0].y - fingers[1].y;
    uint32_t value = uint32_t(dx * dx + dy * dy), root = 0, bit = 1u << 30;
    while (bit > value)
      bit >>= 2;
    while (bit) {
      if (value >= root + bit) {
        value -= root + bit;
        root = (root >> 1) + bit;
      } else
        root >>= 1;
      bit >>= 2;
    }
    return std::max(12u, root);
  }

public:
  bool active() const { return pinching; }
  void reset() {
    for (auto &f : fingers)
      f = {};
    pinching = locked = false;
  }
  Result event(Phase phase, int id, int x, int y, bool eligible) {
    x = std::clamp(x, -4096, 4096);
    y = std::clamp(y, -4096, 4096);
    Contact *finger = nullptr;
    for (auto &f : fingers)
      if (f.down && f.id == id)
        finger = &f;
    if (!finger && phase == Phase::Down)
      for (auto &f : fingers)
        if (!f.down) {
          finger = &f;
          break;
        }
    if (!finger) {
      if (phase == Phase::Cancel && locked) {
        bool ended = pinching;
        reset();
        return {ended ? Action::End : Action::Consume};
      }
      return {locked ? Action::Consume : Action::Pass};
    }
    finger->x = x;
    finger->y = y;
    if (phase == Phase::Down) {
      finger->id = id;
      finger->down = true;
      finger->eligible = eligible;
    }
    if (phase == Phase::Up || phase == Phase::Cancel) {
      finger->down = false;
      bool ended = pinching, consume = locked;
      pinching = false;
      if (!fingers[0].down && !fingers[1].down)
        locked = false;
      return {ended ? Action::End : consume ? Action::Consume : Action::Pass};
    }
    int cx = (fingers[0].x + fingers[1].x) / 2,
        cy = (fingers[0].y + fingers[1].y) / 2;
    if (!locked && fingers[0].down && fingers[1].down && fingers[0].eligible &&
        fingers[1].eligible) {
      pinching = locked = true;
      start_span = span();
      return {Action::Begin, cx, cy, 65536};
    }
    if (pinching)
      return {Action::Update, cx, cy,
              uint32_t(uint64_t(span()) * 65536 / start_span)};
    return {locked ? Action::Consume : Action::Pass};
  }
};
struct Camera {
  int size, pan_x, pan_y;
};
inline Camera zoom(int reference_size, int pan_x, int pan_y, int anchor_x,
                   int anchor_y, int center_x, int center_y, int x, int y,
                   uint32_t scale) {
  int size =
      std::clamp(int(std::min<uint64_t>(
                     128, (uint64_t(reference_size) * scale + 32768) / 65536)),
                 32, 64);
  size = (size + 1) / 2 * 2;
  return {
      size,
      x - center_x -
          int(int64_t(anchor_x - center_x - pan_x) * size / reference_size),
      y - center_y -
          int(int64_t(anchor_y - center_y - pan_y) * size / reference_size)};
}
} // namespace pinch_tracker
