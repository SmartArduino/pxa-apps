// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace crisp {
// Largest required view is 150*100. Simulation draws here, never to a runtime
// framebuffer; the draw hook scales this fixed buffer to the visible canvas.
constexpr int MAX_VIEW_PIXELS = 15000;
const uint16_t *view_pixels();
int view_width();
int view_height();
}
