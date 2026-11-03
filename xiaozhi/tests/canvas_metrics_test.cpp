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
    for (unsigned density : {32768u, 53248u, 65536u, 98304u, 131072u})
        for (unsigned font : {12u, 14u, 18u, 22u}) {
            const auto line = xiaozhi::caption_metrics(font, density);
            const auto physical_height = uint64_t(line.line_height) * density / 65536;
            assert(physical_height >= (font * 3 + 1) / 2 + 2);
            assert(uint64_t(line.advance) * density / 65536 >= font);
            assert(line.line_step > line.line_height);
        }
    std::puts("PASS: Canvas fills native screen at Gen4, standard and high DPI");
}
