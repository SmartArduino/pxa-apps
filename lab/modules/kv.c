#define PXA_LAB_MODULE_PREFIX pxa_lab_kv_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"
#include "pxa_storage.h"

#define COUNTER_NODE 2u
#define REQUEST_GET 1u
#define REQUEST_SET 2u

static uint8_t packet[1664];
static uint8_t storage_payload[128];
static uint32_t count;
static uint8_t saving;
static uint8_t failed;

static int render(void) {
    char text[11];
    static const char hint[] = "Tap to increment";
    static const char saving_text[] = "Saving...";
    static const char error[] = "Storage unavailable";
    const char* status = failed ? error : saving ? saving_text : hint;
    pxa_ui_demo_page_t page;
    pxa_ui_demo_format_u32(text, count);
    page = (pxa_ui_demo_page_t){
        "Private KV counter", text, status, "INCREMENT", NULL,
        PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_PROGRESS, 7,
        (uint8_t)(count % 101u), 0, 0, (uint8_t)(!saving && !failed)};
    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}

static int load_counter(void) {
    static const char key[] = "counter.total";
    return pxa_storage_get(REQUEST_GET, key, sizeof(key) - 1, storage_payload,
                           sizeof(storage_payload), packet, sizeof(packet));
}

static int save_counter(void) {
    static const char key[] = "counter.total";
    uint8_t value[4] = {(uint8_t)count, (uint8_t)(count >> 8),
                        (uint8_t)(count >> 16), (uint8_t)(count >> 24)};
    saving = 1;
    return pxa_storage_set(REQUEST_SET, key, sizeof(key) - 1, value, sizeof(value),
                           storage_payload, sizeof(storage_payload), packet,
                           sizeof(packet));
}


int32_t pxa_app_start(const uint8_t* config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    return pxa_window_fullscreen() && render() && load_counter()
               ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t* event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND && !saving && !failed) {
        ++count;
        if (!save_counter()) return PXA_STATUS_INTERNAL;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service != PXA_SERVICE_STORAGE) return PXA_EVENT_UNHANDLED;
    if (parsed.opcode == PXA_STORAGE_GET && parsed.request_id == REQUEST_GET) {
        pxa_storage_get_result_t result;
        if (!pxa_storage_parse_get(&parsed, &result)) return PXA_STATUS_INTERNAL;
        if (result.status == PXA_STATUS_NOT_FOUND) {
            count = 0;
        } else if (result.status == PXA_STATUS_OK && result.value_length == 4) {
            count = pxa_read_u32(result.value);
        } else {
            failed = 1;
        }
    } else if (parsed.opcode == PXA_STORAGE_SET && parsed.request_id == REQUEST_SET) {
        int32_t status;
        if (!pxa_storage_parse_status(&parsed, PXA_STORAGE_SET, &status))
            return PXA_STATUS_INTERNAL;
        saving = 0;
        if (status != PXA_STATUS_OK) failed = 1;
    } else {
        return PXA_EVENT_UNHANDLED;
    }
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
