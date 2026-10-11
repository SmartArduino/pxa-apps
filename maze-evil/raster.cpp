#include "raster.h"

#define FIXED_SHIFT 16

typedef struct {
    int x0;
    int y0;
    int x1; /* exclusive */
    int y1; /* exclusive */
    int skip_x;
    int skip_y;
} clip_t;

static int clip_rect(const target_t *target, int x, int y, int width,
                     int height, clip_t *out) {
    const int64_t right = (int64_t)x + width;
    const int64_t bottom = (int64_t)y + height;
    if (width == 0 || height == 0 || right <= 0 || bottom <= 0 ||
        x >= target->width || y >= target->height) {
        return 0;
    }
    out->skip_x = x < 0 ? -x : 0;
    out->skip_y = y < 0 ? -y : 0;
    out->x0 = x < 0 ? 0 : x;
    out->y0 = y < 0 ? 0 : y;
    out->x1 = right > target->width ? target->width : (int)right;
    out->y1 = bottom > target->height ? target->height : (int)bottom;
    return out->x1 > out->x0 && out->y1 > out->y0;
}

static uint16_t blend565(uint16_t dst, uint16_t color, uint32_t alpha) {
    const uint32_t inverse = 256u - alpha;
    const uint32_t r = ((dst >> 11) * inverse + (color >> 11) * alpha) >> 8;
    const uint32_t g =
        (((dst >> 5) & 0x3Fu) * inverse + ((color >> 5) & 0x3Fu) * alpha) >> 8;
    const uint32_t b = ((dst & 0x1Fu) * inverse + (color & 0x1Fu) * alpha) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void raster_column(const target_t *target, const texture_t *texture,
                   const uint16_t *lit, int x, int y0, int y1, int u,
                   int32_t v_start, int32_t v_step, int transparent) {
    const uint8_t *texels = texture->pixels + (uint32_t)u * texture->size;
    const uint32_t mask = (uint32_t)texture->size - 1u;
    const uint32_t step = (uint32_t)v_step;
    const int count = y1 - y0 + 1;
    uint16_t *row =
        target->pixels + (uint32_t)y0 * (uint32_t)target->width + (uint32_t)x;
    const uint32_t pitch = (uint32_t)target->width;
    uint32_t v = (uint32_t)v_start;
    int index = 0;
    if (transparent) {
        for (; index < count; ++index) {
            const uint8_t texel = texels[(v >> FIXED_SHIFT) & mask];
            if (texel != 0u) {
                *row = lit[texel];
            }
            row += pitch;
            v += step;
        }
        return;
    }
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
    uint16_t *floor = target->pixels +
                      (uint32_t)y_floor * (uint32_t)target->width + (uint32_t)x0;
    uint16_t *ceiling = target->pixels +
                        (uint32_t)y_ceiling * (uint32_t)target->width +
                        (uint32_t)x0;
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

void raster_sprite(const target_t *target, const sprite_t *sprite,
                   const uint16_t *lit, int x, int y, int width, int height,
                   int source_x, int source_y, int source_width,
                   int source_height, int transparent) {
    clip_t clip;
    const uint32_t stride = sprite->padded_height;
    const uint32_t u_step = ((uint32_t)source_width << FIXED_SHIFT) /
                            (uint32_t)width;
    const uint32_t v_step = ((uint32_t)source_height << FIXED_SHIFT) /
                            (uint32_t)height;
    uint32_t v;
    uint32_t u_start;
    int row;
    if (width <= 0 || height <= 0 || !clip_rect(target, x, y, width, height, &clip)) {
        return;
    }
    v = (uint32_t)clip.skip_y * v_step + (v_step >> 1u);
    u_start = (uint32_t)clip.skip_x * u_step + (u_step >> 1u);
    for (row = clip.y0; row < clip.y1; ++row, v += v_step) {
        uint16_t *out = target->pixels + (uint32_t)row * (uint32_t)target->width +
                        (uint32_t)clip.x0;
        uint32_t u = u_start;
        int column;
        for (column = clip.x0; column < clip.x1; ++column, u += u_step) {
            const uint8_t texel =
                sprite->pixels[((uint32_t)source_x + (u >> FIXED_SHIFT)) * stride +
                               ((uint32_t)source_y + (v >> FIXED_SHIFT))];
            if (transparent && texel == 0u) {
                continue;
            }
            out[column - clip.x0] = lit[texel];
        }
    }
}

void raster_solid_sprite(const target_t *target, const sprite_t *sprite,
                         int x, int y, int width, int height, int source_x,
                         int source_y, int source_width, int source_height,
                         uint16_t color, int transparent) {
    clip_t clip;
    const uint32_t stride = sprite->padded_height;
    const uint32_t u_step = ((uint32_t)source_width << FIXED_SHIFT) /
                            (uint32_t)width;
    const uint32_t v_step = ((uint32_t)source_height << FIXED_SHIFT) /
                            (uint32_t)height;
    uint32_t v;
    uint32_t u_start;
    int row;
    if (width <= 0 || height <= 0 || !clip_rect(target, x, y, width, height, &clip)) {
        return;
    }
    v = (uint32_t)clip.skip_y * v_step + (v_step >> 1u);
    u_start = (uint32_t)clip.skip_x * u_step + (u_step >> 1u);
    for (row = clip.y0; row < clip.y1; ++row, v += v_step) {
        uint16_t *out = target->pixels + (uint32_t)row * (uint32_t)target->width +
                        (uint32_t)clip.x0;
        uint32_t u = u_start;
        int column;
        for (column = clip.x0; column < clip.x1; ++column, u += u_step) {
            const uint8_t texel =
                sprite->pixels[((uint32_t)source_x + (u >> FIXED_SHIFT)) * stride +
                               ((uint32_t)source_y + (v >> FIXED_SHIFT))];
            if (transparent && texel == 0u) {
                continue;
            }
            out[column - clip.x0] = color;
        }
    }
}

void raster_rect(const target_t *target, int x, int y, int width, int height,
                 uint16_t color) {
    clip_t clip;
    int row;
    if (!clip_rect(target, x, y, width, height, &clip)) {
        return;
    }
    for (row = clip.y0; row < clip.y1; ++row) {
        uint16_t *out = target->pixels + (uint32_t)row * (uint32_t)target->width +
                        (uint32_t)clip.x0;
        int column;
        for (column = clip.x0; column < clip.x1; ++column) {
            *out++ = color;
        }
    }
}

void raster_rect_alpha(const target_t *target, int x, int y, int width,
                       int height, uint16_t color, int alpha) {
    clip_t clip;
    int row;
    if (alpha <= 0 || !clip_rect(target, x, y, width, height, &clip)) {
        return;
    }
    if (alpha >= 255) {
        raster_rect(target, x, y, width, height, color);
        return;
    }
    for (row = clip.y0; row < clip.y1; ++row) {
        uint16_t *out = target->pixels + (uint32_t)row * (uint32_t)target->width +
                        (uint32_t)clip.x0;
        int column;
        for (column = clip.x0; column < clip.x1; ++column) {
            *out = blend565(*out, color, (uint32_t)alpha + 1u);
            ++out;
        }
    }
}
