#ifndef TOMB_PALETTE_H
#define TOMB_PALETTE_H

#include <stdint.h>

/* INDEX8 palette shared by every texture: 16 colour ramps x 16 brightness
 * steps. The Host selects one of 16 pre-lit rows per pixel. */

enum {
    TOMB_RAMP_GRAY = 0,
    TOMB_RAMP_SAND,
    TOMB_RAMP_OCHRE,
    TOMB_RAMP_BROWN,
    TOMB_RAMP_MOSS,
    TOMB_RAMP_TEAL,
    TOMB_RAMP_BLUE,
    TOMB_RAMP_GOLD,
    TOMB_RAMP_RED,
    TOMB_RAMP_SKIN,
    TOMB_RAMP_HAIR,
    TOMB_RAMP_CLOTH,
    TOMB_RAMP_LEATHER,
    TOMB_RAMP_BONE,
    TOMB_RAMP_WATER,
    TOMB_RAMP_WHITE,
};

#define TOMB_RAMP_STEPS 16u
/* Light levels the renderer quantises to, matching the original lit palette. */
#define TOMB_LIGHT_LEVELS 16u

static inline uint8_t tomb_palette_index(uint8_t ramp, uint32_t step) {
    if (step > TOMB_RAMP_STEPS - 1u) step = TOMB_RAMP_STEPS - 1u;
    return (uint8_t)(ramp * TOMB_RAMP_STEPS + step);
}

uint16_t tomb_rgb565(uint32_t red, uint32_t green, uint32_t blue);
/* Fills `entries` (16 x 256 canonical RGB565), including the original blue
 * tint in the darkest rows. */
void tomb_build_palette(uint16_t entries[TOMB_LIGHT_LEVELS * 256u]);

#endif
