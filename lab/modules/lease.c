#define PXA_LAB_MODULE_PREFIX pxa_lab_lease_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"
#include "pxa_lease.h"

#define LEASE_REQUEST 1u
#define LEASE_DURATION_MS 5000u
#define LEASE_TICK_MS 100u

static uint8_t packet[1536];
static uint8_t lease_payload[32];
static uint32_t lease_handle;
static uint32_t completed_count;
static uint32_t remaining_ms;
static uint64_t last_tick_us;
static uint8_t waiting;
static uint8_t rejected;

static int render(void) {
    char count_value[12];
    char count_text[36];
    char active_state[64];
    const char *state;
    pxa_ui_demo_page_t page = {0};
    size_t index = 0;
    size_t source;
    (void)pxa_ui_demo_format_u32(count_value, completed_count);
    for (source = 0; "已自动撤销次数: "[source] != '\0'; ++source)
        count_text[index++] = "已自动撤销次数: "[source];
    for (source = 0; count_value[source] != '\0'; ++source)
        count_text[index++] = count_value[source];
    count_text[index] = '\0';
    if (lease_handle != 0) {
        const uint32_t tenths = (remaining_ms + 99u) / 100u;
        const uint32_t whole_seconds = tenths / 10u;
        index = 0;
        for (source = 0; "租约有效，还剩 "[source] != '\0'; ++source)
            active_state[index++] = "租约有效，还剩 "[source];
        if (whole_seconds >= 10u) active_state[index++] =
            (char)('0' + whole_seconds / 10u);
        active_state[index++] = (char)('0' + whole_seconds % 10u);
        active_state[index++] = '.';
        active_state[index++] = (char)('0' + tenths % 10u);
        for (source = 0; " 秒"[source] != '\0'; ++source)
            active_state[index++] = " 秒"[source];
        active_state[index] = '\0';
        state = active_state;
    } else {
        state = rejected ? "租约申请被拒绝" :
                waiting ? "正在申请执行资格..." :
                completed_count != 0 ? "租约已到期并被撤销" :
                                       "申请一次限时前台执行资格";
    }
    page.title = "执行租约";
    page.body = count_text;
    page.status = state;
    page.action = "申请 5 秒租约";
    page.features = PXA_UI_DEMO_PAGE_HAS_BUTTON;
    page.icon = 4;
    page.enabled = (uint8_t)(!waiting && lease_handle == 0);
    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}

static int acquire(void) {
    waiting = 1;
    rejected = 0;
    return pxa_lease_acquire(LEASE_REQUEST, PXA_LEASE_FOREGROUND,
                             LEASE_DURATION_MS, lease_payload,
                             sizeof(lease_payload), packet, sizeof(packet));
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    lease_handle = 0;
    completed_count = 0;
    remaining_ms = 0;
    last_tick_us = 0;
    waiting = 0;
    rejected = 0;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND && !waiting && lease_handle == 0) {
        if (!acquire()) return PXA_STATUS_INTERNAL;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        lease_handle != 0) {
        uint64_t timestamp_us;
        uint32_t elapsed_ms;
        if (!pxa_clock_tick_timestamp_us(&parsed, &timestamp_us))
            return PXA_EVENT_UNHANDLED;
        elapsed_ms = pxa_clock_delta_ms(&last_tick_us, timestamp_us,
                                        LEASE_TICK_MS * 2u);
        if (elapsed_ms != 0) {
            remaining_ms = elapsed_ms >= remaining_ms ? 0 :
                                                        remaining_ms - elapsed_ms;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
        }
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service != PXA_SERVICE_CORE) return PXA_EVENT_UNHANDLED;
    if (parsed.opcode == PXA_CORE_ACQUIRE_LEASE) {
        pxa_lease_result_t result;
        if (!pxa_lease_parse_result(&parsed, &result)) return PXA_EVENT_UNHANDLED;
        waiting = 0;
        lease_handle = result.status == PXA_STATUS_OK ? result.handle : 0;
        rejected = result.status != PXA_STATUS_OK;
        remaining_ms = lease_handle != 0 ? LEASE_DURATION_MS : 0;
        last_tick_us = 0;
        if (!pxa_clock_set_period(lease_handle != 0 ? LEASE_TICK_MS : 0))
            return PXA_STATUS_INTERNAL;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.opcode == PXA_CORE_LEASE_REVOKED) {
        pxa_lease_revoked_t revoked;
        if (!pxa_lease_parse_revoked(&parsed, &revoked) ||
            revoked.handle != lease_handle) return PXA_EVENT_UNHANDLED;
        lease_handle = 0;
        remaining_ms = 0;
        last_tick_us = 0;
        ++completed_count;
        (void)pxa_clock_set_period(0);
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
}
