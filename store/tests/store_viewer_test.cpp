#include "store_viewer.hpp"
#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    for (const auto width : {1u, 135u, 166u, 240u, 360u, 800u, 0xffffffffu})
        for (const auto height : {1u, 100u, 240u, 600u, 0xffffffffu})
            for (const auto source : {1u, 240u, 480u, 65535u}) {
                const auto fit = store::viewer_geometry(width, height, std::uint16_t(source), 320, 1);
                assert(fit.image_width > 0 && fit.image_height > 0);
                assert(fit.image_width <= fit.canvas_width && fit.image_height <= fit.canvas_height);
                for (unsigned zoom = 2; zoom <= 3; ++zoom) {
                    const auto image = store::viewer_geometry(width, height, std::uint16_t(source), 320, zoom);
                    assert(image.image_width == fit.image_width * zoom);
                    assert(image.image_height == fit.image_height * zoom);
                    assert(image.canvas_width <= 24576 && image.canvas_height <= 24576);
                }
            }
    const auto portrait = store::viewer_geometry(240, 160, 240, 480, 1);
    assert(portrait.image_width == 80 && portrait.image_height == 160);
    const auto unknown = store::viewer_geometry(240, 160, 0, 0, 1);
    assert(unknown.image_width == 240 && unknown.image_height == 160);
    store::ViewerGesture gesture;
    using enum store::ViewerAction;
    assert(gesture.end(0, 0, 1, 1) == none);
    gesture.begin(150, 80); gesture.move(110, 81);
    assert(gesture.end(60, 85, 1000000, 1) == next);
    gesture.begin(50, 80);
    assert(gesture.end(130, 81, 1100000, 1) == previous);
    gesture.begin(100, 80);
    assert(gesture.end(20, 81, 1200000, 2) == none); // Pan is not a page change.
    gesture.begin(100, 30);
    assert(gesture.end(20, 130, 1300000, 1) == none); // Diagonal drag.
    gesture.begin(80, 80);
    assert(gesture.end(81, 81, 2000000, 1) == none);
    gesture.begin(82, 80);
    assert(gesture.end(82, 80, 2200000, 1) == zoom);
    gesture.begin(80, 80); gesture.cancel();
    assert(gesture.end(80, 80, 2400000, 1) == none);
    gesture.begin(std::numeric_limits<std::int32_t>::min(), 0);
    assert(gesture.end(std::numeric_limits<std::int32_t>::max(), 0, 3000000, 1) == previous);
}
