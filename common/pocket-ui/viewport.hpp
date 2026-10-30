// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stdint.h>

namespace pocket_ui {
// UI pointer events use dp; Surface positions and dimensions use pixels.
inline int32_t pointer_pixel(int32_t coordinate, uint32_t density_q16) {
    const int64_t scaled = int64_t(coordinate) * (density_q16 ? density_q16 : 65536) / 65536;
    return scaled > INT32_MAX ? INT32_MAX : scaled < INT32_MIN ? INT32_MIN : int32_t(scaled);
}
struct CanvasSpan { int origin, length; };
// Cover a nonnegative physical span with integer-dp Canvas geometry. The Host
// floors positions and lengths independently, so rounding each to nearest dp
// can leave a one-pixel hole in a letterbox matte at fractional densities.
inline CanvasSpan cover_pixel_span(int origin, int length, uint32_t density_q16) {
    if (origin < 0 || length <= 0) return {0, 0};
    const uint64_t density = density_q16 ? density_q16 : 65536;
    const uint64_t logical_origin = ((uint64_t(origin) + 1) * 65536 - 1) / density;
    const uint64_t pixel_origin = logical_origin * density / 65536;
    const uint64_t needed = uint64_t(origin) + length - pixel_origin;
    return {int(logical_origin), int((needed * 65536 + density - 1) / density)};
}
#ifdef PXA_POCKET_RESPONSIVE
constexpr int MAX_SURFACE_WIDTH = 480, MAX_SURFACE_HEIGHT = 480;
#else
constexpr int MAX_SURFACE_WIDTH = 480, MAX_SURFACE_HEIGHT = 320;
#endif
constexpr unsigned MAX_SURFACE_PIXELS = MAX_SURFACE_WIDTH * MAX_SURFACE_HEIGHT;
inline int floor_ratio(int64_t numerator, int denominator) {
    return int(numerator < 0 ? -1 - ((-numerator - 1) / denominator) : numerator / denominator);
}
struct Viewport {
    int x = 0, y = 0, width = 288, height = 224, host_scale = 1;
    int physical_width() const { return width * host_scale; }
    int physical_height() const { return height * host_scale; }
    unsigned pixels() const { return unsigned(width * height); }
    bool contains(int px, int py) const {
        return px >= x && py >= y && int64_t(px) < int64_t(x)+physical_width() &&
            int64_t(py) < int64_t(y)+physical_height();
    }
    int map_x(int px, int logical_width = 288) const {
        return floor_ratio(int64_t(floor_ratio(int64_t(px)-x, host_scale))*logical_width, width);
    }
    int map_y(int py, int logical_height = 224) const {
        return floor_ratio(int64_t(floor_ratio(int64_t(py)-y, host_scale))*logical_height, height);
    }
};
inline void fit_size(int available_w, int available_h, int logical_w, int logical_h, int &w, int &h) {
    if (int64_t(available_w)*logical_h <= int64_t(available_h)*logical_w) {
        w = available_w; h = int(int64_t(w)*logical_h/logical_w);
    } else {
        h = available_h; w = int(int64_t(h)*logical_w/logical_h);
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
}
// PXA scales a surface by the largest fitting exact 1x/2x/4x factor. Select
// bounded guest dimensions first, then apply exactly the same host rule.
inline Viewport fit_viewport(int display_w, int display_h, int logical_w = 288, int logical_h = 224) {
    if (display_w < 1) display_w = 1;
    if (display_h < 1) display_h = 1;
    Viewport v;
    int divisor = 1;
    for (;;) {
        fit_size(display_w/divisor > 0 ? display_w/divisor : 1,
                 display_h/divisor > 0 ? display_h/divisor : 1, logical_w, logical_h, v.width, v.height);
        if ((v.width <= MAX_SURFACE_WIDTH && v.height <= MAX_SURFACE_HEIGHT) || divisor == 4) break;
        divisor *= 2;
    }
    if (v.width > MAX_SURFACE_WIDTH || v.height > MAX_SURFACE_HEIGHT)
        fit_size(MAX_SURFACE_WIDTH, MAX_SURFACE_HEIGHT, logical_w, logical_h, v.width, v.height);
    for (int scale = 1; scale <= 4; scale *= 2)
        if (v.width <= display_w/scale && v.height <= display_h/scale) v.host_scale = scale;
    v.x = (display_w-v.physical_width())/2;
    v.y = (display_h-v.physical_height())/2;
    return v;
}
// Edge-to-edge surface. The layout reflows to this aspect instead of fitting
// a fixed game rectangle. The Host still uses its exact integer scale rule.
inline Viewport fill_viewport(int display_w, int display_h) {
    display_w = display_w > 0 ? display_w : 1;
    display_h = display_h > 0 ? display_h : 1;
    Viewport v;
    int divisor = 1;
    while (divisor < 4 && (display_w / divisor > MAX_SURFACE_WIDTH ||
                           display_h / divisor > MAX_SURFACE_HEIGHT)) divisor *= 2;
    v.width = display_w / divisor > 0 ? display_w / divisor : 1;
    v.height = display_h / divisor > 0 ? display_h / divisor : 1;
    if (v.width > MAX_SURFACE_WIDTH || v.height > MAX_SURFACE_HEIGHT)
        fit_size(MAX_SURFACE_WIDTH, MAX_SURFACE_HEIGHT, display_w, display_h, v.width, v.height);
    v.host_scale = 1;
    for (int scale = 2; scale <= 4; scale *= 2)
        if (v.width <= display_w / scale && v.height <= display_h / scale) v.host_scale = scale;
    v.x = (display_w - v.physical_width()) / 2;
    v.y = (display_h - v.physical_height()) / 2;
    return v;
}
inline void resample(const uint16_t *source, int source_w, int source_h, uint16_t *destination, int w, int h) {
    int columns[MAX_SURFACE_WIDTH];
    for (int x = 0; x < w; ++x) columns[x] = int(int64_t(x)*source_w/w);
    for (int y = 0; y < h; ++y) {
        const uint16_t *row = source + int64_t(y)*source_h/h*source_w;
        for (int x = 0; x < w; ++x) destination[y*w+x] = row[columns[x]];
    }
}
}
