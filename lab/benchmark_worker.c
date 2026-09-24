#include "pxa_ipc.h"

#define BENCHMARK_ITERATIONS UINT32_C(1000000)
#define REPLY_REQUEST 1u
#define BENCHMARK_WORKLOAD_ALU 0u
#define BENCHMARK_WORKLOAD_CALL 1u
#define BENCHMARK_WORKLOAD_BRANCH 2u
#define BENCHMARK_WORKLOAD_MEMORY 3u
#define BENCHMARK_WORKLOAD_COUNT 4u
#define BENCHMARK_MEMORY_WORDS 256u

static uint8_t payload[32];
static uint8_t packet[64];
static volatile uint32_t benchmark_sink;
static uint32_t benchmark_memory[BENCHMARK_MEMORY_WORDS];

static uint32_t benchmark_alu(uint32_t seed) {
    uint32_t value = seed;
    uint32_t index;
    for (index = 0; index < BENCHMARK_ITERATIONS; ++index) {
        value ^= index + UINT32_C(0x9e3779b9);
        value *= UINT32_C(1664525);
        value += UINT32_C(1013904223);
    }
    benchmark_sink = value;
    return value;
}

static uint32_t __attribute__((noinline)) benchmark_call_step(uint32_t value,
                                                               uint32_t index) {
    return (value ^ (index + UINT32_C(0x9e3779b9))) * UINT32_C(1664525) +
           UINT32_C(1013904223);
}

static uint32_t benchmark_call(uint32_t seed) {
    uint32_t value = seed;
    uint32_t index;
    for (index = 0; index < BENCHMARK_ITERATIONS; ++index)
        value = benchmark_call_step(value, index);
    benchmark_sink = value;
    return value;
}

static uint32_t benchmark_branch(uint32_t seed) {
    uint32_t value = seed;
    uint32_t index;
    for (index = 0; index < BENCHMARK_ITERATIONS; ++index) {
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        if ((value & 1u) != 0)
            value += index + UINT32_C(0x7f4a7c15);
        else
            value ^= index + UINT32_C(0x6a09e667);
    }
    benchmark_sink = value;
    return value;
}

static uint32_t benchmark_memory_access(uint32_t seed) {
    uint32_t value = seed;
    uint32_t index;
    for (index = 0; index < BENCHMARK_ITERATIONS; ++index) {
        const uint32_t slot = (value >> 16) & (BENCHMARK_MEMORY_WORDS - 1u);
        value += benchmark_memory[slot] + index;
        benchmark_memory[slot] = value ^ UINT32_C(0x9e3779b9);
    }
    benchmark_sink = value;
    return value;
}

static uint32_t benchmark_work(uint8_t workload, uint32_t seed) {
    if (workload == BENCHMARK_WORKLOAD_ALU) return benchmark_alu(seed);
    if (workload == BENCHMARK_WORKLOAD_CALL) return benchmark_call(seed);
    if (workload == BENCHMARK_WORKLOAD_BRANCH) return benchmark_branch(seed);
    if (workload == BENCHMARK_WORKLOAD_MEMORY)
        return benchmark_memory_access(seed);
    return 0;
}


int32_t pxa_app_start(const uint8_t *config, uint32_t config_length) {
    (void)config;
    (void)config_length;
    return PXA_STATUS_OK;
}

int32_t pxa_app_on_event(const uint8_t *event, uint32_t length) {
    pxa_event_t parsed;
    pxa_ipc_request_t request;
    uint32_t result;
    uint8_t reply[4];
    if (!pxa_parse_event(event, length, &parsed)) return PXA_EVENT_UNHANDLED;
    if (parsed.service != PXA_SERVICE_IPC || parsed.opcode != PXA_IPC_REQUEST ||
        !pxa_ipc_parse_request(&parsed, &request) || request.payload_length != 1 ||
        request.payload[0] >= BENCHMARK_WORKLOAD_COUNT) {
        return PXA_EVENT_UNHANDLED;
    }
    result = benchmark_work(request.payload[0], request.call_id);
    reply[0] = (uint8_t)result;
    reply[1] = (uint8_t)(result >> 8);
    reply[2] = (uint8_t)(result >> 16);
    reply[3] = (uint8_t)(result >> 24);
    return pxa_ipc_reply(REPLY_REQUEST, request.call_id, PXA_STATUS_OK, reply,
                         sizeof(reply), payload, sizeof(payload), packet,
                         sizeof(packet)) ? PXA_EVENT_HANDLED : PXA_STATUS_INTERNAL;
}

void pxa_app_stop(uint32_t reason) { (void)reason; }
