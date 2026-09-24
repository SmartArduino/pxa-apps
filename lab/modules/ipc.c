#define PXA_LAB_MODULE_PREFIX pxa_lab_ipc_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"
#include "pxa_ipc.h"

#define LAB_NODE 2u
#define CALL_REQUEST 1u

static uint8_t packet[1536];
static uint8_t ipc_payload[192];
static uint32_t reply_count;
static uint8_t waiting;
static uint8_t failed;

static int render(void) {
    pxa_ui_demo_page_t page;
    static const char detail[] = "Signed endpoint: demo.echo";
    static const char ping[] = "PING";
    static const char ready[] = "Tap PING to send an async call";
    static const char waiting_text[] = "Waiting for reply...";
    static const char failed_text[] = "IPC request failed";
    const char* state = failed ? failed_text : waiting ? waiting_text : ready;
    page = (pxa_ui_demo_page_t){
        "IPC Echo Lab", detail, state, ping, NULL,
        PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_PROGRESS, 6,
        (uint8_t)(reply_count % 101u), 0, 0, (uint8_t)!waiting};
    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}

static int call_echo(void) {
    static const char endpoint[] = "demo.echo";
    static const uint8_t message[] = {'p', 'i', 'n', 'g'};
    waiting = 1;
    failed = 0;
    return pxa_ipc_call(CALL_REQUEST, endpoint, sizeof(endpoint) - 1,
                        message, sizeof(message), ipc_payload, sizeof(ipc_payload),
                        packet, sizeof(packet));
}


int32_t pxa_app_start(const uint8_t* config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t* event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND && !waiting) {
        if (call_echo()) {
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_UNHANDLED;
    }
    if (parsed.service != PXA_SERVICE_IPC) return PXA_EVENT_UNHANDLED;
    if (parsed.opcode == PXA_IPC_RESULT) {
        pxa_ipc_result_t result;
        if (pxa_ipc_parse_result(&parsed, &result) &&
            result.status == PXA_STATUS_OK && result.payload_length == 4) {
            ++reply_count;
            waiting = 0;
            failed = 0;
        } else {
            waiting = 0;
            failed = 1;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.opcode == PXA_IPC_CALL || parsed.opcode == PXA_IPC_REPLY ||
        parsed.opcode == PXA_IPC_REQUEST) {
        return PXA_EVENT_HANDLED;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
