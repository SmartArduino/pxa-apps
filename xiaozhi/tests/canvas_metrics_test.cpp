#include "../canvas_metrics.hpp"
#include <cassert>
#include <cstdio>

int main() {
    for (unsigned pixels : {128u, 240u, 284u, 320u, 480u})
        for (unsigned density : {53248u, 65536u, 98304u, 131072u}) {
            auto extent = xiaozhi::canvas_extent(pixels, density);
            auto physical = uint64_t(extent) * density / 65536;
            assert(physical >= pixels && physical - pixels <= 2);
        }
    assert(xiaozhi::canvas_extent(240, 65536) == 240);
    assert(xiaozhi::canvas_extent(240, 53248) == 296);
    std::puts("PASS: Canvas fills native screen at Gen4, standard and high DPI");
}
