// SPDX-License-Identifier: MIT
#include "../viewport.hpp"
#include <cassert>
#include <cstdio>

int main() {
  media::Viewport v;
  v.display.width = 296;
  v.display.height = 240;
  assert(v.update());
  assert(v.content.x == 0 && v.content.y == 0 &&
         v.content.w == 296 && v.content.h == 240);
  v.display.display_shape = 1;
  v.display.corner_radii[0] = 58;
  v.display.corner_radii[1] = 20;
  v.display.corner_radii[2] = 36;
  v.display.corner_radii[3] = 4;
  v.display.safe_insets[0] = 23;
  v.display.safe_insets[1] = 5;
  v.display.safe_insets[2] = 8;
  v.display.safe_insets[3] = 7;
  v.window.revision = 1;
  v.window.pixel_width = 296;
  v.window.pixel_height = 240;
  v.window.safe_insets.left = 22;
  v.window.system_bar_insets.bottom = 19;
  for (unsigned dpi : {120u, 125u, 160u, 205u, 320u}) {
    v.display.density_q16 = dpi * 65536 / 160;
    v.update();
    const auto r = v.content;
    const double scale = v.density() / 65536.;
    // Bounds remain inside all independent constraints after DP rounding.
    assert(r.x * scale >= 22 && r.y * scale >= 23);
    assert((r.x + r.w) * scale <= 296 - 11);
    assert((r.y + r.h) * scale <= 240 - 19);
    assert(r.x * scale - 22 < scale && r.y * scale - 23 < scale);
    // Repeated updates are stable; insets are absolute, never added twice.
    assert(!v.update());
  }
  v.display.density_q16 = 65536;
  v.update();
  const auto before = v.content;
  v.window.system_bar_insets.bottom = 0;
  assert(v.update() && v.content.h > before.h);
  v.window.pixel_width = 999;
  v.window.system_bar_insets.left = 200;
  v.update();
  assert(v.content.x == 7); // stale Window geometry is ignored

  v.window = {};
  v.display.width = 296;
  v.display.height = 240;
  v.display.density_q16 = 65536;
  for (unsigned i = 0; i < 4; ++i) v.display.corner_radii[i] = 58;
  v.display.safe_insets[0] = v.display.safe_insets[2] = 8;
  v.display.safe_insets[1] = v.display.safe_insets[3] = 10;
  v.update();
  assert(v.content.x == 10 && v.content.y == 17);
  assert(v.content.w == 269); // no additional left corner reserve
  v.window.revision = 2;
  v.window.pixel_width = 296;
  v.window.pixel_height = 240;
  v.window.system_bar_insets.left = 26;
  v.update();
  assert(v.content.x == 10 && v.touch_left == 16);
  assert(v.content.w == 269); // Back strip does not move the entire page
  v.window = {};

  // The controls' existing inner padding stays inside each left corner,
  // including larger radii and noninteger density scales.
  for (unsigned radius : {32u, 58u, 120u}) {
    for (unsigned dpi : {120u, 160u, 205u}) {
      v.display.density_q16 = dpi * 65536 / 160;
      v.display.corner_radii[0] = v.display.corner_radii[3] = radius;
      v.update();
      const double s = v.density() / 65536.;
      const double x = (v.content.x + 4) * s;
      for (double y : {(v.content.y + 4) * s,
                       (v.content.y + v.content.h - 4) * s}) {
        if (x >= radius) continue;
        const double cy = y < 120 ? radius : 240 - radius;
        const double dx = x - radius, dy = y - cy;
        assert(dx * dx + dy * dy <= double(radius) * radius);
      }
    }
  }

  v.window = {};
  v.display = {};
  v.display.display_shape = 2;
  for (unsigned side : {101u, 240u, 301u}) {
    v.display.width = side + 79;
    v.display.height = side;
    for (unsigned dpi : {120u, 160u, 205u}) {
      v.display.density_q16 = dpi * 65536 / 160;
      v.update();
      const auto r = v.content;
      const double s = v.density() / 65536.;
      for (int x : {r.x, r.x + r.w})
        for (int y : {r.y, r.y + r.h}) {
          const auto dx = x * s - v.display.width / 2.;
          const auto dy = y * s - side / 2.;
          assert(dx * dx + dy * dy <= side * side / 4.);
        }
    }
  }
  v.display.safe_insets[3] = UINT32_MAX;
  v.update();
  assert(v.content.w == 0);
  v.display.width = v.display.height = 0;
  v.display.density_q16 = 0;
  v.update();
  assert(v.content.w == 0 && v.content.h == 0);
  std::puts("media viewport: asymmetric corners/insets, system strips, fractional DPI, circular bounds, stale metrics and empty regions passed");
}
