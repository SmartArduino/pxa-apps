// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace sprites {
struct Span {
  uint8_t y, x, count;
  uint16_t offset;
};
struct ShadowSpan {
  int8_t y, x;
  uint8_t count, alpha;
};
struct Shadow {
  uint16_t count;
  const ShadowSpan *spans;
};
struct Sprite {
  uint8_t w, h;
  uint16_t count;
  const Span *spans;
  const uint16_t *pixels;
  int8_t anchor_x = 0, anchor_y = 0;
  const Shadow *shadow = nullptr;
};
struct Bone {
  const Sprite *sprite;
  const uint8_t *rows;
};
struct Rig {
  const Sprite *body;
  const Bone *arms[2], *legs[2];
  uint8_t arm_end[2];
  uint16_t cloth, shirt;
  bool side;
  uint8_t hip = 23;
};
struct HeldFrame {
  const Sprite *sprite;
  int8_t tip_x, tip_y, second_x, second_y, face_x, face_y;
};
} // namespace sprites
