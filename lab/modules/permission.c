#define PXA_LAB_MODULE_PREFIX pxa_lab_permission_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"
#include "pxa_permission.h"

#define REQUEST_CHECK 1u
#define REQUEST_ACQUIRE 2u

static uint8_t packet[1664];
static uint8_t permission_payload[128];
static uint8_t decision;
static uint8_t pending;

static int render(void) {
    static const char allowed[] = "Allowed";
    static const char denied[] = "Denied";
    static const char checking[] = "Checking...";
    static const char hint[] = "Tap to acquire or refresh";
    const char *value = pending ? checking : decision ? allowed : denied;
    pxa_ui_demo_page_t page = {0};
    page.title = "Permission Lab";
    page.body = "net.client @ demo.local";
    page.status = value;
    page.action = hint;
    page.features = PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_SWITCH;
    page.icon = 7;
    page.switch_value = decision;
    page.enabled = (uint8_t)!pending;
    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}

static int request_permission(uint16_t opcode, uint32_t request_id) {
    static const char name[] = "net.client";
    static const uint8_t scope[] = "demo.local";
    pending = 1;
    return pxa_permission_request(opcode, request_id, name, sizeof(name) - 1,
                                  scope, sizeof(scope) - 1, permission_payload,
                                  sizeof(permission_payload), packet, sizeof(packet));
}

static int close_handle(uint32_t handle) {
    uint8_t payload[4];
    pxa_writer_t writer;
    if (handle == 0) return 0;
    payload[0] = (uint8_t)handle;
    payload[1] = (uint8_t)(handle >> 8);
    payload[2] = (uint8_t)(handle >> 16);
    payload[3] = (uint8_t)(handle >> 24);
    pxa_writer_init(&writer, packet, sizeof(packet));
    return pxa_message(&writer, PXA_SERVICE_CORE, PXA_CORE_CLOSE_HANDLE, 0,
                       payload, sizeof(payload)) &&
           pxa_control(writer.data, (uint32_t)writer.length) == PXA_STATUS_OK;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    if (!pxa_window_fullscreen() || !render() ||
        !request_permission(PXA_PERMISSION_CHECK, REQUEST_CHECK)) {
        return PXA_STATUS_INTERNAL;
    }
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND && !pending) {
        if (!request_permission(decision ? PXA_PERMISSION_CHECK : PXA_PERMISSION_ACQUIRE,
                                decision ? REQUEST_CHECK : REQUEST_ACQUIRE)) {
            return PXA_STATUS_INTERNAL;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service != PXA_SERVICE_PERMISSION) return PXA_EVENT_UNHANDLED;
    if (parsed.opcode == PXA_PERMISSION_CHECK && parsed.request_id == REQUEST_CHECK) {
        pxa_permission_check_result_t result;
        if (!pxa_permission_parse_check(&parsed, &result)) return PXA_STATUS_INTERNAL;
        decision = result.status == PXA_STATUS_OK && result.decision == PXA_PERMISSION_ALLOW;
        pending = 0;
    } else if (parsed.opcode == PXA_PERMISSION_ACQUIRE &&
               parsed.request_id == REQUEST_ACQUIRE) {
        pxa_permission_acquire_result_t result;
        if (!pxa_permission_parse_acquire(&parsed, &result)) return PXA_STATUS_INTERNAL;
        decision = result.status == PXA_STATUS_OK;
        if (decision && !close_handle(result.handle)) return PXA_STATUS_INTERNAL;
        pending = 0;
    } else {
        return PXA_EVENT_UNHANDLED;
    }
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
