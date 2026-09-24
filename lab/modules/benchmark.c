#define PXA_LAB_MODULE_PREFIX pxa_lab_benchmark_
#include "pxa_lab_module.h"

#include "pxa_ipc.h"
#include "pxa_ui.h"
#include "pxa_ui_demo_page.h"

#define CLOCK_AOT_START_REQUEST 1u
#define CLOCK_AOT_END_REQUEST 2u
#define CLOCK_WASM_START_REQUEST 3u
#define CLOCK_WASM_END_REQUEST 4u
#define IPC_AOT_REQUEST 10u
#define IPC_WASM_REQUEST 11u
#define BENCHMARK_ITERATIONS UINT32_C(1000000)
#define BENCHMARK_WORKLOAD_COUNT 4u

typedef enum {
    BENCHMARK_IDLE,
    BENCHMARK_AOT_STARTING,
    BENCHMARK_AOT_RUNNING,
    BENCHMARK_AOT_FINISHING,
    BENCHMARK_WASM_STARTING,
    BENCHMARK_WASM_RUNNING,
    BENCHMARK_WASM_FINISHING,
    BENCHMARK_COMPLETE,
    BENCHMARK_FAILED
} benchmark_state_t;

static uint8_t packet[1664];
static uint8_t payload[128];
static benchmark_state_t state;
static uint64_t started_us;
static uint8_t workload_index;
static uint32_t aot_scores[BENCHMARK_WORKLOAD_COUNT];
static uint32_t wasm_scores[BENCHMARK_WORKLOAD_COUNT];

static const char *const workload_names[BENCHMARK_WORKLOAD_COUNT] = {
    "整数", "调用", "分支", "内存"
};

static size_t append_text(char *output, size_t capacity, size_t offset,
                          const char *text) {
    while (text != NULL && *text != '\0' && offset + 1u < capacity)
        output[offset++] = *text++;
    if (capacity != 0) output[offset < capacity ? offset : capacity - 1u] = '\0';
    return offset;
}

static size_t append_u32(char *output, size_t capacity, size_t offset,
                         uint32_t value) {
    char digits[10];
    size_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    while (count != 0 && offset + 1u < capacity)
        output[offset++] = digits[--count];
    if (capacity != 0) output[offset < capacity ? offset : capacity - 1u] = '\0';
    return offset;
}

static uint32_t score_for_elapsed(uint64_t elapsed_us) {
    uint64_t score;
    if (elapsed_us == 0) return 0;
    score = (uint64_t)BENCHMARK_ITERATIONS * UINT64_C(1000000) / elapsed_us;
    return score > UINT32_MAX ? UINT32_MAX : (uint32_t)score;
}

static size_t append_score_line(char *output, size_t capacity, size_t offset,
                                uint8_t workload) {
    offset = append_text(output, capacity, offset, workload_names[workload]);
    offset = append_text(output, capacity, offset, " A");
    offset = append_u32(output, capacity, offset, aot_scores[workload]);
    offset = append_text(output, capacity, offset, " W");
    offset = append_u32(output, capacity, offset, wasm_scores[workload]);
    if (wasm_scores[workload] != 0) {
        offset = append_text(output, capacity, offset, " ");
        offset = append_u32(output, capacity, offset,
                            (uint32_t)(((uint64_t)aot_scores[workload] * 100u) /
                                       wasm_scores[workload]));
        offset = append_text(output, capacity, offset, "%");
    }
    return append_text(output, capacity, offset, "\n");
}

static int render(void) {
    char status[96];
    char body[192];
    size_t offset = 0;
    const char *state_text = "点击开始，依次比较四种相同负载";
    uint8_t progress = 0;
    if (state == BENCHMARK_AOT_STARTING || state == BENCHMARK_AOT_RUNNING ||
        state == BENCHMARK_AOT_FINISHING) {
        state_text = "正在运行 AOT";
        progress = (uint8_t)((workload_index * 100u + 25u) /
                             BENCHMARK_WORKLOAD_COUNT);
    } else if (state == BENCHMARK_WASM_STARTING || state == BENCHMARK_WASM_RUNNING ||
               state == BENCHMARK_WASM_FINISHING) {
        state_text = "正在运行 WASM";
        progress = (uint8_t)((workload_index * 100u + 75u) /
                             BENCHMARK_WORKLOAD_COUNT);
    } else if (state == BENCHMARK_COMPLETE) {
        state_text = "完成。分数为每秒工作负载迭代次数";
        progress = 100;
    } else if (state == BENCHMARK_FAILED) {
        state_text = "计时或 IPC 请求失败";
    }
    offset = append_text(body, sizeof(body), offset,
                         "每项固定 1000000 次，A=AOT W=WASM\n");
    for (uint8_t index = 0; index < BENCHMARK_WORKLOAD_COUNT; ++index)
        offset = append_score_line(body, sizeof(body), offset, index);
    offset = append_text(status, sizeof(status), 0, state_text);
    if (state == BENCHMARK_AOT_STARTING || state == BENCHMARK_AOT_RUNNING ||
        state == BENCHMARK_AOT_FINISHING || state == BENCHMARK_WASM_STARTING ||
        state == BENCHMARK_WASM_RUNNING || state == BENCHMARK_WASM_FINISHING) {
        offset = append_text(status, sizeof(status), offset, "：");
        offset = append_text(status, sizeof(status), offset,
                             workload_names[workload_index]);
        offset = append_text(status, sizeof(status), offset, " ");
        offset = append_u32(status, sizeof(status), offset, workload_index + 1u);
        offset = append_text(status, sizeof(status), offset, "/4");
    }
    return pxa_ui_demo_page_render(
        &pxa_lab_ui_generation, packet, sizeof(packet),
        &(pxa_ui_demo_page_t){
            "AOT / WASM 跑分", body, status, "运行对比", NULL,
            PXA_UI_DEMO_PAGE_HAS_BUTTON | PXA_UI_DEMO_PAGE_HAS_PROGRESS, 10,
            progress, 0, 0,
            (uint8_t)(state != BENCHMARK_AOT_STARTING &&
                      state != BENCHMARK_AOT_RUNNING &&
                      state != BENCHMARK_AOT_FINISHING &&
                      state != BENCHMARK_WASM_STARTING &&
                      state != BENCHMARK_WASM_RUNNING &&
                      state != BENCHMARK_WASM_FINISHING)});
}

static int request_clock(uint32_t request_id) {
    return pxa_clock_now(request_id);
}

static int call_benchmark(uint32_t request_id, const char *endpoint,
                          size_t endpoint_length) {
    const uint8_t request[1] = {workload_index};
    return pxa_ipc_call(request_id, endpoint, endpoint_length, request,
                        sizeof(request),
                        payload, sizeof(payload), packet, sizeof(packet));
}

static int start_run(void) {
    state = BENCHMARK_AOT_STARTING;
    workload_index = 0;
    for (uint8_t index = 0; index < BENCHMARK_WORKLOAD_COUNT; ++index) {
        aot_scores[index] = 0;
        wasm_scores[index] = 0;
    }
    return request_clock(CLOCK_AOT_START_REQUEST);
}

static int handle_clock_result(const pxa_event_t *event) {
    int32_t status;
    uint64_t timestamp_us;
    if (!pxa_clock_parse_now(event, &status, &timestamp_us) ||
        status != PXA_STATUS_OK || timestamp_us == 0) return 0;
    if (event->request_id == CLOCK_AOT_START_REQUEST &&
        state == BENCHMARK_AOT_STARTING) {
        started_us = timestamp_us;
        state = BENCHMARK_AOT_RUNNING;
        return call_benchmark(IPC_AOT_REQUEST, "bench.aot", 9);
    }
    if (event->request_id == CLOCK_AOT_END_REQUEST &&
        state == BENCHMARK_AOT_FINISHING) {
        if (timestamp_us <= started_us) return 0;
        aot_scores[workload_index] = score_for_elapsed(timestamp_us - started_us);
        state = BENCHMARK_WASM_STARTING;
        return request_clock(CLOCK_WASM_START_REQUEST);
    }
    if (event->request_id == CLOCK_WASM_START_REQUEST &&
        state == BENCHMARK_WASM_STARTING) {
        started_us = timestamp_us;
        state = BENCHMARK_WASM_RUNNING;
        return call_benchmark(IPC_WASM_REQUEST, "bench.wasm", 10);
    }
    if (event->request_id == CLOCK_WASM_END_REQUEST &&
        state == BENCHMARK_WASM_FINISHING) {
        if (timestamp_us <= started_us) return 0;
        wasm_scores[workload_index] = score_for_elapsed(timestamp_us - started_us);
        ++workload_index;
        if (workload_index == BENCHMARK_WORKLOAD_COUNT) {
            state = BENCHMARK_COMPLETE;
            return 1;
        }
        state = BENCHMARK_AOT_STARTING;
        return request_clock(CLOCK_AOT_START_REQUEST);
    }
    return 0;
}

static int handle_ipc_result(const pxa_event_t *event) {
    pxa_ipc_result_t result;
    if (!pxa_ipc_parse_result(event, &result) || result.status != PXA_STATUS_OK ||
        result.payload_length != 4) return 0;
    if (state == BENCHMARK_AOT_RUNNING) {
        state = BENCHMARK_AOT_FINISHING;
        return request_clock(CLOCK_AOT_END_REQUEST);
    }
    if (state == BENCHMARK_WASM_RUNNING) {
        state = BENCHMARK_WASM_FINISHING;
        return request_clock(CLOCK_WASM_END_REQUEST);
    }
    return 0;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    return pxa_window_fullscreen() && render() ? PXA_STATUS_OK : PXA_STATUS_INTERNAL;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ui_event_data_t ui_event;
    int handled = 0;
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (pxa_ui_parse_event(&parsed, &ui_event) &&
        ui_event.node == PXA_UI_DEMO_PAGE_NODE_BUTTON &&
        ui_event.kind == PXA_UI_EVENT_CLICK_KIND &&
        (state == BENCHMARK_IDLE || state == BENCHMARK_COMPLETE ||
         state == BENCHMARK_FAILED)) {
        if (!start_run()) state = BENCHMARK_FAILED;
        return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
    }
    if (parsed.service == PXA_SERVICE_CLOCK &&
        parsed.opcode == PXA_CLOCK_NOW_RESULT) {
        handled = handle_clock_result(&parsed);
    } else if (parsed.service == PXA_SERVICE_IPC &&
               parsed.opcode == PXA_IPC_RESULT) {
        handled = handle_ipc_result(&parsed);
    } else {
        return PXA_EVENT_UNHANDLED;
    }
    if (!handled) state = BENCHMARK_FAILED;
    return render() ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
