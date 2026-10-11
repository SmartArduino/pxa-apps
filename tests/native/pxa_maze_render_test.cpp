#include "font.h"
#include "palette.h"
#include "raycast.h"
#include "render.h"
#include "world.h"

#include <assert.h>

#define MAX_PIXELS (296u * 240u)
#define GUARD_PIXELS 32u
#define GUARD_VALUE UINT16_C(0xA55A)

static uint16_t guarded[GUARD_PIXELS + MAX_PIXELS + GUARD_PIXELS];

static void clear_guards(uint32_t pixels) {
    for (uint32_t index = 0; index < GUARD_PIXELS; ++index) {
        guarded[index] = GUARD_VALUE;
        guarded[GUARD_PIXELS + pixels + index] = GUARD_VALUE;
    }
}

static void check_guards(uint32_t pixels) {
    for (uint32_t index = 0; index < GUARD_PIXELS; ++index) {
        assert(guarded[index] == GUARD_VALUE);
        assert(guarded[GUARD_PIXELS + pixels + index] == GUARD_VALUE);
    }
}

static void render_size(int width, int height) {
    renderer_t renderer;
    world_t world;
    hud_stats_t hud = {30, 120, 33, 40, 1, 1};
    target_t target = {guarded + GUARD_PIXELS, width, height};
    const uint32_t pixels = (uint32_t)width * (uint32_t)height;

    world_reset(&world);
    renderer_init(&renderer, width, height, 1);
    clear_guards(pixels);
    renderer_render(&renderer, &world, &hud, &target);
    renderer_draw_instructions(&renderer, &target);
    renderer_draw_stick(&renderer, &target, 1, width / 4, height / 2,
                        width / 4 + width / 8, height / 2);
    check_guards(pixels);
}

int main(void) {
    palette_build();
    raycast_init_light();
    font_build_atlas();
    render_size(148, 120);
    render_size(296, 240);
    return 0;
}
