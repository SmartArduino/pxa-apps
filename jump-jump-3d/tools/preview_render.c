/* preview_render.c - developer harness for Jump Jump 3D.
 *
 * Runs the Guest draw list through the real Host raster kernel
 * (libpxa/src/services/surface/raster.c) at full resolution and writes a PPM
 * per frame, so the game can be inspected without a display, a simulator
 * window or a board. It also drives the game headlessly: automatic jumps land
 * exactly on a block centre (or with a deliberate error) so scoring, camera
 * motion, bonus blocks and the failure animations can all be exercised.
 *
 * Build with tools/preview.sh, for example:
 *   tools/preview.sh --frames 3 --auto 4 --charge 0.34 --out /tmp/opencode/shot
 */
#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

#include "jump3d_font.h"
#include "jump3d_game.h"
#include "jump3d_palette.h"
#include "jump3d_render.h"

#define MAX_TEXTURE_BYTES (256u * 256u)

/* Host raster entry points and their struct layouts. The Guest headers carry
 * their own identical copies of these types, so the harness declares the Host
 * side locally instead of including pxa/raster.h twice. */
typedef struct {
    const uint8_t *pixels;
    uint16_t width;
    uint16_t height;
} host_texture_t;

typedef struct {
    host_texture_t textures[PXA_RASTER_MAX_TEXTURES];
    const uint16_t *palette;
    uint16_t palette_light_levels;
    uint32_t capabilities;
} host_resources_t;

typedef struct {
    uint16_t *pixels;
    uint16_t *depth_pixels;
    uint32_t stride_pixels;
    uint32_t depth_stride_pixels;
    uint16_t width;
    uint16_t height;
    uint32_t prefilled_commands;
} host_target_t;

typedef struct {
    uint16_t abi_minor;
    uint32_t total_size;
    uint32_t required_capabilities;
    uint32_t command_count;
    uint64_t frame_id;
} host_view_t;

typedef struct {
    uint64_t submitted_frames, draw_list_bytes, covered_pixels, host_raster_us;
    uint64_t queue_wait_us, present_us, dropped_frames;
    uint32_t clear_commands, flat_quad_commands, textured_quad_commands;
    uint32_t sprite_commands, rejected_lists, last_draw_list_bytes;
    uint32_t last_covered_pixels, last_host_raster_us;
    uint64_t rendered_frames, visible_frames;
} host_telemetry_t;

typedef struct {
    uint8_t kind;
    uint8_t slot;
    uint16_t width;
    uint16_t height;
    const uint8_t *payload;
    uint32_t payload_bytes;
} host_upload_t;

int pxa_raster_validate_draw_list(const uint8_t *bytes, size_t size,
                                  const host_target_t *target,
                                  const host_resources_t *resources,
                                  host_view_t *output);
void pxa_raster_execute_draw_list(const uint8_t *bytes, const host_view_t *list,
                                  const host_target_t *target,
                                  const host_resources_t *resources,
                                  host_telemetry_t *telemetry);
int pxa_raster_decode_upload(const uint8_t *bytes, size_t size,
                             host_upload_t *output);

static uint16_t g_palette[256u * 32u];
static uint8_t g_texture_bytes[PXA_RASTER_MAX_TEXTURES][MAX_TEXTURE_BYTES];
static host_resources_t g_resources;
static uint8_t g_draw[PXA_RASTER_MAX_DRAW_BYTES];
static uint32_t g_draw_size;
static uint8_t g_submitted;
/* Big enough for the widest panel in the fleet (800x480) plus headroom. */
#define MAX_FRAME_PIXELS (1024u * 600u)
static uint16_t g_pixels[MAX_FRAME_PIXELS];
static uint16_t g_depth[MAX_FRAME_PIXELS];
static uint64_t g_raster_us_total;
static uint32_t g_raster_samples;
static uint32_t g_raster_min_us = UINT32_MAX;
static int g_width = 296;
static int g_height = 240;

int32_t pxa_control(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
    return PXA_STATUS_OK;
}

int32_t pxa_io(uint32_t handle, uint32_t operation, uint8_t *data,
               uint32_t length) {
    (void)handle;
    if (operation == PXA_GAME_RENDER_IO_UPLOAD) {
        host_upload_t upload;
        if (pxa_raster_decode_upload(data, length, &upload) != PXA_STATUS_OK)
            return PXA_STATUS_INVALID_ARGUMENT;
        if (upload.kind == PXA_RASTER_UPLOAD_PALETTE_RGB565) {
            memcpy(g_palette, upload.payload, upload.payload_bytes);
            g_resources.palette = g_palette;
            g_resources.palette_light_levels = 1;
            return (int32_t)length;
        }
        if (upload.kind == PXA_RASTER_UPLOAD_LIT_PALETTE_RGB565) {
            memcpy(g_palette, upload.payload, upload.payload_bytes);
            g_resources.palette = g_palette;
            g_resources.palette_light_levels = upload.height;
            return (int32_t)length;
        }
        if (upload.kind == PXA_RASTER_UPLOAD_TEXTURE_INDEX8) {
            if (upload.slot >= PXA_RASTER_MAX_TEXTURES ||
                upload.payload_bytes > MAX_TEXTURE_BYTES) {
                fprintf(stderr, "upload rejected slot=%u bytes=%u\n",
                        upload.slot, upload.payload_bytes);
                return PXA_STATUS_INVALID_ARGUMENT;
            }
            memcpy(g_texture_bytes[upload.slot], upload.payload,
                   upload.payload_bytes);
            g_resources.textures[upload.slot].pixels =
                g_texture_bytes[upload.slot];
            g_resources.textures[upload.slot].width = upload.width;
            g_resources.textures[upload.slot].height = upload.height;
            return (int32_t)length;
        }
        return PXA_STATUS_UNSUPPORTED;
    }
    if (operation == PXA_GAME_RENDER_IO_SUBMIT) {
        if (length > sizeof(g_draw)) return PXA_STATUS_LIMIT_EXCEEDED;
        memcpy(g_draw, data, length);
        g_draw_size = length;
        g_submitted = 1;
        return (int32_t)length;
    }
    return PXA_STATUS_UNSUPPORTED;
}

/* Mirrors the Host's degenerate-polygon rejection so a bad record can be
 * reported with its coordinates instead of only a status code. */
static void report_degenerate(const uint8_t *bytes, uint32_t size) {
    uint32_t offset = 32;
    uint32_t index = 0;
    while (offset + 4 <= size) {
        const uint8_t type = bytes[offset];
        const uint16_t record_size = (uint16_t)(bytes[offset + 2] |
                                                (bytes[offset + 3] << 8));
        if (record_size < 4 || offset + record_size > size) break;
        if (type == 2) { /* FLAT_QUAD */
            int16_t x[4], y[4];
            int i;
            long long area = 0;
            for (i = 0; i < 4; ++i) {
                x[i] = (int16_t)(bytes[offset + 8 + i * 4] |
                                 (bytes[offset + 9 + i * 4] << 8));
                y[i] = (int16_t)(bytes[offset + 10 + i * 4] |
                                 (bytes[offset + 11 + i * 4] << 8));
            }
            for (i = 0; i < 4; ++i)
                area += (long long)x[i] * y[(i + 1) & 3] -
                        (long long)y[i] * x[(i + 1) & 3];
            if (area == 0)
                printf("  degenerate FLAT_QUAD at record %u: "
                       "(%d,%d)(%d,%d)(%d,%d)(%d,%d)\n",
                       index, x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3]);
        } else if (type == 3) { /* TEXTURED_QUAD */
            int i;
            long long area = 0;
            int16_t x[4], y[4];
            for (i = 0; i < 4; ++i) {
                x[i] = (int16_t)(bytes[offset + 8 + i * 12] |
                                 (bytes[offset + 9 + i * 12] << 8));
                y[i] = (int16_t)(bytes[offset + 10 + i * 12] |
                                 (bytes[offset + 11 + i * 12] << 8));
            }
            for (i = 0; i < 4; ++i)
                area += (long long)x[i] * y[(i + 1) & 3] -
                        (long long)y[i] * x[(i + 1) & 3];
            if (area == 0)
                printf("  degenerate TEXTURED_QUAD at record %u flags=%u: "
                       "(%d,%d)(%d,%d)(%d,%d)(%d,%d)\n",
                       index, bytes[offset + 1], x[0], y[0], x[1], y[1],
                       x[2], y[2], x[3], y[3]);
        } else if (type == 6) { /* TRIANGLE_BATCH */
            const uint16_t count = (uint16_t)(bytes[offset + 8] |
                                              (bytes[offset + 9] << 8));
            uint16_t triangle;
            for (triangle = 0; triangle < count; ++triangle) {
                int16_t x[3], y[3];
                int i;
                long long area;
                for (i = 0; i < 3; ++i) {
                    const uint32_t base = offset + 12 +
                        ((uint32_t)triangle * 3u + (uint32_t)i) * 12u;
                    x[i] = (int16_t)(bytes[base] | (bytes[base + 1] << 8));
                    y[i] = (int16_t)(bytes[base + 2] | (bytes[base + 3] << 8));
                }
                area = (long long)(x[1] - x[0]) * (y[2] - y[0]) -
                       (long long)(y[1] - y[0]) * (x[2] - x[0]);
                if (area == 0)
                    printf("  degenerate TRIANGLE_BATCH record %u tri %u "
                           "color=%u: (%d,%d)(%d,%d)(%d,%d)\n",
                           index, triangle, bytes[offset + 6] |
                           (bytes[offset + 7] << 8), x[0], y[0], x[1], y[1],
                           x[2], y[2]);
            }
        }
        if (index >= 28 && index <= 32) {
            uint32_t k;
            printf("  record %u type=%u size=%u flags=%u count=%u data=", index,
                   type, record_size, bytes[offset + 1],
                   (unsigned)(bytes[offset + 8] | (bytes[offset + 9] << 8)));
            for (k = 0; k < record_size && k < 72u; ++k)
                printf("%02x", bytes[offset + k]);
            printf("\n");
        }
        offset += record_size;
        ++index;
    }
}

static int present(const char *output, int frame) {
    host_view_t view;
    host_target_t target;
    host_telemetry_t telemetry;
    int status;
    char path[512];
    FILE *file;
    int y;
    int x;
    if (!g_submitted) {
        fprintf(stderr, "frame %d: nothing submitted\n", frame);
        return 0;
    }
    memset(&telemetry, 0, sizeof(telemetry));
    memset(&target, 0, sizeof(target));
    target.pixels = g_pixels;
    target.depth_pixels = g_depth;
    target.stride_pixels = (uint32_t)g_width;
    target.depth_stride_pixels = (uint32_t)g_width;
    target.width = (uint16_t)g_width;
    target.height = (uint16_t)g_height;
    status = pxa_raster_validate_draw_list(g_draw, g_draw_size, &target,
                                           &g_resources, &view);
    if (status != PXA_STATUS_OK) {
        fprintf(stderr, "frame %d: draw list rejected status=%d size=%u\n",
                frame, (int)status, g_draw_size);
        report_degenerate(g_draw, g_draw_size);
        return 0;
    }
    memset(g_depth, 0, sizeof(uint16_t) * (size_t)g_width * (size_t)g_height);
    {
        const uint64_t begin = now_us();
        uint8_t warm;
        for (warm = 0; warm < 8; ++warm)
            pxa_raster_execute_draw_list(g_draw, &view, &target, &g_resources,
                                         &telemetry);
        telemetry.last_host_raster_us =
            (uint32_t)((now_us() - begin) / (uint64_t)(warm != 0 ? warm : 1));
    }
    snprintf(path, sizeof(path), "%s-%03d.ppm", output, frame);
    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    fprintf(file, "P6\n%d %d\n255\n", g_width, g_height);
    for (y = 0; y < g_height; ++y) {
        for (x = 0; x < g_width; ++x) {
            const uint16_t pixel = g_pixels[(size_t)y * g_width + x];
            const uint8_t rgb[3] = {
                (uint8_t)(((pixel >> 11) & 31u) * 255u / 31u),
                (uint8_t)(((pixel >> 5) & 63u) * 255u / 63u),
                (uint8_t)((pixel & 31u) * 255u / 31u),
            };
            fwrite(rgb, 1, 3, file);
        }
    }
    fclose(file);
    printf("frame %d: %s bytes=%u commands=%u covered=%u raster_us=%u\n",
           frame, path, g_draw_size, view.command_count,
           telemetry.last_covered_pixels, telemetry.last_host_raster_us);
    g_raster_us_total += telemetry.last_host_raster_us;
    ++g_raster_samples;
    if (telemetry.last_host_raster_us < g_raster_min_us)
        g_raster_min_us = telemetry.last_host_raster_us;
    return 1;
}

/* Exact charge time that lands on `distance` world units away. */
static float charge_for_distance(float distance) {
    /* distance = 7 c * 2 (13.5 + 1.5 c) / 72 */
    const float a = 7.0F * 2.0F * 1.5F / 72.0F;
    const float b = 7.0F * 2.0F * 13.5F / 72.0F;
    const float c = -distance;
    const float discriminant = b * b - 4.0F * a * c;
    if (discriminant <= 0.0F) return 0.0F;
    return (-b + __builtin_sqrtf(discriminant)) / (2.0F * a);
}

int main(int argc, char **argv) {
    j3_game_t game;
    j3_render_t render;
    const char *output = "/tmp/opencode/jump3d";
    int frames = 1;
    int auto_jumps = 0;
    float fixed_charge = -1.0F;
    float error_units = 0.0F;
    int forced_kind = 0;
    int shots = 0;
    int hold = 0;
    int shot_ticks = 4;
    float shot_charge = 0.4F;
    float pose_yaw = 0.0F;
    float pose_lean = 0.0F;
    int pose_set = 0;
    uint32_t skip_mask = 0u;
    int index;
    uint32_t frame_id = 0;
    uint64_t total_us = 0;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--out") == 0 && index + 1 < argc)
            output = argv[++index];
        else if (strcmp(argv[index], "--frames") == 0 && index + 1 < argc)
            frames = atoi(argv[++index]);
        else if (strcmp(argv[index], "--auto") == 0 && index + 1 < argc)
            auto_jumps = atoi(argv[++index]);
        else if (strcmp(argv[index], "--charge") == 0 && index + 1 < argc)
            fixed_charge = (float)atof(argv[++index]);
        else if (strcmp(argv[index], "--yaw") == 0 && index + 1 < argc) {
            pose_yaw = (float)atof(argv[++index]);
            pose_set = 1;
        } else if (strcmp(argv[index], "--lean") == 0 && index + 1 < argc) {
            pose_lean = (float)atof(argv[++index]);
            pose_set = 1;
        } else if (strcmp(argv[index], "--skip") == 0 && index + 1 < argc) {
            skip_mask = (uint32_t)strtoul(argv[++index], NULL, 0);
        } else if (strcmp(argv[index], "--hold") == 0)
            hold = 1;
        else if (strcmp(argv[index], "--shots") == 0 && index + 1 < argc)
            shots = atoi(argv[++index]);
        else if (strcmp(argv[index], "--shot-ticks") == 0 && index + 1 < argc)
            shot_ticks = atoi(argv[++index]);
        else if (strcmp(argv[index], "--shot-charge") == 0 && index + 1 < argc)
            shot_charge = (float)atof(argv[++index]);
        else if (strcmp(argv[index], "--kind") == 0 && index + 1 < argc)
            forced_kind = atoi(argv[++index]);
        else if (strcmp(argv[index], "--error") == 0 && index + 1 < argc)
            error_units = (float)atof(argv[++index]);
        else if (strcmp(argv[index], "--width") == 0 && index + 1 < argc)
            g_width = atoi(argv[++index]);
        else if (strcmp(argv[index], "--height") == 0 && index + 1 < argc)
            g_height = atoi(argv[++index]);
        else {
            fprintf(stderr, "unknown argument: %s\n", argv[index]);
            return 2;
        }
    }

#if J3_SKIP_PROBE
    j3_skip_probe_mask = skip_mask;
#endif
    /* Mirrors the capabilities the simulator and ESP backends advertise. */
    g_resources.capabilities =
        PXA_RASTER_CAP_FLAT_QUAD | PXA_RASTER_CAP_TEXTURED_QUAD |
        PXA_RASTER_CAP_ADDITIVE_SPRITE | PXA_RASTER_CAP_SPRITE_BATCH |
        PXA_RASTER_CAP_TRIANGLE_BATCH | PXA_RASTER_CAP_AFFINE_UV |
        PXA_RASTER_CAP_TEXTURE_SLOTS_48 | PXA_RASTER_CAP_PAINTER_POLYGON |
        PXA_RASTER_CAP_LIT_PALETTE_DEPTH | PXA_RASTER_CAP_DEPTH_CUTOUT |
        PXA_RASTER_CAP_FIXED_ALPHA_BLEND |
        PXA_RASTER_CAP_SPRITE_PALETTE_RAMP |
        PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA;
    j3_palette_build(g_palette);
    g_resources.palette = g_palette;
    g_resources.palette_light_levels = J3_LIGHT_LEVELS;
    {
        /* The Guest uploads through the same helpers, so run them once with a
           scratch buffer big enough for the largest atlas. */
        /* Same size the app allocates, so a missing buffer shows up here. */
        static uint8_t scratch[PXA_RASTER_UPLOAD_HEADER_BYTES +
                               (J3_PALETTE_ENTRIES * 2u >
                                        J3_FONT_MAX_ATLAS_BYTES
                                    ? J3_PALETTE_ENTRIES * 2u
                                    : J3_FONT_MAX_ATLAS_BYTES)];
        j3_palette_build(g_palette);
        if (!j3_font_upload(1, scratch, sizeof(scratch))) {
            fprintf(stderr, "font upload failed\n");
            return 1;
        }
        if (!j3_render_upload_resources(1, scratch, sizeof(scratch))) {
            fprintf(stderr, "shadow upload failed\n");
            return 1;
        }
    }

    j3_game_reset(&game, 0x51ED2701u);
    j3_render_configure(&render, g_width, g_height, g_resources.capabilities);
    if (pose_set) {
        /* Freeze the little man in a chosen heading / tumble so a single frame
         * shows exactly which faces survive culling. */
        game.root_yaw = pose_yaw;
        game.spin = pose_lean;
        game.tilt = 0.0F;
        game.dir_x = 1.0F;
        game.dir_z = 0.0F;
        game.body_scale = 1.0F;
    }

    if (shots > 0) {
        /* One scripted jump with a shot every `shot_ticks` ticks, so the
         * charge squash, the flip, the landing and the failure animations can
         * all be inspected frame by frame. */
        int shot;
        j3_game_press(&game);
        if (hold) {
            /* Render the charge squash itself: one shot every few ticks while
             * the press is still held. */
            for (shot = 0; shot < shots; ++shot) {
                int tick;
                while (game.state == J3_STATE_CHARGING &&
                       game.charge < shot_charge)
                    j3_game_tick(&game, 0.02F);
                g_submitted = 0;
                frame_id += 1;
                if (!j3_render_frame(&render, &game, 1, g_draw, sizeof(g_draw),
                                     frame_id)) {
                    fprintf(stderr, "render failed at shot %d\n", shot);
                    return 1;
                }
                if (!present(output, shot)) return 1;
                printf("  hold %d charge=%.2f squash=%.2f block=%.2f\n",
                       shot, game.charge, game.body_scale,
                       game.blocks[0].squash);
                for (tick = 0; tick < shot_ticks; ++tick)
                    j3_game_tick(&game, 0.02F);
                shot_charge += 0.12F;
            }
            return 0;
        }
        while (game.state == J3_STATE_CHARGING && game.charge < shot_charge)
            j3_game_tick(&game, 0.02F);
        j3_game_release(&game);
        for (shot = 0; shot < shots; ++shot) {
            int tick;
            g_submitted = 0;
            frame_id += 1;
            if (!j3_render_frame(&render, &game, 1, g_draw, sizeof(g_draw),
                                 frame_id)) {
                fprintf(stderr, "render failed at shot %d\n", shot);
                return 1;
            }
            if (!present(output, shot)) return 1;
            printf("  shot %d state=%u py=%.2f spin=%.2f tilt=%.2f "
                   "land=%u\n",
                   shot, game.state, game.py, game.spin, game.tilt,
                   game.land_result);
            for (tick = 0; tick < shot_ticks; ++tick)
                j3_game_tick(&game, 0.02F);
        }
        return 0;
    }

    for (index = 0; index <= auto_jumps; ++index) {
        int frame;
        if (index > 0) {
            /* Charge exactly enough to land on the next block's centre. */
            const j3_block_t *next = &game.blocks[1];
            const float dx = next->x - game.px;
            const float dz = next->z - game.pz;
            const float distance = __builtin_sqrtf(dx * dx + dz * dz) +
                                   error_units;
            float charge = fixed_charge >= 0.0F ? fixed_charge
                                                : charge_for_distance(distance);
            j3_game_press(&game);
            while (game.state == J3_STATE_CHARGING) {
                /* 20 ms ticks, matching the app's clock period. */
                if (game.charge >= charge) {
                    j3_game_release(&game);
                    break;
                }
                j3_game_tick(&game, 0.02F);
            }
            printf("jump %d: distance=%.3f charge=%.3f result=%u\n", index,
                   distance, charge, game.land_result);
        }
        if (forced_kind > 0 && game.state == J3_STATE_READY)
            game.blocks[1].kind = (uint8_t)forced_kind;
        for (frame = 0; frame < (index == 0 ? frames : 1); ++frame) {
            int b;
            for (b = 0; b < game.block_count; ++b)
                printf("  block %d kind=%u x=%.2f z=%.2f r=%.2f appear=%.2f "
                       "squash=%.2f dot=%u\n",
                       b, game.blocks[b].kind, game.blocks[b].x,
                       game.blocks[b].z, game.blocks[b].radius,
                       game.blocks[b].appear, game.blocks[b].squash,
                       game.blocks[b].center_dot);
            printf("  player x=%.2f y=%.2f z=%.2f state=%u camera=%.2f,%.2f\n",
                   game.px, game.py, game.pz, game.state, game.cam_x,
                   game.cam_z);
            g_submitted = 0;
            frame_id += 1;
            if (!j3_render_frame(&render, &game, 1, g_draw, sizeof(g_draw),
                                 frame_id)) {
                fprintf(stderr, "render failed at frame %u\n", frame_id);
                return 1;
            }
            if (!present(output, (int)frame_id)) return 1;
        }
        /* Fly and settle. */
        for (frame = 0; frame < 90; ++frame) j3_game_tick(&game, 0.02F);
        printf("  state=%u score=%u best=%u combo=%u jumps=%u\n", game.state,
               game.score, game.best, game.combo, game.jump_count);
        total_us += 0;
    }
    (void)total_us;
    if (g_raster_samples != 0u)
        printf("raster: frames=%u avg=%u us min=%u us\n",
               (unsigned)g_raster_samples,
               (unsigned)(g_raster_us_total / g_raster_samples),
               (unsigned)g_raster_min_us);
    return 0;
}
