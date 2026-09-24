#ifndef MAZE_EVIL_RAYCAST_H
#define MAZE_EVIL_RAYCAST_H

#include <stdint.h>

#include "raster.h"

/* The product Surface caps at 320x240, so the column scratch covers 320. */
#define RAY_MAX_COLUMNS 320
#define RAY_MAX_BILLBOARDS 96

typedef struct {
    float x;
    float y;
    float dir_x;
    float dir_y;
    float plane_x;
    float plane_y;
} ray_camera_t;

enum {
    RAY_CELL_EMPTY = 0,
    RAY_CELL_WALL = 1,
    RAY_CELL_SLAB = 2, /* door; `open` is the raised fraction in 1/32768 */
};

typedef struct {
    uint8_t kind;
    uint8_t texture_slot;
    uint16_t open;
} ray_cell_t;

typedef struct {
    float x;
    float y;
    float height; /* fraction of wall height */
    float lift;   /* fraction of wall height above the floor */
    uint8_t texture_slot; /* index into the sprite table */
    uint16_t texture_width;
    uint16_t texture_height;
    uint8_t self_lit;
} ray_billboard_t;

/* Builds the distance light table (maze-evil's 2.6 / 1.05 curve). */
void raycast_init_light(void);

/* Casts the cell grid, paints floor/ceiling and walls/doors, then draws the
 * billboards sorted far to near with a per-column depth test. Floor and
 * ceiling use TEX_FLOOR and TEX_CEILING. */
void raycast_frame(const ray_cell_t *cells, int map_width, int map_height,
                   const ray_camera_t *camera, const texture_t *textures,
                   const sprite_t *sprites, const ray_billboard_t *billboards,
                   int billboard_count, target_t *target);

#endif
