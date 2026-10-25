// SPDX-License-Identifier: MIT
#include "backend.hpp"
#include "../common/pocket-play/runtime.hpp"
#include "vendor/src/lib/machineDependent.h"
#include <algorithm>
#include <cmath>

namespace {
uint16_t image[crisp::MAX_VIEW_PIXELS];
int view_w = 100, view_h = 100;
void fill(int x, int y, int w, int h, uint16_t color) {
    const int right = std::min(view_w, x + w), bottom = std::min(view_h, y + h);
    x = std::max(0, x); y = std::max(0, y);
    for (int py = y; py < bottom; ++py)
        for (int px = x; px < right; ++px) image[py * view_w + px] = color;
}
}
namespace crisp {
const uint16_t *view_pixels() { return image; }
int view_width() { return view_w; }
int view_height() { return view_h; }
}
extern "C" {
void md_initView(int w, int h) {
    if (w <= 0 || h <= 0 || w > 150 || h > 150 || w * h > crisp::MAX_VIEW_PIXELS) {
        pocket::log("crisp invalid upstream view");
        return;
    }
    view_w = w; view_h = h;
    std::fill(image, image + view_w * view_h, uint16_t(0));
}
void md_drawRect(float x, float y, float w, float h, unsigned char r,
                 unsigned char g, unsigned char b) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)) return;
    // Match upstream hit-box rounding, then clip before converting to int.
    // Finite off-screen coordinates must never overflow integer geometry.
    const double left = std::floor(double(x)), top = std::floor(double(y));
    const double right = std::min(double(view_w), left + std::floor(double(w)));
    const double bottom = std::min(double(view_h), top + std::floor(double(h)));
    const double clipped_x = std::max(0.0,left), clipped_y = std::max(0.0,top);
    if (right <= clipped_x || bottom <= clipped_y) return;
    fill(int(clipped_x),int(clipped_y),int(right-clipped_x),int(bottom-clipped_y),pocket::rgb(r,g,b));
}
void md_drawCharacter(unsigned char grid[6][6][3], float x, float y, int) {
    if (!std::isfinite(x) || !std::isfinite(y) || x <= -6 || y <= -6 || x >= view_w || y >= view_h) return;
    const int ix = int(std::floor(x)), iy = int(std::floor(y));
    for (int py = 0; py < 6; ++py) for (int px = 0; px < 6; ++px) {
        const unsigned char *c = grid[py][px];
        // Upstream encodes transparency as RGB 0,0,0 (its BLACK is 97,97,97).
        if (c[0] || c[1] || c[2]) fill(ix + px, iy + py, 1, 1, pocket::rgb(c[0],c[1],c[2]));
    }
}
void md_clearView(unsigned char r, unsigned char g, unsigned char b) {
    std::fill(image, image + view_w * view_h, pocket::rgb(r,g,b));
}
void md_clearScreen(unsigned char r, unsigned char g, unsigned char b) { md_clearView(r,g,b); }
void md_playTone(float freq, float duration, float when) { pocket::tone(freq, duration, when); }
void md_stopTone() { pocket::silence(); }
float md_getAudioTime() { return float(pocket::millis()) / 1000.0f; }
void md_consoleLog(char *msg) { pocket::log(msg); }
}
