#include "palette.h"

typedef struct {
    uint8_t r, g, b;
} rgb_t;

/* Full-brightness colour of each ramp; level 15 hits this, lower levels fade
 * towards black with a slight gamma so mid-levels stay readable. */
static const rgb_t kRampBright[16] = {
    {205, 205, 205}, /* gray */
    {200, 182, 150}, /* warm gray / stone */
    {178, 112, 58},  /* brown */
    {235, 44, 32},   /* red */
    {245, 140, 40},  /* orange */
    {245, 222, 64},  /* yellow */
    {76, 205, 76},   /* green */
    {66, 186, 176},  /* teal */
    {80, 116, 235},  /* blue */
    {156, 76, 205},  /* purple */
    {238, 176, 146}, /* skin */
    {74, 118, 56},   /* moss */
    {132, 150, 178}, /* steel */
    {214, 176, 66},  /* gold */
    {96, 232, 242},  /* cyan */
    {255, 255, 255}, /* white */
};

/* ((level + 1) / 16) ^ 1.25 for level 0..15; precomputed because the Guest
 * links no libm and the table is only 16 entries. */
static const float kLevelGamma[16] = {
    0.031250F, 0.074325F, 0.123382F, 0.176777F, 0.233648F, 0.293453F,
    0.355814F, 0.420448F, 0.487139F, 0.555712F, 0.626024F, 0.697954F,
    0.771399F, 0.846272F, 0.922495F, 1.000000F,
};

static rgb_t gPalette[256];
static uint16_t gColormap[LIGHT_LEVELS][256];

/* Green is quantised to 5 bits like red and blue; see maze-evil palette.cpp. */
static uint16_t pack565(int r, int g, int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xF8) << 3) | (b >> 3));
}

void palette_build(void) {
    int ramp;
    int level;
    for (ramp = 0; ramp < 16; ++ramp) {
        for (level = 0; level < 16; ++level) {
            const float t = kLevelGamma[level];
            const rgb_t bright = kRampBright[ramp];
            gPalette[ramp * 16 + level].r = (uint8_t)(bright.r * t + 0.5F);
            gPalette[ramp * 16 + level].g = (uint8_t)(bright.g * t + 0.5F);
            gPalette[ramp * 16 + level].b = (uint8_t)(bright.b * t + 0.5F);
        }
    }
    for (level = 0; level < LIGHT_LEVELS; ++level) {
        const float scale = (float)(level + 1) / (float)LIGHT_LEVELS;
        int index;
        for (index = 0; index < 256; ++index) {
            const rgb_t c = gPalette[index];
            gColormap[level][index] =
                pack565((int)(c.r * scale), (int)(c.g * scale), (int)(c.b * scale));
        }
    }
}

const uint16_t *palette_light(int light) {
    if (light < 0) {
        light = 0;
    } else if (light >= LIGHT_LEVELS) {
        light = LIGHT_LEVELS - 1;
    }
    return gColormap[light];
}

uint16_t palette_color(uint8_t index) {
    return gColormap[LIGHT_LEVELS - 1][index];
}
