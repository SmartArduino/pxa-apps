#define PXA_LAB_MODULE_PREFIX pxa_lab_controller_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"

#define CONTROLLER_STATUS_BYTES 160u

static uint8_t packet[2304];
static char status_text[CONTROLLER_STATUS_BYTES];
static uint8_t connected;
static uint8_t controller_id;
static uint32_t buttons;

static size_t append_text(size_t offset, const char *text) {
    size_t index = 0;
    while (text[index] != '\0' && offset + 1u < sizeof(status_text))
        status_text[offset++] = text[index++];
    status_text[offset] = '\0';
    return offset;
}

static void format_status(void) {
    static const struct {
        uint32_t mask;
        const char *name;
    } names[] = {
        {PXA_CONTROLLER_UP, "UP"},
        {PXA_CONTROLLER_DOWN, "DOWN"},
        {PXA_CONTROLLER_LEFT, "LEFT"},
        {PXA_CONTROLLER_RIGHT, "RIGHT"},
        {PXA_CONTROLLER_A, "A"},
        {PXA_CONTROLLER_B, "B"},
        {PXA_CONTROLLER_START, "START"},
        {PXA_CONTROLLER_SELECT, "SELECT"},
    };
    size_t offset = 0;
    size_t index;
    if (!connected) {
        (void)append_text(0, "Controller disconnected / waiting");
        return;
    }
    offset = append_text(offset, "P");
    if (offset + 2u < sizeof(status_text)) {
        status_text[offset++] = (char)('1' + controller_id % 9u);
        status_text[offset++] = ':';
        status_text[offset] = '\0';
    }
    if (buttons == 0) {
        (void)append_text(offset, " connected, no buttons");
        return;
    }
    for (index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        if ((buttons & names[index].mask) != 0) {
            offset = append_text(offset, " ");
            offset = append_text(offset, names[index].name);
        }
    }
}

static int render(void) {
    pxa_ui_demo_page_t page = {0};
    uint8_t pressed = 0;
    uint32_t remaining = buttons;
    while (remaining != 0) {
        pressed = (uint8_t)(pressed + (remaining & 1u));
        remaining >>= 1;
    }
    format_status();
    page.title = "NES Controller Lab";
    page.body = "Arrows: D-pad   Z/X: A/B   Enter/Tab: Start/Select";
    page.status = status_text;
    page.features = PXA_UI_DEMO_PAGE_HAS_PROGRESS;
    page.icon = 6;
    page.progress = (uint8_t)(pressed * 100u / 8u);
    page.enabled = 1;
    return pxa_ui_demo_page_render_with_root_event_mask(
        &pxa_lab_ui_generation, packet, sizeof(packet), &page,
        PXA_UI_EVENT_MASK_CONTROLLER_STATE);
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    connected = 0;
    controller_id = 0;
    buttons = 0;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK
                                                : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_controller_data_t state;
    if (!pxa_parse_event(event, length, &parsed) ||
        !pxa_ui_parse_controller(&parsed, &state))
        return PXA_EVENT_UNHANDLED;
    controller_id = state.controller;
    connected = state.connected;
    buttons = state.buttons;
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
