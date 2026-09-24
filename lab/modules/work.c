#define PXA_LAB_MODULE_PREFIX pxa_lab_work_
#include "pxa_lab_module.h"

#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"
#include "pxa_storage.h"
#include "pxa_work.h"

#define STORAGE_REQUEST 1u
#define ENQUEUE_REQUEST 2u
#define RESET_REQUEST 3u
#define CANCEL_REQUEST 4u
#define POLL_PERIOD_MS 250u

static uint8_t packet[1664];
static uint8_t payload[128];
static uint8_t scheduled;
static uint8_t completed;
static uint8_t enqueueing;
static uint8_t failed;
static uint8_t cancelled;
static uint8_t result_loading;
static uint32_t work_id;

static int render(void) {
    static const char ready[] = "任务会在当前页面自动更新";
    static const char preparing[] = "正在准备任务";
    static const char waiting[] = "任务已排队，正在等待完成";
    static const char done[] = "后台任务已完成";
    static const char cancelled_text[] = "排队任务已取消";
    static const char error[] = "任务创建失败";
    const char *status = scheduled ? waiting
                         : enqueueing ? preparing
                         : failed ? error
                         : cancelled ? cancelled_text
                         : completed ? done : ready;
    pxa_ui_demo_page_t page = {
        "后台任务", "延迟执行并实时读取完成状态", status,
        scheduled ? "取消任务" : "创建 2 秒任务",
        NULL, PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_PROGRESS, 10,
        completed ? 100 : (scheduled || enqueueing) ? 50 : 0, 0, 0,
        (uint8_t)!enqueueing};
    return pxa_ui_demo_page_render(&pxa_lab_ui_generation, packet,
                                   sizeof(packet), &page);
}

static int load_result(void) {
    static const char key[] = "work.runs";
    result_loading = 1;
    if (pxa_storage_get(STORAGE_REQUEST, key, sizeof(key) - 1, payload,
                        sizeof(payload), packet, sizeof(packet))) return 1;
    result_loading = 0;
    return 0;
}

static int enqueue_work(void) {
    pxa_work_request_t work = {0};
    work.worker = "worker";
    work.worker_length = 6;
    work.initial_delay_ms = 2000;
    work.execution_hint_ms = 5000;
    work.retry_delay_ms = 2000;
    work.max_attempts = 3;
    return pxa_work_enqueue(ENQUEUE_REQUEST, &work, payload,
                            sizeof(payload), packet, sizeof(packet));
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    if (!pxa_window_fullscreen()) return PXA_STATUS_BAD_STATE;
    if (!render()) return PXA_STATUS_RESOURCE_LIMIT;
    return load_result() && pxa_clock_set_period(POLL_PERIOD_MS)
               ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND && !enqueueing) {
        static const char key[] = "work.runs";
        static const uint8_t value[] = {0};
        if (scheduled) {
            enqueueing = 1;
            if (!pxa_work_cancel(CANCEL_REQUEST, work_id, payload, sizeof(payload),
                                 packet, sizeof(packet))) {
                enqueueing = 0;
                failed = 1;
            }
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
        }
        enqueueing = 1;
        completed = 0;
        failed = 0;
        cancelled = 0;
        if (!pxa_storage_set(RESET_REQUEST, key, sizeof(key) - 1,
                             value, sizeof(value), payload, sizeof(payload),
                             packet, sizeof(packet))) {
            enqueueing = 0;
            failed = 1;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
    }
    if (parsed.service == PXA_SERVICE_WORK &&
        parsed.opcode == PXA_WORK_ENQUEUE && parsed.request_id == ENQUEUE_REQUEST) {
        pxa_work_enqueue_result_t result;
        if (!pxa_work_parse_enqueue(&parsed, &result))
            return PXA_STATUS_INTERNAL;
        enqueueing = 0;
        scheduled = result.status == PXA_STATUS_OK;
        work_id = scheduled ? result.id : 0;
        failed = !scheduled;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
    }
    if (parsed.service == PXA_SERVICE_STORAGE &&
        parsed.opcode == PXA_STORAGE_SET &&
        parsed.request_id == RESET_REQUEST) {
        int32_t status;
        if (!pxa_storage_parse_status(&parsed, PXA_STORAGE_SET, &status))
            return PXA_STATUS_INTERNAL;
        if (status != PXA_STATUS_OK || !enqueue_work()) {
            enqueueing = 0;
            failed = 1;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
    }
    if (parsed.service == PXA_SERVICE_STORAGE && parsed.opcode == PXA_STORAGE_GET &&
        parsed.request_id == STORAGE_REQUEST) {
        pxa_storage_get_result_t result;
        if (!pxa_storage_parse_get(&parsed, &result)) return PXA_STATUS_INTERNAL;
        result_loading = 0;
        completed = result.status == PXA_STATUS_OK && result.value_length == 1 &&
                    result.value[0] == 1;
        if (completed) {
            scheduled = 0;
            work_id = 0;
        }
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
    }
    if (parsed.service == PXA_SERVICE_CLOCK && parsed.opcode == PXA_CLOCK_TICK &&
        scheduled && !enqueueing && !result_loading) {
        if (!load_result()) {
            failed = 1;
            scheduled = 0;
            return render() ? PXA_EVENT_HANDLED : PXA_STATUS_RESOURCE_LIMIT;
        }
        return PXA_EVENT_HANDLED;
    }
    if (parsed.service == PXA_SERVICE_WORK &&
        parsed.opcode == PXA_WORK_CANCEL && parsed.request_id == CANCEL_REQUEST) {
        int32_t status;
        if (!pxa_work_parse_status(&parsed, PXA_WORK_CANCEL, &status))
            return PXA_STATUS_INTERNAL;
        enqueueing = 0;
        scheduled = 0;
        cancelled = status == PXA_STATUS_OK;
        failed = !cancelled;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    return PXA_EVENT_UNHANDLED;
}

void pxa_app_stop(uint32_t reason) {
    (void)reason;
    (void)pxa_clock_set_period(0);
}
