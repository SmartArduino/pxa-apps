#ifndef JUMP3D_RENDER_H_CPP
#define JUMP3D_RENDER_H_CPP

#include <stdint.h>

#include "jump3d_game.hpp"
#include "../common/cpp/draw.hpp"
#include <pxa/ui_geometry.hpp>

namespace jump {

#define J3_POOL_VERTS 768
#define J3_RUN_MAX 24
/* Texture slots: 0..8 are the three font tiers (see jump3d_font.h), then the
 * shadow shapes and a solid texel used for full-screen scrims. */
#define J3_SHADOW_TEXTURE_SIZE 64u
#define J3_TEXTURE_SHADOW_SQUARE 9u
#define J3_TEXTURE_SHADOW_ROUND 10u
#define J3_TEXTURE_SOLID 11u

typedef struct {
    uint8_t color;
    uint16_t triangles;
    uint16_t first;
} j3_run_t;

typedef struct {
    arcade::DrawList list;
    int width;
    int height;
    pxa::ui::PixelRect hud;
    float scale;    /* pixels per world unit */
    float anchor_x; /* where the follow point lands */
    float anchor_y;
    float follow_x, follow_y, follow_z;
    uint32_t capabilities;
    uint8_t has_blend;
    uint8_t dropped;
    /* Interface metrics, scaled with the render target so the HUD stays
     * readable after an automatic half-resolution downgrade. The glyph tiers
     * are chosen to match the target size, so text is always drawn at scale 1. */
    float big_cell_h;
    float badge_w;
    float badge_h;
    float crown_size;
    float score_y;
    float panel_w;
    float panel_h;
    float button_w;
    float button_h;
    j3_run_t runs[J3_RUN_MAX];
    uint16_t run_count;
    pxa::game::Vertex pool[J3_POOL_VERTS];
    uint16_t pool_used;
} j3_render_t;

/* Uploads the two procedural shadow shapes and the solid texel. */
int j3_render_upload_resources(pxa::game::Renderer& context, uint8_t *scratch,
                               uint32_t scratch_capacity, uint16_t* palette, uint8_t scheme = 0);

int j3_render_upload_sky(pxa::game::Renderer&,uint16_t* palette,uint8_t scheme,uint8_t* scratch,uint32_t capacity);

void j3_render_configure(j3_render_t *render, int width, int height,
                         uint32_t capabilities);
void j3_render_adapt(j3_render_t*,const pxa::ui::DisplayMetrics&);

/* Builds and submits one frame. Returns 0 when the draw list overflowed or
 * the Host refused the list. */
int j3_render_frame(j3_render_t *render, const j3_game_t *game, pxa::game::Renderer& context,
                    uint8_t *bytes, uint32_t capacity, uint64_t frame_id);

/* Debug build aid: when J3_SKIP_PROBE is set the App cycles a bitmask of
 * renderer categories (background, shadows, blocks, waves, man, HUD) so one
 * device run reports the Host raster cost of each category. */
#ifndef J3_SKIP_PROBE
#define J3_SKIP_PROBE 0
#endif
#if J3_SKIP_PROBE
extern uint32_t j3_skip_probe_mask;
#endif

} // namespace jump
#endif
