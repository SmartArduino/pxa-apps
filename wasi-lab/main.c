#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pxa_app_messages.h"
#include "pxa_i18n.h"
#include "pxa_ui.h"

#define WASI_CLOCK_REALTIME UINT32_C(0)
#define WASI_CLOCK_MONOTONIC UINT32_C(1)

__attribute__((import_module("wasi_snapshot_preview1"),
               import_name("clock_time_get"))) uint32_t
wasi_clock_time_get(uint32_t clock_id, uint64_t precision, uint64_t *timestamp);
__attribute__((import_module("wasi_snapshot_preview1"), import_name("random_get"))) uint32_t
wasi_random_get(uint8_t *buffer, uint32_t length);

static uint8_t packet[2048];
static uint32_t generation;
static uint32_t last_passed;
static pxa_i18n_t i18n;

static uint32_t run_checks(void) {
    const char text[] = "capability-runtime";
    char *end = NULL;
    uint8_t entropy[16] = {0};
    uint8_t *buffer = (uint8_t *)calloc(8, 1);
    uint64_t monotonic_ns = 0;
    uint64_t wall_ns = 0;
    uint32_t passed = 0;
    uint32_t index;

    passed += strlen(text) == 18u && strcmp(text + 11, "runtime") == 0;
    passed += strtol("2048px", &end, 10) == 2048 && end != NULL &&
              strcmp(end, "px") == 0;
    if (buffer != NULL) {
        memset(buffer, 0x5a, 8);
        passed += buffer[0] == 0x5a && buffer[7] == 0x5a;
        free(buffer);
    }
    passed += wasi_clock_time_get(WASI_CLOCK_MONOTONIC, 1, &monotonic_ns) == 0 &&
              monotonic_ns != 0;
    passed += wasi_clock_time_get(WASI_CLOCK_REALTIME, 1, &wall_ns) == 0 &&
              wall_ns != 0;
    if (wasi_random_get(entropy, sizeof(entropy)) == 0) {
        for (index = 0; index < sizeof(entropy); ++index)
            if (entropy[index] != 0) break;
        passed += index != sizeof(entropy);
    }
    return passed;
}

static int render(uint32_t passed) {
    const char *title = pxa_i18n_cstr(&i18n, PXA_MSG_SCREEN_TITLE);
    const char *detail = pxa_i18n_cstr(&i18n, PXA_MSG_SCREEN_CAPABILITIES);
    const char *policy = pxa_i18n_cstr(&i18n, PXA_MSG_SCREEN_POLICY);
    pxa_i18n_argument_t passed_argument = {
        "passed", 6u, PXA_I18N_ARGUMENT_U32, {.u32 = passed}};
    char summary[48];
    pxa_ui_transaction_t transaction = {0};
    uint32_t next = generation + 1u;
    uint8_t status_color = passed == 6 ? PXA_UI_THEME_SUCCESS
                                       : PXA_UI_THEME_DANGER;
    int ok;

    if (pxa_i18n_format(&i18n, PXA_MSG_RESULT_SUMMARY, &passed_argument, 1u,
                        summary, sizeof(summary)) == 0)
        return 0;
    if (next == 0 ||
        !pxa_ui_transaction_begin(&transaction, next,
                                  PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                  packet, sizeof(packet)))
        return 0;
    ok = pxa_ui_create(&transaction, 1, 0, 0, PXA_UI_NODE_ROOT) &&
         pxa_ui_set_u8(&transaction, 1, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_theme_color(&transaction, 1, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_BACKGROUND) &&
         pxa_ui_create(&transaction, 2, 1, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_length(&transaction, 2, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, 48) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_ROW) &&
         pxa_ui_set_u8(&transaction, 2, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 2, 16, 8, 16, 8) &&
         pxa_ui_set_theme_color(&transaction, 2, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_PRIMARY) &&
         pxa_ui_create(&transaction, 3, 2, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_text(&transaction, 3, title, strlen(title)) &&
         pxa_ui_set_font_role(&transaction, 3, PXA_UI_FONT_ROLE_TITLE) &&
         pxa_ui_set_theme_color(&transaction, 3, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_ON_PRIMARY) &&
         pxa_ui_create(&transaction, 4, 1, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 4, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u16(&transaction, 4, PXA_UI_PROPERTY_GROW, 1) &&
         pxa_ui_set_u8(&transaction, 4, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 4, PXA_UI_PROPERTY_JUSTIFY,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 4, 14, 14, 14, 14) &&
         pxa_ui_set_dp(&transaction, 4, PXA_UI_PROPERTY_GAP, 10) &&
         pxa_ui_create(&transaction, 5, 4, 0, PXA_UI_NODE_BOX) &&
         pxa_ui_set_length(&transaction, 5, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_LAYOUT,
                       PXA_UI_LAYOUT_COLUMN) &&
         pxa_ui_set_u8(&transaction, 5, PXA_UI_PROPERTY_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_padding(&transaction, 5, 12, 14, 12, 14) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_GAP, 8) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_RADIUS, 6) &&
         pxa_ui_set_dp(&transaction, 5, PXA_UI_PROPERTY_BORDER_WIDTH, 1) &&
         pxa_ui_set_theme_color(&transaction, 5, PXA_UI_PROPERTY_BACKGROUND,
                                PXA_UI_THEME_SURFACE) &&
         pxa_ui_set_theme_color(&transaction, 5, PXA_UI_PROPERTY_BORDER_COLOR,
                                PXA_UI_THEME_BORDER) &&
         pxa_ui_create(&transaction, 6, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 6, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 6, summary, strlen(summary)) &&
         pxa_ui_set_font_role(&transaction, 6, PXA_UI_FONT_ROLE_HEADLINE) &&
         pxa_ui_set_u8(&transaction, 6, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 6, PXA_UI_PROPERTY_FOREGROUND,
                                status_color) &&
         pxa_ui_create(&transaction, 7, 5, 0, PXA_UI_NODE_PROGRESS) &&
         pxa_ui_set_length(&transaction, 7, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_i32(&transaction, 7, PXA_UI_PROPERTY_VALUE,
                        (int32_t)(passed * 100u / 6u)) &&
         pxa_ui_create(&transaction, 8, 5, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 8, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 8, detail, strlen(detail)) &&
         pxa_ui_set_u8(&transaction, 8, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 8, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_TEXT) &&
         pxa_ui_create(&transaction, 9, 4, 0, PXA_UI_NODE_TEXT) &&
         pxa_ui_set_length(&transaction, 9, PXA_UI_PROPERTY_WIDTH,
                           PXA_UI_LENGTH_FILL, 0) &&
         pxa_ui_set_text(&transaction, 9, policy, strlen(policy)) &&
         pxa_ui_set_font_role(&transaction, 9, PXA_UI_FONT_ROLE_CAPTION) &&
         pxa_ui_set_u8(&transaction, 9, PXA_UI_PROPERTY_TEXT_ALIGN,
                       PXA_UI_ALIGN_CENTER) &&
         pxa_ui_set_theme_color(&transaction, 9, PXA_UI_PROPERTY_FOREGROUND,
                                PXA_UI_THEME_MUTED);
    if (!ok || !pxa_ui_transaction_commit(&transaction)) {
        if (transaction.active) (void)pxa_ui_transaction_cancel(&transaction);
        return 0;
    }
    generation = next;
    return 1;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    uint32_t passed;
    (void)pxa_i18n_init_from_start_config(
        &i18n, &pxa_app_i18n_bundle, config, config_length);
    passed = run_checks();
    last_passed = passed;
    return pxa_window_fullscreen() && render(passed) && passed == 6
               ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    int locale_result;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    locale_result = pxa_i18n_handle_event(&i18n, &parsed);
    if (locale_result != 0) {
        return locale_result == 1 && !render(last_passed)
                   ? PXA_STATUS_INTERNAL
                   : PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
