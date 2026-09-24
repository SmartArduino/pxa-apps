#ifndef MAZE_SPIKE_PALETTE_H
#define MAZE_SPIKE_PALETTE_H

#include <stdint.h>

#define LIGHT_LEVELS 16

/* Builds the 16 light-level colormaps (16 x 256 canonical RGB565), a direct
 * port of maze-evil gfx/palette.cpp. */
void palette_build(void);

/* One light level of the colormap, clamped to [0, LIGHT_LEVELS - 1]. */
const uint16_t *palette_light(int light);

/* Full-brightness colour of one palette index, for HUD text and fills. */
uint16_t palette_color(uint8_t index);

#endif
