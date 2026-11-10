// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>

namespace fish {
struct Header {
    int x, y, width, height, menu_x, menu_width;
    bool menu_hit(int px, int py) const {
        return px >= menu_x && px < menu_x + menu_width && py >= y && py < y + height;
    }
};
inline Header header(int width) { return {12, 7, width - 60, 28, width - 40, 28}; }
struct Rug {
    int x, y, radius;
};
struct RoomLayout {
    int sx, sy, sw, sh;
    int rug_x, rug_y, travel;
    int pet_width, pet_height;
    int foot_x(int position) const {
        return rug_x + std::clamp(position, -64, 64) * travel / 64;
    }
    int pet_x(int position) const {
        return foot_x(position) - pet_width * 94 / 192;
    }
    int pet_y() const { return rug_y - pet_height * 132 / 144; }
    int drag_position(int offset) const {
        return travel ? std::clamp(offset * 64 / travel, -64, 64) : 0;
    }
};
inline RoomLayout room_layout(int width, int height, int source_width, int source_height, Rug rug) {
    int sw = source_width, sh = sw * height / width;
    if (sh > source_height) { sh = source_height; sw = sh * width / height; }
    const int sx = std::clamp(rug.x - sw / 2, 0, source_width - sw);
    const int sy = std::clamp(rug.y - sh * 72 / 100, 0, source_height - sh);
    const int rug_x = (rug.x - sx) * width / sw;
    const int rug_y = (rug.y - sy) * height / sh;
    const int pet_height = std::clamp(height * 2 / 3, 80, 184);
    const int radius = rug.radius * width / sw;
    const int travel = std::clamp(radius * 3 / 4 - pet_height / 4, 0, 64);
    return {sx, sy, sw, sh, rug_x, rug_y, travel, pet_height * 4 / 3, pet_height};
}
struct Controls {
    int columns, rows, cell, width, height, top, icon;
    int x(unsigned index) const { return 12 + int(index % columns) * cell; }
    int y(unsigned index) const { return top + int(index / columns) * (height + 4); }
    int hit(int px, int py) const {
        for (unsigned i = 0; i < 6; ++i)
            if (px >= x(i) && px < x(i) + width && py >= y(i) && py < y(i) + height) return int(i);
        return -1;
    }
};
inline Controls controls(int width, int height, int touch_size) {
    const int columns = width - 24 >= 6 * std::max(28, touch_size * 32 / 44) ? 6 : 3;
    const int rows = 6 / columns;
    const int cell = (width - 24) / columns;
    const int button_height = std::clamp(touch_size * 36 / 44, 28, 52);
    const int top = height - 12 - rows * (button_height + 4) + 4;
    return {columns, rows, cell, cell - 4, button_height, top,
        std::clamp(touch_size * 20 / 44, 14, std::min(32, button_height - 8))};
}
} // namespace fish
