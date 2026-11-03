#pragma once
#include <cstdint>
#include <algorithm>

namespace xiaozhi {
// Window extents are Surface pixels; Canvas drawing and pointer events use dp.
inline std::uint32_t canvas_extent(std::uint32_t pixels, std::uint32_t density) {
    const auto d = std::max(1u, density);
    return std::uint32_t((std::uint64_t(pixels) * 65536 + d - 1) / d);
}
struct CaptionMetrics {
    int advance, line_height, line_step;
};
inline CaptionMetrics caption_metrics(unsigned font_pixels, std::uint32_t density) {
    // text_sized uses native font pixels; its clipping rectangle uses dp.
    // Reserve ascent/descent room for the system's CJK and Latin fallback fonts.
    const int height = int(canvas_extent((font_pixels * 3 + 1) / 2 + 2, density));
    return {int(canvas_extent(font_pixels, density)), height, height + 3};
}
}
