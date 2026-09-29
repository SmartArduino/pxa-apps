#include <stdint.h>

#include "pxa_ipc.h"

int32_t pxa_app_start(const uint8_t *config, uint32_t size) {
    (void)config;
    (void)size;
    return 0;
}

int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t size) {
    pxa_event_t event;
    if (!pxa_parse_event(bytes, size, &event)) return -1;
    if (event.service == PXA_IPC_SERVICE &&
        event.opcode == PXA_IPC_REQUEST_EVENT) {
        pxa_ipc_request_t request;
        uint8_t packet[64];
        static const uint8_t response[] = {'o', 'k', ':', 'h', 'i'};
        if (!pxa_ipc_parse_request(&event, &request) ||
            request.endpoint.size != sizeof("v1.echo") - 1u ||
            request.payload.size != 2 ||
            request.payload.data[0] != 'h' ||
            request.payload.data[1] != 'i') return -1;
        return pxa_ipc_request_reply(
                   packet, sizeof(packet), UINT64_C(0x1234567800000012),
                   request.call_id, 0, response, sizeof(response)) == 0
                   ? 1 : -1;
    }
    if (event.service == PXA_IPC_SERVICE &&
        event.opcode == PXA_IPC_REPLY) {
        int32_t status;
        return pxa_ipc_parse_reply_result(
                   &event, UINT64_C(0x1234567800000012), &status) &&
                   status == 0 ? 1 : -1;
    }
    return 0;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
