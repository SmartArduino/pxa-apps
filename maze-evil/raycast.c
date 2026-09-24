#include "raycast.h"

#include "assets.h"
#include "palette.h"
#include "rc_math.h"

#define COVER_BLOCK 16
#define COVER_BLOCKS ((RAY_MAX_COLUMNS + COVER_BLOCK - 1) / COVER_BLOCK)
#define MAX_DDA_STEPS 96
#define MIN_DEPTH 0.02F
#define FAR_DEPTH 1e30F
#define MIN_BILLBOARD_DEPTH 0.08F
#define SLAB_OPEN_SCALE 32768
#define SLAB_BLOCKS_BILLBOARDS 0.5F
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
static slice_t g_slabs[RAY_MAX_COLUMNS];
static int16_t g_cover_top[RAY_MAX_COLUMNS];
static int16_t g_cover_bottom[RAY_MAX_COLUMNS];
static int16_t g_block_bottom_min[COVER_BLOCKS];
static int16_t g_block_bottom_max[COVER_BLOCKS];
static int16_t g_block_top_max[COVER_BLOCKS];
static uint8_t g_light_table[LIGHT_ENTRIES];
static uint8_t g_light_levels = LIGHT_LEVELS - 3;
static uint8_t g_side_shade = 4;
static float g_billboard_depth[RAY_MAX_BILLBOARDS];
static uint8_t g_billboard_order[RAY_MAX_BILLBOARDS];
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

static float slab_open(const ray_cell_t *cell) {
    return (float)cell->open / (float)SLAB_OPEN_SCALE;
}

/* Reads one cell; anything outside the grid is a wall so a ray can never leave
 * the map. */
static ray_cell_t cell_at(const ray_cell_t *cells, int map_width, int map_height,
                          int x, int y) {
    ray_cell_t cell;
    if (x < 0 || y < 0 || x >= map_width || y >= map_height) {
        cell.kind = RAY_CELL_WALL;
        cell.texture_slot = 0;
        cell.open = 0;
        return cell;
    }
    return cells[y * map_width + x];
}

static uint8_t log2_pow2(uint32_t value) {
    uint8_t result = 0;
    while (value > 1u) {
        value >>= 1u;
        ++result;
    }
    return result;
}

void raycast_frame(const ray_cell_t *cells, int map_width, int map_height,
                   const ray_camera_t *camera, const texture_t *textures,
                   const sprite_t *sprites, const ray_billboard_t *billboards,
                   int billboard_count, target_t *target) {
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
        ray_cell_t hit_cell;
        float hit_dist = FAR_DEPTH;
        int slab_seen = 0;
        float slab_dist = 0.0F;
        ray_cell_t slab_cell;
        int slab_side = 0;
        int step;

        g_walls[x].y1 = -1;
        g_slabs[x].y1 = -1;
        g_cover_top[x] = (int16_t)height;
        g_cover_bottom[x] = -1;
        g_depth[x] = FAR_DEPTH;

        for (step = 0; step < MAX_DDA_STEPS; ++step) {
            ray_cell_t cell;
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = 0;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = 1;
            }
            cell = cell_at(cells, map_width, map_height, map_x, map_y);
            if (cell.kind == RAY_CELL_SLAB) {
                if (cell.open >= SLAB_OPEN_SCALE) {
                    continue;
                }
                if (cell.open == 0) {
                    hit = 1;
                    hit_cell = cell;
                    hit_dist = side == 0 ? side_x - delta_x : side_y - delta_y;
                    break;
                }
                if (!slab_seen) {
                    slab_seen = 1;
                    slab_dist = side == 0 ? side_x - delta_x : side_y - delta_y;
                    slab_cell = cell;
                    slab_side = side;
                }
                continue;
            }
            if (cell.kind == RAY_CELL_WALL) {
                hit = 1;
                hit_cell = cell;
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

        /* Far wall (or closed slab). */
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
                g_walls[x].texture_slot = hit_cell.texture_slot;
                g_walls[x].light = (uint8_t)light;
                g_cover_top[x] = (int16_t)draw_start;
                g_cover_bottom[x] = (int16_t)draw_end;
            }
            g_depth[x] = hit_dist;
        }

        /* Partially raised slab in front of the far wall. The slab slides up
         * into the ceiling, so the visible part is the bottom (1-open) of the
         * texture pinned to the top of the opening. */
        if (slab_seen) {
            float wall_x;
            int u;
            int line_height;
            if (slab_dist < MIN_DEPTH) {
                slab_dist = MIN_DEPTH;
            }
            wall_x = slab_side == 0 ? p->y + slab_dist * ray_y
                                    : p->x + slab_dist * ray_x;
            wall_x -= rc_floor(wall_x);
            u = (int)(wall_x * (float)TEX_SIZE) & (TEX_SIZE - 1);
            if ((slab_side == 0 && ray_x > 0.0F) ||
                (slab_side == 1 && ray_y < 0.0F)) {
                u = (TEX_SIZE - 1) - u;
            }
            line_height = (int)(height_f / slab_dist);
            if (line_height > 0) {
                const int top = -line_height / 2 + half;
                const float open = slab_open(&slab_cell);
                const int visible =
                    (int)((1.0F - open) * (float)line_height);
                const int draw_start = rc_clampi(top, 0, height - 1);
                int draw_end = rc_clampi(top + visible - 1, -1, height - 1);
                const int32_t v_step = (TEX_SIZE << 16) / line_height;
                const int32_t v_start =
                    (int32_t)(open * (float)TEX_SIZE * 65536.0F) +
                    (draw_start - top) * v_step;
                /* The texture must not wrap past its last row at the slab's
                 * bottom edge; the column kernel wraps, so clip the run. */
                const int32_t last_row = (TEX_SIZE << 16) - 1;
                if (v_step > 0 && v_start <= last_row) {
                    const int rows_in_texture =
                        (last_row - v_start) / v_step + 1;
                    if (draw_end - draw_start + 1 > rows_in_texture) {
                        draw_end = draw_start + rows_in_texture - 1;
                    }
                } else {
                    draw_end = draw_start - 1;
                }
                if (draw_end >= draw_start) {
                    const int light =
                        rc_clampi(light_for(slab_dist) -
                                      (slab_side == 1 ? g_side_shade : 0),
                                  0, g_light_levels - 1);
                    g_slabs[x].y0 = (int16_t)draw_start;
                    g_slabs[x].y1 = (int16_t)draw_end;
                    g_slabs[x].u = (uint16_t)u;
                    g_slabs[x].v_start = v_start;
                    g_slabs[x].v_step = v_step;
                    g_slabs[x].texture_slot = slab_cell.texture_slot;
                    g_slabs[x].light = (uint8_t)light;
                    if (open < SLAB_BLOCKS_BILLBOARDS) {
                        g_depth[x] = slab_dist;
                    }
                }
            }
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

    /* Floor and ceiling rows, skipping pixels hidden by the far wall. Walls
     * are painted afterwards, so painting a covered pixel is only wasted work,
     * never a visible error. */
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
                                             y_ceiling, run_start, column - 1,
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
                                 palette_light(light), y, y_ceiling, run_start,
                                 width - 1, fx + sx * run_start,
                                 fy + sy * run_start, sx, sy);
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

    /* Walls and doors. */
    for (x = 0; x < width; ++x) {
        const slice_t *wall = &g_walls[x];
        const slice_t *slab = &g_slabs[x];
        if (wall->y1 >= wall->y0) {
            raster_column(target, &textures[wall->texture_slot],
                          palette_light(wall->light), x, wall->y0, wall->y1,
                          wall->u, wall->v_start, wall->v_step, 0);
        }
        if (slab->y1 >= slab->y0) {
            raster_column(target, &textures[slab->texture_slot],
                          palette_light(slab->light), x, slab->y0, slab->y1,
                          slab->u, slab->v_start, slab->v_step, 0);
        }
    }

    /* Billboards sorted far to near so nearer ones paint over farther ones. */
    {
        const float det = p->plane_x * p->dir_y - p->dir_x * p->plane_y;
        int visible = 0;
        int count;
        int index;
        if (det == 0.0F) {
            return;
        }
        {
            const float inv_det = 1.0F / det;
            count = billboard_count > RAY_MAX_BILLBOARDS
                        ? RAY_MAX_BILLBOARDS
                        : billboard_count;
            for (index = 0; index < count; ++index) {
                const float sx = billboards[index].x - p->x;
                const float sy = billboards[index].y - p->y;
                const float depth =
                    inv_det * (-p->plane_y * sx + p->plane_x * sy);
                if (depth <= MIN_BILLBOARD_DEPTH) {
                    continue;
                }
                g_billboard_depth[index] = depth;
                g_billboard_order[visible++] = (uint8_t)index;
            }
            for (index = 1; index < visible; ++index) {
                const uint8_t key = g_billboard_order[index];
                int j = index - 1;
                while (j >= 0 &&
                       g_billboard_depth[g_billboard_order[j]] <
                           g_billboard_depth[key]) {
                    g_billboard_order[j + 1] = g_billboard_order[j];
                    --j;
                }
                g_billboard_order[j + 1] = key;
            }
            for (index = 0; index < visible; ++index) {
                const ray_billboard_t *sprite =
                    &billboards[g_billboard_order[index]];
                const sprite_t *texture;
                texture_t column_texture;
                const float sx = sprite->x - p->x;
                const float sy = sprite->y - p->y;
                const float transform_x = inv_det * (p->dir_y * sx - p->dir_x * sy);
                const float transform_y =
                    g_billboard_depth[g_billboard_order[index]];
                const int screen_x = (int)((float)(width / 2) *
                                           (1.0F + transform_x / transform_y));
                const float wall_height = (float)height / transform_y;
                const int sprite_height =
                    (int)(wall_height * sprite->height);
                int bottom;
                int top;
                int sprite_width;
                int left;
                int right;
                uint8_t light;
                int32_t u_step;
                int32_t v_step;
                int y0;
                int y1;
                int32_t v_start;
                int x0;
                int x1;
                int stripe;
                if (sprite_height <= 0 || sprite->texture_width == 0 ||
                    sprite->texture_height == 0) {
                    continue;
                }
                bottom = (int)((float)half + wall_height * 0.5F -
                               wall_height * sprite->lift);
                top = bottom - sprite_height;
                sprite_width =
                    sprite_height * sprite->texture_width / sprite->texture_height;
                if (sprite_width <= 0) {
                    continue;
                }
                left = screen_x - sprite_width / 2;
                right = left + sprite_width;
                if (bottom <= 0 || top >= height || right <= 0 || left >= width) {
                    continue;
                }
                light = sprite->self_lit ? (uint8_t)(g_light_levels - 1)
                                         : light_for(transform_y);
                u_step = (int32_t)((int32_t)sprite->texture_width << 16) /
                         sprite_width;
                v_step = (int32_t)((int32_t)sprite->texture_height << 16) /
                         sprite_height;
                y0 = rc_clampi(top, 0, height - 1);
                y1 = rc_clampi(bottom - 1, 0, height - 1);
                v_start = (y0 - top) * v_step;
                x0 = rc_clampi(left, 0, width - 1);
                x1 = rc_clampi(right - 1, 0, width - 1);
                texture = &sprites[sprite->texture_slot];
                column_texture.pixels = texture->pixels;
                column_texture.size = texture->padded_height;
                column_texture.log2_size = log2_pow2(texture->padded_height);
                for (stripe = x0; stripe <= x1; ++stripe) {
                    const int u = ((stripe - left) * u_step) >> 16;
                    if (transform_y >= g_depth[stripe]) {
                        continue;
                    }
                    raster_column(target, &column_texture,
                                  palette_light(light), stripe, y0, y1,
                                  (uint16_t)u, v_start, v_step, 1);
                }
            }
        }
    }
}
