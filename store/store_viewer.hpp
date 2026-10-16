#pragma once

#include <algorithm>
#include <cstdint>

namespace store {

struct ViewerGeometry {
    std::uint16_t image_width, image_height, canvas_width, canvas_height;
};

// Fit once, then enlarge without changing the aspect ratio. Limit style
// coordinates independently of untrusted screenshot metadata.
[[nodiscard]] constexpr ViewerGeometry viewer_geometry(
    std::uint32_t width, std::uint32_t height, std::uint16_t source_width,
    std::uint16_t source_height, unsigned zoom) noexcept {
    width = std::clamp(width, 1u, 8192u);
    height = std::clamp(height, 1u, 8192u);
    const auto sw = source_width ? source_width : width;
    const auto sh = source_height ? source_height : height;
    auto w = width, h = height;
    if (std::uint64_t{sw} * height > std::uint64_t{sh} * width)
        h = std::max(1u, std::uint32_t(std::uint64_t{width} * sh / sw));
    else
        w = std::max(1u, std::uint32_t(std::uint64_t{height} * sw / sh));
    w *= std::clamp(zoom, 1u, 3u);
    h *= std::clamp(zoom, 1u, 3u);
    return {std::uint16_t(w), std::uint16_t(h),
            std::uint16_t(std::max(w, width)), std::uint16_t(std::max(h, height))};
}

enum class ViewerAction : std::uint8_t { none, previous, next, zoom };

// Coordinates are logical dp. Native scrolling owns pan and inertia; moves
// never produce redraw requests. Swipes change pages only at the fit scale.
struct ViewerGesture {
    std::uint64_t last_tap = 0;
    std::int32_t down_x = 0, down_y = 0;
    bool active = false, moved = false;

    constexpr void begin(std::int32_t x, std::int32_t y) noexcept {
        down_x = x; down_y = y; active = true; moved = false;
    }
    constexpr void move(std::int32_t x, std::int32_t y) noexcept {
        if (magnitude(std::int64_t{x} - down_x) > 6 ||
            magnitude(std::int64_t{y} - down_y) > 6) moved = true;
    }
    constexpr void cancel() noexcept { active = false; moved = false; last_tap = 0; }
    [[nodiscard]] constexpr ViewerAction end(std::int32_t x, std::int32_t y,
        std::uint64_t timestamp_us, unsigned zoom) noexcept {
        if (!active) return ViewerAction::none;
        active = false;
        move(x, y);
        const auto dx = std::int64_t{x} - down_x, dy = std::int64_t{y} - down_y;
        if (moved) {
            last_tap = 0;
            if (zoom == 1 && magnitude(dx) >= 32 && magnitude(dx) >= magnitude(dy) * 2)
                return dx < 0 ? ViewerAction::next : ViewerAction::previous;
        } else {
            const bool twice = last_tap && timestamp_us > last_tap &&
                               timestamp_us - last_tap <= 350000;
            last_tap = twice ? 0 : timestamp_us;
            if (twice) return ViewerAction::zoom;
        }
        return ViewerAction::none;
    }
private:
    static constexpr std::int64_t magnitude(std::int64_t value) noexcept {
        return value < 0 ? -value : value;
    }
};
} // namespace store
