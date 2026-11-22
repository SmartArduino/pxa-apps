// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <algorithm>
namespace unciv {
inline int floor_div(int n, int d) { return n / d - (n < 0 && n % d != 0); }
// Preserve painter order while visiting only the rows and columns near the
// viewport. The final conservative bounds match the old full-board culling.
inline int visible_tiles(const State &s, int ox, int oy, int tw, int width,
                         int top, int bottom, int out[N]) {
  int th = tw * 7 / 8, step = tw * 3 / 4, count = 0;
  int left = std::max(0, floor_div(-tw - ox, step));
  int right = std::min(int(s.width) - 1, floor_div(width + tw - ox, step));
  int first = std::max(0, floor_div(top - th - oy - tw * 7 / 16, th) - 1);
  int last = std::min(int(s.height) - 1, floor_div(bottom + th - oy, th) + 1);
  for (int row = first; row <= last; ++row)
    for (int parity = 0; parity < 2; ++parity)
      for (int col = left + ((left & 1) != parity); col <= right; col += 2) {
        int x = col * step + ox, y = row * th + (col & 1) * (tw * 7 / 16) + oy;
        if (x >= -tw && x <= width + tw && y >= top - th && y <= bottom + th)
          out[count++] = row * W + col;
      }
  return count;
}
} // namespace unciv
