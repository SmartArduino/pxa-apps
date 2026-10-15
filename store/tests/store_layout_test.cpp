#include "store_layout.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

using store::layout::pixels_to_dp;

int main() {
    // Reproduce the affected pai-touch: 296x240 at 130 DPI.
    constexpr auto pai_density = 130u * 65536u / 160u;
    static_assert(pixels_to_dp(296, pai_density) == 365);
    static_assert(pixels_to_dp(240, pai_density) == 296);
    static_assert(pixels_to_dp(240, 65536) == 240);
    static_assert(pixels_to_dp(1080, 3 * 65536) == 360);
    static_assert(pixels_to_dp(240, 0) == 240);
    static_assert(pixels_to_dp(0, 1) == 0);
    static_assert(pixels_to_dp(std::numeric_limits<std::uint32_t>::max(), 1)
                  == std::numeric_limits<std::uint32_t>::max());

    // Converting a physical edge to a dp style must cover the edge again;
    // otherwise a bottom gap or unsafe hit area appears after DPI scaling.
    for (const auto dpi : {80u, 130u, 160u, 216u, 320u, 480u}) {
        const auto density = dpi * 65536u / 160u;
        for (std::uint32_t pixels = 0; pixels <= 4096; ++pixels) {
            const auto dp = pixels_to_dp(pixels, density);
            const auto restored = std::uint64_t{dp} * density / 65536u;
            assert(restored >= pixels);
            if (dp != 0)
                assert(std::uint64_t{dp - 1} * density / 65536u < pixels);
        }
    }

    struct Insets { std::uint32_t left, top, right, bottom; };
    std::uint32_t edges[4]{};
    // Distinct edges catch the previous left/top/right/bottom permutation.
    store::layout::copy_window_insets(edges, Insets{3, 17, 7, 29});
    assert((std::array{edges[0], edges[1], edges[2], edges[3]}
            == std::array{17u, 7u, 29u, 3u}));
}
