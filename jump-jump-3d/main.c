/* Jump Jump 3D: an isometric remake of the WeChat mini game on the PXA
 * GameRender raster service.
 *
 * The Guest keeps the whole game: blocks, physics, scoring, camera and the
 * interface. Only a compact painter draw list reaches the Host, which paints
 * it into its own RGB565 buffer before the panel presenter scales it. */
#include <stdint.h>

#include "pxa.h"
#include "pxa_game_render.h"
#include "pxa_game_screen.h"
#include "pxa_log.h"
#include "pxa_raster.h"
#include "pxa_storage.h"
#include "pxa_ui.h"

#include "jump3d_audio.h"
#include "jump3d_font.h"
#include "jump3d_game.h"
#include "jump3d_palette.h"
#include "jump3d_render.h"

#define INPUT_NODE 2u
#define CREATE_REQUEST UINT32_C(1)
#define TICK_MS 20u
#define MAX_CATCHUP_STEPS 2u
#define REQUIRED_CAPABILITIES PXA_RASTER_CAP_PAINTER_POLYGON
/* Scratch for one upload: the lit palette or the largest glyph atlas. */
#define UPLOAD_PAYLOAD_BYTES \
    (J3_PALETTE_ENTRIES * 2u > J3_FONT_MAX_ATLAS_BYTES \
         ? J3_PALETTE_ENTRIES * 2u \
         : J3_FONT_MAX_ATLAS_BYTES)
#define UPLOAD_BYTES (PXA_RASTER_UPLOAD_HEADER_BYTES + UPLOAD_PAYLOAD_BYTES)
/* Bytes one lit palette upload writes, which is what the Host returns. */
#define PALETTE_UPLOAD_BYTES \
    (PXA_RASTER_UPLOAD_HEADER_BYTES + J3_PALETTE_ENTRIES * 2u)
/* Background music policy. The original plays its menu theme (res/icon.mp3)
 * once while the game loads and has no in-run music, so:
 *   0 = silent, 1 = play the theme once at startup (faithful, default),
 *   2 = loop it quietly during play as background music. */
#define J3_BGM_MODE 1
/* Logs a tick/frame/raster summary every five seconds of play. */
#define J3_PERF_LOG 1
/* Submitting only changed frames saves power while the player thinks, but it
 * makes display FPS meters read zero and hides the pipeline from the panel, so
 * every clock tick renders by default. */
#define J3_SKIP_STATIC_FRAMES 0
#define J3_BGM_LOOP 0
#define STORAGE_GET_REQUEST UINT32_C(0x5301)
#define STORAGE_SET_REQUEST UINT32_C(0x5302)
#define STORAGE_SAVE_DELAY 2.0F
#define QUALITY_WINDOW 48u
/* The ladder only reacts to the Host raster cost. On both boards the panel path
 * (compose/rotate/flush) is slower than the raster at 1x, so a slow raster does
 * not cost frames: esp32s31-korvo-1 holds 1x at ~28 fps with a 27 ms raster,
 * pai-touch holds 1x at ~31 fps with a 12 ms raster. Downgrading on a lower
 * threshold only traded resolution away, so the cap sits above both and a scale
 * that once blew it is never re-tried. */
#define QUALITY_RASTER_CAP_US 40000u
/* Enough headroom at the current scale before trying a sharper one. */
#define QUALITY_UPGRADE_US 20000u
#define QUALITY_DOWNGRADE_WINDOWS 4u
/* A long, quiet window before spending a context rebuild on a sharper mode:
 * the gap between the two thresholds keeps the ladder from oscillating. */
#define QUALITY_UPGRADE_WINDOWS 20u
/* The launch transition composites through the system UI, so the first seconds
 * say nothing about steady state raster cost. */
#define QUALITY_WARMUP_FRAMES 600u
#define MAX_SCALE_SHIFT 2u

/* Quality override for benchmarking and for panels where the Host raster is
 * known to be fast/slow: -1 keeps the automatic ladder, 0..MAX_SCALE_SHIFT
 * pins the render scale (0 = native, 1 = half, ...). */
#ifndef J3_FORCE_SCALE_SHIFT
#define J3_FORCE_SCALE_SHIFT (-1)
#endif

static uint8_t g_packet[192];
static uint8_t g_draw[PXA_RASTER_MAX_DRAW_BYTES];
static uint8_t g_upload[UPLOAD_BYTES];
static uint16_t g_palette[J3_PALETTE_ENTRIES];
static j3_game_t g_game;
static j3_render_t g_render;
static j3_audio_t g_audio;
static pxa_game_screen_t g_screen;
static uint32_t g_context;
static uint64_t g_frame_id;
static uint64_t g_last_tick_us;
static uint32_t g_seed;
static int g_target_w = 296;
static int g_target_h = 240;
static uint8_t g_scale_shift;
static uint8_t g_input_ready;
static uint8_t g_started;
static uint8_t g_bgm_started;
static uint8_t g_frame_rendered;
#if J3_PERF_LOG
static void log_perf(void);
static uint32_t g_perf_ticks;
static uint32_t g_perf_frames;
static uint32_t g_perf_raster_us;
static uint32_t g_perf_present_us;
#endif
static uint32_t g_last_signature;
static uint8_t g_bonus_played;
static float g_bonus_repeat_timer;
static uint8_t g_present_failures;
static uint8_t g_quality_frames;
static uint8_t g_quality_overloads;
static uint8_t g_quality_underloads;
static uint32_t g_quality_total_frames;
static uint32_t g_quality_raster_us;
/* What the ladder learned about each scale: the raster cost measured there.
 * Used to reject an upgrade into a scale that already blew the budget. */
static uint32_t g_scale_raster_us[MAX_SCALE_SHIFT + 1u];
static uint8_t g_palette_scheme;
static uint8_t g_request_scale_change;
static uint8_t g_storage_payload[32];
static uint8_t g_storage_ready;
static uint32_t g_best_saved;
static float g_save_timer;

/* Mirrors of the last tick, used to fire the sound effects. */
static uint8_t g_last_state;
static uint32_t g_last_score;
static uint16_t g_last_jumps;

static void apply_scale(void) {
    int width = (int)g_screen.width;
    int height = (int)g_screen.height;
    int shift;
    if (width < 64) width = 64;
    if (height < 64) height = 64;
    for (shift = 0; shift < (int)g_scale_shift; ++shift) {
        width = (width + 1) / 2;
        height = (height + 1) / 2;
    }
    g_target_w = width;
    g_target_h = height;
}

static int initialize_input_surface(void) {
    pxa_ui_transaction_t transaction = {0};
    if (g_input_ready) return 1;
    if (!pxa_ui_transaction_begin(&transaction, 1,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE, g_packet,
                                  sizeof(g_packet)) ||
        !pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) ||
        !pxa_ui_create(&transaction, INPUT_NODE, 1, 0, PXA_UI_NODE_CANVAS) ||
        !pxa_ui_set_length(&transaction, INPUT_NODE, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_length(&transaction, INPUT_NODE, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_FILL, 0) ||
        !pxa_ui_set_event_mask(&transaction, INPUT_NODE,
                               PXA_UI_EVENT_MASK_POINTER) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active)
            (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    g_input_ready = 1;
    return 1;
}

static int upload_resources(void) {
    int32_t result;
    j3_palette_build(g_palette);
    result = pxa_raster_upload_lit_palette_rgb565(
        g_context, J3_LIGHT_LEVELS, g_palette, g_upload, sizeof(g_upload));
    if (result != (int32_t)PALETTE_UPLOAD_BYTES) {
        char text[48];
        static const char prefix[] = "jump-jump-3d: palette upload status=";
        int length = 0;
        while (prefix[length] != '\0') {
            text[length] = prefix[length];
            ++length;
        }
        text[length++] = (char)('0' + (int)((result < 0 ? -result : result) / 1000) % 10);
        text[length++] = (char)('0' + (int)((result < 0 ? -result : result) / 100) % 10);
        text[length++] = (char)('0' + (int)((result < 0 ? -result : result) / 10) % 10);
        text[length++] = (char)('0' + (int)((result < 0 ? -result : result)) % 10);
        text[length] = '\0';
        (void)pxa_log_error(text);
        return 0;
    }
    if (!j3_font_upload(g_context, g_upload, sizeof(g_upload))) return 0;
    if (!j3_render_upload_resources(g_context, g_upload, sizeof(g_upload))) {
        (void)pxa_log_error("jump-jump-3d: shadow upload failed");
        return 0;
    }
    return 1;
}

static void request_context(uint8_t shift, uint32_t request_id) {
    g_scale_shift = shift;
    apply_scale();
    if (g_context != 0) {
        (void)pxa_close_handle(g_context);
        g_context = 0;
    }
    g_started = 0;
    g_frame_rendered = 0;
    g_quality_frames = 0;
    g_quality_raster_us = 0;
    (void)pxa_game_render_create(request_id, (uint16_t)g_target_w,
                                 (uint16_t)g_target_h, 3, 1, g_packet,
                                 sizeof(g_packet));
}

/* Mirrors the original's audio triggers: a charge swell while the finger is
 * down, the success or combo note on landing, fall sounds on a miss, and the
 * special block jingles when their stay bonus fires. */
/* Mirrors the original's audio triggers: the charge swell and its sustain loop
 * layered together while the finger is down, the success or combo note on
 * landing, fall sounds on a miss, and the special block jingles repeating
 * every three seconds while the player keeps standing on them. */
static void play_state_sounds(void) {
    if (g_game.state == J3_STATE_CHARGING && g_last_state != J3_STATE_CHARGING) {
        /* The original starts both the swell and the loop on touch down. */
        j3_audio_play(&g_audio, J3_CHANNEL_CHARGE, J3_CLIP_SCALE_INTRO,
                      J3_GAIN_FULL, 0);
        j3_audio_play(&g_audio, J3_CHANNEL_SUSTAIN, J3_CLIP_SCALE_LOOP,
                      J3_GAIN_SOFT, 1);
    }
    if (g_last_state == J3_STATE_CHARGING && g_game.state != J3_STATE_CHARGING) {
        j3_audio_stop(&g_audio, J3_CHANNEL_CHARGE);
        j3_audio_stop(&g_audio, J3_CHANNEL_SUSTAIN);
    }
    if (g_game.jump_count != g_last_jumps) {
        j3_audio_play(&g_audio, J3_CHANNEL_LAND, J3_CLIP_SUCCESS, J3_GAIN_FULL,
                      0);
        j3_audio_play(&g_audio, J3_CHANNEL_POP, J3_CLIP_POP, J3_GAIN_SOFT, 0);
        if (g_game.combo > 0u) {
            const uint8_t note = (uint8_t)(J3_CLIP_COMBO1 +
                                           ((g_game.combo - 1u) % 8u));
            j3_audio_play(&g_audio, J3_CHANNEL_COMBO, note, J3_GAIN_LOUD, 0);
        } else {
            j3_audio_stop(&g_audio, J3_CHANNEL_COMBO);
        }
        g_bonus_repeat_timer = 3.0F;
    }
    if (g_game.bonus_popup_timer > 0.0F && g_bonus_played == 0u) {
        g_bonus_played = 1;
        g_bonus_repeat_timer = 3.0F;
        switch (g_game.bonus_kind) {
        case J3_KIND_MUSIC:
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_SING,
                          J3_GAIN_FULL, 0);
            break;
        case J3_KIND_STORE:
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_STORE,
                          J3_GAIN_FULL, 0);
            break;
        case J3_KIND_WELL:
            /* The original stops every other music voice for the manhole. */
            j3_audio_stop(&g_audio, J3_CHANNEL_COMBO);
            j3_audio_stop(&g_audio, J3_CHANNEL_POP);
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_WATER,
                          J3_GAIN_FULL, 0);
            break;
        default:
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_POP,
                          J3_GAIN_FULL, 0);
            break;
        }
    }
    if (g_bonus_repeat_timer > 0.0F) {
        g_bonus_repeat_timer -= (float)TICK_MS * 0.001F;
        if (g_bonus_repeat_timer <= 0.0F && g_bonus_played != 0u &&
            g_game.state == J3_STATE_READY && g_game.bonus_popup_timer <= 0.0F &&
            j3_block_bonus(&g_game.blocks[0]) != 0u &&
            j3_audio_channel_active(&g_audio, J3_CHANNEL_BONUS) == 0) {
            /* Keep the melody going while the player stays, like the
             * original's onEnded timers. */
            g_bonus_repeat_timer = 3.0F;
            j3_audio_play(&g_audio, J3_CHANNEL_BONUS, J3_CLIP_SING,
                          J3_GAIN_SOFT, 0);
        }
    }
    if ((g_game.state == J3_STATE_TIP || g_game.state == J3_STATE_FALL) &&
        g_last_state == J3_STATE_FLYING) {
        j3_audio_play(&g_audio, J3_CHANNEL_LAND,
                      g_game.land_result == J3_LAND_MISS ? J3_CLIP_FALL_2
                                                         : J3_CLIP_FALL,
                      J3_GAIN_FULL, 0);
    }
    if (g_game.bonus_popup_timer <= 0.0F) g_bonus_played = 0;
    g_last_state = g_game.state;
    g_last_score = g_game.score;
    g_last_jumps = g_game.jump_count;
#if J3_BGM_MODE != 0
    if (g_audio.state == J3_AUDIO_READY && !g_bgm_started) {
        g_bgm_started = 1;
        j3_audio_play(&g_audio, J3_CHANNEL_BGM, J3_CLIP_ICON, J3_GAIN_BGM,
                      (uint8_t)J3_BGM_LOOP);
    }
#endif
}

/* Cheap signature of everything that changes the picture. When it matches the
 * last submitted frame the Host keeps showing the previous one, so idle turns
 * cost no raster time at all (the player thinking, the result screen). */
static uint32_t frame_signature(const j3_game_t *game) {
    uint32_t hash = UINT32_C(2166136261);
    uint8_t index;
#define J3_MIX(value) \
    do { \
        hash ^= (uint32_t)(value); \
        hash *= UINT32_C(16777619); \
    } while (0)
    J3_MIX(game->state);
    J3_MIX(game->score);
    J3_MIX(game->best);
    J3_MIX(game->combo);
    J3_MIX(game->jump_count);
    J3_MIX((int32_t)(game->px * 256.0F));
    J3_MIX((int32_t)(game->py * 256.0F));
    J3_MIX((int32_t)(game->pz * 256.0F));
    J3_MIX((int32_t)(game->spin * 256.0F));
    J3_MIX((int32_t)(game->tilt * 256.0F));
    J3_MIX((int32_t)(game->body_scale * 256.0F));
    J3_MIX((int32_t)(game->land_scale * 256.0F));
    J3_MIX((int32_t)(game->root_yaw * 256.0F));
    J3_MIX((int32_t)(game->cam_x * 256.0F));
    J3_MIX((int32_t)(game->cam_z * 256.0F));
    J3_MIX((int32_t)(game->popup_timer * 128.0F));
    J3_MIX((int32_t)(game->bonus_popup_timer * 128.0F));
    J3_MIX(game->popup_points);
    J3_MIX(game->bonus_points);
    J3_MIX(game->bonus_kind);
    for (index = 0; index < J3_WAVE_MAX; ++index) {
        J3_MIX(game->waves[index].active);
        J3_MIX((int32_t)(game->waves[index].t * 128.0F));
    }
    for (index = 0; index < game->block_count && index < J3_BLOCK_MAX;
         ++index) {
        J3_MIX(game->blocks[index].kind);
        J3_MIX(game->blocks[index].color);
        J3_MIX((int32_t)(game->blocks[index].x * 256.0F));
        J3_MIX((int32_t)(game->blocks[index].z * 256.0F));
        J3_MIX((int32_t)(game->blocks[index].radius * 256.0F));
        J3_MIX((int32_t)(game->blocks[index].squash * 256.0F));
        J3_MIX((int32_t)(game->blocks[index].appear * 256.0F));
        J3_MIX(game->blocks[index].center_dot);
    }
#undef J3_MIX
    return hash;
}

#if J3_PERF_LOG
/* "j3 perf ticks frames raster_us covered_px": ticks per report window shows
 * how often the Host ran its loop (a Guest tick may coalesce two steps), frames
 * counts the draw lists this App submitted in that window, and the last two
 * fields come from the Host raster telemetry. If ticks is low the Host loop is
 * the limit; if frames is much lower than ticks the App is skipping work. */
static void log_perf(void) {
    char text[128];
    static const char prefix[] = "j3 perf ticks frames raster_us covered ";
    uint32_t values[4];
    int length = 0;
    int field;
    values[0] = g_perf_ticks;
    values[1] = g_perf_frames;
    values[2] = g_perf_raster_us / (g_perf_frames != 0u ? g_perf_frames : 1u);
    values[3] = g_perf_present_us;
    while (prefix[length] != '\0') {
        text[length] = prefix[length];
        ++length;
    }
    for (field = 0; field < 4; ++field) {
        uint32_t divisor = 10000u;
        while (divisor > 1u && values[field] < divisor) divisor /= 10u;
        while (divisor != 0u) {
            text[length++] = (char)('0' + (values[field] / divisor) % 10u);
            divisor /= 10u;
        }
        text[length++] = ' ';
    }
    {
        /* Report the live scale and canvas so a slow window can be attributed
         * to a downgrade instead of the renderer itself. */
        static const char scale_key[] = "scale ";
        static const char canvas_key[] = "canvas ";
        uint32_t divisor;
        for (field = 0; scale_key[field] != '\0'; ++field)
            text[length++] = scale_key[field];
        text[length++] = (char)('0' + (g_scale_shift % 10u));
        text[length++] = ' ';
        for (field = 0; canvas_key[field] != '\0'; ++field)
            text[length++] = canvas_key[field];
        divisor = 10000u;
        while (divisor > 1u && (uint32_t)g_render.width < divisor) divisor /= 10u;
        while (divisor != 0u) {
            text[length++] =
                (char)('0' + ((uint32_t)g_render.width / divisor) % 10u);
            divisor /= 10u;
        }
        text[length++] = 'x';
        divisor = 10000u;
        while (divisor > 1u && (uint32_t)g_render.height < divisor)
            divisor /= 10u;
        while (divisor != 0u) {
            text[length++] =
                (char)('0' + ((uint32_t)g_render.height / divisor) % 10u);
            divisor /= 10u;
        }
        text[length++] = ' ';
    }
#if J3_SKIP_PROBE
    {
        /* One report window per category, starting with "nothing skipped". */
        static const uint32_t stages[] = {0u,    0x40u, 0x80u, 0x100u, 0x01u,
                                          0x02u, 0x04u, 0x08u, 0x10u,  0x20u,
                                          0x3Eu, 0x3Fu};
        static uint8_t stage;
        uint32_t divisor = 100000u;
        static const char key[] = "skip ";
        for (field = 0; key[field] != '\0'; ++field)
            text[length++] = key[field];
        while (divisor > 1u && j3_skip_probe_mask < divisor) divisor /= 10u;
        while (divisor != 0u) {
            text[length++] = (char)('0' + (j3_skip_probe_mask / divisor) % 10u);
            divisor /= 10u;
        }
        stage = (uint8_t)((stage + 1u) %
                          (uint8_t)(sizeof(stages) / sizeof(stages[0])));
        j3_skip_probe_mask = stages[stage];
    }
#endif
    text[length] = '\0';
    (void)pxa_log_info(text);
    g_perf_ticks = 0;
    g_perf_frames = 0;
    g_perf_raster_us = 0;
    g_perf_present_us = 0;
}
#endif

static int render_frame(void) {
    const uint32_t signature = frame_signature(&g_game);
    if (g_context == 0 || !g_started) return 1;
#if J3_SKIP_STATIC_FRAMES
    if (g_frame_rendered && signature == g_last_signature) return 1;
#endif
    g_last_signature = signature;
    g_frame_rendered = 1;
    (void)signature;
#if J3_PERF_LOG
    ++g_perf_frames;
#endif
    if (!j3_render_frame(&g_render, &g_game, g_context, g_draw, sizeof(g_draw),
                         ++g_frame_id)) {
        if (++g_present_failures >= 3u && g_scale_shift < MAX_SCALE_SHIFT) {
            /* 1 = drop a scale step, 2 = try a sharper step. */
            g_present_failures = 0;
            g_request_scale_change = 1u;
        }
        return 0;
    }
    g_present_failures = 0;
    return 1;
}

/* Recreates the GameRender context at a lower resolution when the Host raster
 * itself is over the frame budget, mirroring the quality ladders of the other
 * games. The panel path, not the raster, is the ceiling at 1x on both boards,
 * so the cap is deliberately high: resolution is only traded away when the
 * raster alone would miss it. */
static void update_quality(void) {
    pxa_raster_telemetry_t telemetry;
    if (pxa_raster_query_telemetry(g_context, &telemetry) !=
        (int32_t)PXA_RASTER_TELEMETRY_BYTES)
        return;
    g_quality_raster_us += telemetry.last_host_raster_us;
#if J3_PERF_LOG
    g_perf_raster_us += telemetry.last_host_raster_us;
    g_perf_present_us = telemetry.last_covered_pixels != 0u
                            ? telemetry.last_covered_pixels
                            : g_perf_present_us;
#endif
    if (++g_quality_frames < QUALITY_WINDOW) return;
    g_quality_total_frames += QUALITY_WINDOW;
    if (J3_FORCE_SCALE_SHIFT < 0 &&
        g_quality_total_frames > QUALITY_WARMUP_FRAMES) {
        const uint32_t average = g_quality_raster_us / QUALITY_WINDOW;
        g_scale_raster_us[g_scale_shift] = average;
        if (average > QUALITY_RASTER_CAP_US) {
            g_quality_underloads = 0;
            /* Sustained overload only: one heavy window (a system UI
             * transition, a resource upload) must not cost the sharper mode. */
            if (++g_quality_overloads >= QUALITY_DOWNGRADE_WINDOWS &&
                g_scale_shift < MAX_SCALE_SHIFT) {
                g_quality_overloads = 0;
                g_request_scale_change = 1u;
            }
        } else {
            g_quality_overloads = 0;
            /* Only step back up into a scale that already proved it can hold the
             * budget, so a mode the ladder left never thrashes back in. */
            if (g_scale_shift > 0u && average <= QUALITY_UPGRADE_US &&
                g_scale_raster_us[g_scale_shift - 1u] <= QUALITY_RASTER_CAP_US) {
                if (++g_quality_underloads >= QUALITY_UPGRADE_WINDOWS) {
                    g_quality_underloads = 0;
                    g_request_scale_change = 2u;
                }
            } else {
                g_quality_underloads = 0;
            }
        }
    }
    g_quality_frames = 0;
    g_quality_raster_us = 0;
}

/* The sky text ramp is baked against the current background scheme, so the
 * palette is rebuilt and re-uploaded when the scheme changes (every 15 jumps).
 * The upload is a couple of milliseconds and lands exactly on the background
 * transition, which hides it. */
static void update_sky_ramp(void) {
    const uint8_t scheme = (uint8_t)((g_game.jump_count / 15u) % J3_BG_SCHEMES);
    if (scheme == g_palette_scheme || g_context == 0) return;
    g_palette_scheme = scheme;
    j3_palette_build_sky_ramp(g_palette, scheme);
    (void)pxa_raster_upload_lit_palette_rgb565(
        g_context, J3_LIGHT_LEVELS, g_palette, g_upload, sizeof(g_upload));
}

/* Persists the best score, debounced so a long run does not stream writes. */
static void update_storage(float dt) {
    uint8_t value[4];
    if (g_game.best <= g_best_saved) return;
    g_save_timer -= dt;
    if (g_save_timer > 0.0F) return;
    g_save_timer = STORAGE_SAVE_DELAY;
    pxa_game_render_store_u32(value, g_game.best);
    if (pxa_storage_set(STORAGE_SET_REQUEST, "best", 4u, value, 4u,
                        g_storage_payload, sizeof(g_storage_payload), g_packet,
                        sizeof(g_packet)))
        g_best_saved = g_game.best;
}

int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    int index;
    pxa_game_screen_from_start(&g_screen, config, config_length);
    g_scale_shift = J3_FORCE_SCALE_SHIFT > 0 ? (uint8_t)J3_FORCE_SCALE_SHIFT
                                             : 0u;
    apply_scale();
    g_frame_id = 0;
    g_last_tick_us = 0;
    g_input_ready = 0;
    g_started = 0;
    g_present_failures = 0;
    g_quality_frames = 0;
    g_quality_raster_us = 0;
    g_request_scale_change = 0;
    g_context = 0;
    g_seed = UINT32_C(0x9E3779B9);
    for (index = 0; index < (int)sizeof(g_game); ++index)
        ((uint8_t *)&g_game)[index] = 0;
    j3_game_reset(&g_game, g_seed);
    g_last_state = g_game.state;
    g_last_score = g_game.score;
    g_last_jumps = g_game.jump_count;
    if (!pxa_window_fullscreen()) return PXA_STATUS_INTERNAL;
    if (!initialize_input_surface()) return PXA_STATUS_INTERNAL;
    j3_audio_start(&g_audio, g_packet, sizeof(g_packet));
    request_context(0, CREATE_REQUEST);
    (void)pxa_log_info("jump-jump-3d ready");
    (void)pxa_storage_get(STORAGE_GET_REQUEST, "best", 4u, g_storage_payload,
                          sizeof(g_storage_payload), g_packet,
                          sizeof(g_packet));
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;

    if (parsed.service == PXA_SERVICE_GAME_RENDER &&
        parsed.opcode == PXA_GAME_RENDER_CREATE_CONTEXT) {
        pxa_game_render_create_result_t created;
        if (!pxa_game_render_parse_create(&parsed, &created))
            return PXA_EVENT_UNHANDLED;
        if (created.status != PXA_STATUS_OK ||
            (created.capabilities & REQUIRED_CAPABILITIES) !=
                REQUIRED_CAPABILITIES) {
            /* No painter polygons: try a smaller target before giving up. */
            if (g_scale_shift < MAX_SCALE_SHIFT) {
                request_context((uint8_t)(g_scale_shift + 1u),
                                (uint32_t)(100u + g_scale_shift));
                return PXA_EVENT_HANDLED;
            }
            return PXA_EVENT_HANDLED;
        }
        g_context = created.context_handle;
        if (!upload_resources()) {
            (void)pxa_log_error("jump-jump-3d: resource upload failed");
            return PXA_EVENT_HANDLED;
        }
        j3_render_configure(&g_render, g_target_w, g_target_h,
                            created.capabilities);
        g_started = 1;
        if (!render_frame()) (void)pxa_log_warn("jump3d first frame failed");
        (void)pxa_clock_set_period(TICK_MS);
        return PXA_EVENT_HANDLED;
    }

    if (j3_audio_handle_event(&g_audio, &parsed, g_packet, sizeof(g_packet)))
        return PXA_EVENT_HANDLED;

    if (parsed.service == PXA_SERVICE_STORAGE &&
        parsed.request_id == STORAGE_GET_REQUEST) {
        pxa_storage_get_result_t result;
        if (pxa_storage_parse_get(&parsed, &result) && result.value != NULL &&
            result.value_length >= 4u) {
            g_game.best = pxa_read_u32(result.value);
            g_best_saved = g_game.best;
        }
        g_storage_ready = 1;
        return PXA_EVENT_HANDLED;
    }

    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        parsed.payload_length == 8) {
        uint64_t timestamp_us = 0;
        uint8_t steps;
        j3_audio_tick(&g_audio, &parsed);
        steps = pxa_clock_tick_steps(&g_last_tick_us, &parsed, TICK_MS,
                                     MAX_CATCHUP_STEPS);
        if (pxa_clock_tick_timestamp_us(&parsed, &timestamp_us))
            g_game.rng ^= (uint32_t)timestamp_us;
        if (steps != 0) {
            uint8_t step;
            for (step = 0; step < steps; ++step)
                j3_game_tick(&g_game, (float)TICK_MS * 0.001F);
            play_state_sounds();
            if (g_storage_ready) update_storage((float)steps * 0.02F);
            update_sky_ramp();
            (void)render_frame();
#if J3_PERF_LOG
            g_perf_ticks += steps;
            if (g_perf_ticks >= 250u) log_perf();
#endif
        }
        if (g_request_scale_change != 0 && g_context != 0) {
            const uint8_t shift = g_request_scale_change == 2u
                                      ? (uint8_t)(g_scale_shift - 1u)
                                      : (uint8_t)(g_scale_shift + 1u);
            g_request_scale_change = 0;
            request_context(shift, (uint32_t)(10u + g_frame_id));
        } else if (g_context != 0) {
            update_quality();
        }
        return PXA_EVENT_HANDLED;
    }

    if (pxa_game_screen_handle_event(&g_screen, &parsed)) {
        apply_scale();
        return PXA_EVENT_HANDLED;
    }

    {
        pxa_ui_pointer_data_t pointer;
        if (pxa_ui_parse_pointer(&parsed, &pointer) &&
            pointer.node == INPUT_NODE) {
            if (pointer.phase == PXA_POINTER_DOWN) {
                if (g_game.state == J3_STATE_OVER) {
                    const uint32_t best = g_game.best;
                    j3_game_reset(&g_game, g_seed ^ (uint32_t)g_frame_id);
                    g_game.best = best;
                    g_save_timer = 0.0F;
                    g_last_state = g_game.state;
                    g_last_score = 0;
                    g_last_jumps = 0;
                    g_bonus_played = 0;
                    j3_audio_play(&g_audio, J3_CHANNEL_LAND, J3_CLIP_START,
                                  J3_GAIN_FULL, 0);
                    j3_audio_stop(&g_audio, J3_CHANNEL_COMBO);
                    j3_audio_stop(&g_audio, J3_CHANNEL_BONUS);
                    g_bonus_repeat_timer = 0.0F;
                } else {
                    j3_game_press(&g_game);
                }
                (void)render_frame();
            } else if (pointer.phase == PXA_POINTER_UP ||
                       pointer.phase == PXA_POINTER_CANCEL) {
                j3_game_release(&g_game);
                play_state_sounds();
                (void)render_frame();
            }
            return PXA_EVENT_HANDLED;
        }
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
    j3_audio_stop_all(&g_audio);
    if (g_audio.session_handle != 0) {
        (void)pxa_close_handle(g_audio.session_handle);
        g_audio.session_handle = 0;
    }
    if (g_context != 0) (void)pxa_close_handle(g_context);
    g_context = 0;
    g_started = 0;
}
