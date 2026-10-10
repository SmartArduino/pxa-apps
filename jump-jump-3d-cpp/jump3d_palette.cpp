#include "jump3d_palette.hpp"

#include <stddef.h>
#include <algorithm>

namespace jump {

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} j3_rgb_t;

/* Classic Jump Jump block bodies (weapp-jump COLORS) plus a few extra pastels
 * so long runs keep introducing new colours. */
static const j3_rgb_t k_block_colors[J3_BLOCK_COLORS] = {
    {0x2C, 0x9F, 0x67}, /* green      */
    {0xF3, 0x9A, 0xB7}, /* pink       */
    {0x00, 0x9F, 0xF7}, /* blue       */
    {0xFF, 0xBE, 0x00}, /* yellow     */
    {0xF7, 0xAA, 0x6C}, /* orange     */
    {0x8A, 0x9A, 0xD6}, /* purple     */
    {0x93, 0xE4, 0xCE}, /* cyan       */
    {0xCC, 0x46, 0x3D}, /* red        */
    {0xD8, 0xD0, 0xD1}, /* warm white */
    {0xF5, 0xF5, 0xF5}, /* cream      */
    {0x59, 0x33, 0x2E}, /* brown      */
    {0xD1, 0xEE, 0xEE}, /* light blue */
    {0x77, 0xD2, 0xEE}, /* sky        */
    {0xA6, 0x79, 0xEE}, /* violet     */
    {0xFF, 0xCF, 0x8B}, /* sand       */
    {0xFF, 0x8C, 0x00}, /* deep orange*/
};

static const j3_rgb_t k_solid_colors[16] = {
    {0xD8, 0xD0, 0xD1}, /* 0  block trim band */
    {0xFF, 0xFF, 0xFF}, /* 1  white */
    {0xF5, 0xF5, 0xF5}, /* 2  cream */
    {0xE2, 0xE2, 0xE8}, /* 3  panel light */
    {0xC8, 0xC8, 0xD0}, /* 4  panel mid */
    {0xA8, 0xA8, 0xB4}, /* 5  panel dark */
    {0xFF, 0xD7, 0x6A}, /* 6  gold */
    {0xE5, 0x6A, 0x3B}, /* 7  orange */
    {0x2C, 0x9F, 0x67}, /* 8  green */
    {0xCC, 0x46, 0x3D}, /* 9  red */
    {0x00, 0x9F, 0xF7}, /* 10 blue */
    {0x59, 0x33, 0x2E}, /* 11 brown */
    {0x40, 0x40, 0x48}, /* 12 dark */
    {0x20, 0x20, 0x28}, /* 13 charcoal */
    {0x6A, 0x6A, 0x74}, /* 14 slate */
    {0xF0, 0xF0, 0xF5}, /* 15 near white */
};

static const j3_rgb_t k_char_colors[16] = {
    {0x3B, 0x3B, 0x52}, /* 0 body */
    {0x4A, 0x4A, 0x68}, /* 1 body lit */
    {0x43, 0x43, 0x5E}, /* 2 head */
    {0x2F, 0x2F, 0x45}, /* 3 body dark */
    {0x6A, 0x6A, 0x8E}, /* 4 highlight */
    {0x35, 0x35, 0x4C}, {0x50, 0x50, 0x70}, {0x5C, 0x5C, 0x80},
    {0x28, 0x28, 0x3C}, {0x62, 0x62, 0x86}, {0x3B, 0x3B, 0x52},
    {0x3B, 0x3B, 0x52}, {0x3B, 0x3B, 0x52}, {0x3B, 0x3B, 0x52},
    {0x3B, 0x3B, 0x52}, {0x3B, 0x3B, 0x52},
};

static const j3_rgb_t k_ui_colors[16] = {
    {0xFF, 0xFF, 0xFF}, /* 0 white ink */
    {0x23, 0x41, 0x5A}, /* 1 panel ink */
    {0x65, 0x84, 0x9A}, /* 2 dim ink */
    {0xE5, 0x6A, 0x3B}, /* 3 accent */
    {0xFF, 0xFF, 0xFF}, /* 4 panel face */
    {0x8C, 0x9A, 0xA8}, /* 5 soft shadow */
    {0xF2, 0xF2, 0xF6}, {0xD0, 0xD0, 0xD8}, {0xB0, 0xB0, 0xBC},
    {0x90, 0x90, 0x9C}, {0x70, 0x70, 0x7C}, {0x50, 0x50, 0x5C},
    {0x30, 0x30, 0x3C}, {0x10, 0x10, 0x18}, {0xE0, 0xE0, 0xE8},
    {0xC0, 0xC0, 0xCC},
};

static const j3_rgb_t k_music_colors[16] = {
    {0x26, 0x26, 0x26}, /* 0 body */
    {0x3A, 0x3A, 0x3A}, /* 1 body lit */
    {0x14, 0x14, 0x14}, /* 2 record grooves */
    {0xC8, 0xC8, 0xC8}, /* 3 light trim */
    {0xF0, 0xF0, 0xF0}, /* 4 white trim */
    {0x6A, 0x6A, 0x6A}, /* 5 mid */
    {0x26, 0x26, 0x26}, {0x26, 0x26, 0x26}, {0x26, 0x26, 0x26},
    {0x26, 0x26, 0x26}, {0x26, 0x26, 0x26}, {0x26, 0x26, 0x26},
    {0x26, 0x26, 0x26}, {0x26, 0x26, 0x26}, {0x26, 0x26, 0x26},
    {0x26, 0x26, 0x26},
};

static const j3_rgb_t k_store_colors[16] = {
    {0xD9, 0x43, 0x3F}, /* 0 awning red */
    {0xFF, 0xFF, 0xFF}, /* 1 wall white */
    {0x6A, 0x9A, 0xB5}, /* 2 window glass */
    {0x33, 0x33, 0x33}, /* 3 dark frame */
    {0xF0, 0xD0, 0xC0}, /* 4 warm trim */
    {0xE8, 0xE8, 0xE8}, /* 5 light grey */
};

static const j3_rgb_t k_well_colors[16] = {
    {0x6E, 0x6E, 0x6E}, /* 0 light */
    {0x4A, 0x4A, 0x4A}, /* 1 mid */
    {0x2A, 0x2A, 0x2A}, /* 2 dark */
    {0x5C, 0x5C, 0x5C}, /* 3 edge */
};

/* The eight original background gradients: variants 0..7 are the top colour,
 * 8..15 the bottom colour. */
static const j3_rgb_t k_bg_colors[16] = {
    {0xD7, 0xDB, 0xE6}, {0xFF, 0xE7, 0xDC}, {0xFF, 0xE0, 0xA3},
    {0xFF, 0xF8, 0xB9}, {0xDA, 0xF4, 0xFF}, {0xDB, 0xEB, 0xFF},
    {0xD8, 0xDA, 0xFF}, {0xCF, 0xCF, 0xCF}, {0xBC, 0xBE, 0xC7},
    {0xFF, 0xC4, 0xCC}, {0xFF, 0xCA, 0x7E}, {0xFF, 0xF5, 0x8B},
    {0xCF, 0xE9, 0xD2}, {0xB9, 0xD5, 0xEB}, {0xA5, 0xB0, 0xE8},
    {0xC7, 0xC4, 0xC9},
};

static j3_rgb_t base_color(uint8_t index) {
    const uint8_t hue = (uint8_t)(index >> 4);
    const uint8_t variant = (uint8_t)(index & 0x0Fu);
    j3_rgb_t color = {0x80, 0x80, 0x80};
    switch (hue) {
    case J3_HUE_NEUTRAL: {
        /* 0 = black, 15 = white. */
        const uint8_t level = (uint8_t)(variant * 17u);
        color.red = level;
        color.green = level;
        color.blue = level;
        break;
    }
    case J3_HUE_BLOCK:
        color = k_block_colors[variant];
        break;
    case J3_HUE_SOLID:
        color = k_solid_colors[variant];
        break;
    case J3_HUE_CHAR:
        color = k_char_colors[variant];
        break;
    case J3_HUE_UI:
        color = k_ui_colors[variant];
        break;
    case J3_HUE_MUSIC:
        color = k_music_colors[variant];
        break;
    case J3_HUE_STORE:
        color = k_store_colors[variant];
        break;
    case J3_HUE_WELL:
        color = k_well_colors[variant];
        break;
    case J3_HUE_BG:
        color = k_bg_colors[variant];
        break;
    default: {
        uint8_t level = (uint8_t)(variant * 17u);
        color.red = level;
        color.green = level;
        color.blue = level;
        break;
    }
    }
    return color;
}

uint16_t j3_color_rgb565(uint8_t index) {
    const j3_rgb_t color = base_color(index);
    return j3_rgb565(color.red, color.green, color.blue);
}

uint16_t j3_color_lit_rgb565(uint8_t index, uint32_t light_level) {
    const j3_rgb_t color = base_color(index);
    float linear;
    float factor;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    if (light_level >= J3_LIGHT_LEVELS) light_level = J3_LIGHT_LEVELS - 1u;
    linear = (float)light_level / (float)(J3_LIGHT_LEVELS - 1u);
    factor = linear * (0.35F + 0.65F * linear);
    red = (uint32_t)((float)color.red * factor + 0.5F);
    green = (uint32_t)((float)color.green * factor + 0.5F);
    blue = (uint32_t)((float)color.blue * factor + 0.5F);
    if (red > 255u) red = 255u;
    if (green > 255u) green = 255u;
    if (blue > 255u) blue = 255u;
    return j3_rgb565(red, green, blue);
}

void j3_palette_build(uint16_t *entries) {
    uint32_t row;
    uint32_t index;
    for (row = 0; row < J3_LIGHT_LEVELS; ++row) {
        /* Small gamma on the light ramp: shadows keep a little more punch than
         * a straight multiply would give. */
        const float linear = (float)row / (float)(J3_LIGHT_LEVELS - 1u);
        const float factor = linear * (0.35F + 0.65F * linear);
        for (index = 0; index < 256u; ++index) {
            const j3_rgb_t color = base_color((uint8_t)index);
            uint32_t red = (uint32_t)((float)color.red * factor + 0.5F);
            uint32_t green = (uint32_t)((float)color.green * factor + 0.5F);
            uint32_t blue = (uint32_t)((float)color.blue * factor + 0.5F);
            if (red > 255u) red = 255u;
            if (green > 255u) green = 255u;
            if (blue > 255u) blue = 255u;
            entries[row * 256u + index] =
                j3_rgb565(red, green, blue);
        }
    }
    /* The sky writes its gradient into the light rows of one index per scheme:
     * row 0 is the top colour and J3_LIGHT_FULL the horizon, so a painter run
     * with a per-band light reproduces the ramp with a plain store per pixel. */
    for (index = 0; index < J3_BG_SCHEMES; ++index) {
        const j3_rgb_t top = k_bg_colors[index];
        const j3_rgb_t bottom = k_bg_colors[index + 8u];
        for (row = 0; row < J3_LIGHT_LEVELS; ++row) {
            const float t = (float)row / (float)(J3_LIGHT_LEVELS - 1u);
            const uint32_t red = (uint32_t)((float)top.red +
                                            ((float)bottom.red - (float)top.red) * t +
                                            0.5F);
            const uint32_t green = (uint32_t)((float)top.green +
                                              ((float)bottom.green - (float)top.green) * t +
                                              0.5F);
            const uint32_t blue = (uint32_t)((float)top.blue +
                                             ((float)bottom.blue - (float)top.blue) * t +
                                             0.5F);
            entries[row * 256u + J3_SKY_INDEX((uint8_t)index)] =
                j3_rgb565(red, green, blue);
        }
    }
    /* Antialiased text ramps: each block holds its ink blended with the panel
     * face at 32 coverage levels in the full-light row, which is what
     * arcade::sprite_palette_ramp looks up. */
    {
        static const uint8_t ramp_bases[2] = {J3_RAMP_INK_PANEL,
                                              J3_RAMP_DIM_PANEL};
        static const uint8_t ramp_inks[2] = {J3_UI_INK, J3_UI_TEXT_DIM};
        const j3_rgb_t background = base_color(J3_UI_PANEL);
        uint32_t ramp;
        for (ramp = 0; ramp < 2u; ++ramp) {
            const j3_rgb_t ink = base_color(ramp_inks[ramp]);
            for (row = 0; row < J3_RAMP_LEVELS; ++row) {
                const float t = (float)row / (float)(J3_RAMP_LEVELS - 1u);
                const uint32_t red = (uint32_t)((float)background.red +
                                                ((float)ink.red -
                                                 (float)background.red) * t +
                                                0.5F);
                const uint32_t green = (uint32_t)((float)background.green +
                                                  ((float)ink.green -
                                                   (float)background.green) * t +
                                                  0.5F);
                const uint32_t blue = (uint32_t)((float)background.blue +
                                                 ((float)ink.blue -
                                                  (float)background.blue) * t +
                                                 0.5F);
                entries[J3_LIGHT_FULL * 256u + ramp_bases[ramp] + row] =
                    j3_rgb565(red, green, blue);
            }
        }
    }
    j3_palette_build_sky_ramp(entries, 0u);
}

void j3_palette_build_sky_ramp(uint16_t *entries, uint8_t scheme, uint8_t* texels) {
    const auto top=k_bg_colors[scheme%J3_BG_SCHEMES];
    const auto bottom=k_bg_colors[scheme%J3_BG_SCHEMES+8];
    static constexpr uint8_t bayer[8][8]={
        {0,32,8,40,2,34,10,42},{48,16,56,24,50,18,58,26},
        {12,44,4,36,14,46,6,38},{60,28,52,20,62,30,54,22},
        {3,35,11,43,1,33,9,41},{51,19,59,27,49,17,57,25},
        {15,47,7,39,13,45,5,37},{63,31,55,23,61,29,53,21}};
    unsigned colors=0;
    // Quantize AFTER spatial dithering. Dithering already quantized ramp
    // indices only hides a small part of each RGB565 transition.
    for(unsigned y=0;y<256;++y)for(unsigned x=0;x<8;++x){
        const unsigned threshold=bayer[y&7][x];
        auto quantize=[&](unsigned a,unsigned b,unsigned levels){
            const unsigned numerator=(a*(255-y)+b*y)*levels;
            return std::min(levels,(numerator*64+threshold*65025)/(65025*64));
        };
        const uint16_t color=uint16_t((quantize(top.red,bottom.red,31)<<11)|
            (quantize(top.green,bottom.green,63)<<5)|quantize(top.blue,bottom.blue,31));
        unsigned index=0;while(index<colors&&entries[160+index]!=color)++index;
        // Authored schemes use at most 33 colors; the reserved capacity is 96.
        if(index==colors&&colors<96)entries[160+colors++]=color;
        if(texels)texels[y*8+x]=uint8_t(160+std::min(index,95u));
    }
    for(unsigned row=1;row<J3_LIGHT_LEVELS;++row)
        for(unsigned i=0;i<colors;++i)entries[row*256+160+i]=entries[160+i];
}

} // namespace jump
