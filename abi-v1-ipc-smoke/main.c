#include <stdint.h>

#include "pxa_ipc.h"

static uint32_t s_call_id;
static uint8_t s_cancelled;

int32_t pxa_app_start(const uint8_t *config, uint32_t size) {
    uint8_t packet[64];
    (void)config;
    (void)size;
    if (pxa_ipc_request_call(
            packet, sizeof(packet), UINT64_C(0x1234567800000011),
            "v1.echo", sizeof("v1.echo") - 1u,
            (const uint8_t *)"hi", 2) != 0 ||
        pxa_ipc_request_call(
            packet, sizeof(packet), UINT64_C(0x1234567800000013),
            "v1.echo", sizeof("v1.echo") - 1u,
            (const uint8_t *)"drop", 4) != 0 ||
        pxa_cancel(UINT64_C(0x1234567800000013)) != 0)
        return -1;
    return 0;
}

int32_t pxa_app_on_event(const uint8_t *bytes, uint32_t size) {
    pxa_event_t event;
    if (!pxa_parse_event(bytes, size, &event)) return -1;
    if (event.service == PXA_IPC_SERVICE &&
        event.opcode == PXA_IPC_CALL) {
        pxa_ipc_call_result_t call;
        if (event.token == UINT64_C(0x1234567800000013)) {
            if (!pxa_ipc_parse_call_result(
                    &event, UINT64_C(0x1234567800000013), &call) ||
                call.status != PXA_STATUS_CANCELLED) return -1;
            s_cancelled = 1;
            return 1;
        }
        if (!pxa_ipc_parse_call_result(
                &event, UINT64_C(0x1234567800000011), &call) ||
            call.status != 0 || call.call_id == 0) return -1;
        s_call_id = call.call_id;
        return 1;
    }
    if (event.service == PXA_IPC_SERVICE &&
        event.opcode == PXA_IPC_RESULT_EVENT) {
        static const uint8_t expected[] = {'o', 'k', ':', 'h', 'i'};
        pxa_ipc_result_t result;
        if (!pxa_ipc_parse_result(&event, &result) ||
            result.call_id != s_call_id || result.status != 0 ||
            !s_cancelled ||
            result.payload.size != sizeof(expected)) return -1;
        for (uint32_t index = 0; index < sizeof(expected); ++index)
            if (result.payload.data[index] != expected[index]) return -1;
        return pxa_log_write(
                   2, "ABI v1 IPC two-component exchange OK") == 0
                   ? 1 : -1;
    }
    return 0;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
