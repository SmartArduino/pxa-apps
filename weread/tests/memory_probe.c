/* Read-only observer around the unmodified product runner. This executable is
 * a validation tool, not application or Host firmware. Run the normal simulator
 * test driver against it, with PXA_WEREAD_MEMORY_REPORT pointing at a private
 * JSON file. Snapshot counters overlap; never sum allocator and linear bytes. */
#ifndef PXA_PRODUCT_RUNNER
#error Define PXA_PRODUCT_RUNNER as the source path of the product simulator
#endif
#define main original_product_main
#include PXA_PRODUCT_RUNNER
#undef main

typedef struct {
    product_host_t *host;
    pxa_wamr_memory_snapshot_t memory;
    pxa_memory_stats_t resources;
    uint64_t samples, previous_write;
    const char *report;
} observer_t;
static void publish(const observer_t *o) {
    if (!o->report || !o->samples) return;
    FILE *f = fopen(o->report, "w");
    if (!f) return;
    fprintf(f, "{\"samples\":%llu,\"linear_current_bytes\":%llu,\"linear_peak_bytes\":%llu,"
        "\"wamr_current_bytes\":%u,\"wamr_peak_bytes\":%u,\"artifact_buffer_bytes\":%llu,"
        "\"event_buffer_bytes\":%llu,\"resource_current_bytes\":%zu,\"resource_peak_internal_bytes\":%zu,\"resource_peak_external_bytes\":%zu,"
        "\"image_current_bytes\":%zu,\"temporary_current_bytes\":%zu,\"temporary_peak_internal_bytes\":%zu,\"temporary_peak_external_bytes\":%zu,"
        "\"scope\":\"current simulator engine; overlapping counters\"}\n",
        (unsigned long long)o->samples,
        (unsigned long long)o->memory.linear_current_bytes,
        (unsigned long long)o->memory.linear_peak_bytes,
        o->memory.current_bytes, o->memory.peak_bytes,
        (unsigned long long)o->memory.artifact_buffer_bytes,
        (unsigned long long)o->memory.event_buffer_bytes,
        o->resources.charged[0]+o->resources.charged[1],
        o->resources.peak[0],o->resources.peak[1],
        o->resources.by_kind[PXA_MEMORY_IMAGE][0]+o->resources.by_kind[PXA_MEMORY_IMAGE][1],
        o->resources.by_kind[PXA_MEMORY_TEMPORARY][0]+o->resources.by_kind[PXA_MEMORY_TEMPORARY][1],
        o->resources.temporary_peak[0],o->resources.temporary_peak[1]);
    fclose(f);
}
static void observe(void *context) {
    observer_t *o = context;
    if (!o->host || !o->host->engine) return;
    pxa_wamr_memory_snapshot_t snapshot;
    if (pxa_wamr_engine_memory_snapshot(o->host->engine, &snapshot)) return;
    const bool changed = o->memory.linear_current_bytes != snapshot.linear_current_bytes;
    o->memory = snapshot; ++o->samples;
    (void)pxa_memory_budget_stats(&o->host->resource_budget,0,&o->resources);
    const uint64_t now = now_us(NULL);
    if (changed || now - o->previous_write > UINT64_C(250000)) {
        o->previous_write = now; publish(o);
    }
}
static void bind_observer(void *context, void (*focus)(void *, bool),
    void (*exit_app)(void *), void *runner) {
    (void)focus; (void)exit_app;
    observer_t *o = context;
    o->host = runner;
}
int main(int argc, char **argv) {
    options_t options;
    if (!parse_options(argc, argv, &options)) return 2;
    observer_t observer = {.report = getenv("PXA_WEREAD_MEMORY_REPORT")};
    if (!observer.report) return 2;
    const int result = run_product_simulator(&options, NULL, NULL, observe,
        &observer, NULL, &observer, bind_observer, NULL, NULL, NULL);
    publish(&observer); return result;
}
