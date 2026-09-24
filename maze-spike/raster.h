#ifndef MAZE_SPIKE_RASTER_H
#define MAZE_SPIKE_RASTER_H

#include <stdint.h>

/* INDEX8 texture. Wall textures are column-major (pixels[u * size + v]); floor
 * and ceiling textures are row-major (pixels[v * size + u]). `size` is a power
 * of two and `log2_size` its base-2 logarithm. */
typedef struct {
    const uint8_t *pixels;
    uint16_t size;
    uint8_t log2_size;
} texture_t;

typedef struct {
    uint16_t *pixels; /* canonical RGB565, native endian */
    int width;
    int height;
} target_t;

/* Ports of micropixel's Host raster kernels (raster_kernels.cpp). The ray caster
 * already clamps every run to the target, so no clipping happens here. */
void raster_column(const target_t *target, const texture_t *texture,
                   const uint16_t *lit, int x, int y0, int y1, int u,
                   int32_t v_start, int32_t v_step);

void raster_span_pair(const target_t *target, const texture_t *floor_tex,
                      const texture_t *ceiling_tex, const uint16_t *lit,
                      int y_floor, int y_ceiling, int x0, int x1, int32_t s,
                      int32_t t, int32_t ds, int32_t dt);

void raster_rect(const target_t *target, int x, int y, int width, int height,
                 uint16_t color);

/* 3x5 pixel debug text, scale 1..4. Used for the on-screen stats overlay. */
void raster_text(const target_t *target, int x, int y, const char *text,
                 uint16_t color, int scale);

#endif
