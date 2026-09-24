#ifndef MAZE_EVIL_RASTER_H
#define MAZE_EVIL_RASTER_H

#include <stdint.h>

/* INDEX8 texture. Wall textures are column-major (pixels[u * size + v]); floor
 * and ceiling textures are row-major (pixels[v * size + u]). `size` is a power
 * of two and `log2_size` its base-2 logarithm. */
typedef struct {
    const uint8_t *pixels;
    uint16_t size;
    uint8_t log2_size;
} texture_t;

/* Column-major INDEX8 sprite padded to a power of two on both axes. `stride`
 * is the padded height; sampling covers [0, width) x [0, height). Index 0 is
 * the transparency key. */
typedef struct {
    const uint8_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t padded_width;
    uint16_t padded_height;
} sprite_t;

typedef struct {
    uint16_t *pixels; /* canonical RGB565, native endian */
    int width;
    int height;
} target_t;

/* Ports of micropixel's Host raster kernels (raster_kernels.cpp) writing to a
 * Guest-owned RGB565 buffer. */
void raster_column(const target_t *target, const texture_t *texture,
                   const uint16_t *lit, int x, int y0, int y1, int u,
                   int32_t v_start, int32_t v_step, int transparent);

void raster_span_pair(const target_t *target, const texture_t *floor_tex,
                      const texture_t *ceiling_tex, const uint16_t *lit,
                      int y_floor, int y_ceiling, int x0, int x1, int32_t s,
                      int32_t t, int32_t ds, int32_t dt);

void raster_sprite(const target_t *target, const sprite_t *sprite,
                   const uint16_t *lit, int x, int y, int width, int height,
                   int source_x, int source_y, int source_width,
                   int source_height, int transparent);

void raster_solid_sprite(const target_t *target, const sprite_t *sprite,
                         int x, int y, int width, int height, int source_x,
                         int source_y, int source_width, int source_height,
                         uint16_t color, int transparent);

void raster_rect(const target_t *target, int x, int y, int width, int height,
                 uint16_t color);

/* Blends `color` over the target with alpha 1..255 (255 = opaque). */
void raster_rect_alpha(const target_t *target, int x, int y, int width,
                       int height, uint16_t color, int alpha);

#endif
