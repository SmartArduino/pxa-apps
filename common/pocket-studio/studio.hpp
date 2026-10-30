// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <math.h>
#include <stddef.h>
#include <stdint.h>
namespace studio {
struct Image {
    int width, height;
    const uint16_t *palette;
    const uint8_t *pixels;
};
inline uint16_t mix(uint16_t a, uint16_t b, int n) {
    n = n < 0 ? 0 : n > 256 ? 256 : n;
    return uint16_t((((((a >> 11) & 31) * (256 - n) + ((b >> 11) & 31) * n) / 256) << 11) |
                    (((((a >> 5) & 63) * (256 - n) + ((b >> 5) & 63) * n) / 256) << 5) |
                    (((a & 31) * (256 - n) + (b & 31) * n) / 256));
}
#ifdef PXA_POCKET_STUDIO
// Cache bitmap-only drawing between restore()/capture(). Native text and its
// occlusion/order metadata are not cached and must still be emitted each frame. A miss
// or allocation failure keeps ordinary drawing available; layout changes miss.
class RasterCache {
    uint16_t *pixels_ = nullptr;
    size_t capacity_ = 0;
    int layout_[8]{};
    int left_ = 0, top_ = 0, width_ = 0, height_ = 0;
    uint32_t key_ = 0;
    uint32_t hits_ = 0, misses_ = 0, allocation_failures_ = 0;
    uint64_t copy_us_ = 0;
    bool valid_ = false, pending_ = false;
    bool restore_pixels(int left, int top, int right, int bottom, uint32_t key);
public:
    RasterCache() = default;
    ~RasterCache() { delete[] pixels_; }
    RasterCache(const RasterCache&) = delete;
    RasterCache& operator=(const RasterCache&) = delete;
    bool restore(int x, int y, int width, int height, uint32_t key = 0);
    bool restore_frame(uint32_t key = 0);
    void capture();
    uint32_t hits() const { return hits_; }
    uint32_t misses() const { return misses_; }
    uint32_t allocation_failures() const { return allocation_failures_; }
    size_t bytes() const { return capacity_ * sizeof(uint16_t); }
    uint32_t mean_copy_us() const { return hits_ ? uint32_t(copy_us_ / hits_) : 0; }
};
void round(int x, int y, int w, int h, int radius, uint16_t color);
void disc(int x, int y, int r, uint16_t color);
void picture(int x, int y, int w, int h, const Image &image, int darkness = 0);
void label(int x, int y, int w, const char *text, uint16_t color, int scale = 1,
           unsigned align = 1);
void picture_region(int x, int y, int w, int h, const Image &image, int sx, int sy, int sw, int sh,
                    int darkness = 0, bool cutout = false);
void raster(int x, int y, int w, int h, const uint16_t *pixels, int source_w, int source_h);
void glow(int x, int y, int radius, uint16_t color, int intensity = 100);
#else
inline void round(int x, int y, int w, int h, int r, uint16_t c) {
    for (int i = 0; i < h; ++i) {
        int dy = i < r ? r - i - 1 : i >= h - r ? i - (h - r) : 0;
        int dx = dy ? r - int(sqrt(double(r * r - dy * dy))) : 0;
        pocket::rect(x + dx, y + i, w - 2 * dx, 1, c);
    }
}
inline void disc(int x, int y, int r, uint16_t c) {
    for (int i = -r; i <= r; ++i) {
        int dx = int(sqrt(double(r * r - i * i)));
        pocket::rect(x - dx, y + i, dx * 2 + 1, 1, c);
    }
}
inline void picture(int x, int y, int w, int h, const Image &im, int dark = 0) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            pocket::rect(
                x + i, y + j, 1, 1,
                mix(im.palette[im.pixels[(j * im.height / h) * im.width + i * im.width / w]], 0,
                    dark));
}
inline void label(int x, int y, int w, const char *s, uint16_t c, int scale = 1,
                  unsigned align = 1) {
    while (scale > 1 && pocket::text_width(s, scale) > w)
        --scale;
    pocket::text(x + (align ? (w - pocket::text_width(s, scale)) / (align == 1 ? 2 : 1) : 0), y, s,
                 c, scale);
}
inline void picture_region(int x, int y, int w, int h, const Image &im, int sx, int sy, int sw,
                           int sh, int dark = 0, bool cutout = false) {
    if (w <= 0 || h <= 0)
        return;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            int xx = sx + i * sw / w, yy = sy + j * sh / h;
            if (xx < 0 || yy < 0 || xx >= im.width || yy >= im.height)
                continue;
            unsigned index = im.pixels[yy * im.width + xx];
            if (!cutout || index)
                pocket::rect(x + i, y + j, 1, 1, mix(im.palette[index], 0, dark));
        }
}
inline void raster(int x, int y, int w, int h, const uint16_t *p, int sw, int sh) {
    if (w <= 0 || h <= 0)
        return;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            pocket::rect(x + i, y + j, 1, 1, p[j * sh / h * sw + i * sw / w]);
}
inline void glow(int x, int y, int r, uint16_t c, int intensity = 100) {
    (void)intensity;
    disc(x, y, r > 3 ? 1 : 0, c);
}
#endif
inline void gradient(int x, int y, int w, int h, uint16_t a, uint16_t b) {
    for (int row = 0; row < h; ++row)
        pocket::rect(x, y + row, w, 1, mix(a, b, row * 256 / (h ? h : 1)));
}
inline void card(int x, int y, int w, int h, uint16_t fill, uint16_t edge, int r = 6) {
    round(x, y + 2, w, h, r, pocket::rgb(6, 16, 27));
    round(x, y, w, h, r, edge);
    round(x + 1, y + 1, w - 2, h - 2, r - 1, fill);
}
inline void button(int x, int y, int w, int h, const char *v, uint16_t fill, uint16_t ink,
                   bool selected = false) {
    card(x, y, w, h, fill, selected ? mix(fill, 0xffff, 120) : mix(fill, 0xffff, 32), 5);
    label(x + 3, y + (h - 10) / 2, w - 6, v, ink);
}
inline void sparkle(int x, int y, uint16_t c) {
    pocket::line(x - 2, y, x + 2, y, c);
    pocket::line(x, y - 2, x, y + 2, c);
}
} // namespace studio
