// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace map_raster {
void begin();
void flush();
void end();
void sprite(int, int, int, int, const uint16_t *, const uint8_t *, int, int,
            int, int, int);
// A zero tint preserves the upstream icon's colors.
void icon(int, int, int, const uint16_t *, const uint8_t *, int, int, uint16_t);
void overlay(int, int, int, int, const uint8_t *, int, int, uint16_t, int, int,
             int);
void stroke(int, int, int, int, int, uint16_t, int, int);
bool minimap_restore(int, int, int, int, uint32_t);
void minimap_capture();
bool text(int, int, int, const char *, uint16_t, int, unsigned);
void record_draw(uint64_t);
uint64_t now();
unsigned frames();
uint64_t render_us();
} // namespace map_raster
