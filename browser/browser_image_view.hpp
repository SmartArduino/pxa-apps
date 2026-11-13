#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace browser {
// Only viewport state. Pixels remain owned by the existing two-entry cache.
// Coordinates are relative to its centre, so resizing preserves the focus.
struct ImageView {
    int index = -1, zoom = 1, x = 0, y = 0;
    bool width_fit = false;
    constexpr bool active() const noexcept { return index >= 0; }
    constexpr void open(int image) noexcept { *this = {image, 1, 0, 0}; }
    constexpr void fit() noexcept { zoom = 1; x = y = 0; width_fit = false; }
    constexpr std::array<int, 4> rectangle(int width, int height, int vw, int vh) const noexcept {
        width = std::clamp(width, 1, 4096);
        height = std::clamp(height, 1, 4096);
        vw = std::clamp(vw, 1, 16384);
        vh = std::clamp(vh, 1, 16384);
        int w = vw, h = std::max(1, int(std::int64_t(w) * height / width));
        if (!width_fit && h > vh) {
            h = vh;
            w = std::max(1, int(std::int64_t(h) * width / height));
        }
        w *= std::clamp(zoom, 1, 8);
        h *= std::clamp(zoom, 1, 8);
        if (h > 16384) { w = std::max(1, int(std::int64_t(w) * 16384 / h)); h = 16384; }
        if (w > 16384) { h = std::max(1, int(std::int64_t(h) * 16384 / w)); w = 16384; }
        const int cx = (vw - w) / 2, cy = (vh - h) / 2;
        return {cx + (w > vw ? std::clamp(x, vw - w - cx, -cx) : 0),
                cy + (h > vh ? std::clamp(y, vh - h - cy, -cy) : 0), w, h};
    }
    constexpr std::array<int, 4> clamp(int width, int height, int vw, int vh) noexcept {
        const auto box = rectangle(width, height, vw, vh);
        x = box[0] - (vw - box[2]) / 2;
        y = box[1] - (vh - box[3]) / 2;
        return box;
    }
    constexpr void resize(int direction, int width, int height, int vw, int vh) noexcept {
        const int next = direction > 0 ? std::min(8, zoom * 2) : std::max(1, zoom / 2);
        x = x * next / zoom;
        y = y * next / zoom;
        zoom = next;
        clamp(width, height, vw, vh);
    }
    constexpr void fit_width(int width, int height, int vw, int vh) noexcept {
        zoom = 1; x = 0; y = 16384; width_fit = true;
        clamp(width, height, vw, vh); // begin at the top of a long page
    }
    constexpr bool pan(int dx, int dy, int width, int height,
                                              int vw, int vh) noexcept {
        const int old_x = x, old_y = y;
        x = int(std::clamp(std::int64_t(x) + dx, std::int64_t(-16384), std::int64_t(16384)));
        y = int(std::clamp(std::int64_t(y) + dy, std::int64_t(-16384), std::int64_t(16384)));
        clamp(width, height, vw, vh);
        return x != old_x || y != old_y;
    }
};
static_assert(sizeof(ImageView) <= 20);
} // namespace browser
