#include "pxa_storage.h"
#include "pxa_work.h"

#define STORAGE_REQUEST 1u
#define COMPLETE_REQUEST 2u

static uint32_t work_id;
static uint8_t payload[32];
static uint8_t packet[64];


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    static const char key[] = "work.runs";
    static const uint8_t value[] = {1};
    pxa_work_context_t work;
    if (!pxa_work_parse_context(config, config_length, &work))
        return PXA_STATUS_INVALID_ARGUMENT;
    work_id = work.id;
    return pxa_storage_set(STORAGE_REQUEST, key, sizeof(key) - 1,
                           value, sizeof(value), payload,
                           sizeof(payload), packet, sizeof(packet))
               ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    int32_t status;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (parsed.service == PXA_SERVICE_STORAGE &&
        parsed.opcode == PXA_STORAGE_SET &&
        parsed.request_id == STORAGE_REQUEST &&
        pxa_storage_parse_status(&parsed, PXA_STORAGE_SET, &status)) {
        if (status != PXA_STATUS_OK) return PXA_STATUS_INTERNAL;
        return pxa_work_complete(COMPLETE_REQUEST, work_id,
                                 PXA_WORK_SUCCESS, payload, sizeof(payload),
                                 packet, sizeof(packet))
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_WORK &&
        parsed.opcode == PXA_WORK_STOP_REQUESTED) {
        pxa_work_stop_t stop;
        if (!pxa_work_parse_stop(&parsed, &stop) || stop.id != work_id) {
            return PXA_EVENT_UNHANDLED;
        }
        return pxa_work_complete(COMPLETE_REQUEST, work_id,
                                 PXA_WORK_RETRY, payload, sizeof(payload),
                                 packet, sizeof(packet))
                   ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_WORK &&
        parsed.opcode == PXA_WORK_COMPLETE &&
        parsed.request_id == COMPLETE_REQUEST &&
        pxa_work_parse_status(&parsed, PXA_WORK_COMPLETE, &status)) {
        return status == PXA_STATUS_OK ? PXA_EVENT_HANDLED
                                       : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
