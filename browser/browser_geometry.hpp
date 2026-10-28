#pragma once
#include <pxa/ui_geometry.hpp>

namespace browser {
using pxa::ui::DisplayMetrics;
using pxa::ui::PixelRect;

// Intersect a horizontal band with the panel outline. Unlike a single
// inscribed rectangle, this keeps the broad middle of a rounded screen usable.
// Called only when display/window metrics change, never while scrolling.
inline PixelRect horizontal_band(const DisplayMetrics &display, int y, int height) {
    const int width = int(std::min(display.width, 65535u));
    const int screen_height = int(std::min(display.height, 65535u));
    const int top = std::clamp(y, int(std::min(display.safe.top, unsigned(screen_height))),
                               screen_height);
    const int limit = std::max(top, screen_height - int(std::min(display.safe.bottom,
                                                               unsigned(screen_height))));
    const int bottom = int(std::clamp(std::int64_t(y) + std::max(0, height),
                                      std::int64_t(top), std::int64_t(limit)));
    int left = int(std::min(display.safe.left, unsigned(width)));
    int right = int(std::min(display.safe.right, unsigned(width)));
    if (display.shape == 2) {
        const double radius = std::min(width, screen_height) / 2.;
        const double dy = std::max(std::abs(top - screen_height / 2.),
                                   std::abs(bottom - screen_height / 2.));
        const double half = std::sqrt(std::max(0., radius * radius - dy * dy));
        left = std::max(left, int(std::ceil(width / 2. - half)));
        right = std::max(right, int(std::ceil(width / 2. - half)));
    } else if (display.shape == 1) {
        auto inset = [&](unsigned corner, int distance) {
            const double radius = std::min(display.corners[corner],
                                            unsigned(std::min(width, screen_height) / 2));
            const double dy = std::max(0., radius - distance);
            return int(std::ceil(radius - std::sqrt(std::max(0., radius * radius - dy * dy))));
        };
        left = std::max({left, inset(0, top), inset(3, screen_height - bottom)});
        right = std::max({right, inset(1, top), inset(2, screen_height - bottom)});
    }
    return {left, top, std::max(0, width - left - right), std::max(0, bottom - top)};
}

struct Viewport {
    PixelRect address, content, toolbar, status;
};

inline Viewport viewport(DisplayMetrics display, pxa::Insets occupied) {
    auto merge = [](auto &margin, auto other) { margin = std::max(margin, other); };
    merge(display.safe.left, occupied.left);
    merge(display.safe.top, occupied.top);
    merge(display.safe.right, occupied.right);
    merge(display.safe.bottom, occupied.bottom);
    const int height = int(std::min(display.height, 65535u));
    auto dp = [&](int n) { return std::max(1, int(std::int64_t(n) * display.density_q16 / 65536)); };
    // A circular panel has almost no width at its poles. Keep controls away
    // from those tips while letting the document use its wider central band.
    const int side = int(std::min({display.width, display.height, 65535u}));
    const int pole = display.shape == 2 ? (height - side + 1) / 2 + side / 8 : 0;
    const int top = std::min(height, std::max(pole, int(std::min(display.safe.top,
                                                               unsigned(height)))) + dp(2));
    const int bottom = std::max(top, height - std::max(pole, int(std::min(display.safe.bottom,
                                                                        unsigned(height)))) - dp(2));
    const int row = std::min(dp(32), std::max(16, (bottom - top) / 5));
    auto inset = [&](PixelRect band, int padding, bool centered = true) {
        // Keep the control rows centered. Reading content can use the actual
        // safe width without mirroring a one-sided gesture strip on the right.
        if (centered) {
            const int width = int(std::min(display.width, 65535u));
            const int margin = std::max(band.x, width - band.x - band.width);
            band.x = margin;
            band.width = std::max(0, width - 2 * margin);
        }
        const int amount = std::min(padding, band.width / 2);
        band.x += amount;
        band.width -= 2 * amount;
        return band;
    };
    auto address = inset(horizontal_band(display, top, row), dp(2));
    auto toolbar = inset(horizontal_band(display, std::max(top, bottom - row), row), dp(2));
    const int begin = address.y + address.height + dp(3);
    auto content = inset(horizontal_band(display, begin,
        std::max(0, toolbar.y - dp(3) - begin)), dp(4), false);
    // Use spare space below the controls only when it is inside both the
    // physical panel outline and the system's safe/gesture margins. Smaller
    // panels use a transient overlay, without reserving a permanent text row.
    const int status_height = dp(14);
    const int status_y = toolbar.y + toolbar.height + dp(2);
    auto status = inset(horizontal_band(display, status_y, status_height), dp(4), false);
    if (status.height != status_height || status.width < dp(96))
        status = {content.x, content.y + std::max(0, content.height - status_height),
                  content.width, std::min(content.height, status_height)};
    return {address, content, toolbar, status};
}
} // namespace browser
