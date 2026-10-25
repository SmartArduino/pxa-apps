/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
typedef struct {
    uint32_t light, age;
    uintptr_t texture;
    int16_t x, y;
    uint8_t valid[8], pixels[64];
} pxa_quake_color_tile_t;
pxa_quake_color_tile_t *Pxa_QuakeColorTile(uint32_t light, uintptr_t texture,
                                         int x, int y);
void Pxa_QuakeColorReset(void);
extern uint32_t pxa_quake_color_hits, pxa_quake_color_misses;
