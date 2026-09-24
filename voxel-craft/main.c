/* Voxel Craft: a first-person voxel sandbox for PXA.
 *
 * The world is a 64 x 24 x 64 block grid generated from value noise. The scene
 * uses a capability-gated GameRender context during play and keeps the
 * GuestMapped pixel renderer as its complex-UI fallback.
 * The Host fuses nearest upscale, rotation and panel byte order conversion. Touch
 * controls: left half is a movement stick, right half looks around. The MINE
 * button holds a mining action with per-block progress and break particles,
 * ATTACK swings at slimes, PLACE builds with the hotbar selection, and JUMP
 * doubles as fly-up; double-tap toggles fly mode. A small PCM synth plays
 * mining, break, place, attack, hit and jump effects.
 */
#include "pxa.h"
#include "pxa_game_render.h"
#include "pxa_log.h"
#include "pxa_raster.h"
#include "pxa_storage.h"
#include "pxa_surface.h"
#include "pxa_ui.h"

#include "game.h"
#include "quality_controller.h"
#include "rc_math.h"
#include "render.h"
#include "sfx.h"
#include "surface_ownership.h"
#include "voxel_raster.h"

#define FRAME_NODE UINT32_C(2)
#define FRAME_PERIOD_MS 33u
#define CLOCK_POLL_PERIOD_MS 16u
#define VOXEL_HOST_RASTER_DEFAULT 1u
#define MAX_CATCHUP_STEPS 3u
#define WINDOW_SNAPSHOT_REQUEST UINT32_C(3)
#define SURFACE_CREATE_REQUEST UINT32_C(4)
#define SURFACE_CONFIGURE_REQUEST UINT32_C(5)
/* Frame-rate window the horizon controller aims for, in frames per second
 * times ten: pull the horizon in below 17 fps, let it back out at 19 and
 * above. Shrinking takes two blocks at a time and growing one, and the
 * controller acts once per completed frame-rate window, so a scene that turns
 * expensive recovers quickly while the horizon creeps back out without
 * hunting. Near geometry dominates the fill in dense scenes, so the upper edge
 * must stay reachable or the horizon would sit at its floor for nothing. */
#define VOXEL_VIEW_SHRINK_FPS_X10 UINT32_C(170)
#define VOXEL_VIEW_GROW_FPS_X10 UINT32_C(190)
#define VOXEL_VIEW_SHRINK_BLOCKS 2.0F
#define VOXEL_VIEW_GROW_BLOCKS 1.0F

#define PERF_CLOCK_FRAME_START UINT32_C(0x70)
#define PERF_CLOCK_UPDATE_END UINT32_C(0x71)
#define PERF_CLOCK_RAYCAST_START UINT32_C(0x72)
#define PERF_CLOCK_RAYCAST_END UINT32_C(0x73)
#define PERF_CLOCK_FRAME_END UINT32_C(0x74)
#define PERF_CLOCK_PROJECT_END UINT32_C(0x75)
#define PERF_CLOCK_SORT_END UINT32_C(0x76)
#define PERF_CLOCK_BUILD_END UINT32_C(0x77)
#define PERF_CLOCK_SUBMIT_END UINT32_C(0x78)
#define PXA_WINDOW_GET_SNAPSHOT_OP 2u
#define PXA_WINDOW_METRICS_CHANGED_OP 0x8001u
#define WINDOW_SNAPSHOT_PERIOD_TICKS 250u
#define QUALITY_SAMPLE_CAP_US UINT64_C(250000)
#define PERF_LOG_INTERVAL_US UINT64_C(2000000)
#define TICK_SECONDS 0.033F
#define LOOK_PER_PIXEL 0.0062F
#define STICK_RADIUS 40.0F
#define DOUBLE_TAP_US UINT64_C(320000)
#define MINE_SOUND_PERIOD 0.22F
#define PAD_TURN_RATE 2.2F
#define PAD_PITCH_RATE 1.6F
#define SURFACE_BUFFER_COUNT 3u
#define SURFACE_RETRY_TICKS 31u
#define FRAME_PIXELS_MAX RENDER_SCENE_PIXELS_MAX
#define SCREEN_MENU 0u
#define SCREEN_SETTINGS 1u
#define SCREEN_PLAY 2u
#define SCREEN_PAUSE 3u
#define UI_RENDER_QUALITY QUALITY_BALANCED
#define SURFACE_MODE_MAPPED 0u
#define SURFACE_MODE_RASTER 1u
/* The Guest CPU renderer draws into the fixed RGB565 frame buffer; the Host
 * raster path can render at the real display resolution instead. */
#define VOXEL_MAPPED_PIXEL_BUDGET 115000
enum {
    BTN_NONE = 0,
    BTN_JUMP,
    BTN_ACTION,
    BTN_PLACE,
    BTN_DOWN,
    BTN_FLY,
    BTN_HOTBAR,
};

typedef struct {
    uint8_t active;
    uint8_t id;
    int16_t origin_x;
    int16_t origin_y;
    int16_t x;
    int16_t y;
    int16_t travel;
    uint64_t down_us;
} finger_t;

typedef struct {
    uint64_t frame_start_us;
    uint64_t update_end_us;
    uint64_t raycast_start_us;
    uint64_t raycast_end_us;
    uint64_t project_end_us;
    uint64_t sort_end_us;
    uint64_t build_end_us;
    uint64_t submit_end_us;
    uint64_t frame_end_us;
    uint8_t active;
} perf_timing_sample_t;

static uint16_t g_surface_buffers[SURFACE_BUFFER_COUNT * FRAME_PIXELS_MAX]
    __attribute__((aligned(PXA_SURFACE_BUFFER_ALIGNMENT)));
static uint8_t g_packet[128];
static uint32_t g_ui_generation;
static uint32_t g_surface_handle;
static uint16_t g_surface_width;
static uint16_t g_surface_height;
static uint64_t g_frame_id;
static uint8_t g_surface_create_pending;
static uint8_t g_surface_start_pending;
static uint8_t g_surface_retry_ticks;
static uint8_t g_surface_mode;
static uint8_t g_surface_request_mode;
static uint8_t g_raster_supported;
static uint8_t g_raster_ready;
static voxel_surface_ownership_t g_surface_ownership;
static uint8_t g_input_initialized;
static uint8_t g_input_dirty;
static uint8_t g_backgrounded;
static player_t g_player;
static voxel_sfx_t g_sfx;
static uint64_t g_last_tick_us;
static uint64_t g_tick_accumulator_us;
static uint32_t g_now_ms;
static uint32_t g_fps_x10;
static uint64_t g_fps_window_start_us;
/* The frame-rate window the horizon controller last acted on. */
static uint64_t g_view_decision_window_us;
static uint32_t g_fps_window_frames;
static uint8_t g_hotbar_selected;
static char g_toast[24];
static uint32_t g_toast_until_ms;

static finger_t g_move_finger;
static finger_t g_look_finger;
static finger_t g_button_finger;
static uint8_t g_button_kind;
static int16_t g_stick_dx;
static int16_t g_stick_dy;
static uint8_t g_jump_held;
static uint8_t g_down_held;
static uint8_t g_action_held;
static float g_attack_cooldown;
static float g_mine_progress;
static float g_mine_sound_timer;
static ray_hit_t g_mine_target;
static float g_swing_timer;
static uint64_t g_last_jump_tap_us;
static perf_timing_sample_t g_perf_timing;
static uint8_t g_perf_sample_counter;
static uint64_t g_perf_log_last_us;
static voxel_raster_stats_t g_perf_raster_stats;
static uint8_t g_perf_raster_stats_valid;
static uint32_t g_update_ema_us;
static uint32_t g_update_max_us;
static voxel_quality_controller_t g_quality_controller;
static uint32_t g_render_total_ema_us;
static uint32_t g_render_total_max_us;
static uint64_t g_buffer_wait_started_us;
static uint32_t g_buffer_wait_ema_us;
static uint32_t g_buffer_wait_max_us;
static uint32_t g_host_raster_ema_us;
static uint32_t g_host_raster_max_us;
static uint32_t g_host_queue_ema_us;
static uint32_t g_host_present_ema_us;
static uint64_t g_host_queue_total_us;
static uint64_t g_host_present_total_us;
static uint64_t g_host_rendered_frames;
static uint64_t g_host_visible_frames;
static uint32_t g_snapshot_ticks;
static uint8_t g_bootstrap_requests_pending;
static uint8_t g_present_failures;
static uint8_t g_quality_manual;
static uint8_t g_game_quality = QUALITY_MIN;
static uint8_t g_screen = SCREEN_MENU;
#if VOXEL_AUTOPLAY
/* Debug/measurement builds start a game once the Surface exists. */
static uint8_t g_autoplay_pending;
#endif
static uint8_t g_settings_return = SCREEN_MENU;
static uint8_t g_inventory_open;
static uint8_t g_craft_table;
static item_stack_t g_cursor;
static int16_t g_pointer_x;
static int16_t g_pointer_y;
static uint8_t g_inv_press_active;
static int16_t g_inv_press_x;
static int16_t g_inv_press_y;
static int16_t g_inv_press_travel;
static uint64_t g_inv_press_us;
static uint32_t g_pad_buttons;
static uint32_t g_pad_previous;
static uint8_t g_pad_seen;
static uint8_t g_pad_jump;
static uint8_t g_pad_mine;
static float g_pad_move_z;
static float g_pad_turn;
static float g_pad_pitch;
static uint64_t g_pad_last_a_us;

static void recreate_surface(void);
static void save_preferences(void);
static void try_finish_surface_recreate(void);
static void update_fps(uint64_t timestamp_us);
static void update_duration_stats(uint64_t duration_us, uint32_t *ema_us,
                                  uint32_t *maximum_us);

static int next_lower_quality(int quality) {
    return quality <= QUALITY_MIN ? QUALITY_MIN
           : quality <= QUALITY_BALANCED ? QUALITY_MIN
                                         : QUALITY_BALANCED;
}

static int next_higher_quality(int quality) {
    return quality < QUALITY_BALANCED ? QUALITY_BALANCED
                                     : QUALITY_PERFORMANCE;
}

static int hit_circle(int x, int y, int cx, int cy, int radius) {
    const int dx = x - cx;
    const int dy = y - cy;
    const int reach = radius + 6;
    return dx * dx + dy * dy <= reach * reach;
}

static void set_toast(const char *text) {
    int index = 0;
    while (text[index] != '\0' && index < (int)sizeof(g_toast) - 1) {
        g_toast[index] = text[index];
        ++index;
    }
    g_toast[index] = '\0';
    g_toast_until_ms = g_now_ms + 1200u;
}

static char *put_u32_text(char *out, uint32_t value) {
    char reversed[10];
    int digits = 0;
    do {
        reversed[digits++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    while (digits != 0) {
        *out++ = reversed[--digits];
    }
    return out;
}

static char *put_perf_metric(char *out, const char *label, uint32_t value) {
    while (*label != '\0') *out++ = *label++;
    return put_u32_text(out, value);
}

static void toast_view(void) {
    char text[24];
    char *out = text;
    *out++ = 'V';
    *out++ = 'I';
    *out++ = 'E';
    *out++ = 'W';
    *out++ = ' ';
    out = put_u32_text(out, (uint32_t)g_layout.view_w);
    *out++ = 'X';
    out = put_u32_text(out, (uint32_t)g_layout.view_h);
    *out = '\0';
    set_toast(text);
}

static void toast_quality_mode(void) {
    char text[24];
    char *out = text;
    static const char prefix[] = "QUALITY ";
    int index;
    for (index = 0; index < (int)sizeof(prefix) - 1; ++index) {
        *out++ = prefix[index];
    }
    if (g_quality_manual == 0) {
        *out++ = 'A';
        *out++ = 'U';
        *out++ = 'T';
        *out++ = 'O';
    } else {
        *out++ = (char)('0' + g_game_quality);
        *out++ = 'X';
    }
    *out = '\0';
    set_toast(text);
}

/* The next Surface uses the Host raster path for gameplay, menus, the
 * settings screens and the inventory; the Guest CPU renderer remains only for
 * Hosts without GameRender support. */
static uint8_t desired_surface_mode(void) {
    return g_raster_supported ? SURFACE_MODE_RASTER : SURFACE_MODE_MAPPED;
}

/* Menus, settings and the inventory render at the real display resolution
 * (1x); gameplay uses the selected or automatic detail level; the CPU path
 * keeps its fixed 320x240 budget. */
static int desired_render_quality(void) {
    if (desired_surface_mode() == SURFACE_MODE_RASTER) {
        return (g_screen == SCREEN_PLAY && !g_inventory_open)
                   ? (int)g_game_quality
                   : (int)QUALITY_MIN;
    }
    /* CPU fallback: gameplay keeps the selected detail, menus use the UI
     * detail (both are clamped by the mapped limits). */
    if (g_screen == SCREEN_PLAY && !g_inventory_open) {
        return (int)g_game_quality;
    }
    return UI_RENDER_QUALITY;
}

/* The scene size limit follows the surface path that the next frame uses.
 * The Host raster path renders at the real display resolution, so quality 1x
 * means one scene pixel per panel pixel; the Guest CPU renderer keeps its
 * static 320x240 tables and the fixed RGB565 frame buffer. */
static void update_scene_limits(void) {
    if (desired_surface_mode() == SURFACE_MODE_RASTER) {
        render_set_scene_limits(g_layout.view_w, g_layout.view_h, 0);
    } else {
        render_set_scene_limits(RENDER_SCENE_MAX_W, RENDER_SCENE_MAX_H,
                                VOXEL_MAPPED_PIXEL_BUDGET);
    }
}

static void apply_quality(void) {
    if (g_quality_manual != 0) {
        g_game_quality = g_quality_manual;
    }
    render_set_quality(desired_render_quality());
}

static void cycle_quality(void) {
    if (g_quality_manual == 0) {
        g_quality_manual = 1;
    } else if (g_quality_manual == QUALITY_MIN) {
        g_quality_manual = QUALITY_BALANCED;
    } else if (g_quality_manual == QUALITY_BALANCED) {
        g_quality_manual = QUALITY_PERFORMANCE;
    } else {
        g_quality_manual = 0;
    }
    g_game_quality = g_quality_manual != 0 ? g_quality_manual
                                          : QUALITY_BALANCED;
    if (g_quality_manual == 0) {
        voxel_quality_controller_reset(&g_quality_controller);
    }
    if (g_screen == SCREEN_PLAY) {
        render_set_quality(g_game_quality);
        recreate_surface();
    }
    save_preferences();
    toast_quality_mode();
}

static void toast_quality(void) {
    char text[24];
    char *out = text;
    *out++ = 'R';
    *out++ = 'E';
    *out++ = 'S';
    *out++ = ' ';
    out = put_u32_text(out, (uint32_t)render_scene_width());
    *out++ = 'X';
    out = put_u32_text(out, (uint32_t)render_scene_height());
    *out = '\0';
    set_toast(text);
}

static void update_quality(uint64_t duration_us) {
    const int quality = render_quality();
    uint64_t effective_render_us = duration_us;
    uint32_t consumer_wait_us = g_buffer_wait_ema_us;
    voxel_quality_action_t action;
    if (g_surface_mode == SURFACE_MODE_RASTER && g_raster_ready) {
        pxa_raster_telemetry_t telemetry;
        if (pxa_raster_query_telemetry(g_surface_handle, &telemetry) ==
            (int32_t)PXA_RASTER_TELEMETRY_BYTES) {
            if (telemetry.last_host_raster_us != 0)
                update_duration_stats(telemetry.last_host_raster_us,
                                      &g_host_raster_ema_us,
                                      &g_host_raster_max_us);
            if (telemetry.rendered_frames > g_host_rendered_frames) {
                const uint64_t frames = telemetry.rendered_frames -
                                        g_host_rendered_frames;
                const uint64_t queue_delta =
                    telemetry.queue_wait_us >= g_host_queue_total_us
                        ? telemetry.queue_wait_us - g_host_queue_total_us
                        : 0;
                const uint64_t queue_average = queue_delta / frames;
                uint32_t queue_sample = queue_average > UINT32_MAX
                                            ? UINT32_MAX
                                            : (uint32_t)queue_average;
                g_host_queue_ema_us = g_host_queue_ema_us == 0
                                          ? queue_sample
                                          : (g_host_queue_ema_us * 7u +
                                             queue_sample) /
                                                8u;
            }
            if (telemetry.visible_frames > g_host_visible_frames) {
                const uint64_t frames = telemetry.visible_frames -
                                        g_host_visible_frames;
                const uint64_t present_delta =
                    telemetry.present_us >= g_host_present_total_us
                        ? telemetry.present_us - g_host_present_total_us
                        : 0;
                const uint64_t present_average = present_delta / frames;
                uint32_t present_sample = present_average > UINT32_MAX
                                              ? UINT32_MAX
                                              : (uint32_t)present_average;
                g_host_present_ema_us = g_host_present_ema_us == 0
                                            ? present_sample
                                            : (g_host_present_ema_us * 7u +
                                               present_sample) /
                                                  8u;
            }
            g_host_queue_total_us = telemetry.queue_wait_us;
            g_host_present_total_us = telemetry.present_us;
            g_host_rendered_frames = telemetry.rendered_frames;
            g_host_visible_frames = telemetry.visible_frames;
        }
        if (g_host_raster_ema_us > effective_render_us)
            effective_render_us = g_host_raster_ema_us;
        if (g_host_queue_ema_us > consumer_wait_us)
            consumer_wait_us = g_host_queue_ema_us;
    }
    /* Only gameplay feeds the automatic controllers: menu frames render at a
     * different resolution and would otherwise skew the sampled cost. */
    if (g_screen != SCREEN_PLAY || g_inventory_open) return;
    /* The horizon is the cheapest quality dial there is: unlike the render
     * scale it needs no Surface recreate, so it can track the measured frame
     * cost while the player is moving. One build then holds the same frame
     * rate on an 800x480 RISC-V panel and a 296x240 Xtensa one. It runs even
     * in manual resolution mode, which pins pixels rather than draw distance. */
    if (g_fps_x10 != 0 &&
        g_fps_window_start_us != g_view_decision_window_us) {
        const float distance = voxel_raster_view_distance();
        g_view_decision_window_us = g_fps_window_start_us;
        if (g_fps_x10 < VOXEL_VIEW_SHRINK_FPS_X10) {
            voxel_raster_set_view_distance(distance -
                                           VOXEL_VIEW_SHRINK_BLOCKS);
        } else if (g_fps_x10 >= VOXEL_VIEW_GROW_FPS_X10 &&
                   consumer_wait_us <
                       VOXEL_QUALITY_BUFFER_WAIT_BLOCK_UPGRADE_US) {
            voxel_raster_set_view_distance(distance +
                                           VOXEL_VIEW_GROW_BLOCKS);
        }
    }
    if (g_quality_manual != 0) return;
    action = voxel_quality_observe(
        &g_quality_controller, effective_render_us, consumer_wait_us,
        (uint8_t)quality, (uint8_t)render_min_quality(), QUALITY_MAX);
    if (action == VOXEL_QUALITY_ACTION_LOWER_DETAIL) {
        g_game_quality = (uint8_t)next_higher_quality(quality);
    } else if (action == VOXEL_QUALITY_ACTION_HIGHER_DETAIL) {
        g_game_quality = (uint8_t)next_lower_quality(quality);
    } else {
        return;
    }
    if (g_surface_mode == SURFACE_MODE_RASTER) {
        /* Recreating the Surface mid-play flashes the display. Record the
         * target detail and let the next Surface create (pause, inventory or
         * a resize) apply it. */
        return;
    }
    render_set_quality(g_game_quality);
    recreate_surface();
    toast_quality();
}

static void update_duration_stats(uint64_t duration_us, uint32_t *ema_us,
                                  uint32_t *maximum_us) {
    uint32_t sample;
    if (duration_us > QUALITY_SAMPLE_CAP_US) {
        duration_us = QUALITY_SAMPLE_CAP_US;
    }
    sample = (uint32_t)duration_us;
    if (*ema_us == 0) {
        *ema_us = sample;
    } else {
        *ema_us = (*ema_us * 7u + sample) / 8u;
    }
    if (sample > *maximum_us) *maximum_us = sample;
}

static int mark_perf_timing(uint32_t request_id) {
    if (!g_perf_timing.active) return 0;
    if (pxa_clock_now(request_id)) return 1;
    g_perf_timing.active = 0;
    return 0;
}

static void mark_raster_phase(uint8_t phase) {
    static const uint32_t ids[] = {PERF_CLOCK_PROJECT_END,
                                   PERF_CLOCK_SORT_END,
                                   PERF_CLOCK_BUILD_END,
                                   PERF_CLOCK_SUBMIT_END};
    if (phase < sizeof(ids) / sizeof(ids[0])) (void)mark_perf_timing(ids[phase]);
}

static int perf_timing_surface_ready(void) {
    const uint8_t all_buffers = (uint8_t)((1u << SURFACE_BUFFER_COUNT) - 1u);
    return g_surface_handle != 0 && !g_surface_create_pending &&
           !g_surface_ownership.recreate_pending &&
           g_surface_ownership.writing_buffer == VOXEL_SURFACE_BUFFER_NONE &&
           g_surface_ownership.host_owned_mask != all_buffers &&
           g_surface_width == (uint16_t)render_scene_width() &&
           g_surface_height == (uint16_t)render_scene_height();
}

static void maybe_begin_perf_timing(void) {
    if (g_perf_timing.active || !perf_timing_surface_ready()) return;
    if (++g_perf_sample_counter < VOXEL_QUALITY_SAMPLE_INTERVAL_FRAMES) return;
    g_perf_sample_counter = 0;
    g_perf_timing = (perf_timing_sample_t){.active = 1};
    g_perf_raster_stats_valid = 0;
    (void)mark_perf_timing(PERF_CLOCK_FRAME_START);
}

static void log_perf_sample(uint64_t timestamp_us, uint64_t guest_render_us) {
    char line[PXA_LOG_MAX_MESSAGE_BYTES + 1u];
    char *out;
    if (g_perf_log_last_us != 0 &&
        timestamp_us - g_perf_log_last_us < PERF_LOG_INTERVAL_US) return;
    g_perf_log_last_us = timestamp_us;

    out = line;
    out = put_perf_metric(out, "VOXEL PERF fps10=", g_fps_x10);
    out = put_perf_metric(out, " mode=", g_surface_mode);
    out = put_perf_metric(out, " quality=", (uint32_t)render_quality());
    out = put_perf_metric(out, " width=", g_surface_width);
    out = put_perf_metric(out, " height=", g_surface_height);
    out = put_perf_metric(out, " update_ema_us=", g_update_ema_us);
    out = put_perf_metric(out, " guest_sample_us=",
                          (uint32_t)guest_render_us);
    out = put_perf_metric(out, " project_us=",
                          (uint32_t)(g_perf_timing.project_end_us >
                                             g_perf_timing.raycast_start_us
                                         ? g_perf_timing.project_end_us -
                                               g_perf_timing.raycast_start_us
                                         : 0));
    out = put_perf_metric(out, " sort_us=",
                          (uint32_t)(g_perf_timing.sort_end_us >
                                             g_perf_timing.project_end_us
                                         ? g_perf_timing.sort_end_us -
                                               g_perf_timing.project_end_us
                                         : 0));
    out = put_perf_metric(out, " build_us=",
                          (uint32_t)(g_perf_timing.build_end_us >
                                             g_perf_timing.sort_end_us
                                         ? g_perf_timing.build_end_us -
                                               g_perf_timing.sort_end_us
                                         : 0));
    out = put_perf_metric(out, " submit_us=",
                          (uint32_t)(g_perf_timing.submit_end_us >
                                             g_perf_timing.build_end_us
                                         ? g_perf_timing.submit_end_us -
                                               g_perf_timing.build_end_us
                                         : 0));
    out = put_perf_metric(out, " total_ema_us=", g_render_total_ema_us);
    out = put_perf_metric(out, " wait_ema_us=", g_buffer_wait_ema_us);
    *out = '\0';
    (void)pxa_log_info(line);

    if (g_surface_mode != SURFACE_MODE_RASTER ||
        !g_perf_raster_stats_valid) return;
    out = line;
    out = put_perf_metric(out, "VOXEL RASTER host_ema_us=",
                          g_host_raster_ema_us);
    out = put_perf_metric(out, " queue_ema_us=", g_host_queue_ema_us);
    out = put_perf_metric(out, " present_ema_us=", g_host_present_ema_us);
    out = put_perf_metric(out, " cached=", g_perf_raster_stats.cached_quads);
    out = put_perf_metric(out, " rebuilt=", g_perf_raster_stats.rebuilt_chunks);
    out = put_perf_metric(out, " mesh_passes=",
                          g_perf_raster_stats.mesh_build_passes);
    out = put_perf_metric(out, " pending=", game_pending_chunk_count());
    out = put_perf_metric(out, " candidates=",
                          g_perf_raster_stats.candidate_quads);
    out = put_perf_metric(out, " submitted=",
                          g_perf_raster_stats.submitted_quads);
    out = put_perf_metric(out, " painter=",
                          g_perf_raster_stats.painter_quads);
    out = put_perf_metric(out, " depth=",
                          g_perf_raster_stats.depth_quads);
    out = put_perf_metric(out, " split=",
                          g_perf_raster_stats.painter_splits);
    out = put_perf_metric(out, " dropped=",
                          g_perf_raster_stats.dropped_quads);
    out = put_perf_metric(out, " list_bytes=",
                          g_perf_raster_stats.draw_list_bytes);
    /* The Host's own telemetry already reports covered pixels and the command
     * mix. Keeping them here too pushed this line past
     * PXA_LOG_MAX_MESSAGE_BYTES, which dropped the whole record. */
    *out = '\0';
    (void)pxa_log_info(line);

    out = line;
    out = put_perf_metric(out, "VOXEL RANGE fog_q8=",
                          g_perf_raster_stats.fog_end_q8);
    out = put_perf_metric(out, " grid=", GRID_COUNT);
    out = put_perf_metric(out, " visible=",
                          g_perf_raster_stats.visible_chunks);
    out = put_perf_metric(out, " unloaded=",
                          g_perf_raster_stats.unloaded_chunks);
    out = put_perf_metric(out, " mesh_pending=",
                          g_perf_raster_stats.pending_mesh_chunks);
    out = put_perf_metric(out, " mesh_input=",
                          g_perf_raster_stats.mesh_input_quads);
    out = put_perf_metric(out, " mesh_visited=",
                          g_perf_raster_stats.mesh_visited_quads);
    out = put_perf_metric(out, " candidate_limit=",
                          g_perf_raster_stats.candidate_limit);
    out = put_perf_metric(out, " chunk_quota=",
                          g_perf_raster_stats.chunk_base_quota);
    out = put_perf_metric(out, " commands=",
                          g_perf_raster_stats.draw_commands);
    *out = '\0';
    (void)pxa_log_info(line);

    out = line;
    out = put_perf_metric(out, "VOXEL DROP mesh_overflow=",
                          g_perf_raster_stats.mesh_overflow_quads);
    out = put_perf_metric(out, " candidate_budget=",
                          g_perf_raster_stats.candidate_budget_omitted);
    out = put_perf_metric(out, " projection_budget=",
                          g_perf_raster_stats.projection_budget_dropped);
    out = put_perf_metric(out, " command_budget=",
                          g_perf_raster_stats.command_budget_dropped);
    out = put_perf_metric(out, " degenerate=",
                          g_perf_raster_stats.degenerate_dropped);
    out = put_perf_metric(out, " append_fail=",
                          g_perf_raster_stats.append_failures);
    *out = '\0';
    (void)pxa_log_info(line);

    out = line;
    out = put_perf_metric(out, "VOXEL CULL backface=",
                          g_perf_raster_stats.backface_culled);
    out = put_perf_metric(out, " frustum=",
                          g_perf_raster_stats.frustum_culled);
    out = put_perf_metric(out, " clipped=",
                          g_perf_raster_stats.clipped_quads);
    out = put_perf_metric(out, " sort_edges=",
                          g_perf_raster_stats.sort_edges);
    out = put_perf_metric(out, " edge_overflow=",
                          g_perf_raster_stats.sort_edge_overflow);
    out = put_perf_metric(out, " sort_cycles=",
                          g_perf_raster_stats.sort_cycles);
    *out = '\0';
    (void)pxa_log_info(line);
}

static int handle_perf_clock_event(const pxa_event_t *event) {
    int32_t status;
    uint64_t timestamp_us;
    if (event->service != PXA_SERVICE_CLOCK ||
        event->opcode != PXA_CLOCK_NOW_RESULT ||
        event->request_id < PERF_CLOCK_FRAME_START ||
        event->request_id > PERF_CLOCK_SUBMIT_END) {
        return 0;
    }
    if (!pxa_clock_parse_now(event, &status, &timestamp_us) ||
        status != PXA_STATUS_OK || !g_perf_timing.active) {
        if (event->request_id == PERF_CLOCK_FRAME_END) {
            g_perf_timing.active = 0;
        }
        return 1;
    }
    switch (event->request_id) {
        case PERF_CLOCK_FRAME_START:
            g_perf_timing.frame_start_us = timestamp_us;
            break;
        case PERF_CLOCK_UPDATE_END:
            g_perf_timing.update_end_us = timestamp_us;
            break;
        case PERF_CLOCK_RAYCAST_START:
            g_perf_timing.raycast_start_us = timestamp_us;
            break;
        case PERF_CLOCK_RAYCAST_END:
            g_perf_timing.raycast_end_us = timestamp_us;
            break;
        case PERF_CLOCK_PROJECT_END:
            g_perf_timing.project_end_us = timestamp_us;
            break;
        case PERF_CLOCK_SORT_END:
            g_perf_timing.sort_end_us = timestamp_us;
            break;
        case PERF_CLOCK_BUILD_END:
            g_perf_timing.build_end_us = timestamp_us;
            break;
        case PERF_CLOCK_SUBMIT_END:
            g_perf_timing.submit_end_us = timestamp_us;
            break;
        case PERF_CLOCK_FRAME_END:
            g_perf_timing.frame_end_us = timestamp_us;
            if (g_perf_timing.frame_start_us <=
                    g_perf_timing.update_end_us &&
                g_perf_timing.update_end_us <=
                    g_perf_timing.raycast_start_us &&
                g_perf_timing.raycast_start_us <
                    g_perf_timing.raycast_end_us &&
                g_perf_timing.raycast_end_us <=
                    g_perf_timing.frame_end_us) {
                const uint64_t raycast_us =
                    g_perf_timing.raycast_end_us -
                    g_perf_timing.raycast_start_us;
                update_duration_stats(
                    g_perf_timing.update_end_us -
                        g_perf_timing.frame_start_us,
                    &g_update_ema_us, &g_update_max_us);
                update_duration_stats(
                    g_perf_timing.frame_end_us -
                        g_perf_timing.frame_start_us,
                    &g_render_total_ema_us, &g_render_total_max_us);
                update_quality(raycast_us);
                log_perf_sample(timestamp_us, raycast_us);
            }
            g_perf_timing.active = 0;
            break;
        default:
            break;
    }
    return 1;
}

static int render_frame(void);

static int initialize_input_surface(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t generation = g_ui_generation + 1u;
    if (g_input_initialized) {
        return 1;
    }
    if (!pxa_ui_transaction_begin(&transaction, generation,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  g_packet, sizeof(g_packet)) ||
        !pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) ||
        !pxa_ui_create(&transaction, FRAME_NODE, 1, 0,
                       PXA_UI_NODE_CANVAS) ||
        !pxa_ui_set_event_mask(&transaction, 1,
                               PXA_UI_EVENT_MASK_CONTROLLER_STATE) ||
        !pxa_ui_set_length(&transaction, FRAME_NODE, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(&transaction, FRAME_NODE, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_event_mask(&transaction, FRAME_NODE,
                               PXA_UI_EVENT_MASK_POINTER) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) {
            (void)pxa_ui_transaction_cancel(&transaction);
        }
        return 0;
    }
    g_input_initialized = 1;
    g_ui_generation = generation;
    return 1;
}

/* The UI node tree binds to the component's primary Surface. Recreating that
 * Surface invalidates the node tree, so rebuild it before the next frame;
 * the tree only carries the pointer event mask and presents no content. */
static void rebind_ui_surface(void) {
    g_input_initialized = 0;
    if (!initialize_input_surface()) {
        (void)pxa_log_error("UI surface rebind failed");
    }
}

static int request_surface_create(void) {
    /* GuestMapped frames use the current internal quality resolution. The
     * Host performs nearest upscale and panel rotation in one native pass. */
    if (g_surface_create_pending || g_surface_handle != 0 ||
        g_layout.view_w <= 0 || g_layout.view_h <= 0) {
        return 0;
    }
    g_surface_request_mode = desired_surface_mode();
    update_scene_limits();
    /* Re-apply the quality for the new path: the mapped limits can clamp a
     * high raster quality while the inventory is open. */
    render_set_quality(desired_render_quality());
    g_surface_width = (uint16_t)render_scene_width();
    g_surface_height = (uint16_t)render_scene_height();
    if (!(g_surface_request_mode == SURFACE_MODE_RASTER
              ? pxa_game_render_create(
                    SURFACE_CREATE_REQUEST, g_surface_width,
                    g_surface_height, SURFACE_BUFFER_COUNT, 1, g_packet,
                    sizeof(g_packet))
              : pxa_surface_create_rgb565_mapped(
                    SURFACE_CREATE_REQUEST, g_surface_width,
                    g_surface_height, SURFACE_BUFFER_COUNT, 1, g_packet,
                    sizeof(g_packet)))) {
        g_surface_start_pending = 1;
        g_surface_retry_ticks = SURFACE_RETRY_TICKS;
        return 0;
    }
    g_surface_start_pending = 0;
    g_surface_create_pending = 1;
    return 1;
}

static void schedule_surface_retry(void) {
    g_surface_start_pending = 1;
    g_surface_retry_ticks = SURFACE_RETRY_TICKS;
}

static void retry_surface_on_tick(void) {
    if (!g_surface_start_pending) return;
    if (g_surface_retry_ticks != 0) {
        --g_surface_retry_ticks;
        return;
    }
    (void)request_surface_create();
}

static void reset_surface_ownership(void) {
    voxel_surface_ownership_reset(&g_surface_ownership);
}

static void try_finish_surface_recreate(void) {
    if (!voxel_surface_can_recreate(&g_surface_ownership)) return;
    if (g_surface_handle != 0) {
        (void)pxa_close_handle(g_surface_handle);
        g_surface_handle = 0;
        g_surface_width = 0;
        g_surface_height = 0;
        g_raster_ready = 0;
        voxel_raster_set_capabilities(0);
        reset_surface_ownership();
    }
    if (!g_surface_create_pending) {
        g_surface_ownership.recreate_pending = 0;
        (void)request_surface_create();
    }
}

static void recreate_surface(void) {
    voxel_surface_request_recreate(&g_surface_ownership);
    try_finish_surface_recreate();
}

typedef struct {
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
} window_insets_t;

static window_insets_t g_safe_insets;
static window_insets_t g_system_bar_insets;

/* The window snapshot carries the authoritative logical display size and the
 * physical safe area plus the system chrome/gesture reserves. Both are used
 * for the metrics-changed event and as a periodic fallback, so a resize or a
 * chrome change is picked up even if the UI environment event is missed. */
static int parse_window_snapshot(const uint8_t *payload, uint32_t length,
                                 uint32_t *out_width, uint32_t *out_height,
                                 window_insets_t *out_safe,
                                 window_insets_t *out_bars) {
    uint32_t offset = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    window_insets_t safe = {0, 0, 0, 0};
    window_insets_t bars = {0, 0, 0, 0};
    while (offset + 4u <= length) {
        const uint16_t tag = pxa_read_u16(payload + offset);
        const uint16_t size = pxa_read_u16(payload + offset + 2u);
        offset += 4u;
        if (size > length - offset) {
            return 0;
        }
        if (tag == 2u && size == 8u) {
            width = pxa_read_u32(payload + offset);
            height = pxa_read_u32(payload + offset + 4u);
        } else if (tag == 5u && size == 16u) {
            safe.left = pxa_read_u32(payload + offset);
            safe.top = pxa_read_u32(payload + offset + 4u);
            safe.right = pxa_read_u32(payload + offset + 8u);
            safe.bottom = pxa_read_u32(payload + offset + 12u);
        } else if (tag == 6u && size == 16u) {
            bars.left = pxa_read_u32(payload + offset);
            bars.top = pxa_read_u32(payload + offset + 4u);
            bars.right = pxa_read_u32(payload + offset + 8u);
            bars.bottom = pxa_read_u32(payload + offset + 12u);
        }
        offset += size;
    }
    if (width == 0 || height == 0) {
        return 0;
    }
    *out_width = width;
    *out_height = height;
    if (out_safe != NULL) *out_safe = safe;
    if (out_bars != NULL) *out_bars = bars;
    return 1;
}

static uint32_t inset_max(uint32_t left, uint32_t right) {
    return left > right ? left : right;
}

static void apply_screen_size(int width, int height) {
    const int size_changed =
        width != g_layout.screen_w || height != g_layout.screen_h;
    /* Interactive controls stay inside the union of the physical safe area
     * and the system chrome/gesture reserves. */
    const int insets_changed = render_set_safe_insets(
        (int)inset_max(g_safe_insets.left, g_system_bar_insets.left),
        (int)inset_max(g_safe_insets.top, g_system_bar_insets.top),
        (int)inset_max(g_safe_insets.right, g_system_bar_insets.right),
        (int)inset_max(g_safe_insets.bottom, g_system_bar_insets.bottom));
    if (!size_changed && !insets_changed) {
        return;
    }
    render_configure(width, height);
    update_scene_limits();
    apply_quality();
    if (size_changed) {
        recreate_surface();
    }
}

static void request_window_snapshot(void) {
    (void)pxa_send(PXA_SERVICE_WINDOW, PXA_WINDOW_GET_SNAPSHOT_OP,
                   WINDOW_SNAPSHOT_REQUEST, (const uint8_t *)0, 0);
}

/* --- menu, settings and save data --------------------------------------- */

#define STORAGE_META_KEY "vx.meta"
#define STORAGE_META_KEY_LEN 7
#define STORAGE_META_GET_REQUEST UINT32_C(0x51)
#define STORAGE_META_SET_REQUEST UINT32_C(0x52)
#define STORAGE_CHUNK_GET_REQUEST UINT32_C(0x53)
#define STORAGE_CHUNK_SET_REQUEST UINT32_C(0x54)
#define STORAGE_REMOVE_REQUEST UINT32_C(0x55)
#define STORAGE_PREFS_GET_REQUEST UINT32_C(0x56)
#define STORAGE_PREFS_SET_REQUEST UINT32_C(0x57)
#define STORAGE_PREFS_KEY "vx.prefs"
#define STORAGE_PREFS_KEY_LEN 8
#define SEED_CLOCK_REQUEST UINT32_C(0x60)
#define SAVE_CHUNK_BYTES 2048
#define SAVE_MAX_CHUNKS 12
#define SAVE_HEADER_BYTES 28
#define SAVE_EDIT_BYTES 11
#define SAVE_BLOB_BYTES (SAVE_HEADER_BYTES + MAX_EDITS * SAVE_EDIT_BYTES)

static void release_finger(finger_t *finger);

static uint8_t g_has_save;
static uint8_t g_game_started;
static uint8_t g_save_active;
static uint8_t g_load_active;
static int g_save_next;
static int g_save_chunks;
static int g_save_length;
static int g_load_next;
static int g_load_chunks;
static uint8_t g_save_blob[SAVE_BLOB_BYTES];
static uint8_t g_storage_payload[2304];
static char g_menu_toast[24];
static uint32_t g_menu_toast_until;
static uint32_t g_clock_seed;
static uint32_t g_seed_counter;
static uint8_t g_show_performance = 1;
static uint8_t g_menu_press_active;
static int16_t g_menu_press_x;
static int16_t g_menu_press_y;
static int16_t g_menu_press_travel;
static uint64_t g_menu_press_us;

static void set_screen(uint8_t screen) {
    if (g_screen == SCREEN_PLAY) {
        g_game_quality = (uint8_t)render_quality();
    }
    g_screen = screen;
    /* Menu screens render at 1x, gameplay at the selected detail, so pause
     * and resume rebuild the Surface; moving between menus does not. */
    if (desired_render_quality() != render_quality() ||
        (g_surface_handle != 0 && g_surface_mode != desired_surface_mode())) {
        recreate_surface();
    }
}

/* Settings auto-save: byte 0 is the performance overlay, byte 1 the manual
 * detail level (0 = AUTO). */
static void save_preferences(void) {
    const uint8_t value[2] = {g_show_performance ? 1u : 0u, g_quality_manual};
    (void)pxa_storage_set(STORAGE_PREFS_SET_REQUEST, STORAGE_PREFS_KEY,
                          STORAGE_PREFS_KEY_LEN, value, sizeof(value),
                          g_storage_payload, sizeof(g_storage_payload),
                          g_packet, sizeof(g_packet));
}

static void menu_toast(const char *text) {
    int index = 0;
    while (text[index] != '\0' && index < (int)sizeof(g_menu_toast) - 1) {
        g_menu_toast[index] = text[index];
        ++index;
    }
    g_menu_toast[index] = '\0';
    g_menu_toast_until = g_now_ms + 2500u;
}

static void storage_chunk_key(char *key, int index) {
    key[0] = 'v';
    key[1] = 'x';
    key[2] = '.';
    key[3] = 'c';
    if (index < 10) {
        key[4] = (char)('0' + index);
    } else {
        key[4] = (char)('a' + (index - 10));
    }
    key[5] = '\0';
}

static void reset_runtime_state(void) {
    g_mine_progress = 0.0F;
    g_mine_sound_timer = 0.0F;
    g_mine_target.hit = 0;
    g_action_held = 0;
    g_attack_cooldown = 0.0F;
    g_swing_timer = 0.0F;
    g_stick_dx = 0;
    g_stick_dy = 0;
    release_finger(&g_move_finger);
    release_finger(&g_look_finger);
    release_finger(&g_button_finger);
    g_button_kind = BTN_NONE;
    g_inventory_open = 0;
    g_craft_table = 0;
    g_cursor.item = BLOCK_AIR;
    g_cursor.count = 0;
    g_inv_press_active = 0;
    g_last_jump_tap_us = 0;
    g_jump_held = 0;
    g_down_held = 0;
    g_toast[0] = '\0';
    g_toast_until_ms = 0;
}

#ifndef VOXEL_FIXED_SEED
/* Measurement builds pin the world seed so frame timing is comparable between
 * runs; release builds derive it from the clock. */
#define VOXEL_FIXED_SEED 0
#endif

static void start_new_game(void) {
    const uint32_t base = g_clock_seed != 0 ? g_clock_seed : 0x5eed1234u;
    const uint32_t seed = VOXEL_FIXED_SEED != 0
                              ? (uint32_t)VOXEL_FIXED_SEED +
                                    g_seed_counter * 2654435761u
                              : base + g_seed_counter * 2654435761u;
    ++g_seed_counter;
    game_generate(seed);
    game_inventory_init();
    game_spawn(&g_player);
    game_spawn_mobs(seed ^ 0xabcd1234u, &g_player);
    reset_runtime_state();
    g_game_started = 1;
    set_screen(SCREEN_PLAY);
    (void)render_frame();
}

static void save_write_next_chunk(void) {
    char key[8];
    int offset;
    int length;
    if (!g_save_active) {
        return;
    }
    if (g_save_next >= g_save_chunks) {
        g_save_active = 0;
        g_has_save = 1;
        menu_toast("SAVED");
        return;
    }
    storage_chunk_key(key, g_save_next);
    offset = g_save_next * SAVE_CHUNK_BYTES;
    length = g_save_length - offset;
    if (length > SAVE_CHUNK_BYTES) {
        length = SAVE_CHUNK_BYTES;
    }
    if (!pxa_storage_set(STORAGE_CHUNK_SET_REQUEST, key, 5,
                         g_save_blob + offset, (size_t)length,
                         g_storage_payload, sizeof(g_storage_payload),
                         g_packet, sizeof(g_packet))) {
        menu_toast("SAVE FAILED");
        g_save_active = 0;
        return;
    }
    ++g_save_next;
}

static void save_start(void) {
    uint8_t meta[8];
    if (g_save_active || g_load_active || !g_game_started) {
        if (!g_game_started) {
            menu_toast("NO GAME TO SAVE");
        }
        return;
    }
    g_save_length =
        game_serialize(&g_player, g_save_blob, (int)sizeof(g_save_blob));
    if (g_save_length <= 0) {
        menu_toast("SAVE FAILED");
        return;
    }
    g_save_chunks = (g_save_length + SAVE_CHUNK_BYTES - 1) / SAVE_CHUNK_BYTES;
    if (g_save_chunks > SAVE_MAX_CHUNKS) {
        menu_toast("SAVE TOO BIG");
        return;
    }
    g_save_active = 1;
    g_save_next = 0;
    meta[0] = (uint8_t)g_save_length;
    meta[1] = (uint8_t)((uint32_t)g_save_length >> 8);
    meta[2] = (uint8_t)((uint32_t)g_save_length >> 16);
    meta[3] = (uint8_t)((uint32_t)g_save_length >> 24);
    meta[4] = (uint8_t)g_save_chunks;
    meta[5] = (uint8_t)((uint32_t)g_save_chunks >> 8);
    meta[6] = 0;
    meta[7] = 0;
    if (!pxa_storage_set(STORAGE_META_SET_REQUEST, STORAGE_META_KEY,
                         STORAGE_META_KEY_LEN, meta, sizeof(meta),
                         g_storage_payload, sizeof(g_storage_payload),
                         g_packet, sizeof(g_packet))) {
        menu_toast("SAVE FAILED");
        g_save_active = 0;
        return;
    }
    menu_toast("SAVING...");
}

static void load_request_chunk(void) {
    char key[8];
    if (g_load_next >= g_load_chunks) {
        if (game_deserialize(g_save_blob, g_save_length, &g_player)) {
            game_spawn_mobs(game_seed() ^ 0xabcd1234u, &g_player);
            reset_runtime_state();
            g_game_started = 1;
            set_screen(SCREEN_PLAY);
            menu_toast("LOADED");
            (void)render_frame();
        } else {
            menu_toast("LOAD FAILED");
        }
        g_load_active = 0;
        return;
    }
    storage_chunk_key(key, g_load_next);
    if (!pxa_storage_get(STORAGE_CHUNK_GET_REQUEST, key, 5,
                         g_storage_payload, sizeof(g_storage_payload),
                         g_packet, sizeof(g_packet))) {
        menu_toast("LOAD FAILED");
        g_load_active = 0;
    }
}

static void load_start(void) {
    if (g_save_active || g_load_active) {
        return;
    }
    g_load_active = 1;
    menu_toast("LOADING...");
    if (!pxa_storage_get(STORAGE_META_GET_REQUEST, STORAGE_META_KEY,
                         STORAGE_META_KEY_LEN, g_storage_payload,
                         sizeof(g_storage_payload), g_packet,
                         sizeof(g_packet))) {
        menu_toast("LOAD FAILED");
        g_load_active = 0;
    }
}

static void delete_save(void) {
    if (!pxa_storage_remove(STORAGE_REMOVE_REQUEST, STORAGE_META_KEY,
                            STORAGE_META_KEY_LEN, g_storage_payload,
                            sizeof(g_storage_payload), g_packet,
                            sizeof(g_packet))) {
        menu_toast("DELETE FAILED");
    }
}

static void menu_activate(int index) {
    if (g_screen == SCREEN_MENU) {
        if (index == 0) {
            start_new_game();
        } else if (index == 1 && g_has_save) {
            load_start();
        } else if (index == 2) {
            g_settings_return = SCREEN_MENU;
            set_screen(SCREEN_SETTINGS);
            (void)render_frame();
        }
        return;
    }
    if (g_screen == SCREEN_PAUSE) {
        if (index == 0) {
            set_screen(SCREEN_PLAY);
            (void)render_frame();
        } else if (index == 1) {
            save_start();
            (void)render_frame();
        } else if (index == 2) {
            g_settings_return = SCREEN_PAUSE;
            set_screen(SCREEN_SETTINGS);
            (void)render_frame();
        } else if (index == 3) {
            save_start();
            set_screen(SCREEN_MENU);
            (void)render_frame();
        }
        return;
    }
    if (g_screen == SCREEN_SETTINGS) {
        if (index == 0) {
            cycle_quality();
            (void)render_frame();
        } else if (index == 1) {
            g_show_performance = g_show_performance ? 0 : 1;
            save_preferences();
            menu_toast(g_show_performance ? "PERFORMANCE ON"
                                          : "PERFORMANCE OFF");
            (void)render_frame();
        } else if (index == 2) {
            save_start();
            (void)render_frame();
        } else if (index == 3 && g_has_save) {
            delete_save();
        } else if (index == 4) {
            set_screen(g_settings_return);
            (void)render_frame();
        }
    }
}

static void on_menu_tap(int x, int y) {
    int index;
    for (index = 0; index < g_menu_button_count; ++index) {
        const menu_button_t *button = &g_menu_buttons[index];
        if (!button->enabled) {
            continue;
        }
        if (x >= button->x && x < button->x + button->w && y >= button->y &&
            y < button->y + button->h) {
            menu_activate(index);
            return;
        }
    }
}

static void handle_storage_event(const pxa_event_t *event) {
    if (event->opcode == PXA_STORAGE_GET &&
        event->request_id == STORAGE_META_GET_REQUEST) {
        pxa_storage_get_result_t result;
        if (pxa_storage_parse_get(event, &result) &&
            result.status == PXA_STATUS_OK && result.value_length == 8) {
            g_save_length = (int)pxa_read_u32(result.value);
            g_load_chunks = (int)pxa_read_u32(result.value + 4);
            g_has_save = 1;
            if (g_load_active) {
                if (g_save_length <= 0 ||
                    g_save_length > (int)sizeof(g_save_blob) ||
                    g_load_chunks <= 0 || g_load_chunks > SAVE_MAX_CHUNKS) {
                    menu_toast("SAVE CORRUPT");
                    g_load_active = 0;
                } else {
                    g_load_next = 0;
                    load_request_chunk();
                }
            }
        } else {
            g_has_save = 0;
            if (g_load_active) {
                menu_toast("NO SAVE");
                g_load_active = 0;
            }
        }
        return;
    }
    if (event->opcode == PXA_STORAGE_GET &&
        event->request_id == STORAGE_CHUNK_GET_REQUEST) {
        pxa_storage_get_result_t result;
        if (pxa_storage_parse_get(event, &result) &&
            result.status == PXA_STATUS_OK && g_load_active &&
            g_load_next < g_load_chunks) {
            const int offset = g_load_next * SAVE_CHUNK_BYTES;
            int length = result.value_length;
            int index;
            if (offset + length > (int)sizeof(g_save_blob)) {
                length = (int)sizeof(g_save_blob) - offset;
            }
            for (index = 0; index < length; ++index) {
                g_save_blob[offset + index] = result.value[index];
            }
            ++g_load_next;
            load_request_chunk();
        } else {
            menu_toast("LOAD FAILED");
            g_load_active = 0;
        }
        return;
    }
    if (event->opcode == PXA_STORAGE_SET &&
        event->request_id == STORAGE_META_SET_REQUEST) {
        int32_t status;
        if (pxa_storage_parse_status(event, PXA_STORAGE_SET, &status) &&
            status == PXA_STATUS_OK) {
            g_save_next = 0;
            save_write_next_chunk();
        } else {
            menu_toast("SAVE FAILED");
            g_save_active = 0;
        }
        return;
    }
    if (event->opcode == PXA_STORAGE_SET &&
        event->request_id == STORAGE_CHUNK_SET_REQUEST) {
        int32_t status;
        if (pxa_storage_parse_status(event, PXA_STORAGE_SET, &status) &&
            status == PXA_STATUS_OK) {
            save_write_next_chunk();
        } else {
            menu_toast("SAVE FAILED");
            g_save_active = 0;
        }
        return;
    }
    if (event->opcode == PXA_STORAGE_REMOVE &&
        event->request_id == STORAGE_REMOVE_REQUEST) {
        g_has_save = 0;
        menu_toast("SAVE DELETED");
        return;
    }
    if (event->opcode == PXA_STORAGE_GET &&
        event->request_id == STORAGE_PREFS_GET_REQUEST) {
        pxa_storage_get_result_t result;
        if (pxa_storage_parse_get(event, &result) &&
            result.status == PXA_STATUS_OK && result.value_length >= 1) {
            g_show_performance = result.value[0] & 1u;
            if (result.value_length >= 2) {
                g_quality_manual = result.value[1];
                apply_quality();
            }
            (void)render_frame();
        }
    }
}

static void begin_button(const pxa_ui_pointer_data_t *pointer, uint8_t kind) {
    if (g_button_finger.active) {
        return;
    }
    g_button_finger.active = 1;
    g_button_finger.id = pointer->pointer_id;
    g_button_finger.travel = 0;
    g_button_finger.down_us = pointer->timestamp_us;
    g_button_kind = kind;
}

static void begin_finger(finger_t *finger,
                         const pxa_ui_pointer_data_t *pointer) {
    finger->active = 1;
    finger->id = pointer->pointer_id;
    finger->origin_x = (int16_t)pointer->x;
    finger->origin_y = (int16_t)pointer->y;
    finger->x = (int16_t)pointer->x;
    finger->y = (int16_t)pointer->y;
    finger->travel = 0;
    finger->down_us = pointer->timestamp_us;
}

static void toggle_fly(void) {
    g_player.flying = g_player.flying ? 0 : 1;
    g_player.vy = 0.0F;
    set_toast(g_player.flying ? "FLY MODE ON" : "FLY MODE OFF");
}

static void reset_mining(void) {
    g_mine_progress = 0.0F;
    g_mine_sound_timer = 0.0F;
    g_mine_target.hit = 0;
}

static void open_craft_table(void);

static void open_craft_table(void);
static void release_finger(finger_t *finger);

static int do_attack(void) {
    ray_hit_t target;
    if (game_target_block(&g_player, &target) && target.hit &&
        game_block(target.x, target.y, target.z) == BLOCK_TABLE) {
        open_craft_table();
        return 1;
    }
    voxel_sfx_play(&g_sfx, VOXEL_SFX_ATTACK);
    g_swing_timer = 0.25F;
    if (game_attack(&g_player, g_inventory[g_hotbar_selected].item)) {
        voxel_sfx_play(&g_sfx, VOXEL_SFX_HIT);
        set_toast("HIT");
        return 1;
    }
    return 0;
}

static void do_place(void) {
    item_stack_t *slot = &g_inventory[g_hotbar_selected];
    if (slot->item == BLOCK_AIR || slot->count == 0) {
        set_toast("NO BLOCKS");
        return;
    }
    if (!game_item_is_block(slot->item)) {
        set_toast("NOT A BLOCK");
        return;
    }
    if (game_place_block(&g_player, slot->item)) {
        const int placed = slot->item;
        (void)game_inventory_remove(g_hotbar_selected, 1);
        voxel_sfx_play(&g_sfx, VOXEL_SFX_PLACE);
        set_toast("PLACED");
        if (placed == BLOCK_TABLE) {
            open_craft_table();
        }
    }
}

static item_stack_t *inventory_slot_ptr(int hit) {
    const int index = render_inventory_slot(hit);
    if (index < 0) {
        return (item_stack_t *)0;
    }
    if (hit >= INV_HIT_CRAFT && hit < INV_HIT_RESULT) {
        return g_craft_table ? &g_table_craft[index] : &g_craft[index];
    }
    return &g_inventory[index];
}

static void inventory_update_craft(void) {
    if (g_craft_table) {
        game_table_craft_update();
    } else {
        game_craft_update();
    }
}

static int inventory_take_result(void) {
    uint8_t item;
    uint8_t count;
    if (g_craft_table) {
        if (!game_table_craft_take(&item, &count)) {
            return 0;
        }
    } else {
        if (!game_craft_take(&item, &count)) {
            return 0;
        }
    }
    if (g_cursor.item == BLOCK_AIR) {
        g_cursor.item = item;
        g_cursor.count = count;
    } else if (g_cursor.item == item &&
               (int)g_cursor.count + count <= ITEM_MAX_STACK) {
        g_cursor.count = (uint8_t)(g_cursor.count + count);
    } else {
        (void)game_inventory_add(item, count);
    }
    voxel_sfx_play(&g_sfx, VOXEL_SFX_PLACE);
    return 1;
}

static void inventory_return_items(void) {
    int index;
    if (g_cursor.item != BLOCK_AIR && g_cursor.count != 0) {
        (void)game_inventory_add(g_cursor.item, g_cursor.count);
    }
    g_cursor.item = BLOCK_AIR;
    g_cursor.count = 0;
    for (index = 0; index < CRAFT_SLOTS; ++index) {
        if (g_craft[index].item != BLOCK_AIR && g_craft[index].count != 0) {
            (void)game_inventory_add(g_craft[index].item,
                                     g_craft[index].count);
        }
        g_craft[index].item = BLOCK_AIR;
        g_craft[index].count = 0;
    }
    for (index = 0; index < TABLE_CRAFT_SLOTS; ++index) {
        if (g_table_craft[index].item != BLOCK_AIR &&
            g_table_craft[index].count != 0) {
            (void)game_inventory_add(g_table_craft[index].item,
                                     g_table_craft[index].count);
        }
        g_table_craft[index].item = BLOCK_AIR;
        g_table_craft[index].count = 0;
    }
    game_craft_update();
    game_table_craft_update();
}

static void close_inventory(void) {
    inventory_return_items();
    g_inventory_open = 0;
    recreate_surface();
}

static void open_inventory(void) {
    g_inventory_open = 1;
    g_craft_table = 0;
    release_finger(&g_move_finger);
    release_finger(&g_look_finger);
    g_stick_dx = 0;
    g_stick_dy = 0;
    g_cursor.item = BLOCK_AIR;
    g_cursor.count = 0;
    render_inventory_set_mode(0);
    game_craft_update();
    recreate_surface();
}

static void open_craft_table(void) {
    g_inventory_open = 1;
    g_craft_table = 1;
    release_finger(&g_move_finger);
    release_finger(&g_look_finger);
    g_stick_dx = 0;
    g_stick_dy = 0;
    g_cursor.item = BLOCK_AIR;
    g_cursor.count = 0;
    render_inventory_set_mode(1);
    game_table_craft_update();
    set_toast("CRAFTING TABLE");
    recreate_surface();
}

/* Long press: take half a stack, or drop a single item when one is held. */
static void inventory_split(int x, int y) {
    const int hit = render_inventory_hit(x, y);
    item_stack_t *slot;
    if (hit == INV_HIT_NONE) {
        return;
    }
    if (hit == INV_HIT_CLOSE) {
        close_inventory();
        return;
    }
    if (hit == INV_HIT_RESULT) {
        inventory_take_result();
        return;
    }
    slot = inventory_slot_ptr(hit);
    if (slot == (item_stack_t *)0) {
        return;
    }
    if (g_cursor.item == BLOCK_AIR) {
        if (slot->item == BLOCK_AIR) {
            return;
        }
        if (slot->count > 1) {
            const int half = (slot->count + 1) / 2;
            g_cursor.item = slot->item;
            g_cursor.count = (uint8_t)half;
            slot->count = (uint8_t)(slot->count - half);
        } else {
            g_cursor = *slot;
            slot->item = BLOCK_AIR;
            slot->count = 0;
        }
    } else if (slot->item == BLOCK_AIR) {
        slot->item = g_cursor.item;
        slot->count = 1;
        if (--g_cursor.count == 0) {
            g_cursor.item = BLOCK_AIR;
        }
    } else if (slot->item == g_cursor.item &&
               slot->count < ITEM_MAX_STACK) {
        ++slot->count;
        if (--g_cursor.count == 0) {
            g_cursor.item = BLOCK_AIR;
        }
    } else {
        return;
    }
    if (hit >= INV_HIT_CRAFT && hit < INV_HIT_RESULT) {
        inventory_update_craft();
    }
}

static void inventory_tap(int x, int y) {
    const int hit = render_inventory_hit(x, y);
    item_stack_t *slot;
    if (hit == INV_HIT_NONE) {
        return;
    }
    if (hit == INV_HIT_CLOSE) {
        close_inventory();
        return;
    }
    if (hit == INV_HIT_RESULT) {
        inventory_take_result();
        return;
    }
    slot = inventory_slot_ptr(hit);
    if (slot == (item_stack_t *)0) {
        return;
    }
    if (g_cursor.item == BLOCK_AIR) {
        if (slot->item != BLOCK_AIR) {
            g_cursor = *slot;
            slot->item = BLOCK_AIR;
            slot->count = 0;
        }
    } else if (slot->item == BLOCK_AIR) {
        *slot = g_cursor;
        g_cursor.item = BLOCK_AIR;
        g_cursor.count = 0;
    } else if (slot->item == g_cursor.item) {
        const int room = ITEM_MAX_STACK - (int)slot->count;
        const int take =
            (int)g_cursor.count < room ? g_cursor.count : room;
        slot->count = (uint8_t)(slot->count + take);
        g_cursor.count = (uint8_t)(g_cursor.count - take);
        if (g_cursor.count == 0) {
            g_cursor.item = BLOCK_AIR;
        }
    } else {
        const item_stack_t swap = *slot;
        *slot = g_cursor;
        g_cursor = swap;
    }
    if (hit >= INV_HIT_CRAFT && hit < INV_HIT_RESULT) {
        inventory_update_craft();
    }
}

static int jump_active(void) {
    return g_jump_held != 0 || g_pad_jump != 0;
}

static int action_active(void) {
    return g_action_held != 0 || (g_pad_mine != 0 && !g_player.flying);
}

static int descend_active(void) {
    return g_down_held != 0 || (g_pad_mine != 0 && g_player.flying);
}

/* Simulator pad mapping: arrows = d-pad (up/down move, left/right turn),
 * Z = A (jump, double tap toggles fly), X = B (mine; descend while flying),
 * Enter = Start (place), Tab = Select (attack). */
static void on_controller_state(const pxa_ui_controller_data_t *state) {
    const uint32_t pressed = state->buttons & ~g_pad_previous;
    const uint32_t released_buttons = g_pad_previous & ~state->buttons;
    g_pad_previous = state->buttons;
    if (!state->connected) {
        g_pad_buttons = 0;
        g_pad_jump = 0;
        g_pad_mine = 0;
        g_pad_move_z = 0.0F;
        g_pad_turn = 0.0F;
        return;
    }
    g_pad_buttons = state->buttons;
    if (!g_pad_seen) {
        g_pad_seen = 1;
        set_toast("GAMEPAD READY");
    }
    if ((pressed & PXA_CONTROLLER_A) != 0) {
        if (g_pad_last_a_us != 0 &&
            state->timestamp_us - g_pad_last_a_us < DOUBLE_TAP_US) {
            toggle_fly();
            g_pad_last_a_us = 0;
        } else {
            g_pad_last_a_us = state->timestamp_us;
        }
    }
    if ((pressed & PXA_CONTROLLER_START) != 0) {
        do_place();
    }
    if ((pressed & PXA_CONTROLLER_SELECT) != 0) {
        if (!do_attack()) {
            /* Nothing to attack: toggle mining on the next tick. */
            g_action_held = 1;
        }
    }
    if ((released_buttons & PXA_CONTROLLER_SELECT) != 0) {
        g_action_held = 0;
        reset_mining();
    }
    g_pad_move_z = ((state->buttons & PXA_CONTROLLER_UP) != 0 ? 1.0F : 0.0F) -
                   ((state->buttons & PXA_CONTROLLER_DOWN) != 0 ? 1.0F : 0.0F);
    g_pad_turn = ((state->buttons & PXA_CONTROLLER_RIGHT) != 0 ? 1.0F : 0.0F) -
                 ((state->buttons & PXA_CONTROLLER_LEFT) != 0 ? 1.0F : 0.0F);
    g_pad_jump = (state->buttons & PXA_CONTROLLER_A) != 0;
    g_pad_mine = (state->buttons & PXA_CONTROLLER_B) != 0;
}

static void on_pointer_down(const pxa_ui_pointer_data_t *pointer) {
    const int x = (int)pointer->x;
    const int y = (int)pointer->y;
    g_pointer_x = (int16_t)x;
    g_pointer_y = (int16_t)y;
    if (g_screen != SCREEN_PLAY) {
        g_menu_press_active = 1;
        g_menu_press_x = (int16_t)x;
        g_menu_press_y = (int16_t)y;
        g_menu_press_travel = 0;
        g_menu_press_us = pointer->timestamp_us;
        return;
    }
    if (g_inventory_open) {
        g_inv_press_active = 1;
        g_inv_press_x = (int16_t)x;
        g_inv_press_y = (int16_t)y;
        g_inv_press_travel = 0;
        g_inv_press_us = pointer->timestamp_us;
        return;
    }
    if (!g_button_finger.active) {
        if (hit_circle(x, y, g_layout.menu_x, g_layout.menu_y,
                       g_layout.menu_r)) {
            reset_runtime_state();
            set_screen(SCREEN_PAUSE);
            (void)render_frame();
            return;
        }
        if (hit_circle(x, y, g_layout.bag_x, g_layout.bag_y,
                       g_layout.bag_r)) {
            open_inventory();
            return;
        }
        /* The quality box has no visible target in any renderer, so tapping
         * the top-left by accident used to change the detail level. Quality is
         * changed in the settings screen instead. */
        if (hit_circle(x, y, g_layout.fly_x, g_layout.fly_y, g_layout.fly_r)) {
            toggle_fly();
            begin_button(pointer, BTN_FLY);
            return;
        }
        if (hit_circle(x, y, g_layout.jump_x, g_layout.jump_y,
                       g_layout.jump_r)) {
            const uint64_t now = pointer->timestamp_us;
            if (g_last_jump_tap_us != 0 &&
                now - g_last_jump_tap_us < DOUBLE_TAP_US) {
                toggle_fly();
                g_last_jump_tap_us = 0;
            } else {
                g_last_jump_tap_us = now;
            }
            g_jump_held = 1;
            begin_button(pointer, BTN_JUMP);
            return;
        }
        if (hit_circle(x, y, g_layout.action_x, g_layout.action_y,
                       g_layout.action_r)) {
            ray_hit_t target;
            if (game_target_block(&g_player, &target) && target.hit &&
                game_block(target.x, target.y, target.z) == BLOCK_TABLE) {
                open_craft_table();
                return;
            }
            g_action_held = 1;
            reset_mining();
            begin_button(pointer, BTN_ACTION);
            return;
        }
        if (hit_circle(x, y, g_layout.place_x, g_layout.place_y,
                       g_layout.place_r)) {
            do_place();
            begin_button(pointer, BTN_PLACE);
            return;
        }
        if (g_player.flying &&
            hit_circle(x, y, g_layout.down_x, g_layout.down_y,
                       g_layout.down_r)) {
            g_down_held = 1;
            begin_button(pointer, BTN_DOWN);
            return;
        }
        if (y >= g_layout.hotbar_y &&
            y < g_layout.hotbar_y + g_layout.hotbar_slot &&
            x >= g_layout.hotbar_x &&
            x < g_layout.hotbar_x +
                    HOTBAR_SLOTS * g_layout.hotbar_slot) {
            const int slot =
                (x - g_layout.hotbar_x) / g_layout.hotbar_slot;
            g_hotbar_selected = (uint8_t)slot;
            set_toast(g_inventory[slot].item == BLOCK_AIR
                          ? "EMPTY"
                          : game_item_name(g_inventory[slot].item));
            begin_button(pointer, BTN_HOTBAR);
            return;
        }
    }
    if (x < g_layout.view_x + g_layout.view_w / 2) {
        if (!g_move_finger.active) {
            begin_finger(&g_move_finger, pointer);
        }
    } else if (!g_look_finger.active) {
        begin_finger(&g_look_finger, pointer);
    }
}

static int abs_int(int value) { return value < 0 ? -value : value; }

static void on_pointer_move(const pxa_ui_pointer_data_t *pointer) {
    const int x = (int)pointer->x;
    const int y = (int)pointer->y;
    const int stick_radius = (int)(STICK_RADIUS * g_layout.ui_scale);
    g_pointer_x = (int16_t)x;
    g_pointer_y = (int16_t)y;
    if (g_screen != SCREEN_PLAY) {
        if (g_menu_press_active) {
            g_menu_press_travel = (int16_t)(g_menu_press_travel +
                                            abs_int(x - g_menu_press_x) +
                                            abs_int(y - g_menu_press_y));
            g_menu_press_x = (int16_t)x;
            g_menu_press_y = (int16_t)y;
        }
        return;
    }
    if (g_inventory_open) {
        if (g_inv_press_active) {
            g_inv_press_travel = (int16_t)(g_inv_press_travel +
                                           abs_int(x - g_inv_press_x) +
                                           abs_int(y - g_inv_press_y));
            g_inv_press_x = (int16_t)x;
            g_inv_press_y = (int16_t)y;
        }
        return;
    }
    if (g_move_finger.active && g_move_finger.id == pointer->pointer_id) {
        int dx = x - g_move_finger.origin_x;
        int dy = y - g_move_finger.origin_y;
        g_move_finger.x = (int16_t)x;
        g_move_finger.y = (int16_t)y;
        if (dx > stick_radius) {
            dx = stick_radius;
        } else if (dx < -stick_radius) {
            dx = -stick_radius;
        }
        if (dy > stick_radius) {
            dy = stick_radius;
        } else if (dy < -stick_radius) {
            dy = -stick_radius;
        }
        g_stick_dx = (int16_t)dx;
        g_stick_dy = (int16_t)dy;
    } else if (g_look_finger.active &&
               g_look_finger.id == pointer->pointer_id) {
        const int dx = x - g_look_finger.x;
        const int dy = y - g_look_finger.y;
        g_look_finger.x = (int16_t)x;
        g_look_finger.y = (int16_t)y;
        g_look_finger.travel = (int16_t)(g_look_finger.travel + abs_int(dx) +
                                         abs_int(dy));
        const float look_per_pixel = LOOK_PER_PIXEL / g_layout.ui_scale;
        g_player.yaw += (float)dx * look_per_pixel;
        if (g_player.yaw > RC_PI) {
            g_player.yaw -= RC_TWO_PI;
        } else if (g_player.yaw < -RC_PI) {
            g_player.yaw += RC_TWO_PI;
        }
        g_player.pitch = rc_clampf(
            g_player.pitch - (float)dy * look_per_pixel, -1.55F, 1.55F);
    } else if (g_button_finger.active &&
               g_button_finger.id == pointer->pointer_id) {
        g_button_finger.travel = (int16_t)(g_button_finger.travel +
                                           abs_int(x - g_button_finger.x) +
                                           abs_int(y - g_button_finger.y));
        g_button_finger.x = (int16_t)x;
        g_button_finger.y = (int16_t)y;
    }
}

static void release_finger(finger_t *finger) {
    finger->active = 0;
    finger->id = 0;
}

static void on_pointer_up(const pxa_ui_pointer_data_t *pointer,
                          int cancelled) {
    if (g_screen != SCREEN_PLAY) {
        if (g_menu_press_active) {
            if (!cancelled &&
                g_menu_press_travel < 14 * g_layout.ui_scale) {
                on_menu_tap((int)pointer->x, (int)pointer->y);
            }
            g_menu_press_active = 0;
        }
        return;
    }
    if (g_inventory_open) {
        /* A button that opened this screen (for example PLACE on a crafting
         * table) is still held: release it here or it would stay stuck and
         * block every later button press. */
        if (g_move_finger.active &&
            g_move_finger.id == pointer->pointer_id) {
            release_finger(&g_move_finger);
            g_stick_dx = 0;
            g_stick_dy = 0;
        }
        if (g_look_finger.active &&
            g_look_finger.id == pointer->pointer_id) {
            release_finger(&g_look_finger);
        }
        if (g_button_finger.active &&
            g_button_finger.id == pointer->pointer_id) {
            if (g_button_kind == BTN_JUMP) {
                g_jump_held = 0;
            } else if (g_button_kind == BTN_DOWN) {
                g_down_held = 0;
            } else if (g_button_kind == BTN_ACTION) {
                g_action_held = 0;
                reset_mining();
            }
            release_finger(&g_button_finger);
            g_button_kind = BTN_NONE;
        }
        if (g_inv_press_active) {
            const uint64_t held = pointer->timestamp_us - g_inv_press_us;
            if (!cancelled &&
                g_inv_press_travel < 14 * g_layout.ui_scale) {
                if (held < UINT64_C(350000)) {
                    inventory_tap((int)pointer->x, (int)pointer->y);
                } else {
                    inventory_split((int)pointer->x, (int)pointer->y);
                }
            }
            g_inv_press_active = 0;
        }
        return;
    }
    if (g_move_finger.active && g_move_finger.id == pointer->pointer_id) {
        release_finger(&g_move_finger);
        g_stick_dx = 0;
        g_stick_dy = 0;
        return;
    }
    if (g_look_finger.active && g_look_finger.id == pointer->pointer_id) {
        (void)cancelled;
        release_finger(&g_look_finger);
        return;
    }
    if (g_button_finger.active &&
        g_button_finger.id == pointer->pointer_id) {
        if (g_button_kind == BTN_JUMP) {
            g_jump_held = 0;
        } else if (g_button_kind == BTN_DOWN) {
            g_down_held = 0;
        } else if (g_button_kind == BTN_ACTION) {
            g_action_held = 0;
            reset_mining();
        }
        release_finger(&g_button_finger);
        g_button_kind = BTN_NONE;
    }
}

static void update_mining(float dt) {
    ray_hit_t hit;
    float hardness;
    int block;
    if (!action_active()) {
        return;
    }
    if (!game_target_block(&g_player, &hit) || !hit.hit) {
        reset_mining();
        return;
    }
    block = game_block(hit.x, hit.y, hit.z);
    hardness = game_block_hardness(block);
    if (hardness <= 0.0F) {
        reset_mining();
        return;
    }
    if (!g_mine_target.hit || g_mine_target.x != hit.x ||
        g_mine_target.y != hit.y || g_mine_target.z != hit.z) {
        g_mine_target = hit;
        g_mine_progress = 0.0F;
    }
    g_mine_progress += dt * game_mining_speed(
                                 g_inventory[g_hotbar_selected].item, block) /
                       hardness;
    g_mine_sound_timer -= dt;
    if (g_mine_sound_timer <= 0.0F) {
        voxel_sfx_play(&g_sfx, VOXEL_SFX_MINE);
        g_mine_sound_timer = MINE_SOUND_PERIOD;
    }
    if (g_mine_progress >= 1.0F) {
        game_set_block(hit.x, hit.y, hit.z, BLOCK_AIR);
        (void)game_inventory_add(block, 1);
        game_spawn_particles((float)hit.x + 0.5F, (float)hit.y + 0.5F,
                             (float)hit.z + 0.5F, block, 12);
        voxel_sfx_play(&g_sfx, VOXEL_SFX_BREAK);
        set_toast("MINED");
        reset_mining();
    }
}

static void build_hud(hud_state_t *hud) {
    render_perf_stats_t perf;
    render_get_perf_stats(&perf);
    hud->now_ms = g_now_ms;
    hud->fps_x10 = g_fps_x10;
    hud->guest_update_us_div_100 = (uint16_t)(
        g_update_ema_us / 100u > UINT16_MAX ? UINT16_MAX :
                                              g_update_ema_us / 100u);
    hud->guest_render_us_div_100 = (uint16_t)(
        g_quality_controller.raycast_ema_us / 100u > UINT16_MAX
            ? UINT16_MAX
            : g_quality_controller.raycast_ema_us / 100u);
    hud->guest_total_us_div_100 = (uint16_t)(
        g_render_total_ema_us / 100u > UINT16_MAX ? UINT16_MAX :
                                                   g_render_total_ema_us / 100u);
    hud->buffer_wait_us_div_100 = (uint16_t)(
        g_buffer_wait_ema_us / 100u > UINT16_MAX ? UINT16_MAX :
                                                   g_buffer_wait_ema_us / 100u);
    hud->dda_steps_x10 = perf.rays == 0 ? 0 :
        (uint16_t)((perf.total_steps * 10u) / perf.rays);
    hud->dda_steps_max = perf.max_steps;
    hud->fog_terminated_percent = perf.rays == 0 ? 0 :
        (uint8_t)((perf.fog_terminated_rays * 100u) / perf.rays);
    hud->solid_hit_percent = perf.rays == 0 ? 0 :
        (uint8_t)((perf.solid_hit_rays * 100u) / perf.rays);
    hud->pos_x = rc_floor_int(g_player.x);
    hud->pos_z = rc_floor_int(g_player.z);
    hud->layout = g_layout;
    hud->hotbar_selected = g_hotbar_selected;
    {
        int slot;
        for (slot = 0; slot < HOTBAR_SLOTS; ++slot) {
            hud->hotbar_items[slot] = g_inventory[slot].item;
            hud->hotbar_counts[slot] = g_inventory[slot].count;
        }
    }
    hud->flying = g_player.flying;
    hud->move_active = g_move_finger.active;
    hud->move_origin_x = g_move_finger.origin_x;
    hud->move_origin_y = g_move_finger.origin_y;
    hud->move_dx = g_stick_dx;
    hud->move_dy = g_stick_dy;
    hud->jump_held = jump_active();
    hud->down_held = g_down_held;
    hud->action_held = action_active();
    hud->show_performance = g_show_performance;
    hud->quality = (uint8_t)render_quality();
    hud->quality_manual = g_quality_manual;
    hud->inventory_open = g_inventory_open;
    hud->craft_table = g_craft_table;
    hud->cursor_item = g_cursor.item;
    hud->cursor_count = g_cursor.count;
    hud->pointer_x = g_pointer_x;
    hud->pointer_y = g_pointer_y;
    hud->mine_progress = g_mine_progress;
    hud->swing = g_swing_timer > 0.0F ? 1.0F : 0.0F;
    hud->toast = g_toast_until_ms > g_now_ms ? g_toast : NULL;
}

static void begin_buffer_wait(void) {
    if (g_buffer_wait_started_us == 0) {
        g_buffer_wait_started_us = g_last_tick_us;
    }
}

static void end_buffer_wait(void) {
    if (g_buffer_wait_started_us == 0) {
        g_buffer_wait_ema_us =
            (g_buffer_wait_ema_us * 15u) / 16u;
        return;
    }
    if (g_last_tick_us > g_buffer_wait_started_us) {
        const uint64_t elapsed = g_last_tick_us - g_buffer_wait_started_us;
        const uint32_t sample = elapsed > UINT32_MAX ? UINT32_MAX :
                                                       (uint32_t)elapsed;
        g_buffer_wait_ema_us = g_buffer_wait_ema_us == 0 ? sample :
            (g_buffer_wait_ema_us * 7u + sample) / 8u;
        if (sample > g_buffer_wait_max_us) g_buffer_wait_max_us = sample;
    }
    g_buffer_wait_started_us = 0;
}

/* Returns 1 once ownership moved to Host, 0 while back-pressured and -1 when
 * the Surface must be recreated. A blocked Present retains the acquired
 * buffer and retries it without rendering over those pixels. */
static int present_writing_buffer(void) {
    int32_t result;
    const uint8_t index = g_surface_ownership.writing_buffer;
    if (index == VOXEL_SURFACE_BUFFER_NONE) return 1;
    result = pxa_surface_present_buffer(g_surface_handle, index,
                                        g_surface_ownership.writing_frame_id);
    if (result == (int32_t)PXA_SURFACE_PRESENT_RECORD_BYTES) {
        g_frame_id = g_surface_ownership.writing_frame_id;
        if (!voxel_surface_mark_presented(&g_surface_ownership,
                                          SURFACE_BUFFER_COUNT)) {
            return -1;
        }
        end_buffer_wait();
        update_fps(g_last_tick_us);
        return 1;
    }
    if (result == PXA_STATUS_WOULD_BLOCK) {
        begin_buffer_wait();
        return 0;
    }
    return -1;
}

/* Fills the state both the raster and the CPU menu renderers draw. */
static void build_menu_state(menu_state_t *menu) {
    const uint32_t base = g_clock_seed != 0 ? g_clock_seed : 0x5eed1234u;
    menu->overlay = (uint8_t)(g_screen == SCREEN_PAUSE ? 1 : 0);
    menu->screen = (uint8_t)(g_screen == SCREEN_SETTINGS
                                 ? 1
                                 : (g_screen == SCREEN_PAUSE ? 2 : 0));
    menu->has_save = g_has_save;
    menu->has_game = g_game_started;
    menu->quality_manual = g_quality_manual;
    menu->quality = g_game_quality;
    menu->show_performance = g_show_performance;
    menu->seed = base + g_seed_counter * 2654435761u;
    menu->toast = g_menu_toast_until > g_now_ms ? g_menu_toast : NULL;
}

static int render_frame(void) {
    hud_state_t hud;
    ray_hit_t target;
    const size_t pixels = (size_t)g_surface_width * g_surface_height;
    uint16_t *frame;
    uint8_t buffer_index;
    int32_t acquire_result;
    int present_result;
    if (g_surface_ownership.recreate_pending) {
        try_finish_surface_recreate();
        return 1;
    }
    if (g_surface_handle == 0 ||
        (g_surface_mode == SURFACE_MODE_MAPPED && pixels > FRAME_PIXELS_MAX) ||
        g_surface_width != (uint16_t)render_scene_width() ||
        g_surface_height != (uint16_t)render_scene_height()) {
        return 0;
    }
    if (g_surface_mode == SURFACE_MODE_RASTER) {
        int32_t raster_result;
        const uint8_t quality = (uint8_t)render_quality();
        if (!g_raster_ready) return 1;
        if (g_screen == SCREEN_PLAY) {
            game_target_block(&g_player, &target);
            build_hud(&hud);
            hud.target_table = (target.hit &&
                                game_block(target.x, target.y, target.z) ==
                                    BLOCK_TABLE)
                                   ? 1
                                   : 0;
            hud.action_mode = hud.target_table ? 2
                              : game_attack_target(&g_player) ? 1
                                                             : 0;
            (void)mark_perf_timing(PERF_CLOCK_RAYCAST_START);
            raster_result = voxel_raster_render(
                g_surface_handle, g_frame_id + 1u, &g_player, quality, &hud,
                NULL, &target);
            (void)mark_perf_timing(PERF_CLOCK_RAYCAST_END);
        } else {
            /* Menus are drawn into the raster list at the real resolution;
             * the pause screen keeps the frozen 3D frame behind them. */
            menu_state_t menu;
            build_menu_state(&menu);
            (void)mark_perf_timing(PERF_CLOCK_RAYCAST_START);
            raster_result = voxel_raster_render(
                g_surface_handle, g_frame_id + 1u, &g_player, quality, NULL,
                &menu, NULL);
            (void)mark_perf_timing(PERF_CLOCK_RAYCAST_END);
        }
        if (raster_result >= PXA_STATUS_OK) {
            if (g_perf_timing.active && !g_perf_raster_stats_valid) {
                voxel_raster_get_stats(&g_perf_raster_stats);
                g_perf_raster_stats_valid = 1;
            }
            ++g_frame_id;
            end_buffer_wait();
            update_fps(g_last_tick_us);
            return 1;
        }
        if (raster_result == PXA_STATUS_WOULD_BLOCK) {
            begin_buffer_wait();
            return 1;
        }
        if (raster_result == PXA_STATUS_INVALID_ARGUMENT)
            (void)pxa_log_error("voxel raster submit: invalid argument");
        else if (raster_result == PXA_STATUS_BAD_STATE)
            (void)pxa_log_error("voxel raster submit: bad state");
        else if (raster_result == PXA_STATUS_UNSUPPORTED)
            (void)pxa_log_error("voxel raster submit: unsupported");
        else if (raster_result == PXA_STATUS_RESOURCE_LIMIT)
            (void)pxa_log_error("voxel raster submit: resource limit");
        else if (raster_result == PXA_STATUS_PROTOCOL_ERROR)
            (void)pxa_log_error("voxel raster submit: protocol error");
        else if (raster_result == PXA_STATUS_LIMIT_EXCEEDED)
            (void)pxa_log_error("voxel raster submit: limit exceeded");
        else
            (void)pxa_log_error("voxel raster submit: other failure");
        /* A malformed or oversized frame must not permanently demote the app
         * to the low-resolution mapped renderer. Keep the last valid frame;
         * only recreate the same GameRender context when its state is gone. */
        if (raster_result == PXA_STATUS_BAD_STATE ||
            raster_result == PXA_STATUS_CANCELLED ||
            raster_result == PXA_STATUS_NOT_FOUND) {
            recreate_surface();
        }
        return 0;
    }
    if (g_surface_ownership.writing_buffer != VOXEL_SURFACE_BUFFER_NONE) {
        present_result = present_writing_buffer();
        if (present_result < 0) recreate_surface();
        return present_result >= 0;
    }
    acquire_result = pxa_surface_acquire_buffer(g_surface_handle,
                                                &buffer_index);
    if (acquire_result == PXA_STATUS_WOULD_BLOCK) {
        begin_buffer_wait();
        return 1;
    }
    if (acquire_result != (int32_t)PXA_SURFACE_ACQUIRE_RECORD_BYTES ||
        buffer_index >= SURFACE_BUFFER_COUNT) {
        return 0;
    }
    end_buffer_wait();
    if (!voxel_surface_begin_write(&g_surface_ownership, buffer_index,
                                   SURFACE_BUFFER_COUNT, g_frame_id + 1u)) {
        recreate_surface();
        return 0;
    }
    frame = g_surface_buffers + (size_t)buffer_index * pixels;
    if (g_screen == SCREEN_PLAY || g_screen == SCREEN_PAUSE) {
        game_target_block(&g_player, &target);
        (void)mark_perf_timing(PERF_CLOCK_RAYCAST_START);
        if (!render_3d(frame, g_surface_width, &g_player,
                       g_now_ms, &target, g_mine_progress)) {
            recreate_surface();
            return 0;
        }
        (void)mark_perf_timing(PERF_CLOCK_RAYCAST_END);
        if (g_screen == SCREEN_PLAY) {
            build_hud(&hud);
            hud.target_table = (target.hit &&
                                game_block(target.x, target.y, target.z) ==
                                    BLOCK_TABLE)
                                   ? 1
                                   : 0;
            hud.action_mode = 0;
            if (hud.target_table) {
                hud.action_mode = 2;
            } else if (game_attack_target(&g_player)) {
                hud.action_mode = 1;
            }
            render_hud(&hud);
        }
    }
    if (g_screen != SCREEN_PLAY) {
        menu_state_t menu;
        render_target(frame, g_surface_width);
        build_menu_state(&menu);
        render_menu(&menu);
    }
    present_result = present_writing_buffer();
    if (present_result < 0) recreate_surface();
    return present_result >= 0;
}

/* A detail level the Host rejected is stepped down and persisted, with a
 * visible note, so it does not silently flip on every launch. */
static void notify_quality_fallback(int requested, int fallback) {
    char text[32];
    char *out = text;
    *out++ = 'R';
    *out++ = 'E';
    *out++ = 'S';
    *out++ = ' ';
    out = put_u32_text(out, (uint32_t)requested);
    *out++ = 'X';
    *out++ = ' ';
    *out++ = '-';
    *out++ = '>';
    *out++ = ' ';
    out = put_u32_text(out, (uint32_t)fallback);
    *out++ = 'X';
    *out = '\0';
    if (g_screen == SCREEN_PLAY) {
        set_toast(text);
    } else {
        menu_toast(text);
    }
    (void)pxa_log_warn(text);
}

static int handle_surface_create(const pxa_event_t *event) {
    pxa_surface_create_result_t created;
    const uint32_t expected_stride = (uint32_t)g_surface_width *
                                     sizeof(g_surface_buffers[0]);
    const uint32_t expected_bytes = expected_stride * g_surface_height;
    if (event == NULL || event->request_id != SURFACE_CREATE_REQUEST)
        return 0;
    if (g_surface_request_mode == SURFACE_MODE_RASTER) {
        pxa_game_render_create_result_t renderer;
        const uint32_t required = PXA_RASTER_CAP_TEXTURED_QUAD;
        if (!pxa_game_render_parse_create(event, &renderer)) return 0;
        g_surface_create_pending = 0;
        if (renderer.status == PXA_STATUS_UNSUPPORTED) {
            /* A Host with a smaller GameRender limit rejects this context
             * size. Step down one detail level, tell the user and persist the
             * new setting; only give up on raster when even 4x fails. */
            const int requested = render_quality();
            if (requested < QUALITY_PERFORMANCE) {
                const int fallback = next_higher_quality(requested);
                if (g_quality_manual != 0)
                    g_quality_manual = (uint8_t)fallback;
                g_game_quality = (uint8_t)fallback;
                render_set_quality(fallback);
                save_preferences();
                notify_quality_fallback(requested, fallback);
                schedule_surface_retry();
                return 1;
            }
            g_raster_supported = 0;
            schedule_surface_retry();
            return 1;
        }
        if (renderer.status != PXA_STATUS_OK ||
            (renderer.capabilities & required) != required ||
            g_surface_width != (uint16_t)render_scene_width() ||
            g_surface_height != (uint16_t)render_scene_height()) {
            if (renderer.context_handle != 0)
                (void)pxa_close_handle(renderer.context_handle);
            schedule_surface_retry();
            return 1;
        }
        g_surface_handle = renderer.context_handle;
        g_surface_mode = SURFACE_MODE_RASTER;
        g_surface_ownership.recreate_pending = 0;
        reset_surface_ownership();
        voxel_raster_reset();
        voxel_raster_set_capabilities(renderer.capabilities);
        if (!voxel_raster_upload_assets(g_surface_handle)) {
            (void)pxa_close_handle(g_surface_handle);
            g_surface_handle = 0;
            g_raster_supported = 0;
            voxel_raster_set_capabilities(0);
            schedule_surface_retry();
            return 1;
        }
        g_raster_ready = 1;
        voxel_raster_set_phase_marker(mark_raster_phase);
        (void)pxa_log_info("voxel: raster surface ready");
        g_surface_retry_ticks = 0;
        rebind_ui_surface();
        (void)render_frame();
        return 1;
    }
    if (!pxa_surface_parse_create(event, &created)) return 0;
    g_surface_create_pending = 0;
    if (created.status != PXA_STATUS_OK ||
        created.stride_bytes != expected_stride ||
        created.frame_bytes != expected_bytes ||
        created.buffer_count != SURFACE_BUFFER_COUNT) {
        schedule_surface_retry();
        return 1;
    }
    if (g_surface_width != (uint16_t)render_scene_width() ||
        g_surface_height != (uint16_t)render_scene_height()) {
        (void)pxa_close_handle(created.surface_handle);
        g_surface_ownership.recreate_pending = 0;
        (void)request_surface_create();
        return 1;
    }
    g_surface_handle = created.surface_handle;
    g_surface_mode = g_surface_request_mode;
    g_surface_ownership.recreate_pending = 0;
    reset_surface_ownership();
    if (pxa_surface_register_buffers(
                   g_surface_handle, g_surface_buffers, created.frame_bytes,
                   created.buffer_count) !=
               (int32_t)(created.frame_bytes * created.buffer_count)) {
        (void)pxa_close_handle(g_surface_handle);
        g_surface_handle = 0;
        schedule_surface_retry();
        return 1;
    }
    if (!pxa_surface_configure_layer(
            SURFACE_CONFIGURE_REQUEST, g_surface_handle, g_layout.view_x,
            g_layout.view_y, g_surface_width, g_surface_height, 0, 1,
            g_packet, sizeof(g_packet))) {
        (void)pxa_close_handle(g_surface_handle);
        g_surface_handle = 0;
        schedule_surface_retry();
        return 1;
    }
    g_surface_retry_ticks = 0;
    voxel_raster_reset();
    rebind_ui_surface();
    (void)render_frame();
    return 1;
}

static void update_fps(uint64_t timestamp_us) {
    if (g_fps_window_start_us == 0) {
        g_fps_window_start_us = timestamp_us;
        g_fps_window_frames = 0;
        return;
    }
    ++g_fps_window_frames;
    if (timestamp_us > g_fps_window_start_us &&
        timestamp_us - g_fps_window_start_us >= UINT64_C(500000)) {
        uint64_t measured = (uint64_t)g_fps_window_frames * UINT64_C(10000000) /
                            (timestamp_us - g_fps_window_start_us);
        if (measured > 9999u) {
            measured = 9999u;
        }
        g_fps_x10 = (uint32_t)measured;
        g_fps_window_start_us = timestamp_us;
        g_fps_window_frames = 0;
    }
}

int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    pxa_ui_environment_t environment;
    g_screen = SCREEN_MENU;
    g_game_quality = QUALITY_MIN;
    update_scene_limits();
    if (pxa_ui_parse_start_environment(config, length, &environment) &&
        environment.width > 0 && environment.height > 0) {
        render_configure((int)environment.width, (int)environment.height);
    } else {
        render_configure(SCREEN_W_DEFAULT, SCREEN_H_DEFAULT);
    }
    /* Start at native resolution. Automatic scaling remains available from
     * Settings, but must be selected explicitly so it cannot silently reduce
     * display resolution on first launch. */
    render_set_quality(QUALITY_MIN);
    g_surface_handle = 0;
    g_surface_width = 0;
    g_surface_height = 0;
    g_frame_id = 0;
    g_surface_create_pending = 0;
    g_surface_start_pending = 0;
    g_surface_retry_ticks = 0;
    g_surface_mode = SURFACE_MODE_MAPPED;
    g_surface_request_mode = SURFACE_MODE_MAPPED;
    /* The host owns the pixel work while the Guest supplies world geometry and
     * the same UI state used by the mapped renderer. */
    g_raster_supported = VOXEL_HOST_RASTER_DEFAULT;
    g_raster_ready = 0;
    voxel_raster_set_capabilities(0);
    reset_surface_ownership();
    voxel_raster_reset();
    g_input_initialized = 0;
    g_ui_generation = 0;
    g_input_dirty = 0;
    g_backgrounded = 0;
    g_last_tick_us = 0;
    g_tick_accumulator_us = 0;
    g_now_ms = 0;
    g_fps_x10 = 0;
    g_fps_window_start_us = 0;
    g_fps_window_frames = 0;
    g_hotbar_selected = 0;
    g_toast[0] = '\0';
    g_toast_until_ms = 0;
    release_finger(&g_move_finger);
    release_finger(&g_look_finger);
    release_finger(&g_button_finger);
    g_button_kind = BTN_NONE;
    g_stick_dx = 0;
    g_stick_dy = 0;
    g_jump_held = 0;
    g_down_held = 0;
    g_action_held = 0;
    g_attack_cooldown = 0.0F;
    g_mine_progress = 0.0F;
    g_mine_sound_timer = 0.0F;
    g_mine_target.hit = 0;
    g_swing_timer = 0.0F;
    g_last_jump_tap_us = 0;
    g_perf_timing = (perf_timing_sample_t){0};
    g_perf_sample_counter = 0;
    g_perf_log_last_us = 0;
    g_perf_raster_stats_valid = 0;
    g_update_ema_us = 0;
    g_update_max_us = 0;
    voxel_quality_controller_reset(&g_quality_controller);
    g_render_total_ema_us = 0;
    g_render_total_max_us = 0;
    g_buffer_wait_started_us = 0;
    g_buffer_wait_ema_us = 0;
    g_buffer_wait_max_us = 0;
    g_host_raster_ema_us = 0;
    g_host_raster_max_us = 0;
    g_host_queue_ema_us = 0;
    g_host_present_ema_us = 0;
    g_host_queue_total_us = 0;
    g_host_present_total_us = 0;
    g_host_rendered_frames = 0;
    g_host_visible_frames = 0;
    g_snapshot_ticks = 0;
    g_bootstrap_requests_pending = 1;
    g_present_failures = 0;
    g_quality_manual = QUALITY_MIN;
    g_inventory_open = 0;
    g_craft_table = 0;
    g_cursor.item = BLOCK_AIR;
    g_cursor.count = 0;
    g_pointer_x = 0;
    g_pointer_y = 0;
    g_inv_press_active = 0;
    g_inv_press_travel = 0;
    g_inv_press_us = 0;
    g_pad_buttons = 0;
    g_pad_previous = 0;
    g_pad_seen = 0;
    g_pad_jump = 0;
    g_pad_mine = 0;
    g_pad_move_z = 0.0F;
    g_pad_turn = 0.0F;
    g_pad_pitch = 0.0F;
    g_pad_last_a_us = 0;
    g_sfx.state = VOXEL_SFX_OFF;
    g_has_save = 0;
    g_game_started = 0;
    g_save_active = 0;
    g_load_active = 0;
    g_save_next = 0;
    g_save_chunks = 0;
    g_save_length = 0;
    g_load_next = 0;
    g_load_chunks = 0;
    g_menu_toast[0] = '\0';
    g_menu_toast_until = 0;
    g_show_performance = 1;
    g_clock_seed = 0;
    g_seed_counter = 0;
    g_menu_press_active = 0;
    g_menu_press_travel = 0;
    g_menu_press_us = 0;
    game_inventory_init();
    if (!pxa_window_fullscreen()) {
        (void)pxa_log_error("voxel: fullscreen request failed");
        return PXA_STATUS_INTERNAL;
    }
    if (!initialize_input_surface()) {
        (void)pxa_log_error("voxel: input surface setup failed");
        return PXA_STATUS_INTERNAL;
    }
    if (!request_surface_create()) {
        (void)pxa_log_error("voxel: surface create request failed");
        return PXA_STATUS_INTERNAL;
    }
    /* Audio is optional and may open a runtime modal. The Host modal barrier
     * keeps this already-requested Surface behind trusted UI until dismissal. */
    voxel_sfx_start(&g_sfx, g_packet, sizeof(g_packet));
    (void)pxa_storage_get(STORAGE_PREFS_GET_REQUEST, STORAGE_PREFS_KEY,
                          STORAGE_PREFS_KEY_LEN, g_storage_payload,
                          sizeof(g_storage_payload), g_packet,
                          sizeof(g_packet));
    (void)pxa_storage_get(STORAGE_META_GET_REQUEST, STORAGE_META_KEY,
                          STORAGE_META_KEY_LEN, g_storage_payload,
                          sizeof(g_storage_payload), g_packet,
                          sizeof(g_packet));
#ifndef VOXEL_AUTOPLAY
#define VOXEL_AUTOPLAY 0
#endif
#if VOXEL_AUTOPLAY
    g_autoplay_pending = 1;
#endif
    return pxa_clock_set_period(CLOCK_POLL_PERIOD_MS) ? PXA_STATUS_OK
                                                 : PXA_STATUS_INTERNAL;
}

static uint8_t consume_simulation_steps(uint64_t timestamp_us) {
    const uint64_t step_us = (uint64_t)FRAME_PERIOD_MS * 1000u;
    const uint64_t maximum_elapsed_us = step_us * MAX_CATCHUP_STEPS;
    uint64_t elapsed_us;
    uint64_t steps;
    if (timestamp_us == 0) return 0;
    if (g_last_tick_us == 0 || timestamp_us <= g_last_tick_us) {
        g_last_tick_us = timestamp_us;
        g_tick_accumulator_us = 0;
        return 1;
    }
    elapsed_us = timestamp_us - g_last_tick_us;
    g_last_tick_us = timestamp_us;
    if (elapsed_us > maximum_elapsed_us) elapsed_us = maximum_elapsed_us;
    g_tick_accumulator_us += elapsed_us;
    steps = g_tick_accumulator_us / step_us;
    if (steps > MAX_CATCHUP_STEPS) steps = MAX_CATCHUP_STEPS;
    g_tick_accumulator_us -= steps * step_us;
    return (uint8_t)steps;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_pointer_data_t pointer;
    uint64_t timestamp_us;
    if (!pxa_parse_event(event, length, &parsed)) {
        return PXA_EVENT_UNHANDLED;
    }
    if (parsed.service == PXA_SERVICE_SYSTEM &&
        parsed.opcode == PXA_SYSTEM_LIFECYCLE_EVENT &&
        parsed.payload_length == 1) {
        g_backgrounded = parsed.payload[0] == PXA_SYSTEM_LIFECYCLE_BACKGROUND;
        g_last_tick_us = 0;
        g_tick_accumulator_us = 0;
        release_finger(&g_move_finger);
        release_finger(&g_look_finger);
        release_finger(&g_button_finger);
        g_button_kind = BTN_NONE;
        g_action_held = 0;
        g_jump_held = 0;
        g_down_held = 0;
        g_pad_move_z = 0.0F;
        g_pad_turn = 0.0F;
        g_pad_pitch = 0.0F;
        return PXA_EVENT_HANDLED;
    }
    if (voxel_sfx_handle_event(&g_sfx, &parsed, g_packet, sizeof(g_packet))) {
        return PXA_EVENT_HANDLED;
    }
    if (handle_surface_create(&parsed)) {
        return PXA_EVENT_HANDLED;
    }
    {
        pxa_surface_released_event_t released;
        if (pxa_surface_parse_released(&parsed, &released)) {
            if (released.surface_handle != g_surface_handle) {
                return PXA_EVENT_UNHANDLED;
            }
            if (released.buffer_index < SURFACE_BUFFER_COUNT) {
                (void)voxel_surface_mark_released(
                    &g_surface_ownership, released.buffer_index,
                    SURFACE_BUFFER_COUNT);
                try_finish_surface_recreate();
            }
            return PXA_EVENT_HANDLED;
        }
    }
    if (parsed.service == PXA_SERVICE_STORAGE) {
        handle_storage_event(&parsed);
        g_input_dirty = 1;
        return PXA_EVENT_HANDLED;
    }
    {
        pxa_ui_environment_t environment;
        if (pxa_ui_parse_environment_event(&parsed, &environment)) {
            g_safe_insets.top = environment.safe_insets[0];
            g_safe_insets.right = environment.safe_insets[1];
            g_safe_insets.bottom = environment.safe_insets[2];
            g_safe_insets.left = environment.safe_insets[3];
            if (environment.width > 0 && environment.height > 0) {
                apply_screen_size((int)environment.width,
                                  (int)environment.height);
            }
            return PXA_EVENT_HANDLED;
        }
    }
    {
        pxa_ui_controller_data_t state;
        if (pxa_ui_parse_controller(&parsed, &state)) {
            if (g_backgrounded) return PXA_EVENT_HANDLED;
            on_controller_state(&state);
            return PXA_EVENT_HANDLED;
        }
    }
    if (parsed.service == PXA_SERVICE_WINDOW &&
        (parsed.opcode == PXA_WINDOW_METRICS_CHANGED_OP ||
         (parsed.opcode == PXA_WINDOW_GET_SNAPSHOT_OP &&
          parsed.request_id == WINDOW_SNAPSHOT_REQUEST))) {
        uint32_t width;
        uint32_t height;
        window_insets_t safe;
        window_insets_t bars;
        if (parse_window_snapshot(parsed.payload, parsed.payload_length,
                                  &width, &height, &safe, &bars)) {
            g_safe_insets = safe;
            g_system_bar_insets = bars;
            apply_screen_size((int)width, (int)height);
        }
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SERVICE_CLOCK &&
        parsed.opcode == PXA_CLOCK_NOW_RESULT &&
        parsed.request_id == SEED_CLOCK_REQUEST) {
        int32_t status;
        uint64_t seed_us;
        if (pxa_clock_parse_now(&parsed, &status, &seed_us) &&
            status == PXA_STATUS_OK) {
            g_clock_seed =
                (uint32_t)seed_us ^ (uint32_t)(seed_us >> 32);
            g_input_dirty = 1;
        }
        return PXA_EVENT_HANDLED;
    }
    if (handle_perf_clock_event(&parsed)) {
        return PXA_EVENT_HANDLED;
    }
    if (pxa_clock_tick_timestamp_us(&parsed, &timestamp_us)) {
        if (g_backgrounded) {
            g_last_tick_us = timestamp_us;
            g_tick_accumulator_us = 0;
            return PXA_EVENT_HANDLED;
        }
        const uint8_t steps = consume_simulation_steps(timestamp_us);
        uint8_t index;
        float move_x;
        float move_z;
        voxel_sfx_tick(&g_sfx, &parsed);
        retry_surface_on_tick();
        if (g_bootstrap_requests_pending) {
            /* Async completion events cannot be delivered until start has
             * returned and the component is RUNNING. The first timer event is
             * the earliest portable point for these bootstrap requests. */
            g_bootstrap_requests_pending = 0;
            request_window_snapshot();
            (void)pxa_clock_now(SEED_CLOCK_REQUEST);
        }
        if (++g_snapshot_ticks >= WINDOW_SNAPSHOT_PERIOD_TICKS) {
            g_snapshot_ticks = 0;
            request_window_snapshot();
        }
        if (g_screen != SCREEN_PLAY) {
            const uint8_t toast_was_visible =
                g_menu_toast_until > g_now_ms;
            g_now_ms += (uint32_t)steps * FRAME_PERIOD_MS;
            if (g_input_dirty ||
                toast_was_visible != (g_menu_toast_until > g_now_ms)) {
                g_input_dirty = 0;
                (void)render_frame();
            }
            return PXA_EVENT_HANDLED;
        }
#ifndef VOXEL_FIXED_VIEW
/* Measurement builds pin the camera so run-to-run raster timings compare the
 * same view instead of wherever the player happened to fall. */
#define VOXEL_FIXED_VIEW 0
#endif
#if VOXEL_FIXED_VIEW
        g_player.x = 8.5F;
        g_player.y = 22.0F;
        g_player.z = 8.5F;
        g_player.vx = 0.0F;
        g_player.vy = 0.0F;
        g_player.vz = 0.0F;
        g_player.yaw = 0.9F;
        g_player.pitch = -0.28F;
        g_player.flying = 1;
#endif
        if (steps != 0) {
#if VOXEL_AUTOPLAY
            /* Debug/measurement build: start a game once the Surface exists,
             * so screenshots and frame timing need no input injection. */
            if (g_autoplay_pending && g_surface_handle != 0) {
                g_autoplay_pending = 0;
                start_new_game();
            }
#endif
            maybe_begin_perf_timing();
        }
        for (index = 0; index < steps; ++index) {
            const uint8_t was_ground = g_player.on_ground;
            if (g_inventory_open) {
                g_now_ms += FRAME_PERIOD_MS;
                continue;
            }
            move_x = rc_clampf(
                (float)g_stick_dx /
                    (STICK_RADIUS * (float)g_layout.ui_scale),
                -1.0F, 1.0F);
            move_z = rc_clampf(
                -(float)g_stick_dy /
                        (STICK_RADIUS * (float)g_layout.ui_scale) +
                    g_pad_move_z,
                -1.0F, 1.0F);
            g_player.yaw += g_pad_turn * PAD_TURN_RATE * TICK_SECONDS;
            if (g_player.yaw > RC_PI) {
                g_player.yaw -= RC_TWO_PI;
            } else if (g_player.yaw < -RC_PI) {
                g_player.yaw += RC_TWO_PI;
            }
            if (g_pad_pitch != 0.0F) {
                g_player.pitch = rc_clampf(
                    g_player.pitch + g_pad_pitch * PAD_PITCH_RATE * TICK_SECONDS,
                    -1.55F, 1.55F);
            }
            game_step(&g_player, TICK_SECONDS, move_x, move_z, jump_active(),
                      jump_active(), descend_active());
            if (was_ground && !g_player.on_ground && g_player.vy > 6.0F) {
                voxel_sfx_play(&g_sfx, VOXEL_SFX_JUMP);
            }
            {
                int attacked = 0;
                if (g_action_held && g_attack_cooldown <= 0.0F &&
                    game_attack(&g_player,
                                g_inventory[g_hotbar_selected].item)) {
                    g_attack_cooldown = 0.35F;
                    attacked = 1;
                    voxel_sfx_play(&g_sfx, VOXEL_SFX_HIT);
                    set_toast("HIT");
                }
                if (g_attack_cooldown > 0.0F) {
                    g_attack_cooldown -= TICK_SECONDS;
                }
                if (!attacked) {
                    update_mining(TICK_SECONDS);
                }
            }
            game_update_mobs(TICK_SECONDS, &g_player);
            game_update_particles(TICK_SECONDS);
            if (g_swing_timer > 0.0F) {
                g_swing_timer -= TICK_SECONDS;
            }
            g_now_ms += FRAME_PERIOD_MS;
        }
        if (steps != 0 || g_input_dirty) {
            if (steps != 0) (void)game_stream_chunks(1);
            g_input_dirty = 0;
            (void)mark_perf_timing(PERF_CLOCK_UPDATE_END);
            if (render_frame()) {
                g_present_failures = 0;
            } else if (++g_present_failures >= 3u) {
                /* The Host refused the display list, usually because the view
                 * is larger than its Canvas budget: shrink and retry. */
                g_present_failures = 0;
                render_shrink_view();
                apply_quality();
                (void)render_frame();
                toast_view();
            }
            (void)mark_perf_timing(PERF_CLOCK_FRAME_END);
        }
        return PXA_EVENT_HANDLED;
    }
    if (!pxa_ui_parse_pointer(&parsed, &pointer) || pointer.node != FRAME_NODE) {
        return PXA_EVENT_UNHANDLED;
    }
    if (g_backgrounded) return PXA_EVENT_HANDLED;
    if (pointer.phase == PXA_POINTER_DOWN) {
        on_pointer_down(&pointer);
    } else if (pointer.phase == PXA_POINTER_MOVE) {
        on_pointer_move(&pointer);
    } else if (pointer.phase == PXA_POINTER_UP) {
        on_pointer_up(&pointer, 0);
    } else if (pointer.phase == PXA_POINTER_CANCEL) {
        on_pointer_up(&pointer, 1);
    }
    g_input_dirty = 1;
    return PXA_EVENT_HANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
    if (g_sfx.session_handle != 0) {
        (void)pxa_close_handle(g_sfx.session_handle);
        g_sfx.session_handle = 0;
    }
    if (g_surface_handle != 0) {
        (void)pxa_close_handle(g_surface_handle);
    }
    g_surface_handle = 0;
    g_surface_create_pending = 0;
    g_surface_start_pending = 0;
    g_surface_retry_ticks = 0;
    g_raster_ready = 0;
    voxel_raster_set_capabilities(0);
}
