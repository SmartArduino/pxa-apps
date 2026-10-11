#include "raster.h"

#define FIXED_SHIFT 16

void raster_column(const target_t *target, const texture_t *texture,
                   const uint16_t *lit, int x, int y0, int y1, int u,
                   int32_t v_start, int32_t v_step) {
    const uint8_t *texels = texture->pixels + (uint32_t)u * texture->size;
    const uint32_t mask = (uint32_t)texture->size - 1u;
    const uint32_t step = (uint32_t)v_step;
    const int count = y1 - y0 + 1;
    uint16_t *row = target->pixels + (uint32_t)y0 * (uint32_t)target->width + (uint32_t)x;
    const uint32_t pitch = (uint32_t)target->width;
    uint32_t v = (uint32_t)v_start;
    int index = 0;
    /* Four independent texel chains per iteration, matching the Host kernel. */
    for (; index + 4 <= count; index += 4) {
        const uint8_t t0 = texels[(v >> FIXED_SHIFT) & mask];
        const uint8_t t1 = texels[((v + step) >> FIXED_SHIFT) & mask];
        const uint8_t t2 = texels[((v + 2u * step) >> FIXED_SHIFT) & mask];
        const uint8_t t3 = texels[((v + 3u * step) >> FIXED_SHIFT) & mask];
        row[0] = lit[t0];
        row[pitch] = lit[t1];
        row[2u * pitch] = lit[t2];
        row[3u * pitch] = lit[t3];
        row += 4u * pitch;
        v += 4u * step;
    }
    for (; index < count; ++index) {
        *row = lit[texels[(v >> FIXED_SHIFT) & mask]];
        row += pitch;
        v += step;
    }
}

void raster_span_pair(const target_t *target, const texture_t *floor_tex,
                      const texture_t *ceiling_tex, const uint16_t *lit,
                      int y_floor, int y_ceiling, int x0, int x1, int32_t s,
                      int32_t t, int32_t ds, int32_t dt) {
    uint16_t *floor = target->pixels + (uint32_t)y_floor * (uint32_t)target->width + (uint32_t)x0;
    uint16_t *ceiling = target->pixels + (uint32_t)y_ceiling * (uint32_t)target->width + (uint32_t)x0;
    const uint32_t count = (uint32_t)(x1 - x0 + 1);
    const uint32_t shift_s = FIXED_SHIFT - floor_tex->log2_size;
    const uint32_t shift_t = FIXED_SHIFT - floor_tex->log2_size;
    const uint32_t mask = (uint32_t)floor_tex->size - 1u;
    const uint32_t log2_size = floor_tex->log2_size;
    const uint8_t *floor_texels = floor_tex->pixels;
    const uint8_t *ceiling_texels = ceiling_tex->pixels;
    uint32_t us = (uint32_t)s;
    uint32_t ut = (uint32_t)t;
    const uint32_t uds = (uint32_t)ds;
    const uint32_t udt = (uint32_t)dt;
    uint32_t index;
    for (index = 0; index < count; ++index) {
        const uint32_t texel =
            (((ut >> shift_t) & mask) << log2_size) | ((us >> shift_s) & mask);
        floor[index] = lit[floor_texels[texel]];
        ceiling[index] = lit[ceiling_texels[texel]];
        us += uds;
        ut += udt;
    }
}

void raster_rect(const target_t *target, int x, int y, int width, int height,
                 uint16_t color) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width;
    int y1 = y + height;
    int row;
    if (x1 > target->width) {
        x1 = target->width;
    }
    if (y1 > target->height) {
        y1 = target->height;
    }
    for (row = y0; row < y1; ++row) {
        uint16_t *out = target->pixels + (uint32_t)row * (uint32_t)target->width + (uint32_t)x0;
        int column;
        for (column = x0; column < x1; ++column) {
            *out++ = color;
        }
    }
}

typedef struct {
    char ch;
    uint8_t rows[5];
} glyph_t;

static const glyph_t kGlyphs[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}},
    {'2', {7, 1, 7, 4, 7}}, {'3', {7, 1, 7, 1, 7}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}},
    {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}},
    {'8', {7, 5, 7, 5, 7}}, {'9', {7, 5, 7, 1, 7}},
    {'.', {0, 0, 0, 0, 2}}, {':', {0, 2, 0, 2, 0}},
    {'-', {0, 0, 7, 0, 0}}, {'/', {1, 1, 2, 4, 4}},
    {'A', {7, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
    {'C', {7, 4, 4, 4, 7}}, {'D', {6, 5, 5, 5, 6}},
    {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}},
    {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}},
    {'M', {5, 7, 7, 5, 5}}, {'N', {5, 7, 7, 7, 5}},
    {'P', {7, 5, 7, 4, 4}}, {'Q', {7, 5, 5, 7, 1}},
    {'R', {6, 5, 6, 5, 5}}, {'S', {7, 4, 7, 1, 7}},
    {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}},
    {'V', {5, 5, 5, 5, 2}}, {'W', {5, 5, 7, 7, 5}},
    {'X', {5, 5, 2, 5, 5}}, {'Y', {5, 5, 2, 2, 2}},
    {'Z', {7, 1, 2, 4, 7}}, {' ', {0, 0, 0, 0, 0}},
};

static const uint8_t *glyph_for(char ch) {
    unsigned index;
    for (index = 0; index < sizeof(kGlyphs) / sizeof(kGlyphs[0]); ++index) {
        if (kGlyphs[index].ch == ch) {
            return kGlyphs[index].rows;
        }
    }
    return kGlyphs[sizeof(kGlyphs) / sizeof(kGlyphs[0]) - 1].rows; /* space */
}

void raster_text(const target_t *target, int x, int y, const char *text,
                 uint16_t color, int scale) {
    int cursor = x;
    for (; *text != '\0'; ++text) {
        const uint8_t *rows = glyph_for(*text);
        int row;
        for (row = 0; row < 5; ++row) {
            int column;
            for (column = 0; column < 3; ++column) {
                if ((rows[row] & (4u >> column)) == 0u) {
                    continue;
                }
                if (scale == 1) {
                    const int px = cursor + column;
                    const int py = y + row;
                    if (px >= 0 && px < target->width && py >= 0 && py < target->height) {
                        target->pixels[py * target->width + px] = color;
                    }
                } else {
                    raster_rect(target, cursor + column * scale, y + row * scale,
                                scale, scale, color);
                }
            }
        }
        cursor += 4 * scale;
    }
}
