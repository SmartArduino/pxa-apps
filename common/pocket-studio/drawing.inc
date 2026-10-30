// SPDX-License-Identifier: CC0-1.0
#include <new>
#include <cstring>
#include <time.h>
namespace studio {
static uint64_t raster_cache_clock() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
bool RasterCache::restore(int x, int y, int width, int height, uint32_t key) {
    pending_ = false;
    if (!canvas || width <= 0 || height <= 0 ||
        int64_t(x) + width > INT32_MAX || int64_t(y) + height > INT32_MAX)
        return false;
    const int left = std::max(0, studio_x(x)), top = std::max(0, studio_y(y));
    const int right = std::min(viewport.width, studio_x(x + width)),
              bottom = std::min(viewport.height, studio_y(y + height));
    return restore_pixels(left, top, right, bottom, key);
}
bool RasterCache::restore_frame(uint32_t key) {
    pending_ = false;
    if (!canvas) return false;
    return restore_pixels(0, 0, viewport.width, viewport.height, key);
}
bool RasterCache::restore_pixels(int left, int top, int right, int bottom, uint32_t key) {
    if (left >= right || top >= bottom) return false;
    const int layout[] = {viewport.width, viewport.height, WIDTH, HEIGHT,
        studio_x(0), studio_y(0), studio_dx(WIDTH), studio_dy(HEIGHT)};
    if (valid_ && key_ == key && left_ == left && top_ == top &&
        width_ == right - left && height_ == bottom - top &&
        std::equal(layout, layout + 8, layout_)) {
        const uint64_t started = raster_cache_clock();
        if (left_ == 0 && width_ == viewport.width)
            std::memcpy(canvas + top_ * viewport.width, pixels_, size_t(width_) * height_ * 2);
        else for (int row = 0; row < height_; ++row)
            std::memcpy(canvas + (top_ + row) * viewport.width + left_,
                        pixels_ + row * width_, size_t(width_) * 2);
        copy_us_ += raster_cache_clock() - started;
        ++hits_;
        return true;
    }
    valid_ = false;
    ++misses_;
    std::copy(layout, layout + 8, layout_);
    left_ = left; top_ = top; width_ = right - left; height_ = bottom - top;
    key_ = key;
    pending_ = true;
    return false;
}
void RasterCache::capture() {
    if (!pending_ || !canvas) return;
    pending_ = false;
    const size_t pixels = size_t(width_) * height_;
    if (capacity_ < pixels) {
        auto* replacement = new (std::nothrow) uint16_t[pixels];
        if (!replacement) { ++allocation_failures_; return; }
        delete[] pixels_;
        pixels_ = replacement;
        capacity_ = pixels;
    }
#ifdef PXA_POCKET_FILL
    // Initialize the full native frame's matte before saving a base layer.
    // Logical content can be inset from it by safe areas or rounded corners.
    if (left_ == 0 && top_ == 0 && width_ == viewport.width && height_ == viewport.height)
        studio_bleed_edges();
#endif
    if (left_ == 0 && width_ == viewport.width)
        std::memcpy(pixels_, canvas + top_ * viewport.width, pixels * 2);
    else for (int row = 0; row < height_; ++row)
        std::memcpy(pixels_ + row * width_,
                    canvas + (top_ + row) * viewport.width + left_, size_t(width_) * 2);
    valid_ = true;
}
void label(int x, int y, int w, const char *s, uint16_t c, int scale, unsigned align) {
    while (scale > 1 && pocket::text_width(s, scale) > w)
        --scale;
    studio_add(x, y, w, s, c, scale, align, 10);
}
void round(int x, int y, int w, int h, int radius, uint16_t c) {
    if (!canvas || w <= 0 || h <= 0)
        return;
    studio_occlude(x, y, w, h);
    const int x0 = studio_x(x), x1 = studio_x(x + w), y0 = studio_y(y), y1 = studio_y(y + h);
    int rx = std::max(1, studio_dx(radius)), ry = std::max(1, studio_dy(radius));
    rx = std::min(rx, (x1 - x0) / 2);
    ry = std::min(ry, (y1 - y0) / 2);
    for (int j = std::max(0, y0); j < std::min(viewport.height, y1); ++j) {
        float dy = j < y0 + ry    ? float(y0 + ry - j - .5f) / std::max(1, ry)
                   : j >= y1 - ry ? float(j - (y1 - ry) + .5f) / std::max(1, ry)
                                  : 0;
        int inset = dy > 0 ? int(rx * (1 - sqrtf(std::max(0.f, 1 - dy * dy))) + .5f) : 0;
        int a = std::max(0, x0 + inset), b = std::min(viewport.width, x1 - inset);
        if (a < b)
            std::fill(canvas + j * viewport.width + a, canvas + j * viewport.width + b, c);
    }
}
void disc(int x, int y, int r, uint16_t c) { round(x - r, y - r, r * 2 + 1, r * 2 + 1, r, c); }
void picture_region(int x, int y, int w, int h, const Image &im, int sx, int sy, int sw, int sh,
                    int darkness, bool cutout) {
    if (!canvas || w <= 0 || h <= 0 || sw <= 0 || sh <= 0 || im.width <= 0 || im.height <= 0)
        return;
    studio_occlude(x, y, w, h);
    const int x0 = studio_x(x), x1 = studio_x(x + w), y0 = studio_y(y), y1 = studio_y(y + h);
    if (x0 == x1 || y0 == y1)
        return;
    const int left = std::max(0, x0), right = std::min(viewport.width, x1);
    int columns[pocket_ui::MAX_SURFACE_WIDTH];
    for (int i = left; i < right; ++i)
        columns[i] = sx + (i - x0) * sw / (x1 - x0);
    uint16_t shaded[256];
    const uint16_t *palette = im.palette;
    if (darkness) {
        for (int i = 0; i < 256; ++i)
            shaded[i] = mix(palette[i], 0, darkness);
        palette = shaded;
    }
    for (int j = std::max(0, y0); j < std::min(viewport.height, y1); ++j) {
        int source_y = sy + (j - y0) * sh / (y1 - y0);
        if (source_y < 0 || source_y >= im.height)
            continue;
        const uint8_t *row = im.pixels + source_y * im.width;
        uint16_t *out = canvas + j * viewport.width;
        for (int i = left; i < right; ++i) {
            int source_x = columns[i];
            if (source_x < 0 || source_x >= im.width)
                continue;
            unsigned index = row[source_x];
            if (!cutout || index)
                out[i] = palette[index];
        }
    }
}
void picture(int x, int y, int w, int h, const Image &im, int darkness) {
    picture_region(x, y, w, h, im, 0, 0, im.width, im.height, darkness, false);
}
void raster(int x, int y, int w, int h, const uint16_t *p, int sw, int sh) {
    if (!canvas || !p || w <= 0 || h <= 0 || sw <= 0 || sh <= 0)
        return;
    studio_occlude(x, y, w, h);
    const int x0 = studio_x(x), x1 = studio_x(x + w), y0 = studio_y(y), y1 = studio_y(y + h);
    if (x0 == x1 || y0 == y1)
        return;
    const int left = std::max(0, x0), right = std::min(viewport.width, x1);
    int columns[pocket_ui::MAX_SURFACE_WIDTH];
    for (int i = left; i < right; ++i)
        columns[i] = (i - x0) * sw / (x1 - x0);
    for (int j = std::max(0, y0); j < std::min(viewport.height, y1); ++j) {
        const uint16_t *row = p + (j - y0) * sh / (y1 - y0) * sw;
        uint16_t *out = canvas + j * viewport.width;
        for (int i = left; i < right; ++i)
            out[i] = row[columns[i]];
    }
}
void glow(int x, int y, int radius, uint16_t c, int intensity) {
    if (!canvas || radius < 1 || intensity < 1)
        return;
    const int cx = studio_x(x), cy = studio_y(y), rx = std::max(1, studio_dx(radius)),
              ry = std::max(1, studio_dy(radius));
    // Equal radii cancel a common r*r factor in the ellipse equation. Keep
    // the exact integer quotients while avoiding a software 64-bit division
    // for every texel on 32-bit targets. Larger/elliptical glows use the
    // original wide arithmetic below.
    if (rx == ry && rx <= 2048 && intensity <= 256) {
        const int square = rx * rx;
        for (int j = std::max(0, cy - ry); j < std::min(viewport.height, cy + ry + 1); ++j) {
            const int dy = j - cy, remaining = square - dy * dy;
            for (int i = std::max(0, cx - rx); i < std::min(viewport.width, cx + rx + 1); ++i) {
                const int dx = i - cx, distance = remaining - dx * dx;
                if (distance <= 0) continue;
                int strength = intensity * distance / square;
                strength = strength * strength / intensity;
                uint16_t &pixel = canvas[j * viewport.width + i];
                pixel = mix(pixel, c, strength);
            }
        }
        return;
    }
    const int64_t limit = int64_t(rx) * rx * ry * ry;
    for (int j = std::max(0, cy - ry); j < std::min(viewport.height, cy + ry + 1); ++j)
        for (int i = std::max(0, cx - rx); i < std::min(viewport.width, cx + rx + 1); ++i) {
            int64_t distance =
                int64_t(i - cx) * (i - cx) * ry * ry + int64_t(j - cy) * (j - cy) * rx * rx;
            if (distance >= limit)
                continue;
            int strength = int(int64_t(intensity) * (limit - distance) / limit);
            strength = strength * strength / std::max(1, intensity);
            uint16_t &pixel = canvas[j * viewport.width + i];
            pixel = mix(pixel, c, strength);
        }
}
} // namespace studio
