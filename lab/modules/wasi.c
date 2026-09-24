#define PXA_LAB_MODULE_PREFIX pxa_lab_wasi_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"

#define WASI_CLOCK_REALTIME UINT32_C(0)
#define WASI_CLOCK_MONOTONIC UINT32_C(1)
#define WASI_CHECKS 4u

__attribute__((import_module("wasi_snapshot_preview1"),
               import_name("clock_time_get"))) uint32_t
wasi_clock_time_get(uint32_t clock_id, uint64_t precision, uint64_t *timestamp);
__attribute__((import_module("wasi_snapshot_preview1"),
               import_name("random_get"))) uint32_t
wasi_random_get(uint8_t *buffer, uint32_t length);

static uint8_t packet[1664];
static uint8_t passed_checks;
static uint8_t has_run;

static uint8_t run_wasi_checks(void) {
    uint8_t entropy[16] = {0};
    uint64_t monotonic_ns = 0;
    uint64_t wall_ns = 0;
    uint32_t index;
    uint8_t entropy_nonzero = 0;
    uint8_t passed = 0;

    passed += wasi_clock_time_get(WASI_CLOCK_MONOTONIC, 1, &monotonic_ns) == 0;
    passed += monotonic_ns != 0;
    passed += wasi_clock_time_get(WASI_CLOCK_REALTIME, 1, &wall_ns) == 0;
    if (wasi_random_get(entropy, sizeof(entropy)) == 0) {
        for (index = 0; index < sizeof(entropy); ++index)
            entropy_nonzero |= entropy[index];
    }
    passed += entropy_nonzero != 0;
    return passed;
}

static int render(void) {
    static const char body[] = "WASI Preview 1: clock_time_get 和 random_get";
    static const char ready[] = "点击调用受签名约束的时钟与随机数能力";
    static const char success[] = "4/4 项通过: monotonic、wall clock、random";
    static const char failure[] = "WASI 能力调用失败，请检查宿主支持";
    const char *status = !has_run ? ready
                       : passed_checks == WASI_CHECKS ? success : failure;
    pxa_ui_demo_page_t page = {
        "WASI 系统能力", body, status, "运行 WASI 检查", NULL,
        PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_PROGRESS, 9,
        (uint8_t)(passed_checks * 100u / WASI_CHECKS), 0, 0, 1};

    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    has_run = 0;
    passed_checks = 0;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK
                                                : PXA_STATUS_RESOURCE_LIMIT;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (!pxa_ui_parse_event(&parsed, &ui_event) ||
        ui_event.node != PXA_UI_DEMO_PAGE_NODE_BUTTON ||
        ui_event.kind != PXA_UI_EVENT_CLICK_KIND) {
        return PXA_EVENT_UNHANDLED;
    }
    passed_checks = run_wasi_checks();
    has_run = 1;
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
}
