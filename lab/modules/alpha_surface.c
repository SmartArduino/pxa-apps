#define PXA_LAB_MODULE_PREFIX pxa_lab_alpha_surface_
#include "pxa_lab_module.h"

#include "pxa_surface.h"
#include "pxa_ui.h"

#define ALPHA_WIDTH 208u
#define ALPHA_HEIGHT 144u
#define ALPHA_X 44
#define ALPHA_Y 48
#define ALPHA_FRAME_BYTES (ALPHA_WIDTH * ALPHA_HEIGHT * 4u)
#define CREATE_REQUEST UINT32_C(1)
#define CONFIGURE_REQUEST UINT32_C(2)
#define CAPABILITY_REQUEST UINT32_C(3)
#define FIRST_STATS_REQUEST UINT32_C(4)
#define FRAME_PERIOD_MS 33u
#define FPS_WINDOW_US UINT64_C(500000)
#define BUTTON_X 20u
#define BUTTON_Y 90u
#define OVERLAY_WIDTH 168u
#define OVERLAY_HEIGHT 40u

static uint32_t pixels[ALPHA_WIDTH * ALPHA_HEIGHT];
static uint8_t ui_packet[768];
static uint8_t surface_packet[96];
static char metrics[40];
static char button_label[32];
static uint32_t surface_handle;
static uint32_t stats_request;
static uint64_t frame_id;
static uint64_t stats_started_us;
static uint64_t query_sent_us;
static uint64_t sample_us;
static uint64_t sample_presented_frames;
static uint64_t presented_frames;
static uint16_t fps_tenths;
static uint16_t overlay_taps;
static uint8_t query_pending;
static uint8_t phase;

static uint8_t multiply_alpha(uint8_t color, uint8_t alpha) {
    return (uint8_t)(((uint16_t)color * alpha + 127u) / 255u);
}

static size_t string_length(const char *text) {
    size_t length = 0;
    while (text[length] != '\0') ++length;
    return length;
}

static size_t append_text(size_t offset, const char *text) {
    while (*text != '\0' && offset + 1u < sizeof(metrics))
        metrics[offset++] = *text++;
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
    offset = append_text(offset, "  presented ");
    (void)append_u64(offset, presented_frames);
}

static void format_button_label(void) {
    static const char prefix[] = "LVGL overlay ";
    uint16_t value = overlay_taps;
    char reversed[5];
    size_t offset = 0;
    size_t digits = 0;
    while (prefix[offset] != '\0' && offset + 1u < sizeof(button_label)) {
        button_label[offset] = prefix[offset];
        ++offset;
    }
    do {
        reversed[digits++] = (char)('0' + value % 10u);
        value = (uint16_t)(value / 10u);
    } while (value != 0);
    while (digits != 0 && offset + 1u < sizeof(button_label))
        button_label[offset++] = reversed[--digits];
    button_label[offset] = '\0';
}

static void draw_frame(void) {
    uint32_t y;
    for (y = 0; y < ALPHA_HEIGHT; ++y) {
        uint32_t x;
        for (x = 0; x < ALPHA_WIDTH; ++x) {
            const uint8_t alpha = (uint8_t)(48u +
                ((x + (uint32_t)phase * 3u) % ALPHA_WIDTH) * 207u /
                    (ALPHA_WIDTH - 1u));
            const uint8_t red = (uint8_t)(48u + y * 160u /
                                                    (ALPHA_HEIGHT - 1u));
            const uint8_t green = (uint8_t)(80u + x * 120u /
                                                      (ALPHA_WIDTH - 1u));
            const uint8_t blue = (uint8_t)(220u - y * 96u /
                                                     (ALPHA_HEIGHT - 1u));
            pixels[y * ALPHA_WIDTH + x] =
                ((uint32_t)alpha << 24) |
                ((uint32_t)multiply_alpha(red, alpha) << 16) |
                ((uint32_t)multiply_alpha(green, alpha) << 8) |
                multiply_alpha(blue, alpha);
        }
    }
}

static int render_chrome(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t generation = pxa_lab_ui_generation + 1u;
    int ok;
    format_metrics();
    format_button_label();
    if (generation == 0 ||
        !pxa_ui_transaction_begin(&transaction, generation,
                                 PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                 ui_packet, sizeof(ui_packet))) {
        return 0;
    }
    ok = pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_theme_color(&transaction, 1, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 2, 1, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_Y,
                           PXA_UI_LENGTH_PX, 12) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 2, "Premultiplied Alpha Surface",
                         sizeof("Premultiplied Alpha Surface") - 1u) &&
         pxa_ui_set_font_role(&transaction, 2, PXA_UI_FONT_ROLE_BODY) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 3, 1, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_u8(&transaction, 3, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&transaction, 3, PXA_UI_PROPERTY_Y,
                           PXA_UI_LENGTH_PX, 202) &&
         pxa_ui_set_length(&transaction, 3, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 3, metrics, string_length(metrics)) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_u8(&transaction, 3, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED) &&
         pxa_ui_create_typed(&transaction, 4, 1, 0,
                             PXA_UI_NODE_CONTROL, PXA_UI_CONTROL_BUTTON) &&
         pxa_ui_set_u8(&transaction, 4, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_X,
                           PXA_UI_LENGTH_PX, ALPHA_X + BUTTON_X) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_Y,
                           PXA_UI_LENGTH_PX, ALPHA_Y + BUTTON_Y) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_PX, OVERLAY_WIDTH) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, OVERLAY_HEIGHT) &&
         pxa_ui_set_dp(&transaction, 4, PXA_UI_PROPERTY_RADIUS, 10) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_set_theme_color(&transaction, 4, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_set_u8(&transaction, 4, PXA_UI_PROPERTY_COMPOSITION,
                       PXA_UI_COMPOSITION_ALPHA_OVERLAY) &&
         pxa_ui_set_event_mask(&transaction, 4, PXA_UI_EVENT_MASK_CLICK) &&
         pxa_ui_create(&transaction, 5, 4, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 5, button_label,
                         string_length(button_label)) &&
         pxa_ui_set_theme_color(&transaction, 5, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_lab_ui_generation = generation;
    return 1;
}

static int render_metrics(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t generation = pxa_lab_ui_generation + 1u;
    format_metrics();
    if (generation == 0 ||
        !pxa_ui_transaction_begin(&transaction, generation,
                                 PXA_UI_TRANSACTION_PATCH,
                                 ui_packet, sizeof(ui_packet)) ||
        !pxa_ui_set_text(&transaction, 3, metrics, string_length(metrics)) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_lab_ui_generation = generation;
    return 1;
}

static int render_button_label(void) {
    pxa_ui_transaction_t transaction = {0};
    const uint32_t generation = pxa_lab_ui_generation + 1u;
    format_button_label();
    if (generation == 0 ||
        !pxa_ui_transaction_begin(&transaction, generation,
                                 PXA_UI_TRANSACTION_PATCH,
                                 ui_packet, sizeof(ui_packet)) ||
        !pxa_ui_set_text(&transaction, 5, button_label,
                         string_length(button_label)) ||
        !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    pxa_lab_ui_generation = generation;
    return 1;
}

static int submit_frame(void) {
    const int32_t write_status = pxa_surface_write_frame(
        surface_handle, (uint8_t *)pixels, ALPHA_FRAME_BYTES);
    if (write_status == PXA_STATUS_WOULD_BLOCK) return 1;
    if (write_status != (int32_t)ALPHA_FRAME_BYTES) return 0;
    if (pxa_surface_queue_frame(surface_handle, frame_id + 1u, NULL, 0,
                                 surface_packet, sizeof(surface_packet)) !=
        PXA_STATUS_OK) {
        return 0;
    }
    ++frame_id;
    return 1;
}

static int start_animation(void) {
    draw_frame();
    return submit_frame() && pxa_clock_set_period(FRAME_PERIOD_MS);
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    surface_handle = 0;
    stats_request = FIRST_STATS_REQUEST;
    frame_id = 0;
    stats_started_us = 0;
    query_sent_us = 0;
    sample_us = 0;
    sample_presented_frames = 0;
    presented_frames = 0;
    fps_tenths = 0;
    overlay_taps = 0;
    query_pending = 0;
    phase = 0;
    if (!pxa_window_fullscreen() || !render_chrome() ||
        !pxa_surface_create_argb8888_premultiplied(
            CREATE_REQUEST, ALPHA_WIDTH, ALPHA_HEIGHT, 3, surface_packet,
            sizeof(surface_packet))) {
        return PXA_STATUS_INTERNAL;
    }
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
        return render_button_label() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_CREATE) {
        pxa_surface_create_result_t created;
        if (!pxa_surface_parse_create(&parsed, &created) ||
            created.status != PXA_STATUS_OK ||
            created.frame_bytes != ALPHA_FRAME_BYTES) {
            return PXA_STATUS_INTERNAL;
        }
        surface_handle = created.surface_handle;
        return pxa_surface_configure_layer(
                   CONFIGURE_REQUEST, surface_handle, ALPHA_X, ALPHA_Y,
                   ALPHA_WIDTH, ALPHA_HEIGHT, 0, 1, surface_packet,
                   sizeof(surface_packet))
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
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
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_QUERY_STATE &&
        parsed.request_id == CAPABILITY_REQUEST) {
        pxa_surface_state_result_t state;
        if (!pxa_surface_parse_state(&parsed, &state) ||
            state.status != PXA_STATUS_OK)
            return PXA_STATUS_INTERNAL;
        return (state.flags & PXA_SURFACE_STATE_FLAG_UI_ALPHA_PLANE_ACTIVE) != 0 &&
                       start_animation()
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_SURFACE &&
        parsed.opcode == PXA_SURFACE_QUERY_STATE && query_pending &&
        parsed.request_id == stats_request) {
        pxa_surface_state_result_t state;
        query_pending = 0;
        if (!pxa_surface_parse_state(&parsed, &state) ||
            state.status != PXA_STATUS_OK)
            return PXA_STATUS_INTERNAL;
        if (sample_us != 0 && query_sent_us > sample_us &&
            state.presented_frames >= sample_presented_frames) {
            uint64_t measured = (state.presented_frames -
                                 sample_presented_frames) *
                                UINT64_C(10000000) /
                                (query_sent_us - sample_us);
            if (measured > UINT16_MAX) measured = UINT16_MAX;
            fps_tenths = (uint16_t)measured;
        }
        sample_us = query_sent_us;
        sample_presented_frames = state.presented_frames;
        presented_frames = state.presented_frames;
        return render_metrics() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (!pxa_clock_tick_timestamp_us(&parsed, &timestamp_us) ||
        surface_handle == 0) {
        return PXA_EVENT_UNHANDLED;
    }
    (void)timestamp_us;
    ++phase;
    draw_frame();
    if (!submit_frame()) return PXA_STATUS_INTERNAL;
    if (stats_started_us == 0) {
        stats_started_us = timestamp_us;
    } else if (timestamp_us > stats_started_us &&
               timestamp_us - stats_started_us >= FPS_WINDOW_US) {
        stats_started_us = timestamp_us;
        if (!query_pending) {
            ++stats_request;
            if (stats_request == 0) ++stats_request;
            if (!pxa_surface_query_state(stats_request, surface_handle,
                                          surface_packet,
                                          sizeof(surface_packet))) {
                return PXA_STATUS_INTERNAL;
            }
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
