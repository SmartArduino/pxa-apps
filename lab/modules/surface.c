#define PXA_LAB_MODULE_PREFIX pxa_lab_surface_
#include "pxa_lab_module.h"

#include "pxa_surface.h"
#include "pxa_ui.h"

#define SURFACE_WIDTH 240u
#define SURFACE_HEIGHT 176u
#define SURFACE_X 28
#define SURFACE_Y 32
#define SURFACE_FRAME_BYTES (SURFACE_WIDTH * SURFACE_HEIGHT * 2u)
#define CREATE_REQUEST UINT32_C(1)
#define CONFIGURE_REQUEST UINT32_C(2)
#define CAPABILITY_REQUEST UINT32_C(3)
#define OVERLAY_REQUEST UINT32_C(4)
#define FRAME_PERIOD_MS 16u
#define FPS_WINDOW_US UINT64_C(500000)

static uint16_t pixels[SURFACE_WIDTH * SURFACE_HEIGHT];
static uint8_t ui_packet[768];
static uint8_t surface_packet[128];
static char metrics[72];
static char overlay_label[32];
static uint32_t surface_handle;
static uint64_t frame_id;
static uint64_t stats_started_us;
static uint64_t query_sent_us;
static uint64_t sample_us;
static uint64_t sample_presented_frames;
static uint32_t blocked_frames;
static uint16_t fps_tenths;
static uint16_t scan_x;
static uint32_t query_request;
static uint8_t query_pending;
static uint64_t presented_frames;
static uint64_t dropped_frames;
static uint32_t free_buffers;
static uint16_t overlay_taps;

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

static size_t append_u64(size_t offset, uint64_t value) {
    char reversed[20];
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

static size_t overlay_label_length(void) {
    size_t length = 0;
    while (overlay_label[length] != '\0') ++length;
    return length;
}

static void format_overlay_label(void) {
    static const char prefix[] = "LVGL overlay ";
    size_t offset = 0;
    char reversed[5];
    size_t digits = 0;
    uint16_t value = overlay_taps;
    while (prefix[offset] != '\0' && offset + 1u < sizeof(overlay_label)) {
        overlay_label[offset] = prefix[offset];
        ++offset;
    }
    do {
        reversed[digits++] = (char)('0' + value % 10u);
        value = (uint16_t)(value / 10u);
    } while (value != 0);
    while (digits != 0 && offset + 1u < sizeof(overlay_label))
        overlay_label[offset++] = reversed[--digits];
    overlay_label[offset] = '\0';
}

static void format_metrics(void) {
    size_t offset = append_text(0, "FPS ");
    if (fps_tenths == 0) {
        offset = append_text(offset, "--.-");
    } else {
        offset = append_u64(offset, fps_tenths / 10u);
        offset = append_text(offset, ".");
        if (offset + 1u < sizeof(metrics)) {
            metrics[offset++] = (char)('0' + fps_tenths % 10u);
            metrics[offset] = '\0';
        }
    }
    offset = append_text(offset, " Q");
    offset = append_u64(offset, frame_id);
    offset = append_text(offset, " P");
    offset = append_u64(offset, presented_frames);
    offset = append_text(offset, " D");
    offset = append_u64(offset, dropped_frames);
    offset = append_text(offset, " F");
    offset = append_u64(offset, free_buffers);
    offset = append_text(offset, " B");
    (void)append_u64(offset, blocked_frames);
}

static int render_chrome(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t next = pxa_lab_ui_generation + 1u;
    int ok;
    format_metrics();
    format_overlay_label();
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  ui_packet, sizeof(ui_packet)))
        return 0;
    ok = pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_theme_color(&transaction, 1,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 2, 1, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_X,
                           PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_Y,
                           PXA_UI_LENGTH_PX, 7) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 2, "PXA Surface / RGB565",
                         sizeof("PXA Surface / RGB565") - 1u) &&
         pxa_ui_set_font_role(&transaction, 2, PXA_UI_FONT_ROLE_BODY) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 2,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 3, 1, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u8(&transaction, 3, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&transaction, 3, PXA_UI_PROPERTY_X,
                           PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_length(&transaction, 3, PXA_UI_PROPERTY_Y,
                           PXA_UI_LENGTH_PX, 214) &&
         pxa_ui_set_length(&transaction, 3, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 3, metrics, metrics_length()) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_u8(&transaction, 3, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 3,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create_typed(&transaction, 4, 1, 0,
                             PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
         pxa_ui_set_u8(&transaction, 4, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_X,
                           PXA_UI_LENGTH_PX, SURFACE_X + 8) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_Y,
                           PXA_UI_LENGTH_PX, SURFACE_Y + 8) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_PX, 124) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 34) &&
         pxa_ui_set_theme_color(&transaction, 4,
                                PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_set_theme_color(&transaction, 4,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_set_event_mask(&transaction, 4, PXA_UI_EVENT_MASK_CLICK) &&
         pxa_ui_create(&transaction, 5, 4, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 5, overlay_label,
                         overlay_label_length()) &&
         pxa_ui_set_theme_color(&transaction, 5,
                                PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY);
    if (!ok) {
        (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    if (!pxa_ui_transaction_commit(&transaction)) return 0;
    pxa_lab_ui_generation = next;
    return 1;
}

static int render_metrics(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t next = pxa_lab_ui_generation + 1u;
    format_metrics();
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_PATCH,
                                  ui_packet, sizeof(ui_packet)))
        return 0;
    if (!pxa_ui_set_text(&transaction, 3, metrics, metrics_length())) {
        (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    if (!pxa_ui_transaction_commit(&transaction)) return 0;
    pxa_lab_ui_generation = next;
    return 1;
}

static int render_overlay_label(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t next = pxa_lab_ui_generation + 1u;
    format_overlay_label();
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_PATCH,
                                  ui_packet, sizeof(ui_packet)))
        return 0;
    if (!pxa_ui_set_text(&transaction, 5, overlay_label,
                         overlay_label_length())) {
        (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    if (!pxa_ui_transaction_commit(&transaction)) return 0;
    pxa_lab_ui_generation = next;
    return 1;
}

static void initialize_pixels(void) {
    uint32_t y;
    for (y = 0; y < SURFACE_HEIGHT; ++y) {
        uint32_t x;
        for (x = 0; x < SURFACE_WIDTH; ++x) {
            const uint8_t red = (uint8_t)(x * 255u / (SURFACE_WIDTH - 1u));
            const uint8_t green =
                (uint8_t)(y * 255u / (SURFACE_HEIGHT - 1u));
            const uint8_t blue = (uint8_t)(255u - red);
            pixels[y * SURFACE_WIDTH + x] = rgb565(red, green, blue);
        }
    }
}

static void update_scanline(void) {
    const uint16_t previous = scan_x;
    uint32_t y;
    scan_x = (uint16_t)((scan_x + 3u) % SURFACE_WIDTH);
    for (y = 0; y < SURFACE_HEIGHT; ++y) {
        const uint8_t red =
            (uint8_t)(previous * 255u / (SURFACE_WIDTH - 1u));
        const uint8_t green =
            (uint8_t)(y * 255u / (SURFACE_HEIGHT - 1u));
        pixels[y * SURFACE_WIDTH + previous] =
            rgb565(red, green, (uint8_t)(255u - red));
        pixels[y * SURFACE_WIDTH + scan_x] = UINT16_C(0xffff);
    }
}

static int submit_frame(void) {
    pxa_surface_damage_rect_t damage[2];
    int32_t status;
    const uint16_t previous =
        (uint16_t)((scan_x + SURFACE_WIDTH - 3u) % SURFACE_WIDTH);
    status = pxa_surface_write_frame(
        surface_handle, (uint8_t *)pixels, SURFACE_FRAME_BYTES);
    if (status == PXA_STATUS_WOULD_BLOCK) {
        ++blocked_frames;
        return 1;
    }
    if (status != (int32_t)SURFACE_FRAME_BYTES) return 0;
    damage[0] = (pxa_surface_damage_rect_t){previous, 0, 1,
                                             SURFACE_HEIGHT};
    damage[1] = (pxa_surface_damage_rect_t){scan_x, 0, 1,
                                             SURFACE_HEIGHT};
    status = pxa_surface_queue_frame(surface_handle, frame_id + 1u,
                                      damage, 2, surface_packet,
                                      sizeof(surface_packet));
    if (status != PXA_STATUS_OK) return 0;
    ++frame_id;
    return 1;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    surface_handle = 0;
    frame_id = 0;
    stats_started_us = 0;
    query_sent_us = 0;
    sample_us = 0;
    sample_presented_frames = 0;
    blocked_frames = 0;
    fps_tenths = 0;
    scan_x = 0;
    query_request = OVERLAY_REQUEST;
    query_pending = 0;
    presented_frames = 0;
    dropped_frames = 0;
    free_buffers = 0;
    overlay_taps = 0;
    initialize_pixels();
    if (!pxa_window_fullscreen() || !render_chrome() ||
        !pxa_surface_create_rgb565(CREATE_REQUEST, SURFACE_WIDTH,
                                    SURFACE_HEIGHT, 3, surface_packet,
                                    sizeof(surface_packet)))
        return PXA_STATUS_INTERNAL;
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    uint64_t timestamp_us;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) && ui_event.node == 4 &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND) {
        ++overlay_taps;
        return render_overlay_label() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_CREATE) {
        pxa_surface_create_result_t result;
        if (!pxa_surface_parse_create(&parsed, &result) ||
            result.status != PXA_STATUS_OK ||
            result.frame_bytes != SURFACE_FRAME_BYTES)
            return PXA_STATUS_INTERNAL;
        surface_handle = result.surface_handle;
        return pxa_surface_configure_layer(
                   CONFIGURE_REQUEST, surface_handle, SURFACE_X, SURFACE_Y,
                   SURFACE_WIDTH, SURFACE_HEIGHT, 0, 1, surface_packet,
                   sizeof(surface_packet))
                   ? PXA_EVENT_HANDLED
                   : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_CONFIGURE_LAYER &&
        parsed.request_id == CONFIGURE_REQUEST) {
        if (parsed.payload_length != 4 ||
            (int32_t)pxa_read_u32(parsed.payload) != PXA_STATUS_OK)
            return PXA_STATUS_INTERNAL;
        return pxa_surface_query_state(CAPABILITY_REQUEST, surface_handle,
                                        surface_packet,
                                        sizeof(surface_packet))
                   ? PXA_EVENT_HANDLED
                   : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_QUERY_STATE &&
        parsed.request_id == CAPABILITY_REQUEST) {
        const pxa_surface_damage_rect_t overlay = {8, 8, 124, 34};
        pxa_surface_state_result_t result;
        if (!pxa_surface_parse_state(&parsed, &result) ||
            result.status != PXA_STATUS_OK)
            return PXA_STATUS_INTERNAL;
        if ((result.flags & PXA_SURFACE_STATE_FLAG_SUPPORTS_OPAQUE_UI_REGIONS) == 0) {
            update_scanline();
            return submit_frame() && pxa_clock_set_period(FRAME_PERIOD_MS)
                       ? PXA_EVENT_HANDLED
                       : PXA_STATUS_INTERNAL;
        }
        return pxa_surface_configure_opaque_ui_regions(
                   OVERLAY_REQUEST, surface_handle, &overlay, 1,
                   surface_packet, sizeof(surface_packet))
                   ? PXA_EVENT_HANDLED
                   : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_CONFIGURE_OPAQUE_UI_REGIONS &&
        parsed.request_id == OVERLAY_REQUEST) {
        if (parsed.payload_length != 4 ||
            (int32_t)pxa_read_u32(parsed.payload) != PXA_STATUS_OK)
            return PXA_STATUS_INTERNAL;
        update_scanline();
        return submit_frame() && pxa_clock_set_period(FRAME_PERIOD_MS)
                   ? PXA_EVENT_HANDLED
                   : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_QUERY_STATE && query_pending &&
        parsed.request_id == query_request) {
        pxa_surface_state_result_t result;
        query_pending = 0;
        if (!pxa_surface_parse_state(&parsed, &result) ||
            result.status != PXA_STATUS_OK)
            return PXA_STATUS_INTERNAL;
        if (sample_us != 0 && query_sent_us > sample_us &&
            result.presented_frames >= sample_presented_frames) {
            const uint64_t elapsed_us = query_sent_us - sample_us;
            uint64_t measured =
                (result.presented_frames - sample_presented_frames) *
                UINT64_C(10000000) / elapsed_us;
            if (measured > UINT16_MAX) measured = UINT16_MAX;
            fps_tenths = (uint16_t)measured;
        }
        sample_us = query_sent_us;
        sample_presented_frames = result.presented_frames;
        presented_frames = result.presented_frames;
        dropped_frames = result.dropped_frames;
        free_buffers = result.free_buffers;
        (void)render_metrics();
        return PXA_EVENT_HANDLED;
    }
    if (!pxa_clock_tick_timestamp_us(&parsed, &timestamp_us) ||
        surface_handle == 0)
        return PXA_EVENT_UNHANDLED;
    update_scanline();
    if (!submit_frame()) return PXA_STATUS_INTERNAL;
    if (stats_started_us == 0) {
        stats_started_us = timestamp_us;
    } else if (timestamp_us > stats_started_us &&
               timestamp_us - stats_started_us >= FPS_WINDOW_US) {
        stats_started_us = timestamp_us;
        if (!query_pending) {
            ++query_request;
            if (query_request == 0) ++query_request;
            if (!pxa_surface_query_state(query_request, surface_handle,
                                          surface_packet,
                                          sizeof(surface_packet)))
                return PXA_STATUS_INTERNAL;
            query_sent_us = timestamp_us;
            query_pending = 1;
        }
    }
    return PXA_EVENT_HANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
    if (surface_handle != 0) {
        (void)pxa_close_handle(surface_handle);
        surface_handle = 0;
    }
}
