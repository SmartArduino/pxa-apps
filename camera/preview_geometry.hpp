// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cstdint>

namespace camera_geometry {
inline int native_preview_extent(uint32_t pixels, uint32_t density_q16) {
    const auto density = std::max(uint32_t(1), density_q16);
    // Preview images are bounded to320 pixels. Round the DP box outward so
    // Host's integer DP conversion never shrinks a native image by one pixel.
    const uint64_t scaled = uint64_t(pixels) * 65536;
    return int((scaled + density - 1) / density);
}
}
