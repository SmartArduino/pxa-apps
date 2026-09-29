/* Maze Evil: full port of micropixel's maze-evil to PXA.
 *
 * The raycaster writes a 148x120 canonical RGB565 frame directly into a
 * GuestMapped triple buffer. The Host performs 2x nearest upscale and the panel
 * transform asynchronously. Older Hosts fall back to the original 296x240
 * copied Surface. Input remains in panel coordinates; sound profiles prefer
 * Host tone commands and retain Guest PCM synthesis as a compatibility path.
 */
#include <stdint.h>

#include "pxa_canvas.h"
#include "assets.h"
#include "audio.h"
#include "font.h"
#include "input.h"
#include "palette.h"
#include "pxa.h"
#include "pxa_mapped_surface.h"
#include "pxa_clock.h"
#include "pxa_surface.h"
#include "pxa_window.h"
#include "raycast.h"
#include "render.h"
#include "world.h"

#define DISPLAY_WIDTH 296
#define DISPLAY_HEIGHT 240
#define VIEW_WIDTH 148
#define VIEW_HEIGHT 120
#define VIEW_SCALE 2
#define SURFACE_BUFFER_COUNT 3u
#define VIEW_PIXELS (VIEW_WIDTH * VIEW_HEIGHT)
#define VIEW_FRAME_BYTES (VIEW_PIXELS * 2u)
#define FALLBACK_FRAME_BYTES (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2u)
#define FRAME_PERIOD_MS 33u
#define STATS_TICKS 30u
#define POINTER_NODE 2u
#define CREATE_REQUEST UINT32_C(1)
#define CONFIGURE_REQUEST UINT32_C(2)
#define QUERY_REQUEST UINT32_C(100)
#define CLOCK_END_REQUEST UINT32_C(201)

typedef union {
    uint16_t mapped[SURFACE_BUFFER_COUNT * VIEW_PIXELS];
    uint16_t fallback[DISPLAY_WIDTH * DISPLAY_HEIGHT];
} frame_storage_t;

static frame_storage_t g_frames
    __attribute__((aligned(PXA_SURFACE_BUFFER_ALIGNMENT)));
static target_t g_target;
static uint8_t g_packet[128];
static uint8_t g_canvas_buffer[64];
static uint8_t g_canvas_packet[128];
static uint32_t g_canvas_generation;
static uint8_t g_canvas_initialized;
static uint64_t g_surface_handle;
static uint32_t g_surface_frame_bytes;
static uint16_t g_surface_width;
static uint16_t g_surface_height;
static uint8_t g_surface_mapped;
static uint8_t g_request_mapped;
static int g_display_width = DISPLAY_WIDTH;
static int g_display_height = DISPLAY_HEIGHT;
static int g_present_scale = VIEW_SCALE;
static int g_present_x;
static int g_present_y;
static uint8_t g_rendered_this_call;
static pxa_mapped_surface_t g_surface_ownership;
static uint64_t g_frame_id;
static uint64_t g_prev_tick_us;
static uint64_t g_now_us;
static int g_started;
static int g_start_touch_down;
static uint32_t g_start_touch_id;

static world_t g_world;
static renderer_t g_renderer;
static touch_controls_t g_touch;
static game_audio_t g_audio;
static hud_stats_t g_hud;

static uint64_t g_tick_sum_us;
static uint32_t g_tick_count;
static uint64_t g_tick_max_us;
static uint32_t g_ticks_since_stats;
static uint32_t g_query_pending;
static uint64_t g_query_sent_us;
static uint64_t g_sample_ts_us;
static uint64_t g_sample_presented;
static uint64_t g_submitted;
static uint64_t g_presented;
static uint64_t g_dropped;
static uint32_t g_free_buffers;
static uint32_t g_blocked;
static uint64_t g_render_ref_us;
static uint64_t g_clock_pending_token;
static uint64_t g_render_sum_us;
static uint64_t g_render_max_us;
static uint32_t g_render_count;
static uint32_t g_render_us;

static void pump_sounds(void) {
    sound_event_t sounds[MAX_PENDING_SOUNDS];
    const int count = world_take_sounds(&g_world, sounds, MAX_PENDING_SOUNDS);
    int index;
    for (index = 0; index < count; ++index) {
        audio_play(&g_audio, sounds[index].id, sounds[index].gain);
    }
}

static int present_mapped_frame(void) {
    const uint64_t frame_id = g_surface_ownership.writing_frame_id;
    int32_t status = pxa_surface_present_buffer(
        g_surface_handle, g_surface_ownership.writing_buffer, frame_id);
    if (status == PXA_STATUS_WOULD_BLOCK) {
        ++g_blocked;
        return 1;
    }
    if (status != 16 ||
        !pxa_mapped_surface_presented(&g_surface_ownership,
                                      SURFACE_BUFFER_COUNT)) {
        return 0;
    }
    g_frame_id = frame_id;
    return 1;
}

static int render_and_submit_frame(void);

/* The Host presents the Surface at the largest exact 1x/2x/4x scale that fits
 * the display (pxa_surface_fit_scale) and centers it because the layer stays at
 * (0, 0). Input arrives in display pixels, so map it back into Surface pixels
 * before drawing the touch controls. */
static void update_present_geometry(void) {
    const int width = g_surface_width != 0
                          ? (int)g_surface_width
                          : (g_request_mapped ? VIEW_WIDTH : DISPLAY_WIDTH);
    const int height = g_surface_height != 0
                           ? (int)g_surface_height
                           : (g_request_mapped ? VIEW_HEIGHT : DISPLAY_HEIGHT);
    const uint32_t scale =
        pxa_surface_fit_scale((uint32_t)width, (uint32_t)height,
                              (uint32_t)g_display_width,
                              (uint32_t)g_display_height);
    g_present_scale = scale != 0 ? (int)scale : 1;
    g_present_x = (g_display_width - width * g_present_scale) / 2;
    g_present_y = (g_display_height - height * g_present_scale) / 2;
    if (g_present_x < 0) g_present_x = 0;
    if (g_present_y < 0) g_present_y = 0;
}

static void apply_display_size(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    g_display_width = (int)width;
    g_display_height = (int)height;
    g_touch.half_width = g_display_width / 2;
    update_present_geometry();
}

static int surface_from_display_x(int value) {
    const int mapped = (value - g_present_x) / g_present_scale;
    return mapped < 0 ? 0 : mapped;
}

static int surface_from_display_y(int value) {
    const int mapped = (value - g_present_y) / g_present_scale;
    return mapped < 0 ? 0 : mapped;
}

static int request_surface(void) {
    pxa_surface_desc_t desc = {0};
    uint32_t packet_size = 0;
    desc.width = g_request_mapped ? VIEW_WIDTH : DISPLAY_WIDTH;
    desc.height = g_request_mapped ? VIEW_HEIGHT : DISPLAY_HEIGHT;
    desc.format = PXA_SURFACE_RGB565;
    desc.buffer_count = SURFACE_BUFFER_COUNT;
    desc.flags = PXA_SURFACE_PREFER_DIRECT_SCANOUT |
                 (g_request_mapped ? PXA_SURFACE_GUEST_MAPPED : 0);
    return pxa_surface_build_create(g_packet, sizeof(g_packet),
                                        CREATE_REQUEST, desc, &packet_size) &&
           pxa_submit(g_packet, packet_size) == PXA_STATUS_OK;
}

static int submit_fallback_frame(void) {
    int32_t status = pxa_surface_write_frame(
        g_surface_handle, (uint8_t *)g_frames.fallback,
        FALLBACK_FRAME_BYTES);
    if (status == PXA_STATUS_WOULD_BLOCK) {
        ++g_blocked;
        return 1;
    }
    if (status != (int32_t)FALLBACK_FRAME_BYTES) return 0;
    uint32_t packet_size = 0;
    if (!pxa_surface_build_queue_frame(
            g_packet, sizeof(g_packet), g_surface_handle, g_frame_id + 1u,
            NULL, 0, &packet_size) ||
        pxa_submit(g_packet, packet_size) != PXA_STATUS_OK) {
        return 0;
    }
    ++g_frame_id;
    return 1;
}

static void render_frame(void) {
    if (g_started) {
        renderer_render(&g_renderer, &g_world, &g_hud, &g_target);
        if (g_touch.stick.down) {
            renderer_draw_stick(
                &g_renderer, &g_target, 1,
                surface_from_display_x(g_touch.stick.origin_x),
                surface_from_display_y(g_touch.stick.origin_y),
                surface_from_display_x(g_touch.stick.x),
                surface_from_display_y(g_touch.stick.y));
        }
    } else {
        hud_stats_t hidden = g_hud;
        hidden.visible = 0;
        renderer_render(&g_renderer, &g_world, &hidden, &g_target);
        renderer_draw_instructions(&g_renderer, &g_target);
    }
}

static int render_and_submit_frame(void) {
    int32_t status;
    g_rendered_this_call = 0;
    if (g_surface_mapped) {
        uint8_t buffer_index;
        if (g_surface_ownership.writing_buffer !=
            PXA_MAPPED_SURFACE_BUFFER_NONE) {
            return present_mapped_frame();
        }
        status = pxa_surface_acquire_buffer(g_surface_handle, &buffer_index);
        if (status == PXA_STATUS_WOULD_BLOCK) {
            ++g_blocked;
            return 1;
        }
        if (status != 4 ||
            !pxa_mapped_surface_begin(&g_surface_ownership, buffer_index,
                                      SURFACE_BUFFER_COUNT, g_frame_id + 1u)) {
            return 0;
        }
        g_target.pixels = g_frames.mapped + (size_t)buffer_index * VIEW_PIXELS;
        g_rendered_this_call = 1;
        render_frame();
        return present_mapped_frame();
    }

    g_target.pixels = g_frames.fallback;
    g_rendered_this_call = 1;
    render_frame();
    return submit_fallback_frame();
}

static void update_stats_from_state(const pxa_surface_state_t *state) {
    if (g_sample_ts_us != 0 && g_query_sent_us > g_sample_ts_us &&
        state->presented_frames >= g_sample_presented) {
        const uint64_t elapsed_us = g_query_sent_us - g_sample_ts_us;
        uint64_t measured =
            (state->presented_frames - g_sample_presented) *
            UINT64_C(10000000) / elapsed_us;
        if (measured > 99999u) {
            measured = 99999u;
        }
        g_hud.fps = (uint32_t)measured / 10u;
    }
    g_sample_ts_us = g_query_sent_us;
    g_sample_presented = state->presented_frames;
    g_submitted = state->submitted_frames;
    g_presented = state->presented_frames;
    g_dropped = state->dropped_frames;
    g_free_buffers = state->free_buffers;
}

#if defined(MAZE_EVIL_METRICS_LOG)
static void emit_metric(uint16_t id, uint32_t value) {
    char line[40] = "maze evil metric ";
    char digits[10];
    uint8_t count = 0;
    uint8_t offset = 17;
    line[offset++] = (char)('0' + (id / 10u) % 10u);
    line[offset++] = (char)('0' + id % 10u);
    line[offset++] = ' ';
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(digits));
    while (count != 0) line[offset++] = digits[--count];
    line[offset] = '\0';
    (void)pxa_log_write(2, line);
}

static void log_metrics(void) {
    emit_metric(1, g_hud.fps);
    emit_metric(2, (uint32_t)(g_render_count == 0
                                  ? 0
                                  : g_render_sum_us / g_render_count));
    emit_metric(3, (uint32_t)g_render_max_us);
    emit_metric(4, g_tick_count == 0
                       ? 0
                       : (uint32_t)(g_tick_sum_us / g_tick_count / 1000u));
    emit_metric(5, (uint32_t)(g_tick_max_us / 1000u));
    emit_metric(6, g_blocked);
    emit_metric(7, (uint32_t)g_presented);
    emit_metric(8, (uint32_t)g_dropped);
}
#endif

static void send_query(uint64_t timestamp_us) {
    uint32_t packet_size = 0;
    if (!pxa_surface_build_query_state(
            g_packet, sizeof(g_packet), QUERY_REQUEST, g_surface_handle,
            &packet_size) ||
        pxa_submit(g_packet, packet_size) != PXA_STATUS_OK) {
        return;
    }
    g_query_sent_us = timestamp_us;
    g_query_pending = 1;
}

static int setup_pointer_node(void) {
    pxa_canvas_frame_t frame;
    pxa_canvas_begin(&frame, g_canvas_buffer, sizeof(g_canvas_buffer));
    return pxa_canvas_present_with_root_event_mask(
        POINTER_NODE, &frame, &g_canvas_generation, &g_canvas_initialized,
        PXA_UI_EVENT_MASK_POINTER, g_canvas_packet, sizeof(g_canvas_packet),
        g_packet, sizeof(g_packet));
}

int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    pxa_ui_environment_t environment;
    if (pxa_ui_parse_start_environment(config, length, &environment))
        apply_display_size(environment.width, environment.height);
    g_surface_handle = 0;
    g_frame_id = 0;
    g_clock_pending_token = 0;
    g_query_pending = 0;
    g_request_mapped = 1;
    g_surface_mapped = 0;
    g_surface_frame_bytes = 0;
    pxa_mapped_surface_reset(&g_surface_ownership);
    g_target.pixels = g_frames.mapped;
    g_target.width = VIEW_WIDTH;
    g_target.height = VIEW_HEIGHT;
    palette_build();
    raycast_init_light();
    font_build_atlas();
    world_reset(&g_world);
    renderer_init(&g_renderer, VIEW_WIDTH, VIEW_HEIGHT, 1);
    touch_init(&g_touch, g_display_width);
    audio_init(&g_audio);
    g_hud.visible = 1;
    g_hud.show_perf = 1;
    if (!setup_pointer_node() ||
        pxa_window_fullscreen() != PXA_STATUS_OK) {
        return PXA_STATUS_INTERNAL;
    }
    if (!request_surface()) {
        return PXA_STATUS_INTERNAL;
    }
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    uint64_t timestamp_us;
    if (!pxa_parse_event(event, length, &parsed)) {
        return PXA_EVENT_UNHANDLED;
    }
    if (audio_handle_event(&g_audio, &parsed, g_packet, sizeof(g_packet))) {
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SURFACE_SERVICE &&
        parsed.opcode == PXA_SURFACE_CREATE &&
        parsed.token == CREATE_REQUEST) {
        pxa_surface_create_result_t result;
        uint32_t packet_size = 0;
        if (!pxa_surface_parse_create(&parsed, CREATE_REQUEST, &result)) {
            return PXA_STATUS_INTERNAL;
        }
        if (result.status == PXA_STATUS_UNSUPPORTED && g_request_mapped) {
            g_request_mapped = 0;
            g_target.pixels = g_frames.fallback;
            g_target.width = DISPLAY_WIDTH;
            g_target.height = DISPLAY_HEIGHT;
            renderer_init(&g_renderer, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
            return request_surface() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        g_surface_width = g_request_mapped ? VIEW_WIDTH : DISPLAY_WIDTH;
        g_surface_height = g_request_mapped ? VIEW_HEIGHT : DISPLAY_HEIGHT;
        update_present_geometry();
        g_surface_frame_bytes = g_request_mapped ? VIEW_FRAME_BYTES
                                                 : FALLBACK_FRAME_BYTES;
        if (result.status != PXA_STATUS_OK ||
            result.stride_bytes != (uint32_t)g_surface_width * 2u ||
            result.frame_bytes != g_surface_frame_bytes ||
            result.buffer_count != SURFACE_BUFFER_COUNT) {
            return PXA_STATUS_INTERNAL;
        }
        g_surface_handle = result.handle;
        g_surface_mapped = g_request_mapped;
        if (g_surface_mapped &&
            pxa_surface_register_buffers(
                g_surface_handle, g_frames.mapped, g_surface_frame_bytes,
                SURFACE_BUFFER_COUNT) !=
                (int32_t)(g_surface_frame_bytes * SURFACE_BUFFER_COUNT)) {
            return PXA_STATUS_INTERNAL;
        }
        return pxa_surface_build_configure_layer(
                   g_packet, sizeof(g_packet), CONFIGURE_REQUEST,
                   g_surface_handle, 0, 0, g_surface_width,
                   g_surface_height, 0, 1, &packet_size) &&
                   pxa_submit(g_packet, packet_size) == PXA_STATUS_OK
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SURFACE_SERVICE &&
        parsed.opcode == PXA_SURFACE_CONFIGURE_LAYER &&
        parsed.token == CONFIGURE_REQUEST) {
        int32_t configure_status;
        if (!pxa_surface_parse_status(
                &parsed, CONFIGURE_REQUEST, PXA_SURFACE_CONFIGURE_LAYER,
                &configure_status) || configure_status != PXA_STATUS_OK) {
            return PXA_STATUS_INTERNAL;
        }
        if (!render_and_submit_frame()) {
            return PXA_STATUS_INTERNAL;
        }
        return pxa_clock_set_period(FRAME_PERIOD_MS) == PXA_STATUS_OK
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_UI_SERVICE &&
        parsed.opcode == PXA_UI_ENVIRONMENT_CHANGED) {
        pxa_ui_environment_t environment;
        if (pxa_ui_parse_environment_event(&parsed, &environment)) {
            apply_display_size(environment.width, environment.height);
            return PXA_EVENT_HANDLED;
        }
    }
    if (parsed.service == PXA_UI_SERVICE && parsed.opcode == PXA_UI_EVENT) {
        pxa_ui_pointer_data_t pointer;
        if (!pxa_ui_parse_pointer(&parsed, &pointer) ||
            (pointer.node != POINTER_NODE && pointer.node != 1u)) {
            return PXA_EVENT_UNHANDLED;
        }
        if (!g_started) {
            if (pointer.phase == PXA_POINTER_DOWN) {
                g_start_touch_down = 1;
                g_start_touch_id = pointer.pointer_id;
            } else if (g_start_touch_down &&
                       pointer.pointer_id == g_start_touch_id &&
                       (pointer.phase == PXA_POINTER_UP ||
                        pointer.phase == PXA_POINTER_CANCEL)) {
                g_start_touch_down = 0;
                if (pointer.phase == PXA_POINTER_UP) {
                    g_started = 1;
                    g_prev_tick_us = 0;
                    g_tick_sum_us = 0;
                    g_tick_count = 0;
                    g_tick_max_us = 0;
                    g_render_sum_us = 0;
                    g_render_max_us = 0;
                    g_render_count = 0;
                }
            }
            return PXA_EVENT_HANDLED;
        }
        if (pointer.phase == PXA_POINTER_DOWN) {
            touch_on_down(&g_touch, pointer.pointer_id, pointer.x, pointer.y,
                          pointer.timestamp_us);
        } else if (pointer.phase == PXA_POINTER_MOVE) {
            touch_on_move(&g_touch, pointer.pointer_id, pointer.x, pointer.y,
                          pointer.timestamp_us);
        } else {
            touch_on_up(&g_touch, pointer.pointer_id, pointer.timestamp_us);
        }
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_CLOCK_SERVICE &&
        parsed.opcode == PXA_CLOCK_NOW) {
        pxa_clock_now_result_t result;
        if (g_clock_pending_token != 0 &&
            pxa_clock_parse_now(&parsed, g_clock_pending_token, &result) &&
            result.status == PXA_STATUS_OK && g_render_ref_us != 0 &&
            result.timestamp_us >= g_render_ref_us) {
            const uint64_t us = result.timestamp_us - g_render_ref_us;
            if (us < UINT64_C(2000000)) {
                g_render_us = (uint32_t)us;
                g_render_sum_us += us;
                ++g_render_count;
                if (us > g_render_max_us) {
                    g_render_max_us = us;
                }
            }
        }
        if (parsed.token == g_clock_pending_token)
            g_clock_pending_token = 0;
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SURFACE_SERVICE &&
        parsed.opcode == PXA_SURFACE_QUERY_STATE && g_query_pending) {
        pxa_surface_state_t result;
        g_query_pending = 0;
        if (!pxa_surface_parse_state(&parsed, QUERY_REQUEST, &result) ||
            result.status != PXA_STATUS_OK) {
            return PXA_STATUS_INTERNAL;
        }
        update_stats_from_state(&result);
        return PXA_EVENT_HANDLED;
    }
    {
        pxa_surface_released_t released;
        if (pxa_surface_parse_released(&parsed, &released)) {
            if (!g_surface_mapped || released.handle != g_surface_handle ||
                !pxa_mapped_surface_released(&g_surface_ownership,
                                             released.buffer_index,
                                             SURFACE_BUFFER_COUNT)) {
                return PXA_EVENT_UNHANDLED;
            }
            return PXA_EVENT_HANDLED;
        }
    }
    if (!pxa_clock_parse_tick(&parsed, &timestamp_us) ||
        g_surface_handle == 0) {
        return PXA_EVENT_UNHANDLED;
    }

    {
        uint64_t dt_us = 0;
        float dt;
        if (g_prev_tick_us != 0 && timestamp_us > g_prev_tick_us) {
            const uint64_t delta_us = timestamp_us - g_prev_tick_us;
            dt_us = delta_us;
            g_tick_sum_us += delta_us;
            ++g_tick_count;
            if (delta_us > g_tick_max_us) {
                g_tick_max_us = delta_us;
            }
        }
        g_prev_tick_us = timestamp_us;
        g_now_us = timestamp_us;
        if (dt_us > 50000u) {
            dt_us = 50000u;
        }
        dt = (float)dt_us * 1e-6F;

        if (g_started) {
            const controls_t controls = touch_consume(&g_touch, timestamp_us);
            const uint8_t phase_before = g_world.phase;
            world_update(&g_world, dt, &controls);
            if (phase_before != PHASE_PLAYING &&
                g_world.phase == PHASE_PLAYING) {
                /* Win and death retries return to the start screen. */
                g_started = 0;
                g_start_touch_down = 0;
                touch_init(&g_touch, g_display_width);
            }
            pump_sounds();
        }
        audio_tick(&g_audio, timestamp_us);

        /* Keep one timestamp request in flight so the reference frame cannot
         * be overwritten before its completion arrives. */
        if (g_clock_pending_token == 0)
            g_render_ref_us = timestamp_us;
        if (!render_and_submit_frame()) {
            return PXA_STATUS_INTERNAL;
        }
        if (g_rendered_this_call && g_clock_pending_token == 0 &&
            pxa_clock_now(CLOCK_END_REQUEST) == PXA_STATUS_OK)
            g_clock_pending_token = CLOCK_END_REQUEST;
    }

    ++g_ticks_since_stats;
    if (g_ticks_since_stats >= STATS_TICKS && !g_query_pending) {
        g_hud.render_ms_x10 =
            (uint32_t)(g_render_count == 0
                           ? 0
                           : g_render_sum_us / g_render_count / 100u);
        g_hud.tick_avg_ms =
            g_tick_count == 0
                ? 0
                : (uint32_t)(g_tick_sum_us / g_tick_count / 1000u);
        g_hud.tick_max_ms = (uint32_t)(g_tick_max_us / 1000u);
#if defined(MAZE_EVIL_METRICS_LOG)
        log_metrics();
#endif
        g_ticks_since_stats = 0;
        g_tick_sum_us = 0;
        g_tick_count = 0;
        g_tick_max_us = 0;
        g_render_sum_us = 0;
        g_render_count = 0;
        g_render_max_us = 0;
        send_query(timestamp_us);
    }
    return PXA_EVENT_HANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    g_surface_handle = 0;
}
