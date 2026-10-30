// SPDX-License-Identifier: CC0-1.0
// Exercise the production native blitter with synthetic images and guarded memory.
#define PXA_POCKET_STUDIO 1
#include "../../pocket-game/runtime.hpp"
#include "../../pocket-ui/viewport.hpp"
#include "../studio.hpp"
#include "../particles.hpp"
#include <algorithm>
#include <assert.h>
#include <iterator>
#include <stdio.h>
#include <string.h>
#include <vector>
namespace {
pocket_ui::Viewport viewport;
constexpr int WIDTH = pocket::WIDTH, HEIGHT = pocket::HEIGHT;
uint16_t guarded[pocket_ui::MAX_SURFACE_PIXELS + 2];
uint16_t *canvas = guarded + 1;
int studio_dx(int x) { return int(int64_t(x) * viewport.width / WIDTH); }
int studio_dy(int y) { return int(int64_t(y) * viewport.height / HEIGHT); }
int studio_x(int x) { return studio_dx(x); }
int studio_y(int y) { return studio_dy(y); }
void studio_occlude(int, int, int, int) {}
void studio_add(int, int, int, const char *, uint16_t, int, unsigned, int) {}
void reset(uint16_t value = 0) {
    std::fill(std::begin(guarded), std::end(guarded), uint16_t(0x5aa5));
    std::fill(canvas, canvas + viewport.pixels(), value);
}
void guard() {
    assert(guarded[0] == 0x5aa5);
    assert(guarded[viewport.pixels() + 1] == 0x5aa5);
}
} // namespace
namespace pocket {
int text_width(const char *s, int scale) { return pocket_ui::text_width(s, scale); }
} // namespace pocket
#include "../drawing.inc"
static void check_glow(int x, int y, int radius, uint16_t color, int intensity) {
    reset();
    for (unsigned i = 0; i < viewport.pixels(); ++i) canvas[i] = uint16_t(i * 137u + 19u);
    std::vector<uint16_t> expected(canvas, canvas + viewport.pixels());
    if (radius > 0 && intensity > 0) {
        const int cx = studio_x(x), cy = studio_y(y),
                  rx = std::max(1, studio_dx(radius)), ry = std::max(1, studio_dy(radius));
        const int64_t limit = int64_t(rx) * rx * ry * ry;
        for (int j = std::max(0, cy - ry); j < std::min(viewport.height, cy + ry + 1); ++j)
            for (int i = std::max(0, cx - rx); i < std::min(viewport.width, cx + rx + 1); ++i) {
                const int64_t distance = int64_t(i - cx) * (i - cx) * ry * ry +
                                         int64_t(j - cy) * (j - cy) * rx * rx;
                if (distance >= limit) continue;
                int strength = int(int64_t(intensity) * (limit - distance) / limit);
                strength = strength * strength / intensity;
                auto& pixel = expected[j * viewport.width + i];
                pixel = studio::mix(pixel, color, strength);
            }
    }
    studio::glow(x, y, radius, color, intensity);
    assert(std::equal(expected.begin(), expected.end(), canvas));
    guard();
}
int main() {
    const uint16_t red = pocket::rgb(250, 30, 20), green = pocket::rgb(30, 240, 50),
                   blue = pocket::rgb(30, 50, 240), bg = pocket::rgb(11, 22, 33);
    uint16_t palette[256] = {0, red, green, blue};
    const uint8_t indexed[] = {1, 2, 0, 3, 0, 1};
    const studio::Image image{3, 2, palette, indexed};
    const uint16_t raster[] = {red, green, blue, red};
    for (auto size : {std::pair{480, 320}, std::pair{288, 224}, std::pair{262, 203}}) {
        viewport.width = size.first;
        viewport.height = size.second;
        reset(bg);
        studio::raster(0, 0, WIDTH, HEIGHT, raster, 2, 2);
        assert(canvas[0] == red && canvas[viewport.width - 1] == green &&
               canvas[(viewport.height - 1) * viewport.width] == blue &&
               canvas[viewport.pixels() - 1] == red);
        guard();
        reset(bg);
        studio::picture_region(0, 0, WIDTH, HEIGHT, image, 0, 0, 3, 2, 0, true);
        assert(canvas[0] == red && canvas[viewport.width - 1] == bg &&
               canvas[(viewport.height - 1) * viewport.width] == blue &&
               canvas[viewport.pixels() - 1] == red);
        guard();
        reset(bg);
        studio::picture_region(0, 0, WIDTH, HEIGHT, image, 1, 0, 1, 1, 0, false);
        for (unsigned i = 0; i < viewport.pixels(); ++i)
            assert(canvas[i] == green);
        guard();
        reset(bg);
        studio::picture_region(-50, -40, 100, 80, image, 0, 0, 3, 2, 0, true);
        studio::round(-8, -9, 25, 30, 8, red);
        studio::round(WIDTH - 5, HEIGHT - 5, 20, 30, 8, blue);
        guard();
        reset();
        studio::glow(WIDTH / 2, HEIGHT / 2, 12, 0xffff, 150);
        assert(canvas[studio_y(HEIGHT / 2) * viewport.width + studio_x(WIDTH / 2)] != 0);
        assert(canvas[0] == 0 && canvas[viewport.pixels() - 1] == 0);
        guard();
        uint16_t center = canvas[studio_y(HEIGHT / 2) * viewport.width + studio_x(WIDTH / 2)];
        studio::glow(WIDTH / 2, HEIGHT / 2, 12, red, 0);
        assert(canvas[studio_y(HEIGHT / 2) * viewport.width + studio_x(WIDTH / 2)] == center);
        studio::glow(-2, -2, 15, blue, 200);
        studio::glow(WIDTH + 2, HEIGHT + 2, 15, green, 200);
        guard();
        studio::Particles<8> particles;assert(!particles.active()&&!particles.tick(100));
        particles.burst(0,0,red,1000);assert(particles.active()==8);particles.draw();guard();
        particles.burst(WIDTH-1,HEIGHT-1,green,8);assert(particles.active()==8);particles.draw();guard();
        particles.tick(1000);assert(!particles.active()&&!particles.tick(100));
        particles.burst(50,50,blue,4);particles.clear();assert(!particles.active());
        // A cache must retain physical pixels, including clipped borders, and
        // miss after layout/key changes. It never captures later animation.
        studio::RasterCache cache;
        reset(bg);
        assert(!cache.restore(-3, -4, WIDTH + 8, HEIGHT + 9));
        studio::glow(WIDTH / 2, HEIGHT / 2, 23, red, 65);
        std::vector<uint16_t> cached(canvas, canvas + viewport.pixels());
        cache.capture();
        studio::glow(WIDTH / 2, HEIGHT / 2, 12, green, 150);
        assert(cache.restore(-3, -4, WIDTH + 8, HEIGHT + 9));
        assert(std::equal(cached.begin(), cached.end(), canvas));
        assert(!cache.restore(-3, -4, WIDTH + 8, HEIGHT + 9, 1));
        reset(blue); cache.capture();
        reset(red);
        assert(cache.restore(-3, -4, WIDTH + 8, HEIGHT + 9, 1));
        for (unsigned i = 0; i < viewport.pixels(); ++i) assert(canvas[i] == blue);
        const int old_width = viewport.width;
        --viewport.width;
        assert(!cache.restore(-3, -4, WIDTH + 8, HEIGHT + 9, 1));
        cache.capture();
        viewport.width = old_width;
        assert(!cache.restore(-3, -4, WIDTH + 8, HEIGHT + 9, 1));
        reset(bg); cache.capture();
        assert(!cache.restore(0, 0, 0, 1));
        assert(!cache.restore(WIDTH + 1, 0, 1, 1));
        assert(!cache.restore(INT32_MAX, 0, 4, 4));
        guard();
        for (int radius : {0, 1, 12, 23, 78, 2048, 2049})
            for (int intensity : {0, 1, 19, 38, 65, 150, 256, 257}) {
                check_glow(WIDTH / 2, HEIGHT / 2, radius, 0xffff, intensity);
                check_glow(-2, -2, radius, blue, intensity);
                check_glow(WIDTH + 2, HEIGHT + 2, radius, green, intensity);
            }
    }
    puts("PASS native drawing: RGB565 scaling, transparent sprite index, source subregions, "
         "clipping, byte-exact circular/elliptical glow and guarded edges at three sizes");
}
