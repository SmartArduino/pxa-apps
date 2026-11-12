// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "art.hpp"
#include "core.hpp"
namespace gfx {
void toggle_language();
void terrain(const md::State &, int ox, int oy, int zoom, bool title);
void minimap(const md::State &, int x, int y, int scale = 1);
void preload(const char *text, int font = 12);
void sprite(const art::Sprite &, int x, int y, int w, int h, int rotation = 0,
            int light = 255, uint16_t tint = 0, bool flipY = false);
void icon(const art::Sprite &, int x, int y, int w, int h, uint16_t color);
void label(int x, int y, int w, const char *, uint16_t color, int font = 12,
           int align = 0);
} // namespace gfx
