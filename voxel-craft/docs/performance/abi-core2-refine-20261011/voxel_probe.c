#undef NDEBUG
#define PXSYS_PRODUCT_RUNNER_LIBRARY 1
#include PXA_PRODUCT_RUNNER
#include <assert.h>

typedef struct {
    product_host_t *host;
    const char *output;
    unsigned profile, stage;
    uint64_t started, background_at, visible_before;
} probe_t;
static void bind_probe(void *context, void (*focus)(void *, bool),
                       void (*exit_app)(void *), void *runner) {
    (void)focus; (void)exit_app;
    ((probe_t *)context)->host = runner;
}
static void pump_probe(void *context) {
    probe_t *p = context;
    product_host_t *h = p->host;
    if (!h) return;
    assert(now_us(NULL) - p->started < 30000000);
    if (p->stage == 0 && h->raster_telemetry.visible_frames >= 120) {
        p->visible_before = h->raster_telemetry.visible_frames;
        product_focus_changed(h, false); dispatch_lifecycle(h);
        p->background_at = now_us(NULL); p->stage = 1;
    } else if (p->stage == 1 && now_us(NULL) - p->background_at >= 200000) {
        assert(h->raster_telemetry.visible_frames == p->visible_before);
        product_focus_changed(h, true); dispatch_lifecycle(h); p->stage = 2;
    } else if (p->stage == 2 && h->raster_telemetry.visible_frames >= 160) {
        assert(h->surface_display_buffer && h->raster_telemetry.rejected_lists == 0);
        char path[2048];
        snprintf(path, sizeof(path), "%s/profile-%u.rgb565", p->output, p->profile);
        FILE *f = fopen(path, "wb"); assert(f);
        assert(fwrite(h->surface_display_buffer, 1, h->surface_display_frame_bytes, f) == h->surface_display_frame_bytes);
        assert(fclose(f) == 0);
        pxa_wamr_memory_snapshot_t m = {0};
        assert(pxa_wamr_engine_memory_snapshot(h->engine, &m) == 0);
        snprintf(path, sizeof(path), "%s/profile-%u.json", p->output, p->profile);
        f = fopen(path, "w"); assert(f);
#ifdef PRODUCT_SURFACE_FLAG_BORROW_COMMANDS
        const size_t command_buffers = h->raster_draw_list ? 1 : 0;
#else
        const size_t command_buffers = 2;
#endif
        fprintf(f,"{\"width\":%u,\"height\":%u,\"stride\":%u,\"visible\":%llu,\"rendered\":%llu,\"dropped\":%llu,\"wamr\":%u,\"wamr_peak\":%u,\"linear\":%llu,\"linear_peak\":%llu,\"command_bytes\":%zu,\"host_struct\":%zu,\"background_resume\":true}\n",
            h->surface_display_width,h->surface_display_height,h->surface_display_stride_bytes,
            (unsigned long long)h->raster_telemetry.visible_frames,
            (unsigned long long)h->raster_telemetry.rendered_frames,
            (unsigned long long)h->raster_telemetry.dropped_frames,m.current_bytes,m.peak_bytes,
            (unsigned long long)m.linear_current_bytes,(unsigned long long)m.linear_peak_bytes,
            command_buffers*h->raster_max_draw_bytes,sizeof(*h));
        assert(fclose(f) == 0); p->stage = 3; h->exit_requested = 1;
    }
}
int main(int argc, char **argv) {
    assert(argc == 5);
    setenv("SDL_VIDEODRIVER", "dummy", 1); setenv("SDL_AUDIODRIVER", "dummy", 1);
    for (unsigned i = 0; i < 2; ++i) {
        options_t o = {0};
        o.package_path = argv[1]; o.publisher_key = argv[2]; o.state_root = argv[3]; o.locale = "zh-CN";
        o.width = i ? 454 : 296; o.height = i ? 454 : 240; o.density_dpi = i ? 305 : 160;
        if (i) { o.display_shape = PXA_UI_DISPLAY_SHAPE_CIRCLE; for (unsigned j=0;j<4;++j) o.safe_insets[j]=67; }
        probe_t p = {.output=argv[4], .profile=i, .started=now_us(NULL)};
        assert(run_product_simulator(&o, NULL, NULL, pump_probe, &p, NULL, &p, bind_probe, NULL, NULL, NULL) == 0);
        assert(p.stage == 3 && !p.host);
    }
    puts("Voxel real AOT: frame capture, focus/resume, 296x240/160 DPI and round 454x454/305 DPI passed");
}
