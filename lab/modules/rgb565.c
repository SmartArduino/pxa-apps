#define PXA_LAB_MODULE_PREFIX pxa_lab_rgb565_
#include "pxa_lab_module.h"

#include "pxa_canvas.h"

#define FRAME_NODE UINT32_C(2)
#define FRAME_WIDTH 256u
#define FRAME_HEIGHT 240u
#define FRAME_STRIDE (FRAME_WIDTH * 2u)
#define FRAME_PERIOD_MS 16u
#define FPS_WINDOW_US UINT64_C(500000)
#define STREAM_REQUEST UINT32_C(1)

static uint8_t draw_data[FRAME_STRIDE * FRAME_HEIGHT + 512u];
static uint8_t commands[1];
static uint8_t packet[PXA_CANVAS_STREAM_PACKET_BYTES];
static char metrics[64];
static uint8_t initialized;
static uint8_t phase;
static uint8_t dirty_mode;
static uint8_t force_full_frame;
static uint8_t metrics_dirty;
static uint16_t fps_tenths;
static uint32_t total_frames;
static uint32_t fps_window_frames;
static uint64_t fps_window_start_us;
static uint32_t stream_handle;
static uint8_t pixels_initialized;

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return (uint16_t)(((uint16_t)(red & 0xf8u) << 8) |
                      ((uint16_t)(green & 0xfcu) << 3) | (blue >> 3));
}

static size_t append_text(size_t offset, const char *text) {
    size_t index = 0;
    while (text[index] != '\0' && offset + 1u < sizeof(metrics))
        metrics[offset++] = text[index++];
    metrics[offset] = '\0';
    return offset;
}

static size_t append_u32(size_t offset, uint32_t value) {
    char reversed[10];
    size_t digits = 0;
    do {
        reversed[digits++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && digits < sizeof(reversed));
    while (digits != 0 && offset + 1u < sizeof(metrics))
        metrics[offset++] = reversed[--digits];
    metrics[offset] = '\0';
    return offset;
}

static size_t metrics_length(void) {
    size_t length = 0;
    while (metrics[length] != '\0') ++length;
    return length;
}

static void format_metrics(void) {
    size_t offset = 0;
    if (fps_tenths == 0) {
        offset = append_text(offset, "FPS --.-  F");
    } else {
        offset = append_text(offset, "FPS ");
        offset = append_u32(offset, fps_tenths / 10u);
        offset = append_text(offset, ".");
        if (offset + 1u < sizeof(metrics)) {
            metrics[offset++] = (char)('0' + fps_tenths % 10u);
            metrics[offset] = '\0';
        }
        offset = append_text(offset, "  F");
    }
    offset = append_u32(offset, total_frames);
    offset = append_text(offset, stream_handle != 0 ? " IO" : " CTL");
    (void)append_text(offset, dirty_mode ? " DIRTY" : " FULL");
}

static uint16_t pattern_pixel(uint32_t x, uint32_t y) {
    static const uint16_t bars[] = {
        UINT16_C(0xf800), UINT16_C(0x07e0), UINT16_C(0x001f),
        UINT16_C(0xffe0), UINT16_C(0xf81f), UINT16_C(0x07ff),
        UINT16_C(0xffff), UINT16_C(0x0000),
    };
    if (y < 72u) return bars[((x >> 5) + y / 18u) & 7u];
    if (y < 168u) {
        const uint8_t green = (uint8_t)((y - 72u) * 255u / 95u);
        return rgb565((uint8_t)x, green, (uint8_t)(255u - x));
    }
    {
        const uint8_t light =
            (uint8_t)((((x >> 3) + (y >> 3)) & 1u) != 0 ? 0xe0u : 0x20u);
        return rgb565(light, light, light);
    }
}

static uint16_t *frame_row(uint8_t *first_strip, uint8_t *second_strip,
                           uint32_t y) {
    return (uint16_t *)(
        (y < FRAME_HEIGHT / 2u ? first_strip : second_strip) +
        (size_t)(y % (FRAME_HEIGHT / 2u)) * FRAME_STRIDE);
}

static void generate_frame(uint8_t *first_strip, uint8_t *second_strip) {
    const uint32_t scan_x = (uint8_t)((uint32_t)phase * 3u);
    uint32_t y;
    if (!pixels_initialized) {
        for (y = 0; y < FRAME_HEIGHT; ++y) {
            uint16_t *row = frame_row(first_strip, second_strip, y);
            uint32_t x;
            for (x = 0; x < FRAME_WIDTH; ++x)
                row[x] = pattern_pixel(x, y);
        }
        pixels_initialized = 1;
    } else {
        const uint32_t previous_x =
            (uint8_t)((uint32_t)(uint8_t)(phase - 1u) * 3u);
        for (y = 0; y < FRAME_HEIGHT; ++y)
            frame_row(first_strip, second_strip, y)[previous_x] =
                pattern_pixel(previous_x, y);
    }
    for (y = 0; y < FRAME_HEIGHT; ++y)
        frame_row(first_strip, second_strip, y)[scan_x] = UINT16_C(0xffff);
}

static int render(void) {
    static const char full_title[] = "RGB565 120KiB FULL";
    static const char dirty_title[] = "RGB565 120KiB DIRTY";
    const char *title = dirty_mode ? dirty_title : full_title;
    const size_t title_length = dirty_mode ? sizeof(dirty_title) - 1u
                                           : sizeof(full_title) - 1u;
    pxa_canvas_frame_t frame;
    pxa_canvas_dirty_rect_t dirty[3];
    uint8_t dirty_count = 0;
    uint8_t *first_strip;
    uint8_t *second_strip;
    int presented;
    format_metrics();
    pxa_canvas_begin(&frame, draw_data, sizeof(draw_data));
    if (!pxa_canvas_rect(&frame, 0, 0, 296, 240, 0x101820, 0)) return 0;
    first_strip = pxa_canvas_bitmap_rgb565_reserve(
        &frame, 20, 0, FRAME_WIDTH, FRAME_HEIGHT / 2u, FRAME_STRIDE);
    second_strip = pxa_canvas_bitmap_rgb565_reserve(
        &frame, 20, FRAME_HEIGHT / 2u, FRAME_WIDTH, FRAME_HEIGHT / 2u,
        FRAME_STRIDE);
    if (first_strip == NULL || second_strip == NULL) return 0;
    generate_frame(first_strip, second_strip);
    if (!pxa_canvas_rect_rgba(&frame, 25, 8, 246, 25,
                              pxa_canvas_rgba(0x101820), 3, 0, 0) ||
        !pxa_canvas_text(&frame, 29, 15, 238, 0xffffff,
                         PXA_CANVAS_ALIGN_CENTER, title,
                         title_length) ||
        !pxa_canvas_rect_rgba(&frame, 34, 207, 228, 24,
                              pxa_canvas_rgba(0x101820), 3, 0, 0) ||
        !pxa_canvas_text(&frame, 38, 214, 220, 0xd0dae0,
                         PXA_CANVAS_ALIGN_CENTER, metrics,
                         metrics_length()))
        return 0;
    if (dirty_mode && initialized && !force_full_frame) {
        const int32_t scan_x = 20 + (int32_t)(uint8_t)((uint32_t)phase * 3u);
        const int32_t previous_x =
            20 + (int32_t)(uint8_t)((uint32_t)(uint8_t)(phase - 1u) * 3u);
        dirty[dirty_count++] = (pxa_canvas_dirty_rect_t){
            previous_x, 0, 1, FRAME_HEIGHT};
        dirty[dirty_count++] = (pxa_canvas_dirty_rect_t){
            scan_x, 0, 1, FRAME_HEIGHT};
        if (metrics_dirty)
            dirty[dirty_count++] =
                (pxa_canvas_dirty_rect_t){34, 207, 228, 24};
    }
    force_full_frame = 0;
    presented = stream_handle != 0
                    ? pxa_canvas_present_stream_regions(
                          stream_handle, FRAME_NODE, &frame,
                          &pxa_lab_ui_generation, &initialized, commands,
                          sizeof(commands), packet, sizeof(packet), dirty,
                          dirty_count)
                    : pxa_canvas_present_regions(
                          FRAME_NODE, &frame, &pxa_lab_ui_generation,
                          &initialized, 0, commands, sizeof(commands), packet,
                          sizeof(packet), dirty, dirty_count);
    if (presented) metrics_dirty = 0;
    return presented;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    initialized = 0;
    phase = 0;
    dirty_mode = 0;
    force_full_frame = 0;
    metrics_dirty = 1;
    fps_tenths = 0;
    total_frames = 1;
    fps_window_frames = 0;
    fps_window_start_us = 0;
    stream_handle = 0;
    pixels_initialized = 0;
    if (!pxa_window_fullscreen() || !render()) return PXA_STATUS_INTERNAL;
    (void)pxa_canvas_stream_open(STREAM_REQUEST, FRAME_NODE);
    return pxa_clock_set_period(FRAME_PERIOD_MS) ? PXA_STATUS_OK
                                                 : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_pointer_data_t pointer;
    uint64_t timestamp_us;
    if (!pxa_parse_event(event, length, &parsed))
        return PXA_EVENT_UNHANDLED;
    {
        uint32_t request;
        uint32_t handle;
        int32_t status;
        if (pxa_canvas_parse_stream_ready(&parsed, &request, &handle, &status) &&
            request == STREAM_REQUEST) {
            if (status == PXA_STATUS_OK) stream_handle = handle;
            return PXA_EVENT_HANDLED;
        }
    }
    if (pxa_clock_tick_timestamp_us(&parsed, &timestamp_us)) {
        if (fps_window_start_us == 0) {
            fps_window_start_us = timestamp_us;
        } else if (timestamp_us > fps_window_start_us) {
            const uint64_t elapsed_us = timestamp_us - fps_window_start_us;
            ++fps_window_frames;
            if (elapsed_us >= FPS_WINDOW_US) {
                uint64_t measured =
                    (uint64_t)fps_window_frames * UINT64_C(10000000) /
                    elapsed_us;
                if (measured > UINT16_MAX) measured = UINT16_MAX;
                fps_tenths = (uint16_t)measured;
                fps_window_start_us = timestamp_us;
                fps_window_frames = 0;
                metrics_dirty = 1;
            }
        } else {
            fps_window_start_us = timestamp_us;
            fps_window_frames = 0;
        }
        ++phase;
        ++total_frames;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (!pxa_ui_parse_pointer(&parsed, &pointer) ||
        pointer.phase != PXA_POINTER_DOWN)
        return PXA_EVENT_UNHANDLED;
    dirty_mode = (uint8_t)!dirty_mode;
    force_full_frame = 1;
    metrics_dirty = 1;
    fps_window_start_us = 0;
    fps_window_frames = 0;
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
    if (stream_handle != 0) {
        (void)pxa_close_handle(stream_handle);
        stream_handle = 0;
    }
}
