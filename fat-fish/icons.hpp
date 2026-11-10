// SPDX-License-Identifier: MIT
#pragma once
#include "controls.inc"

namespace fish {
inline void icon(unsigned action, int x, int y, int size, bool sleeping = false) {
    const unsigned id = action == 3 && sleeping ? 9 : action;
    if (id >= 10 || size <= 0) return;
    studio::picture_region(x, y, size, size, controls_image,
        int(id % 5) * 64, int(id / 5) * 64, 64, 64, 0, true);
}
} // namespace fish
