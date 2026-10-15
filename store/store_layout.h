#pragma once

#include <cstdint>
#include <limits>

namespace store::layout {

// UI environment dimensions and Window insets are panel pixels. UI style
// lengths (including the legacy PXA_UI_LENGTH_PX alias) are density-scaled dp.
// Round outward so a safe area never loses a pixel when converted back.
[[nodiscard]] constexpr std::uint32_t pixels_to_dp(
    std::uint32_t pixels, std::uint32_t density_q16) noexcept {
    const auto density = density_q16 == 0 ? 65536u : density_q16;
    const auto value = ((std::uint64_t{pixels} << 16) + density - 1) / density;
    return value > std::numeric_limits<std::uint32_t>::max()
        ? std::numeric_limits<std::uint32_t>::max()
        : static_cast<std::uint32_t>(value);
}

// Window names each edge; UI arrays use top, right, bottom, left.
template <typename Insets>
constexpr void copy_window_insets(std::uint32_t (&output)[4],
                                  const Insets& insets) noexcept {
    output[0] = insets.top;
    output[1] = insets.right;
    output[2] = insets.bottom;
    output[3] = insets.left;
}

} // namespace store::layout
