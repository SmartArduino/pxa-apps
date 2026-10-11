#ifndef JUMP3D_PALETTE_H
#define JUMP3D_PALETTE_H

#include <stdint.h>

/* The Host painter pipeline reads colours as palette[light * 256 + index],
 * where `light` is interpolated per vertex and `index` is the record colour.
 * Jump Jump therefore uploads a lit palette with J3_LIGHT_LEVELS rows: row
 * J3_LIGHT_FULL holds the authored colours and lower rows hold the same
 * colours scaled towards black, which is what gives the flat blocks their
 * isometric shading and the cylinders their smooth gradient. */

#define J3_LIGHT_LEVELS UINT32_C(32)
#define J3_LIGHT_FULL UINT32_C(31)
#define J3_PALETTE_ENTRIES (J3_LIGHT_LEVELS * UINT32_C(256))

/* Colour ramps. An index is (hue << 4) | variant. */
enum {
    J3_HUE_NEUTRAL = 0, /* 0 = black .. 15 = white */
    J3_HUE_BLOCK,       /* 16 classic block body colours */
    J3_HUE_SOLID,       /* block trim, accents and result panel colours */
    J3_HUE_CHAR,        /* the little man */
    J3_HUE_UI,          /* interface inks */
    J3_HUE_MUSIC,       /* music box */
    J3_HUE_STORE,       /* convenience store */
    J3_HUE_WELL,        /* manhole */
    J3_HUE_BG,          /* 0..7 gradient tops, 8..15 gradient bottoms */
    J3_HUE_SKY,         /* one index per background scheme: the light row
                         * selects the ramp step, not a brightness scale */
    J3_HUE_COUNT
};

#define J3_INDEX(hue, variant) ((uint8_t)(((uint8_t)(hue) << 4) | (uint8_t)(variant)))

/* Neutral ramp (also used for shadows and text). */
#define J3_BLACK J3_INDEX(J3_HUE_NEUTRAL, 0)
#define J3_GRAY_2 J3_INDEX(J3_HUE_NEUTRAL, 2)
#define J3_GRAY_4 J3_INDEX(J3_HUE_NEUTRAL, 4)
#define J3_GRAY_6 J3_INDEX(J3_HUE_NEUTRAL, 6)
#define J3_GRAY_8 J3_INDEX(J3_HUE_NEUTRAL, 8)
#define J3_GRAY_10 J3_INDEX(J3_HUE_NEUTRAL, 10)
#define J3_GRAY_12 J3_INDEX(J3_HUE_NEUTRAL, 12)
#define J3_WHITE J3_INDEX(J3_HUE_NEUTRAL, 15)

/* Fixed trims. */
#define J3_TRIM_BAND J3_INDEX(J3_HUE_SOLID, 0)
#define J3_SOLID_WHITE J3_INDEX(J3_HUE_SOLID, 1)
#define J3_SOLID_CREAM J3_INDEX(J3_HUE_SOLID, 2)
#define J3_SOLID_GOLD J3_INDEX(J3_HUE_SOLID, 6)
#define J3_SOLID_ORANGE J3_INDEX(J3_HUE_SOLID, 7)
#define J3_SOLID_GREEN J3_INDEX(J3_HUE_SOLID, 8)
#define J3_SOLID_RED J3_INDEX(J3_HUE_SOLID, 9)
#define J3_SOLID_BLUE J3_INDEX(J3_HUE_SOLID, 10)
#define J3_SOLID_BROWN J3_INDEX(J3_HUE_SOLID, 11)
#define J3_SOLID_DARK J3_INDEX(J3_HUE_SOLID, 12)
#define J3_SOLID_CHARCOAL J3_INDEX(J3_HUE_SOLID, 13)
#define J3_SOLID_SLATE J3_INDEX(J3_HUE_SOLID, 14)

/* The little man. */
#define J3_CHAR_BODY J3_INDEX(J3_HUE_CHAR, 0)
#define J3_CHAR_BODY_LIT J3_INDEX(J3_HUE_CHAR, 1)
#define J3_CHAR_HEAD J3_INDEX(J3_HUE_CHAR, 2)
#define J3_CHAR_BODY_DARK J3_INDEX(J3_HUE_CHAR, 3)
#define J3_CHAR_HIGHLIGHT J3_INDEX(J3_HUE_CHAR, 4)

/* Interface inks. */
#define J3_UI_WHITE J3_INDEX(J3_HUE_UI, 0)
#define J3_UI_INK J3_INDEX(J3_HUE_UI, 1)
#define J3_UI_TEXT_DIM J3_INDEX(J3_HUE_UI, 2)
#define J3_UI_ACCENT J3_INDEX(J3_HUE_UI, 3)
#define J3_UI_PANEL J3_INDEX(J3_HUE_UI, 4)
#define J3_UI_SHADOW J3_INDEX(J3_HUE_UI, 5)

/* Special blocks. */
#define J3_MUSIC_BODY J3_INDEX(J3_HUE_MUSIC, 0)
#define J3_MUSIC_BODY_LIT J3_INDEX(J3_HUE_MUSIC, 1)
#define J3_MUSIC_RECORD J3_INDEX(J3_HUE_MUSIC, 2)
#define J3_MUSIC_LIGHT J3_INDEX(J3_HUE_MUSIC, 3)
#define J3_STORE_AWNING J3_INDEX(J3_HUE_STORE, 0)
#define J3_STORE_WALL J3_INDEX(J3_HUE_STORE, 1)
#define J3_STORE_WINDOW J3_INDEX(J3_HUE_STORE, 2)
#define J3_STORE_DARK J3_INDEX(J3_HUE_STORE, 3)
#define J3_WELL_LIGHT J3_INDEX(J3_HUE_WELL, 0)
#define J3_WELL_MID J3_INDEX(J3_HUE_WELL, 1)
#define J3_WELL_DARK J3_INDEX(J3_HUE_WELL, 2)
#define J3_WELL_EDGE J3_INDEX(J3_HUE_WELL, 3)

/* Number of distinct block body colours generated per run. */
#define J3_BLOCK_COLORS 16u

/* Background schemes: 8 gradients, each with a top and a horizon colour. The
 * sky is drawn through the painter's light axis (one palette index per scheme,
 * see j3_palette_build), which keeps the full-screen gradient on the span fill
 * path instead of the general triangle rasteriser. */
#define J3_BG_SCHEMES 8u
#define J3_SKY_INDEX(scheme) \
    J3_INDEX(J3_HUE_SKY, (uint8_t)((scheme) % J3_BG_SCHEMES))

/* Antialiased text ramps: 32 pre-blended coverage colours per (ink, background)
 * pair, written into the palette's full-light row. The font atlas stores 8-bit
 * coverage and PXA_RASTER_SPRITE_PALETTE_RAMP indexes one of these blocks, so
 * panel text antialiases for the cost of the palette lookup the sprite path
 * already does. They live above the sky block (indices 144..159). */
#define J3_RAMP_LEVELS 32u
#define J3_RAMP_INK_PANEL UINT8_C(160)
#define J3_RAMP_DIM_PANEL UINT8_C(192)
/* White ink over the current background scheme's sky. Rebuilt and re-uploaded
 * whenever the background changes, so text over the sky antialiases with the
 * same zero-arithmetic palette lookup as the panel text. */
#define J3_RAMP_SKY UINT8_C(224)

/* Builds the J3_LIGHT_LEVELS x 256 RGB565 palette. `entries` must hold
 * J3_PALETTE_ENTRIES values. */
void j3_palette_build(uint16_t *entries);

/* Rewrites the sky text ramp (J3_RAMP_SKY .. +J3_RAMP_LEVELS-1) for `scheme`,
 * blending white ink with that scheme's sky. The caller re-uploads the palette
 * so text over the sky antialiases without per-pixel arithmetic. */
void j3_palette_build_sky_ramp(uint16_t *entries, uint8_t scheme);

/* Canonical RGB565 helper shared by the palette and the flat background. */
static inline uint16_t j3_rgb565(uint32_t red, uint32_t green, uint32_t blue) {
    return (uint16_t)(((red & 0xF8u) << 8) | ((green & 0xFCu) << 3) |
                      ((blue & 0xF8u) >> 3));
}

/* Authored colour of a palette index at full light, as RGB565. */
uint16_t j3_color_rgb565(uint8_t index);

/* The same colour with a palette light row applied, as RGB565. Flat quads carry
 * their colour directly, and the Host fills them with a plain store loop
 * instead of the per-pixel palette lookup the painter path needs. */
uint16_t j3_color_lit_rgb565(uint8_t index, uint32_t light_level);

#endif
