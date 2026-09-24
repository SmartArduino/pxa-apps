#include "pxa_ipc.h"

#define REPLY_REQUEST 2u

static uint8_t ipc_payload[192];
static uint8_t packet[256];


int32_t pxa_app_start(const uint8_t* config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t* event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ipc_request_t request;
    static const uint8_t reply[] = {'p', 'o', 'n', 'g'};
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (parsed.service != PXA_SERVICE_IPC || parsed.opcode != PXA_IPC_REQUEST ||
        !pxa_ipc_parse_request(&parsed, &request)) {
        return PXA_EVENT_UNHANDLED;
    }
    return pxa_ipc_reply(REPLY_REQUEST, request.call_id, PXA_STATUS_OK, reply,
                         sizeof(reply), ipc_payload, sizeof(ipc_payload), packet,
                         sizeof(packet))
               ? PXA_EVENT_HANDLED
               : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
