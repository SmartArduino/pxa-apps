#ifndef MAZE_SPIKE_RAYCAST_H
#define MAZE_SPIKE_RAYCAST_H

#include <stdint.h>

#include "raster.h"

/* The product Surface caps at 320x240, so the spike sizes its column scratch
 * for 320 columns. */
#define RAY_MAX_COLUMNS 320

typedef struct {
    float x;
    float y;
    float dir_x;
    float dir_y;
    float plane_x;
    float plane_y;
} ray_camera_t;

/* Builds the distance light table (maze-evil's 2.6 / 1.05 curve). */
void raycast_init_light(void);

/* Casts the map, paints floor/ceiling and walls into `target`. Map cells are
 * -1 for empty and >= 0 for a texture index in `textures`; floor and ceiling
 * use TEX_FLOOR and TEX_CEILING. Light levels are resolved through the lit
 * palette built by palette_build(). */
void raycast_frame(const int8_t *map, int map_width, int map_height,
                   const ray_camera_t *camera, const texture_t *textures,
                   target_t *target);

#endif
