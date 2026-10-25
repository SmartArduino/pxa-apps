// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "core.hpp"
namespace marble {
constexpr int VIEW_W=144,VIEW_H=100;
// Software rasterization via unchanged small3dlib, no heap, full 57,600-byte z buffer.
void render(const Run &run,uint16_t *pixels,uint32_t foreground_ms);
}
