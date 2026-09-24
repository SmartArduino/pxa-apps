/* Tomb Explorer: third-person room-and-portal explorer, ported from micropixel
 * guest/apps/tomb-explorer to the PXA Guest SDK.
 *
 * The original rendered through the micropixel HostSurface / MeshRenderer
 * polygon path. This port keeps the same level, collision, character and
 * camera code and uses the original painter's ordering table, affine polygon
 * subdivision and pre-lit palette through the PXA GameRender raster service.
 *
 * Touch: left half is a stick, dragging on the right half orbits the camera,
 * a tap on the right half (or controller A) jumps.
 *
 * Build options via PXA_APP_DEFINES:
 *   TOMB_RENDER_SCALE_SHIFT=N   override automatic render scale (0..3)
 *   TOMB_BENCHMARK=1            scripted route, fixed 30 Hz step, stats every
 *                               120 frames
 *   TOMB_PERF=1                 stats while playing
 */

#include <stdint.h>

#include "character.h"
#include "input.h"
#include "level.h"
#include "mesh.h"
#include "palette.h"
#include "player.h"
#include "pxa.h"
#include "pxa_canvas.h"
#include "pxa_game_render.h"
#include "pxa_log.h"
#include "pxa_raster.h"
#include "pxa_ui.h"
#include "textures.h"
#include "tomb_math.h"
#include "world.h"

#define TOMB_CREATE_REQUEST UINT32_C(1)
/* Full-screen Canvas root that exists only to receive pointer events. */
#define TOMB_POINTER_NODE UINT32_C(2)
#define TOMB_POINTER_ROOT_NODE UINT32_C(1)
#ifndef TOMB_FRAME_PERIOD_MS
#define TOMB_FRAME_PERIOD_MS 16u
#endif
#define TOMB_BUFFER_COUNT 3u
#define TOMB_STATS_WINDOW_FRAMES 120u
#define TOMB_MAX_FRAME_DT_US UINT64_C(100000)
#define TOMB_BENCHMARK_DT_US UINT64_C(33333)
#define TOMB_FIELD_OF_VIEW 1.25f /* ~72 degrees horizontal */
#define TOMB_NATIVE_SURFACE_PIXEL_LIMIT (UINT64_C(800) * UINT64_C(480))

#ifndef TOMB_RENDER_SCALE_SHIFT
#define TOMB_RENDER_SCALE_SHIFT (-1)
#endif
#if TOMB_RENDER_SCALE_SHIFT < -1 || TOMB_RENDER_SCALE_SHIFT > 3
#error "TOMB_RENDER_SCALE_SHIFT must be -1 (automatic) or between 0 and 3"
#endif

#ifndef TOMB_BENCHMARK
#define TOMB_BENCHMARK 0
#endif
#ifndef TOMB_PERF
#define TOMB_PERF 0
#endif

static uint8_t g_packet[128];
static uint8_t g_canvas_buffer[64];
static uint8_t g_canvas_packet[128];
static uint32_t g_canvas_generation;
static uint8_t g_canvas_initialized;
static uint8_t g_upload[PXA_RASTER_UPLOAD_HEADER_BYTES +
                        TOMB_LIGHT_LEVELS * 256u * sizeof(uint16_t)];
static uint8_t g_draw[PXA_RASTER_MAX_DRAW_BYTES];
static uint16_t g_palette[TOMB_LIGHT_LEVELS * 256u];
static uint32_t g_context;
static uint64_t g_frame_id;
static uint64_t g_last_tick_us;
static uint32_t g_display_width = 296u;
static uint32_t g_display_height = 240u;
static uint32_t g_render_width = 296u;
static uint32_t g_render_height = 240u;
static uint8_t g_render_scale_shift;
static uint32_t g_frame_index;
static uint8_t g_have_surface;

static tomb_renderer_t g_renderer;
static tomb_player_t g_player;
static tomb_transform_t g_identity;
static const tomb_level_t *g_level;

typedef struct {
    uint32_t frames;
    uint64_t window_start_us;
    uint32_t faces;
    uint32_t culled;
    uint32_t subdivided;
    uint32_t polygons;
    uint32_t dropped;
    uint32_t pixel_estimate;
    uint32_t rooms;
} tomb_stats_t;

static tomb_stats_t g_stats;

/* Scripted route for TOMB_BENCHMARK: a loop through every room. */
#if TOMB_BENCHMARK
static const float kRoute[][2] = {
    {3.5f, 4.5f},   {3.5f, 10.5f}, {3.5f, 13.5f}, {4.5f, 15.5f},  {8.5f, 15.0f},  {12.5f, 15.5f},
    {13.5f, 17.0f}, {8.5f, 15.5f}, {0.0f, 15.0f}, {-4.5f, 15.5f}, {-6.5f, 14.0f}, {-2.5f, 15.0f},
    {2.0f, 13.0f},  {3.5f, 10.5f}, {3.5f, 4.5f},  {1.0f, 1.5f},   {5.5f, 1.5f},   {3.5f, 1.5f},
};
static const uint32_t kRouteLength = sizeof(kRoute) / sizeof(kRoute[0]);
static uint32_t g_route_index;
#endif

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)(((uint16_t)(red >> 3) << 11) |
                      ((uint16_t)(green >> 2) << 5) | (blue >> 3));
}

static uint8_t choose_render_scale_shift(uint32_t width, uint32_t height) {
#if TOMB_RENDER_SCALE_SHIFT >= 0
    (void)width;
    (void)height;
    return (uint8_t)TOMB_RENDER_SCALE_SHIFT;
#else
    const uint64_t pixels = (uint64_t)width * height;
    return pixels > TOMB_NATIVE_SURFACE_PIXEL_LIMIT && (width & 1u) == 0u &&
                   (height & 1u) == 0u
               ? 1u
               : 0u;
#endif
}

/* A GameRender component receives pointer events only through a UI node.
 * Present an empty full-screen Canvas root with the pointer event mask. */
static int setup_pointer_node(void) {
    pxa_canvas_frame_t frame;
    pxa_canvas_begin(&frame, g_canvas_buffer, sizeof(g_canvas_buffer));
    return pxa_canvas_present_with_root_event_mask(
        TOMB_POINTER_NODE, &frame, &g_canvas_generation, &g_canvas_initialized,
        PXA_UI_EVENT_MASK_POINTER, g_canvas_packet, sizeof(g_canvas_packet),
        g_packet, sizeof(g_packet));
}

static int upload_resources(void) {
    uint8_t slot;
    if (pxa_raster_upload_lit_palette_rgb565(
            g_context, TOMB_LIGHT_LEVELS, g_palette, g_upload,
            sizeof(g_upload)) !=
        (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                  TOMB_LIGHT_LEVELS * 256u * sizeof(uint16_t)))
        return 0;
    for (slot = 0u; slot < TOMB_TEXTURE_COUNT; ++slot) {
        const uint8_t *texels = tomb_texture_texels(slot);
        if (texels == 0 ||
            pxa_raster_upload_texture_index8(
                g_context, slot, TOMB_TEXTURE_SIZE, TOMB_TEXTURE_SIZE, texels,
                g_upload, sizeof(g_upload)) !=
                (int32_t)(PXA_RASTER_UPLOAD_HEADER_BYTES +
                          TOMB_TEXTURE_SIZE * TOMB_TEXTURE_SIZE))
            return 0;
    }
    return 1;
}

static void append_rect(pxa_raster_draw_list_t *list, int x, int y, int width,
                        int height, uint16_t color) {
    int16_t xy[8];
    xy[0] = xy[6] = (int16_t)(x << 4);
    xy[1] = xy[3] = (int16_t)(y << 4);
    xy[2] = xy[4] = (int16_t)((x + width) << 4);
    xy[5] = xy[7] = (int16_t)((y + height) << 4);
    (void)pxa_raster_flat_quad(list, xy, color);
}

static void draw_stick_overlay(pxa_raster_draw_list_t *list) {
    const tomb_stick_overlay_t overlay = tomb_input_overlay();
    int ring;
    int knob;
    int ox;
    int oy;
    int kx;
    int ky;
    const uint16_t ring_color = rgb565(220u, 220u, 220u);
    const uint16_t knob_color = rgb565(255u, 210u, 90u);
    if (!overlay.stick_active) return;
    /* Panel coordinates are emitted directly; the draw list shift scales all
     * coordinates to the render surface. */
    ring = 70 * (int)g_display_width / 480;
    if (ring < 20) ring = 20;
    knob = 14 * (int)g_display_width / 480;
    if (knob < 6) knob = 6;
    ox = overlay.origin_x;
    oy = overlay.origin_y;
    append_rect(list, ox - ring, oy - ring, ring * 2, 2, ring_color);
    append_rect(list, ox - ring, oy + ring - 2, ring * 2, 2, ring_color);
    append_rect(list, ox - ring, oy - ring, 2, ring * 2, ring_color);
    append_rect(list, ox + ring - 2, oy - ring, 2, ring * 2, ring_color);
    kx = overlay.x;
    ky = overlay.y;
    if (kx < ox - ring) kx = ox - ring;
    if (kx > ox + ring) kx = ox + ring;
    if (ky < oy - ring) ky = oy - ring;
    if (ky > oy + ring) ky = oy + ring;
    append_rect(list, kx - knob / 2, ky - knob / 2, knob, knob, knob_color);
}

static tomb_camera_t build_camera(void) {
    tomb_camera_t camera;
    tomb_player_camera(&g_player, tomb_camera_focal_length(
                                      TOMB_FIELD_OF_VIEW, (int)g_display_width),
                       &camera);
    return camera;
}

#if TOMB_BENCHMARK
static tomb_controls_t autopilot(void) {
    tomb_controls_t controls;
    const tomb_vec3_t position = g_player.position;
    const float dx = kRoute[g_route_index][0] - position.x;
    const float dz = kRoute[g_route_index][1] - position.z;
    float heading;
    float relative;
    controls.forward = 0.0f;
    controls.strafe = 0.0f;
    controls.orbit = 0.0f;
    controls.tilt = 0.0f;
    controls.jump = 0;
    if (dx * dx + dz * dz < 0.35f * 0.35f) {
        g_route_index = (g_route_index + 1u) % kRouteLength;
    }
    heading = tomb_atan2(dx, dz);
    relative = tomb_wrap_angle(heading - g_player.camera_yaw);
    controls.forward = tomb_cos(relative);
    controls.strafe = tomb_sin(relative);
    /* A gentle camera sway exercises the portal scissors from changing angles. */
    controls.tilt = tomb_sin((float)g_frame_index * 0.02f) * 0.0025f;
    controls.jump = (g_frame_index % 300u) == 150u;
    return controls;
}
#endif

static int render_frame(void) {
    pxa_raster_draw_list_t list;
    tomb_render_config_t config;
    tomb_lighting_t lighting;
    tomb_camera_t camera;
    tomb_visible_room_t visible[TOMB_MAX_VISIBLE];
    tomb_rect_t view;
    uint32_t count;
    uint32_t index;
    int submitted;
    pxa_raster_draw_list_begin_scaled(&list, g_draw, sizeof(g_draw),
                                      g_frame_id + 1u,
                                      g_render_scale_shift);
    if (g_frame_id == 0u) (void)pxa_raster_clear(&list, 0u);
    camera = build_camera();
    lighting.levels = (uint8_t)TOMB_LIGHT_LEVELS;
    lighting.minimum = 1u;
    lighting.full_distance = 5.0f;
    lighting.dark_distance = 40.0f;
    config.width = (int)g_display_width;
    config.height = (int)g_display_height;
    config.far = 48.0f;
    config.subdivide_depth_ratio = 1.6f;
    config.subdivide_min_pixels = config.width / 4;
    config.subdivide_levels = 2u;
    tomb_renderer_begin(&g_renderer, &list, &camera, &lighting, &config);
    view.x = 0;
    view.y = 0;
    view.width = config.width;
    view.height = config.height;
    count = tomb_world_compute_visible(g_level, &g_renderer, g_player.camera_room,
                                       view, visible, TOMB_MAX_VISIBLE);
    for (index = 0u; index < count; ++index) {
        const tomb_visible_room_t *entry = &visible[index];
        const tomb_room_t *room = &g_level->rooms[entry->room];
        tomb_submit_options_t options;
        options.group = entry->group;
        options.depth_bias = 0.0f;
        options.scissor = entry->scissor;
        (void)tomb_renderer_submit(&g_renderer, room->vertices,
                                   room->vertex_count, room->faces,
                                   room->face_count, &g_identity, &options);
        if (entry->room == g_player.room) {
            const uint32_t light = (uint32_t)room->ambient + 110u;
            (void)tomb_character_submit(
                &g_renderer, g_player.position, g_player.yaw, &g_player.pose,
                (uint8_t)(light > 255u ? 255u : light), entry->group,
                entry->scissor);
        }
    }
    (void)tomb_renderer_flush(&g_renderer);
    draw_stick_overlay(&list);
    g_stats.faces += g_renderer.stats.faces;
    g_stats.culled += g_renderer.stats.culled;
    g_stats.subdivided += g_renderer.stats.subdivided;
    g_stats.polygons += g_renderer.stats.polygons;
    g_stats.dropped += g_renderer.stats.dropped;
    g_stats.pixel_estimate += g_renderer.stats.pixel_estimate;
    g_stats.rooms += count;
    submitted = pxa_raster_submit(g_context, &list);
    if (submitted > 0) {
        ++g_frame_id;
        return 1;
    }
    return submitted == PXA_STATUS_WOULD_BLOCK ? 1 : 0;
}

static char *append_text(char *out, const char *text) {
    while (*text != '\0') *out++ = *text++;
    return out;
}

static char *append_uint(char *out, uint32_t value) {
    char digits[10];
    int count = 0;
    if (value == 0u) {
        *out++ = '0';
        return out;
    }
    while (value != 0u && count < 10) {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count > 0) *out++ = digits[--count];
    return out;
}

#if TOMB_BENCHMARK || TOMB_PERF
static void log_stats(uint64_t now_us) {
    const uint64_t elapsed_us = now_us - g_stats.window_start_us;
    const uint32_t frames = g_stats.frames == 0u ? 1u : g_stats.frames;
    char line[192];
    char *out = line;
    out = append_text(out, TOMB_BENCHMARK ? "tomb-bench: frames=" : "tomb: frames=");
    out = append_uint(out, frames);
    out = append_text(out, " fps_x100=");
    out = append_uint(out, (uint32_t)(elapsed_us == 0u
                                          ? 0u
                                          : (uint64_t)frames * 100000000ULL / elapsed_us));
    out = append_text(out, " rooms=");
    out = append_uint(out, g_stats.rooms / frames);
    out = append_text(out, " faces=");
    out = append_uint(out, g_stats.faces / frames);
    out = append_text(out, " culled=");
    out = append_uint(out, g_stats.culled / frames);
    out = append_text(out, " polygons=");
    out = append_uint(out, g_stats.polygons / frames);
    out = append_text(out, " subdivided=");
    out = append_uint(out, g_stats.subdivided / frames);
    out = append_text(out, " overdraw_x100=");
    out = append_uint(out,
                      g_stats.pixel_estimate * 100ULL / frames /
                          ((uint64_t)g_display_width * g_display_height));
    out = append_text(out, " dropped=");
    out = append_uint(out, g_stats.dropped);
    out = append_text(out, " room=");
    out = append_uint(out, g_player.room);
    *out = '\0';
    (void)pxa_log_info(line);
}
#endif

static void reset_stats(uint64_t now_us) {
    g_stats.frames = 0u;
    g_stats.window_start_us = now_us;
    g_stats.faces = 0u;
    g_stats.culled = 0u;
    g_stats.subdivided = 0u;
    g_stats.polygons = 0u;
    g_stats.dropped = 0u;
    g_stats.pixel_estimate = 0u;
    g_stats.rooms = 0u;
}

static void handle_tick(uint64_t timestamp_us) {
    uint64_t dt_us;
    float dt;
    tomb_controls_t controls;
    if (!g_have_surface || g_context == 0u) return;
    if (g_last_tick_us == 0u) {
        g_last_tick_us = timestamp_us;
        g_stats.window_start_us = timestamp_us;
        return;
    }
    dt_us = timestamp_us - g_last_tick_us;
    g_last_tick_us = timestamp_us;
    if (dt_us > TOMB_MAX_FRAME_DT_US) dt_us = TOMB_MAX_FRAME_DT_US;
#if TOMB_BENCHMARK
    dt_us = TOMB_BENCHMARK_DT_US;
    controls = autopilot();
#else
    controls = tomb_input_consume();
#endif
    dt = (float)dt_us * 1e-6f;
    tomb_player_update(&g_player, g_level, &controls, dt);
    (void)render_frame();
    ++g_frame_index;
    ++g_stats.frames;
#if TOMB_BENCHMARK || TOMB_PERF
    if (g_stats.frames >= TOMB_STATS_WINDOW_FRAMES) {
        log_stats(timestamp_us);
        reset_stats(timestamp_us);
    }
#else
    if (g_stats.frames >= TOMB_STATS_WINDOW_FRAMES) reset_stats(timestamp_us);
#endif
}

int32_t pxa_app_start(const uint8_t *config, uint32_t length) {
    pxa_ui_environment_t environment;
    if (pxa_ui_parse_start_environment(config, length, &environment) &&
        environment.width > 0u && environment.height > 0u) {
        g_display_width = environment.width;
        g_display_height = environment.height;
    }
    g_render_scale_shift =
        choose_render_scale_shift(g_display_width, g_display_height);
    g_render_width = g_display_width >> g_render_scale_shift;
    g_render_height = g_display_height >> g_render_scale_shift;
    if (g_render_width == 0u) g_render_width = 1u;
    if (g_render_height == 0u) g_render_height = 1u;
    if (g_render_width > UINT16_MAX || g_render_height > UINT16_MAX) {
        (void)pxa_log_error("tomb: display dimensions exceed GameRender ABI");
        return PXA_STATUS_LIMIT_EXCEEDED;
    }
    g_context = 0u;
    g_frame_id = 0u;
    g_last_tick_us = 0u;
    g_frame_index = 0u;
    g_have_surface = 0u;
    g_level = tomb_level();
    g_stats = (tomb_stats_t){0};
    tomb_transform_identity(&g_identity);
    if (!pxa_game_render_create(TOMB_CREATE_REQUEST, (uint16_t)g_render_width,
                                (uint16_t)g_render_height, TOMB_BUFFER_COUNT,
                                1u, g_packet, sizeof(g_packet))) {
        (void)pxa_log_error("tomb: GameRender create request failed");
        return PXA_STATUS_INTERNAL;
    }
    if (!setup_pointer_node() || !pxa_window_fullscreen()) {
        (void)pxa_log_error("tomb: pointer node or fullscreen request failed");
        return PXA_STATUS_INTERNAL;
    }
    tomb_build_palette(g_palette);
    tomb_character_init();
    tomb_input_init((int)g_display_width);
    tomb_renderer_init(&g_renderer, TOMB_MAX_GROUPS);
    tomb_player_reset(&g_player, g_level);
#if TOMB_BENCHMARK
    g_route_index = 0u;
#endif
    return PXA_STATUS_OK;
}

static int handle_create_event(const pxa_event_t *event) {
    pxa_game_render_create_result_t created;
    const uint32_t required = PXA_RASTER_CAP_FLAT_QUAD |
                              PXA_RASTER_CAP_TEXTURED_QUAD |
                              PXA_RASTER_CAP_TRIANGLE_BATCH |
                              PXA_RASTER_CAP_PAINTER_POLYGON;
    if (event->request_id != TOMB_CREATE_REQUEST) return 0;
    if (!pxa_game_render_parse_create(event, &created)) return 0;
    if (created.status != PXA_STATUS_OK) {
        (void)pxa_log_error("tomb: GameRender context creation failed");
        return 1;
    }
    if ((created.capabilities & required) != required) {
        (void)pxa_close_handle(created.context_handle);
        (void)pxa_log_error("tomb: GameRender raster capabilities missing");
        return 1;
    }
    g_context = created.context_handle;
    if (!upload_resources()) {
        (void)pxa_close_handle(g_context);
        g_context = 0u;
        (void)pxa_log_error("tomb: raster resource upload failed");
        return 1;
    }
    g_have_surface = 1u;
    (void)render_frame();
#if TOMB_BENCHMARK
    (void)pxa_clock_set_period(33u);
#else
    (void)pxa_clock_set_period(TOMB_FRAME_PERIOD_MS);
#endif
    {
        char line[128];
        char *out = line;
        out = append_text(out, "tomb: ");
        out = append_uint(out, g_display_width);
        out = append_text(out, "x");
        out = append_uint(out, g_display_height);
        out = append_text(out, " GameRender render=");
        out = append_uint(out, g_render_width);
        out = append_text(out, "x");
        out = append_uint(out, g_render_height);
        out = append_text(out, " scale=");
        out = append_uint(out, 1u << g_render_scale_shift);
        out = append_text(out, " rooms=");
        out = append_uint(out, g_level->room_count);
        out = append_text(out, " touch: left stick, right drag orbits, tap jumps");
        *out = '\0';
        (void)pxa_log_info(line);
    }
    return 1;
}

int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t length) {
    pxa_event_t event;
    uint64_t timestamp_us;
    pxa_ui_pointer_data_t pointer;
    pxa_ui_controller_data_t controller;
    if (!pxa_parse_event(bytes, length, &event)) return PXA_EVENT_UNHANDLED;
    if (handle_create_event(&event)) return PXA_EVENT_HANDLED;
    if (pxa_ui_parse_pointer(&event, &pointer)) {
        if (pointer.node == TOMB_POINTER_NODE ||
            pointer.node == TOMB_POINTER_ROOT_NODE)
            tomb_input_pointer(&pointer);
        return PXA_EVENT_HANDLED;
    }
    if (pxa_ui_parse_controller(&event, &controller)) {
        if (controller.connected) tomb_input_button(controller.buttons);
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
    (void)pxa_clock_set_period(0);
    if (g_context != 0u) (void)pxa_close_handle(g_context);
    g_context = 0u;
    g_have_surface = 0u;
}
