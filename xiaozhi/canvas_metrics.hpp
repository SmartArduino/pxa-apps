#pragma once
#include <cstdint>
#include <algorithm>

namespace xiaozhi {
// Window extents are Surface pixels; Canvas drawing and pointer events use dp.
inline std::uint32_t canvas_extent(std::uint32_t pixels, std::uint32_t density) {
    const auto d = std::max(1u, density);
    return std::uint32_t((std::uint64_t(pixels) * 65536 + d - 1) / d);
}
}
