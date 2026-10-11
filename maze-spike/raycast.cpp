#include "raycast.h"

#include "assets.h"
#include "palette.h"
#include "rc_math.h"

#define COVER_BLOCK 16
#define COVER_BLOCKS ((RAY_MAX_COLUMNS + COVER_BLOCK - 1) / COVER_BLOCK)
#define MAX_DDA_STEPS 96
#define MIN_DEPTH 0.02F
#define FAR_DEPTH 1e30F
#define LIGHT_ENTRIES 512

/* One textured column produced by the ray cast and painted after the floor so
 * the floor pass can skip what it covers. */
typedef struct {
    int16_t y0;
    int16_t y1;
    uint16_t u;
    int32_t v_start;
    int32_t v_step;
    uint8_t texture_slot;
    uint8_t light;
} slice_t;

static float g_depth[RAY_MAX_COLUMNS];
static slice_t g_walls[RAY_MAX_COLUMNS];
static int16_t g_cover_top[RAY_MAX_COLUMNS];
static int16_t g_cover_bottom[RAY_MAX_COLUMNS];
static int16_t g_block_bottom_min[COVER_BLOCKS];
static int16_t g_block_bottom_max[COVER_BLOCKS];
static int16_t g_block_top_max[COVER_BLOCKS];
static uint8_t g_light_table[LIGHT_ENTRIES];
static uint8_t g_light_levels = LIGHT_LEVELS - 3;
static uint8_t g_side_shade = 4;
static float g_camera_x[RAY_MAX_COLUMNS];
static int g_camera_x_width;

void raycast_init_light(void) {
    const float curve_levels = 31.0F;
    const float full_distance = 2.6F;
    const float falloff = 1.05F;
    const int levels = LIGHT_LEVELS - 3;
    const int minimum = 2;
    int index;
    for (index = 0; index < LIGHT_ENTRIES; ++index) {
        const float distance = (float)index / 16.0F;
        const int light = (int)(curve_levels * full_distance /
                                    (full_distance + distance * falloff) +
                                0.5F);
        g_light_table[index] =
            (uint8_t)rc_clampi(light, minimum, levels - 1);
    }
    g_light_levels = (uint8_t)levels;
    g_side_shade = 4;
}

static uint8_t light_for(float distance) {
    const int index = (int)(distance * 16.0F);
    return g_light_table[rc_clampi(index, 0, LIGHT_ENTRIES - 1)];
}

/* Reads one map cell; anything outside the grid is a wall so a ray can never
 * leave the map. */
static int map_cell(const int8_t *map, int map_width, int map_height, int x,
                    int y) {
    if (x < 0 || y < 0 || x >= map_width || y >= map_height) {
        return TEX_BRICK;
    }
    return map[y * map_width + x];
}

void raycast_frame(const int8_t *map, int map_width, int map_height,
                   const ray_camera_t *camera, const texture_t *textures,
                   target_t *target) {
    const int width = target->width;
    const int height = target->height;
    const int half = height / 2;
    const float height_f = (float)height;
    const float pos_z = 0.5F * height_f;
    const float inv_width = 1.0F / (float)width;
    const ray_camera_t *p = camera;
    int x;

    if (g_camera_x_width != width) {
        for (x = 0; x < width; ++x) {
            g_camera_x[x] = 2.0F * (float)x * inv_width - 1.0F;
        }
        g_camera_x_width = width;
    }

    for (x = 0; x < width; ++x) {
        const float camera_x = g_camera_x[x];
        const float ray_x = p->dir_x + p->plane_x * camera_x;
        const float ray_y = p->dir_y + p->plane_y * camera_x;
        int map_x = rc_floor_int(p->x);
        int map_y = rc_floor_int(p->y);
        const float delta_x = ray_x == 0.0F ? FAR_DEPTH : rc_fabs(1.0F / ray_x);
        const float delta_y = ray_y == 0.0F ? FAR_DEPTH : rc_fabs(1.0F / ray_y);
        const int step_x = ray_x < 0.0F ? -1 : 1;
        const int step_y = ray_y < 0.0F ? -1 : 1;
        float side_x = ray_x < 0.0F ? (p->x - (float)map_x) * delta_x
                                    : ((float)map_x + 1.0F - p->x) * delta_x;
        float side_y = ray_y < 0.0F ? (p->y - (float)map_y) * delta_y
                                    : ((float)map_y + 1.0F - p->y) * delta_y;
        int side = 0;
        int hit = 0;
        int hit_tex = 0;
        float hit_dist = FAR_DEPTH;
        int step;

        g_walls[x].y1 = -1;
        g_cover_top[x] = (int16_t)height;
        g_cover_bottom[x] = -1;
        g_depth[x] = FAR_DEPTH;

        for (step = 0; step < MAX_DDA_STEPS; ++step) {
            int cell;
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = 0;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = 1;
            }
            cell = map_cell(map, map_width, map_height, map_x, map_y);
            if (cell >= 0) {
                hit = 1;
                hit_tex = cell;
                hit_dist = side == 0 ? side_x - delta_x : side_y - delta_y;
                break;
            }
        }
        if (!hit) {
            continue;
        }
        if (hit_dist < MIN_DEPTH) {
            hit_dist = MIN_DEPTH;
        }

        {
            float wall_x =
                side == 0 ? p->y + hit_dist * ray_y : p->x + hit_dist * ray_x;
            int u;
            int line_height;
            wall_x -= rc_floor(wall_x);
            u = (int)(wall_x * (float)TEX_SIZE) & (TEX_SIZE - 1);
            if ((side == 0 && ray_x > 0.0F) || (side == 1 && ray_y < 0.0F)) {
                u = (TEX_SIZE - 1) - u;
            }
            line_height = (int)(height_f / hit_dist);
            if (line_height > 0) {
                const int draw_start =
                    rc_clampi(-line_height / 2 + half, 0, height - 1);
                const int draw_end =
                    rc_clampi(line_height / 2 + half, 0, height - 1);
                const int32_t v_step = (TEX_SIZE << 16) / line_height;
                const int32_t v_start =
                    (draw_start - half + line_height / 2) * v_step;
                const int light =
                    rc_clampi(light_for(hit_dist) -
                                  (side == 1 ? g_side_shade : 0),
                              0, g_light_levels - 1);
                g_walls[x].y0 = (int16_t)draw_start;
                g_walls[x].y1 = (int16_t)draw_end;
                g_walls[x].u = (uint16_t)u;
                g_walls[x].v_start = v_start;
                g_walls[x].v_step = v_step;
                g_walls[x].texture_slot = (uint8_t)hit_tex;
                g_walls[x].light = (uint8_t)light;
                g_cover_top[x] = (int16_t)draw_start;
                g_cover_bottom[x] = (int16_t)draw_end;
            }
            g_depth[x] = hit_dist;
        }
    }

    /* Block summaries let the floor pass classify most blocks without touching
     * individual columns. */
    {
        const int blocks = (width + COVER_BLOCK - 1) / COVER_BLOCK;
        int block;
        for (block = 0; block < blocks; ++block) {
            const int x0 = block * COVER_BLOCK;
            const int x1 =
                x0 + COVER_BLOCK > width ? width : x0 + COVER_BLOCK;
            int bottom_min = g_cover_bottom[x0];
            int bottom_max = g_cover_bottom[x0];
            int top_max = g_cover_top[x0];
            int column;
            for (column = x0 + 1; column < x1; ++column) {
                if (g_cover_bottom[column] < bottom_min) {
                    bottom_min = g_cover_bottom[column];
                }
                if (g_cover_bottom[column] > bottom_max) {
                    bottom_max = g_cover_bottom[column];
                }
                if (g_cover_top[column] > top_max) {
                    top_max = g_cover_top[column];
                }
            }
            g_block_bottom_min[block] = (int16_t)bottom_min;
            g_block_bottom_max[block] = (int16_t)bottom_max;
            g_block_top_max[block] = (int16_t)top_max;
        }
    }

    /* Floor and ceiling rows, skipping pixels hidden by the far wall. The
     * walls are painted afterwards, so painting a covered pixel is only
     * wasted work, never a visible error. */
    {
        const float ray0_x = p->dir_x - p->plane_x;
        const float ray0_y = p->dir_y - p->plane_y;
        const float ray1_x = p->dir_x + p->plane_x;
        const float ray1_y = p->dir_y + p->plane_y;
        const int blocks = (width + COVER_BLOCK - 1) / COVER_BLOCK;
        const texture_t *floor_tex = &textures[TEX_FLOOR];
        const texture_t *ceiling_tex = &textures[TEX_CEILING];
        int y;
        for (y = half + 1; y < height; ++y) {
            const int p_row = y - half;
            const float row_distance = pos_z / (float)p_row;
            const float step_x = row_distance * (ray1_x - ray0_x) * inv_width;
            const float step_y = row_distance * (ray1_y - ray0_y) * inv_width;
            const float floor_x = p->x + row_distance * ray0_x;
            const float floor_y = p->y + row_distance * ray0_y;
            const int32_t fx = (int32_t)(floor_x * 65536.0F);
            const int32_t fy = (int32_t)(floor_y * 65536.0F);
            const int32_t sx = (int32_t)(step_x * 65536.0F);
            const int32_t sy = (int32_t)(step_y * 65536.0F);
            const int y_ceiling = height - 1 - y;
            const uint8_t light = light_for(row_distance);
            int run_start = -1;
            int block;
            for (block = 0; block < blocks; ++block) {
                const int x0 = block * COVER_BLOCK;
                const int x1 =
                    x0 + COVER_BLOCK > width ? width : x0 + COVER_BLOCK;
                int column;
                if (y > g_block_bottom_max[block]) {
                    if (run_start < 0) {
                        run_start = x0;
                    }
                    continue;
                }
                if (y <= g_block_bottom_min[block] &&
                    y_ceiling >= g_block_top_max[block]) {
                    if (run_start >= 0) {
                        raster_span_pair(target, floor_tex, ceiling_tex,
                                         palette_light(light), y, y_ceiling,
                                         run_start, x0 - 1,
                                         fx + sx * run_start,
                                         fy + sy * run_start, sx, sy);
                        run_start = -1;
                    }
                    continue;
                }
                for (column = x0; column < x1; ++column) {
                    const int hidden = y <= g_cover_bottom[column] &&
                                       y_ceiling >= g_cover_top[column];
                    if (hidden) {
                        if (run_start >= 0) {
                            raster_span_pair(target, floor_tex, ceiling_tex,
                                             palette_light(light), y,
                                             y_ceiling, run_start,
                                             column - 1,
                                             fx + sx * run_start,
                                             fy + sy * run_start, sx, sy);
                            run_start = -1;
                        }
                    } else if (run_start < 0) {
                        run_start = column;
                    }
                }
            }
            if (run_start >= 0) {
                raster_span_pair(target, floor_tex, ceiling_tex,
                                 palette_light(light), y, y_ceiling,
                                 run_start, width - 1,
                                 fx + sx * run_start, fy + sy * run_start, sx,
                                 sy);
            }
        }
        if ((height & 1) == 0) {
            /* Even heights leave row half-1 unmirrored; paint it as the far
             * ceiling at the horizon's distance from the ceiling texture. */
            raster_span_pair(target, ceiling_tex, ceiling_tex,
                             palette_light(light_for(pos_z)), half - 1, half - 1,
                             0, width - 1, 0, 0, 0, 0);
        }
    }

    /* Walls and slabs. */
    for (x = 0; x < width; ++x) {
        const slice_t *wall = &g_walls[x];
        if (wall->y1 >= wall->y0) {
            raster_column(target, &textures[wall->texture_slot],
                          palette_light(wall->light), x, wall->y0, wall->y1,
                          wall->u, wall->v_start, wall->v_step);
        }
    }
}
