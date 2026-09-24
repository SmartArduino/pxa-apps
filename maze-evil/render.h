#ifndef MAZE_EVIL_RENDER_H
#define MAZE_EVIL_RENDER_H

#include <stdint.h>

#include "raster.h"
#include "world.h"

typedef struct {
    int width;
    int height;
    int hud_scale; /* 1 at 240 px wide, 2 at 480, 3 at 720 */
    int design_divisor; /* 2 for the 148x120 internal render target */
} view_config_t;

typedef struct {
    uint32_t fps;
    uint32_t render_ms_x10;
    uint32_t tick_avg_ms;
    uint32_t tick_max_ms;
    int show_perf;
    int visible; /* hide gameplay labels beneath the start-screen diagram */
} hud_stats_t;

typedef struct {
    view_config_t view;
    int half_height;
    int hud_height;
} renderer_t;

void renderer_init(renderer_t *renderer, int width, int height, int hud_scale);

/* Full frame: floor/ceiling, walls/doors, billboards, weapon, damage tint and
 * the HUD. */
void renderer_render(renderer_t *renderer, const world_t *world,
                     const hud_stats_t *hud, target_t *target);

void renderer_draw_text(renderer_t *renderer, target_t *target, int x, int y,
                        const char *text, uint16_t color, int scale);

void renderer_draw_circle(renderer_t *renderer, target_t *target, int cx,
                          int cy, int radius, uint16_t color, int filled);

/* Start-screen diagram over a frozen first frame. */
void renderer_draw_instructions(renderer_t *renderer, target_t *target);

/* Virtual stick ring and knob, in render-target pixels. */
void renderer_draw_stick(renderer_t *renderer, target_t *target,
                         int stick_active, int origin_x, int origin_y,
                         int stick_x, int stick_y);

#endif
