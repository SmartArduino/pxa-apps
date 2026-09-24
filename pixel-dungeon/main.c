/* Pixel Dungeon: a PXA roguelike rendered through the GameRender raster API.
 *
 * The app follows Shattered Pixel Dungeon's core loop: procedurally generated
 * floors, turn-based movement and combat, mobs with simple hunting AI, loot,
 * hunger, XP and a 25-floor descent that ends in a boss fight. Input is touch
 * first (tap to walk/attack, action bar and map zoom) with controller
 * support. Progress is saved through the Host key-value Storage service. */
#include <stdint.h>

#include "assets.h"
#include "audio.h"
#include "dungeon.h"
#include "font.h"
#include "font_data.h"
#include "game.h"
#include "input.h"
#include "layout.h"
#include "pxa.h"
#include "pxa_canvas.h"
#include "pxa_game_render.h"
#include "pxa_audio.h"
#include "pxa_log.h"
#include "pxa_permission.h"
#include "pxa_raster.h"
#include "pxa_storage.h"
#include "pxa_ui.h"
#include "pxa_i18n.h"
#include "render.h"
#include "strings.h"
#include "rng.h"

#define PD_CREATE_REQUEST UINT32_C(1)
#define PD_POINTER_NODE UINT32_C(2)
#define PD_STORAGE_GET_REQUEST UINT32_C(3)
#define PD_STORAGE_SET_REQUEST UINT32_C(4)
#define PD_ZOOM_GET_REQUEST UINT32_C(5)
#define PD_ZOOM_SET_REQUEST UINT32_C(6)
#define PD_STORAGE_KEY "pixel-dungeon.save"
#define PD_STORAGE_KEY_BYTES 18
#define PD_ZOOM_KEY "pixel-dungeon.zoom"
#define PD_ZOOM_KEY_BYTES 18
#define PD_ACTIVE_PERIOD_MS 40u
#define PD_IDLE_PERIOD_MS 240u
#define PD_AUDIO_PERIOD_MS 20u
#define PD_DEFAULT_WIDTH 296u
#define PD_DEFAULT_HEIGHT 240u

static uint8_t g_packet[192];
static uint8_t g_canvas_buffer[64];
static uint8_t g_canvas_packet[128];
static uint8_t g_storage_payload[1024];
static uint8_t g_storage_packet[1024];
static uint8_t g_save_blob[1024];
#define PD_UPLOAD_BYTES (PXA_RASTER_UPLOAD_HEADER_BYTES + 65536u)
static uint8_t g_upload[PD_UPLOAD_BYTES];
static uint8_t g_draw[PD_MAX_DRAW_BYTES];
static uint8_t g_fog_texels[16 * 16];
static uint8_t g_search_texels[64 * 16];
static uint32_t g_canvas_generation;
static uint8_t g_canvas_initialized;
static uint32_t g_context;
static uint32_t g_capabilities;
static uint8_t g_have_context;
static uint8_t g_want_scale = 1;
static uint8_t g_create_attempt; /* 0 auto, 1 auto at the board default, 2 legacy */
static uint32_t g_display_width = PD_DEFAULT_WIDTH;
static uint32_t g_display_height = PD_DEFAULT_HEIGHT;
static uint32_t g_render_width = PD_DEFAULT_WIDTH;
static uint32_t g_render_height = PD_DEFAULT_HEIGHT;
static uint64_t g_frame_id;
static uint16_t g_clock_period;
static uint32_t g_controller_buttons;
static uint8_t g_have_controller;
static uint8_t g_progress_dirty;
static uint32_t g_anim_ms;
static uint32_t g_render_phase;
static int g_safe_top = 8;
static int g_safe_right = 10;
static int g_safe_bottom = 8;
static int g_safe_left = 10;
static uint32_t g_display_shape;
static uint32_t g_corner_radii[4];
static pd_game_t g_game;
static pd_layout_t g_layout;
static pd_audio_t g_audio;
static uint8_t g_seeded;

static void rebuild_layout(void) {
    const int display_w = g_display_width ? (int)g_display_width : 1;
    const int display_h = g_display_height ? (int)g_display_height : 1;
    const int zoom = g_layout.zoom ? g_layout.zoom :
                     (g_display_shape == 2 && g_display_width >= 360 &&
                      g_display_height >= 360 ? 2 : 1);
    int radii[4];
    pd_layout_build(&g_layout, (int)g_render_width, (int)g_render_height,
                    g_safe_top * (int)g_render_height / display_h,
                    g_safe_right * (int)g_render_width / display_w,
                    g_safe_bottom * (int)g_render_height / display_h,
                    g_safe_left * (int)g_render_width / display_w);
    for (int index = 0; index < 4; ++index)
        radii[index] = (int)((uint64_t)g_corner_radii[index] *
                             g_render_width / (uint32_t)display_w);
    pd_layout_fit_display_shape(&g_layout, g_display_shape, radii);
    pd_layout_set_zoom(&g_layout, zoom);
}

static uint8_t preferred_render_scale(void) {
    const uint32_t shorter = g_display_width < g_display_height ?
                             g_display_width : g_display_height;
    const uint32_t longer = g_display_width > g_display_height ?
                            g_display_width : g_display_height;
    return (uint8_t)(shorter >= 400 && longer >= 600 ? 2 : 1);
}

static char *append_text_local(char *out, const char *text) {
    while (*text != '\0') *out++ = *text++;
    return out;
}

static char *append_uint_local(char *out, uint32_t value) {
    char digits[12];
    int count = 0;
    if (value == 0) {
        *out++ = '0';
        return out;
    }
    while (value != 0 && count < 11) {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count > 0) *out++ = digits[--count];
    return out;
}

/* ---------------------------------------------------------------------- */
/* Rendering                                                               */
/* ---------------------------------------------------------------------- */

static void render_frame(void) {
    if (!g_have_context || g_context == 0) return;
    (void)pd_render_present(g_context, g_capabilities, &g_game, &g_layout,
                            g_draw, sizeof(g_draw), ++g_frame_id, g_anim_ms);
}

static int game_animating(void) {
    if (g_game.walk_active) return 1;
    if (g_game.hero_moving > 0) return 1;
    if (g_game.potion_hint > 0) return 1;
    for (int index = 0; index < PD_MOBS_MAX; ++index)
        if (g_game.mobs[index].dying > 0 ||
            g_game.mobs[index].moving > 0 ||
            g_game.mobs[index].attacking > 0) return 1;
    for (int index = 0; index < PD_EFFECTS_MAX; ++index)
        if (g_game.effects[index].ttl > 0) return 1;
    return 0;
}

static void update_clock_period(void) {
    uint16_t wanted;
    if (pd_audio_active(&g_audio)) {
        /* The Host consumes one 20 ms PCM frame per tick, so the clock runs at
         * the audio frame rate and rendering stays on demand. */
        wanted = (uint16_t)PD_AUDIO_PERIOD_MS;
    } else {
        wanted = game_animating() ? (uint16_t)PD_ACTIVE_PERIOD_MS
                                  : (uint16_t)PD_IDLE_PERIOD_MS;
    }
    if (wanted == g_clock_period) return;
    if (pxa_clock_set_period(wanted)) g_clock_period = wanted;
}

/* ---------------------------------------------------------------------- */
/* Persistence                                                             */
/* ---------------------------------------------------------------------- */

static void save_progress(void) {
    int length;
    if (g_game.phase != PD_PHASE_PLAY && g_game.phase != PD_PHASE_BAG &&
        g_game.phase != PD_PHASE_INFO && g_game.phase != PD_PHASE_SETTINGS &&
        g_game.phase != PD_PHASE_PAUSE)
        return;
    length = pd_game_serialize(&g_game, g_save_blob, (int)sizeof(g_save_blob));
    if (length <= 0) return;
    /* Storage SET is a synchronous control message; the Host answers inside
     * the call, so no result event follows. */
    (void)pxa_storage_set(PD_STORAGE_SET_REQUEST, PD_STORAGE_KEY,
                          PD_STORAGE_KEY_BYTES, g_save_blob, (size_t)length,
                          g_storage_payload, sizeof(g_storage_payload),
                          g_storage_packet, sizeof(g_storage_packet));
}

static void clear_progress(void) {
    (void)pxa_storage_remove(PD_STORAGE_SET_REQUEST, PD_STORAGE_KEY,
                             PD_STORAGE_KEY_BYTES, g_storage_payload,
                             sizeof(g_storage_payload), g_storage_packet,
                             sizeof(g_storage_packet));
}

static void request_progress(void) {
    (void)pxa_storage_get(PD_STORAGE_GET_REQUEST, PD_STORAGE_KEY,
                          PD_STORAGE_KEY_BYTES, g_storage_payload,
                          sizeof(g_storage_payload), g_storage_packet,
                          sizeof(g_storage_packet));
}

static void save_zoom(void) {
    const uint8_t zoom = (uint8_t)g_layout.zoom;
    (void)pxa_storage_set(PD_ZOOM_SET_REQUEST, PD_ZOOM_KEY,
                          PD_ZOOM_KEY_BYTES, &zoom, 1,
                          g_storage_payload, sizeof(g_storage_payload),
                          g_storage_packet, sizeof(g_storage_packet));
}

static void request_zoom(void) {
    (void)pxa_storage_get(PD_ZOOM_GET_REQUEST, PD_ZOOM_KEY,
                          PD_ZOOM_KEY_BYTES, g_storage_payload,
                          sizeof(g_storage_payload), g_storage_packet,
                          sizeof(g_storage_packet));
}

/* ---------------------------------------------------------------------- */
/* Resources                                                               */
/* ---------------------------------------------------------------------- */

static int upload_resources(void) {
    const uint32_t tile_bytes = PD_ATLAS_WIDTH * PD_TILE_ATLAS_HEIGHT;
    const uint32_t sprite_bytes = PD_ATLAS_WIDTH * PD_SPRITE_ATLAS_HEIGHT;
    if (pxa_raster_upload_palette_rgb565(g_context, pd_palette, g_upload,
                                         sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                  PXA_RASTER_PALETTE_COLORS * sizeof(uint16_t)))
        return 0;
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_TILES, PD_ATLAS_WIDTH, PD_TILE_ATLAS_HEIGHT,
            pd_tile_atlas, g_upload, sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + tile_bytes))
        return 0;
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_WALLS, PD_ATLAS_WIDTH, PD_WALL_ATLAS_HEIGHT,
            pd_wall_atlas, g_upload, sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                  PD_ATLAS_WIDTH * PD_WALL_ATLAS_HEIGHT))
        return 0;
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_SPRITES, PD_ATLAS_WIDTH, PD_SPRITE_ATLAS_HEIGHT,
            pd_sprite_atlas, g_upload, sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + sprite_bytes))
        return 0;
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_FONT_ASCII, PD_ASCII_ATLAS_W,
            PD_ASCII_ATLAS_H, pd_font_ascii, g_upload, sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                  PD_ASCII_ATLAS_W * PD_ASCII_ATLAS_H))
        return 0;
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_FONT_CJK, PD_CJK_ATLAS_W, PD_CJK_ATLAS_H,
            pd_font_cjk, g_upload, sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                  PD_CJK_ATLAS_W * PD_CJK_ATLAS_H))
        return 0;
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_UI, PD_UI_ATLAS_WIDTH, PD_UI_ATLAS_HEIGHT,
            pd_ui_atlas, g_upload, sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                  PD_UI_ATLAS_WIDTH * PD_UI_ATLAS_HEIGHT))
        return 0;
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_TITLE, PD_TITLE_ATLAS_WIDTH,
            PD_TITLE_ATLAS_HEIGHT, pd_title_atlas, g_upload,
            sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                  PD_TITLE_ATLAS_WIDTH * PD_TITLE_ATLAS_HEIGHT))
        return 0;
    {
        /* One-colour texture used by the remembered-terrain overlay. */
        for (uint32_t index = 0; index < 16u * 16u; ++index)
            g_fog_texels[index] = PD_FOG_INDEX;
        if (pxa_raster_upload_texture_index8(
                g_context, PD_TEXTURE_FOG, 16, 16, g_fog_texels, g_upload,
                sizeof(g_upload)) !=
            (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + 16u * 16u))
            return 0;
    }
    for (uint32_t row = 0; row < 16u; ++row) {
        for (uint32_t column = 0; column < 64u; ++column) {
            const uint32_t fade = column / 16u;
            static const uint8_t pattern_values[16] = {
                0, 8, 2, 10, 12, 4, 14, 6,
                3, 11, 1, 9, 15, 7, 13, 5};
            const uint32_t pattern =
                pattern_values[(column & 3u) + (row & 3u) * 4u];
            g_search_texels[row * 64u + column] =
                pattern < 16u - fade * 4u ? PD_SEARCH_INDEX : 0;
        }
    }
    if (pxa_raster_upload_texture_index8(
            g_context, PD_TEXTURE_SEARCH, 64, 16, g_search_texels,
            g_upload, sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES + 64u * 16u))
        return 0;
    return 1;
}

static int setup_pointer_node(void) {
    pxa_canvas_frame_t frame;
    pxa_canvas_begin(&frame, g_canvas_buffer, sizeof(g_canvas_buffer));
    return pxa_canvas_present_with_root_event_mask(
        PD_POINTER_NODE, &frame, &g_canvas_generation, &g_canvas_initialized,
        PXA_UI_EVENT_MASK_POINTER | PXA_UI_EVENT_MASK_CONTROLLER_STATE,
        g_canvas_packet, sizeof(g_canvas_packet), g_packet, sizeof(g_packet));
}

static uint8_t fallback_shift(void) {
    return (uint8_t)(preferred_render_scale() == 2 ? 1 : 0);
}

/* Boards that advertise CREATE_AUTO_CONTEXT pick the render resolution from
 * the panel. Hosts without it get an explicit size derived from the UI
 * environment, keeping the same half-resolution rule for large panels. */
static void request_context(void) {
    if (g_create_attempt < 2) {
        if (!pxa_game_render_create_auto(PD_CREATE_REQUEST, g_want_scale, 3, 1,
                                         g_packet, sizeof(g_packet))) {
            (void)pxa_log_error("pd: GameRender create request failed");
        }
        return;
    }
    {
        const uint8_t shift = fallback_shift();
        const uint16_t width = (uint16_t)(g_display_width >> shift);
        const uint16_t height = (uint16_t)(g_display_height >> shift);
        if (!pxa_game_render_create(PD_CREATE_REQUEST, width, height, 3, 1,
                                    g_packet, sizeof(g_packet)))
            (void)pxa_log_error("pd: GameRender create request failed");
    }
}

/* ---------------------------------------------------------------------- */
/* Lifecycle                                                               */
/* ---------------------------------------------------------------------- */

int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    pxa_ui_environment_t environment;
    if (pxa_ui_parse_start_environment(config, length, &environment) &&
        environment.width > 0 && environment.height > 0) {
        g_display_width = environment.width;
        g_display_height = environment.height;
        g_safe_top = (int)environment.safe_insets[0];
        g_safe_right = (int)environment.safe_insets[1];
        g_safe_bottom = (int)environment.safe_insets[2];
        g_safe_left = (int)environment.safe_insets[3];
        g_display_shape = environment.display_shape;
        for (int index = 0; index < 4; ++index)
            g_corner_radii[index] = environment.corner_radii[index];
    }
    g_want_scale = preferred_render_scale();
    g_create_attempt = 0;
    g_context = 0;
    g_capabilities = 0;
    g_have_context = 0;
    g_frame_id = 0;
    g_clock_period = 0;
    g_seeded = 0;
    g_progress_dirty = 0;
    g_have_controller = 0;
    g_controller_buttons = 0;
    pd_audio_init(&g_audio);
    pd_audio_set_music(&g_audio, PD_MUSIC_TITLE);
    {
        /* Follow the system locale: Chinese when the Host reports a zh tag. */
        pxa_i18n_t i18n;
        if (pxa_i18n_init_from_start_config(&i18n, NULL, config, length) &&
            i18n.locale_size >= 2 && i18n.locale[0] == 'z' &&
            i18n.locale[1] == 'h') {
            pd_strings_set_language(1);
        }
    }
    rebuild_layout();
    pd_game_reset(&g_game, UINT32_C(0x51ed270b));
    if (!setup_pointer_node() || !pxa_window_fullscreen()) {
        (void)pxa_log_error("pd: pointer node or fullscreen request failed");
        return PXA_STATUS_INTERNAL;
    }
    request_context();
    request_progress();
    request_zoom();
    return PXA_STATUS_OK;
}

static void finish_context(uint32_t handle, uint32_t capabilities,
                           uint32_t display_width, uint32_t display_height,
                           uint32_t render_width, uint32_t render_height) {
    const uint32_t required = PXA_RASTER_CAP_SPRITE_BATCH |
                              PXA_RASTER_CAP_FLAT_QUAD;
    if ((capabilities & required) != required) {
        (void)pxa_close_handle(handle);
        (void)pxa_log_error("pd: GameRender capabilities missing");
        return;
    }
    g_context = handle;
    g_capabilities = capabilities;
    if (display_width > 0 && display_height > 0) {
        g_display_width = display_width;
        g_display_height = display_height;
    }
    g_render_width = render_width;
    g_render_height = render_height;
    rebuild_layout();
    if (!upload_resources()) {
        (void)pxa_log_error("pd: GameRender resource upload failed");
        (void)pxa_close_handle(g_context);
        g_context = 0;
        return;
    }
    g_have_context = 1;
    render_frame();
    update_clock_period();
    {
        char line[128];
        char *out = line;
        out = append_text_local(out, "pd: display ");
        out = append_uint_local(out, g_display_width);
        *out++ = 'x';
        out = append_uint_local(out, g_display_height);
        out = append_text_local(out, " render ");
        out = append_uint_local(out, g_render_width);
        *out++ = 'x';
        out = append_uint_local(out, g_render_height);
        *out = '\0';
        (void)pxa_log_info(line);
    }
}

static void log_failure(const char *kind, int32_t status) {
    char line[96];
    char *out = line;
    out = append_text_local(out, "pd: GameRender ");
    out = append_text_local(out, kind);
    out = append_text_local(out, " create failed status=");
    out = append_uint_local(out, (uint32_t)status);
    *out = '\0';
    (void)pxa_log_error(line);
}

static void handle_create_event(const pxa_event_t *event) {
    pxa_game_render_auto_create_result_t auto_created;
    if (event->opcode == PXA_GAME_RENDER_CREATE_AUTO_CONTEXT) {
        if (!pxa_game_render_parse_auto_create(event, &auto_created)) return;
        if (auto_created.context.status != PXA_STATUS_OK) {
            if (g_create_attempt == 0 && g_want_scale > 0) {
                /* The board refused the requested scale; take its default. */
                g_create_attempt = 1;
                g_want_scale = 0;
                request_context();
                return;
            }
            if (g_create_attempt < 2) {
                g_create_attempt = 2;
                request_context();
                return;
            }
            log_failure("auto", auto_created.context.status);
            return;
        }
        finish_context(auto_created.context.context_handle,
                       auto_created.context.capabilities,
                       auto_created.display_width, auto_created.display_height,
                       auto_created.render_width, auto_created.render_height);
        return;
    }
    {
        pxa_game_render_create_result_t created;
        if (!pxa_game_render_parse_create(event, &created)) return;
        if (created.status != PXA_STATUS_OK) {
            log_failure("legacy", created.status);
            return;
        }
        const uint8_t shift = fallback_shift();
        finish_context(created.context_handle, created.capabilities,
                       g_display_width, g_display_height,
                       g_display_width >> shift, g_display_height >> shift);
    }
}

static void handle_pointer(const pxa_event_t *event) {
    pxa_ui_pointer_data_t pointer;
    int x;
    int y;
    if (!pxa_ui_parse_pointer(event, &pointer)) return;
    if (pointer.node != PD_POINTER_NODE) return;
    x = pxa_game_render_map_coord(pointer.x, (uint16_t)g_display_width,
                                  (uint16_t)g_render_width);
    y = pxa_game_render_map_coord(pointer.y, (uint16_t)g_display_height,
                                  (uint16_t)g_render_height);
    {
        const uint8_t phase_before = g_game.phase;
        const int zoom_before = g_layout.zoom;
        pd_input_pointer(&g_game, &g_layout, x, y, pointer.pointer_id,
                         pointer.phase,
                         pointer.timestamp_us);
        if (zoom_before != g_layout.zoom) save_zoom();
        if (phase_before == PD_PHASE_CLASS && g_game.phase == PD_PHASE_PLAY) {
            clear_progress();
            save_progress();
        }
        if (phase_before == PD_PHASE_PAUSE && g_game.phase == PD_PHASE_TITLE) {
            g_game.phase = PD_PHASE_PAUSE;
            save_progress();
            g_game.phase = PD_PHASE_TITLE;
        }
        if (g_game.phase != phase_before &&
            (g_game.phase == PD_PHASE_DEAD || g_game.phase == PD_PHASE_WON))
            clear_progress();
    }
    g_progress_dirty = 1;
    pd_audio_set_music(&g_audio,
        (g_game.phase == PD_PHASE_TITLE || g_game.phase == PD_PHASE_SAVES ||
         g_game.phase == PD_PHASE_CLASS) ? PD_MUSIC_TITLE :
        (uint8_t)((g_game.depth - 1) / 5));
    render_frame();
    update_clock_period();
}

static void handle_controller(const pxa_event_t *event) {
    pxa_ui_controller_data_t controller;
    uint8_t phase_before;
    if (!pxa_ui_parse_controller(event, &controller)) return;
    if (!controller.connected) {
        g_have_controller = 0;
        return;
    }
    if (!g_have_controller) {
        g_have_controller = 1;
        g_controller_buttons = controller.buttons;
        return;
    }
    phase_before = g_game.phase;
    pd_input_controller(&g_game, controller.buttons, g_controller_buttons);
    if (phase_before == PD_PHASE_CLASS && g_game.phase == PD_PHASE_PLAY) {
        clear_progress();
        save_progress();
    }
    g_controller_buttons = controller.buttons;
    g_progress_dirty = 1;
    pd_audio_set_music(&g_audio,
        (g_game.phase == PD_PHASE_TITLE || g_game.phase == PD_PHASE_SAVES ||
         g_game.phase == PD_PHASE_CLASS) ? PD_MUSIC_TITLE :
        (uint8_t)((g_game.depth - 1) / 5));
    render_frame();
    update_clock_period();
}

static void pump_audio(void) {
    uint8_t sounds[8];
    const int count = pd_game_take_sounds(&g_game, sounds, 8);
    for (int index = 0; index < count; ++index)
        pd_audio_play(&g_audio, sounds[index]);
    pd_audio_tick(&g_audio);
}

static void handle_tick(uint64_t timestamp_us) {
    g_anim_ms = (uint32_t)(timestamp_us / 1000u);
    pump_audio();
    if (!g_seeded) {
        g_game.run_seed = pd_rng_mix((uint32_t)(timestamp_us >> 8),
                                     (uint32_t)timestamp_us);
        g_seeded = 1;
    }
    if (g_game.phase != PD_PHASE_PLAY) {
        if (g_progress_dirty) {
            render_frame();
            g_progress_dirty = 0;
        }
        return;
    }
    {
        const uint16_t turn_before = g_game.turn;
        pd_game_tick(&g_game);
        if (g_game.turn != turn_before) {
            /* Descending, dying or the world moving all want a new frame. */
            g_progress_dirty = 1;
        }
        if (g_game.phase == PD_PHASE_DEAD) {
            clear_progress();
        } else if (g_game.turn != turn_before &&
                   (g_game.turn % 40u) == 0u) {
            save_progress();
        }
    }
    if (pd_audio_active(&g_audio)) {
        if ((g_render_phase & 1u) == 0u) render_frame();
        ++g_render_phase;
    } else {
        render_frame();
    }
    g_progress_dirty = 0;
    update_clock_period();
}

int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
    pxa_event_t event;
    uint64_t timestamp_us;
    if (!pxa_parse_event(bytes, length, &event)) return PXA_EVENT_UNHANDLED;

    if (event.service == PXA_SERVICE_WINDOW &&
        event.opcode == PXA_WINDOW_BACK_REQUESTED) {
        if (!pd_input_back(&g_game)) return PXA_EVENT_UNHANDLED;
        g_progress_dirty = 1;
        render_frame();
        update_clock_period();
        return PXA_EVENT_HANDLED;
    }

    if (event.service == PXA_SERVICE_GAME_RENDER &&
        (event.opcode == PXA_GAME_RENDER_CREATE_AUTO_CONTEXT ||
         event.opcode == PXA_GAME_RENDER_CREATE_CONTEXT) &&
        event.request_id == PD_CREATE_REQUEST) {
        handle_create_event(&event);
        return PXA_EVENT_HANDLED;
    }
    if (event.service == PXA_SERVICE_STORAGE &&
        event.opcode == PXA_STORAGE_GET &&
        event.request_id == PD_STORAGE_GET_REQUEST) {
        pxa_storage_get_result_t result;
        if (pxa_storage_parse_get(&event, &result) &&
            result.status == PXA_STATUS_OK && result.value != NULL &&
            pd_game_restore(&g_game, result.value, (int)result.value_length)) {
            g_game.phase = PD_PHASE_SAVES;
            g_seeded = 1;
            (void)pxa_log_info("pd: progress restored");
            render_frame();
        }
        return PXA_EVENT_HANDLED;
    }
    if (event.service == PXA_SERVICE_STORAGE &&
        event.opcode == PXA_STORAGE_GET &&
        event.request_id == PD_ZOOM_GET_REQUEST) {
        pxa_storage_get_result_t result;
        if (pxa_storage_parse_get(&event, &result) &&
            result.status == PXA_STATUS_OK && result.value != NULL &&
            result.value_length == 1) {
            pd_layout_set_zoom(&g_layout, result.value[0]);
            render_frame();
        }
        return PXA_EVENT_HANDLED;
    }
    if (event.service == PXA_SERVICE_STORAGE &&
        event.opcode == PXA_STORAGE_SET &&
        (event.request_id == PD_STORAGE_SET_REQUEST ||
         event.request_id == PD_ZOOM_SET_REQUEST)) {
        return PXA_EVENT_HANDLED;
    }
    if (event.service == PXA_SERVICE_PERMISSION ||
        event.service == PXA_SERVICE_AUDIO) {
        pd_audio_handle_event(&g_audio, &event, g_packet, sizeof(g_packet));
        update_clock_period();
        return PXA_EVENT_HANDLED;
    }
    if (event.service == PXA_SERVICE_UI && event.opcode == PXA_UI_EVENT) {
        pxa_ui_event_data_t ui_event;
        if (!pxa_ui_parse_event(&event, &ui_event)) return PXA_EVENT_UNHANDLED;
        if (ui_event.node != PD_POINTER_NODE)
            return PXA_EVENT_UNHANDLED;
        if (ui_event.kind == PXA_UI_EVENT_POINTER_KIND) {
            handle_pointer(&event);
            return PXA_EVENT_HANDLED;
        }
        if (ui_event.kind == PXA_UI_EVENT_CONTROLLER_STATE_KIND) {
            handle_controller(&event);
            return PXA_EVENT_HANDLED;
        }
        return PXA_EVENT_UNHANDLED;
    }
    if (event.service == PXA_SERVICE_UI &&
        event.opcode == PXA_UI_ENVIRONMENT_CHANGED) {
        pxa_ui_environment_t environment;
        if (pxa_ui_parse_environment_event(&event, &environment) &&
            environment.width > 0 && environment.height > 0) {
            const int resized = environment.width != g_display_width ||
                                environment.height != g_display_height;
            g_display_width = environment.width;
            g_display_height = environment.height;
            g_safe_top = (int)environment.safe_insets[0];
            g_safe_right = (int)environment.safe_insets[1];
            g_safe_bottom = (int)environment.safe_insets[2];
            g_safe_left = (int)environment.safe_insets[3];
            g_display_shape = environment.display_shape;
            for (int index = 0; index < 4; ++index)
                g_corner_radii[index] = environment.corner_radii[index];
            if (resized && g_have_context) {
                (void)pxa_close_handle(g_context);
                g_have_context = 0;
                g_context = 0;
                g_create_attempt = 0;
                g_want_scale = preferred_render_scale();
                request_context();
            } else {
                rebuild_layout();
                render_frame();
            }
        }
        return PXA_EVENT_HANDLED;
    }
    if (pxa_clock_tick_timestamp_us(&event, &timestamp_us)) {
        handle_tick(timestamp_us);
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    pd_audio_stop(&g_audio);
    save_progress();
    (void)pxa_clock_set_period(0);
    if (g_context != 0) (void)pxa_close_handle(g_context);
    g_context = 0;
    g_have_context = 0;
}
