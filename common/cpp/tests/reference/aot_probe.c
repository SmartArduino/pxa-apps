/* Test-only observer of the actual signed AOT Guest and product Host.
 * CPU time excludes sleep, panel transfer and device queueing. */
#define _POSIX_C_SOURCE 200809L
#undef NDEBUG
#define PXSYS_PRODUCT_RUNNER_LIBRARY 1
#include <stdint.h>
static void observe_frame(void *, void *, uint64_t, uint64_t);
#define PXSYS_PRODUCT_FRAME_OBSERVER(context, host, frame, cpu_us) \
    observe_frame(context, host, frame, cpu_us)
#include PXA_PRODUCT_RUNNER
#include <assert.h>

typedef struct {
    product_host_t *host;
    uint64_t started, previous, warm_frames, warm_visible, warm_us;
    unsigned samples;
    int warmed, completed, static_frame, lifecycle, lifecycle_stage, pixel_play, play_stage;
    int controller_input, save_started;
    unsigned inputs;
    uint64_t paused_frames, paused_instance;
    uint64_t next_input_us, finish_us;
    FILE *output;
} probe_t;

static void bind_probe(void *context, void (*focus)(void *, bool),
                       void (*exit_app)(void *), void *runner) {
    (void)focus; (void)exit_app;
    ((probe_t *)context)->host = runner;
}

static void snapshot(probe_t *probe, product_host_t *host, const char *phase) {
    pxa_wamr_memory_snapshot_t engine;
    pxa_memory_stats_t budget;
    pxa_asset_cache_stats_t cache = {0};
    size_t metadata = 0;
    assert(pxa_wamr_engine_memory_snapshot(host->engine, &engine) == 0);
    assert(pxa_memory_budget_stats(&host->resource_budget, 0, &budget) == 0);
    if (host->asset_worker)
        pxa_posix_asset_worker_stats(host->asset_worker, &cache, &metadata);
    fprintf(probe->output,
        "{\"phase\":\"%s\",\"linear_current\":%llu,\"linear_peak\":%llu,"
        "\"engine_current\":%u,\"engine_peak\":%u,\"artifact_buffers\":%llu,"
        "\"resource_internal\":%zu,\"resource_external\":%zu,"
        "\"resource_peak_internal\":%zu,\"resource_peak_external\":%zu,"
        "\"cache_external\":%zu,\"cache_peak_external\":%zu,"
        "\"metadata\":%zu,\"framebuffers\":%u,\"depth\":%u,\"mailbox\":%u,"
        "\"render_width\":%u,\"render_height\":%u,"
        "\"rendered\":%llu,\"visible\":%llu,\"dropped\":%llu,"
        "\"capture_us\":%llu,\"visible_delta\":%llu,\"draw_bytes\":%u}\n",
        phase, (unsigned long long)engine.linear_current_bytes,
        (unsigned long long)engine.linear_peak_bytes, engine.current_bytes,
        engine.peak_bytes, (unsigned long long)engine.artifact_buffer_bytes,
        budget.charged[0], budget.charged[1], budget.peak[0], budget.peak[1],
        cache.charged[1], cache.peak_charged[1], metadata,
        host->surface_frame_bytes * host->surface_buffer_count,
        host->raster_depth_buffer ? host->surface_width * host->surface_height * 2u : 0u,
        host->raster_max_draw_bytes * 2u, host->surface_width, host->surface_height,
        (unsigned long long)host->raster_telemetry.rendered_frames,
        (unsigned long long)host->raster_telemetry.visible_frames,
        (unsigned long long)host->raster_telemetry.dropped_frames,
        (unsigned long long)(probe->warm_us ? now_us(NULL)-probe->warm_us : 0),
        (unsigned long long)(host->raster_telemetry.visible_frames-probe->warm_visible),
        host->raster_telemetry.last_draw_list_bytes);
    fflush(probe->output);
}

static void observe_frame(void *context, void *runner, uint64_t frame,
                          uint64_t cpu_us) {
    probe_t *probe = context;
    product_host_t *host = runner;
    if (!probe || !probe->warmed || probe->completed || frame <= probe->previous)
        return;
    probe->previous = frame;
    fprintf(probe->output,
        "{\"phase\":\"frame\",\"sample\":%u,\"cpu_us\":%llu,"
        "\"raster_us\":%u,\"draw_bytes\":%u}\n", probe->samples,
        (unsigned long long)cpu_us, host->raster_telemetry.last_host_raster_us,
        host->raster_telemetry.last_draw_list_bytes);
    ++probe->samples;
    if ((!probe->pixel_play && probe->samples == 60) ||
        (probe->pixel_play && now_us(NULL)-probe->warm_us>=UINT64_C(4000000))) {
        snapshot(probe, host, "end");
        probe->completed = 1;
        if(!probe->pixel_play)host->exit_requested = 1;
    }
}

static void tap(probe_t *probe, int x, int y) {
    uint8_t pointer[12]={0};
    pxa_write_u32(pointer+4,x);pxa_write_u32(pointer+8,y);
    ui_event(1,2,PXA_UI_EVENT_POINTER,PXA_UI_EVENT_FLAG_RELIABLE,pointer,sizeof(pointer),probe->host);
    pointer[1]=2;
    ui_event(1,2,PXA_UI_EVENT_POINTER,PXA_UI_EVENT_FLAG_RELIABLE,pointer,sizeof(pointer),probe->host);
}

static void search(probe_t *probe) {
    if(!probe->controller_input){tap(probe,113,221);return;}
    uint8_t controller[8]={0,1,0,0,0,0,0,0};
    // The C helper subscribes on root 1; the C++ input Canvas uses node 2.
    const uint32_t node=pxa_ui_accepts_event(probe->host->ui,
        probe->host->active_component,1,1,PXA_UI_EVENT_CONTROLLER_STATE)?1:2;
    // The first connected state primes both apps' edge detectors.
    pxa_status_t status=pxa_ui_queue_event(probe->host->ui,probe->host->active_component,1,node,
        PXA_UI_EVENT_CONTROLLER_STATE,PXA_UI_EVENT_FLAG_RELIABLE,now_us(NULL),controller,sizeof(controller));
    if(status)fprintf(stderr,"controller queue rejected %d\n",status);
    assert(status==PXA_STATUS_OK);
    dispatch_component_events(probe->host);
    pxa_write_u32(controller+4,16);
    assert(pxa_ui_queue_event(probe->host->ui,probe->host->active_component,1,node,
        PXA_UI_EVENT_CONTROLLER_STATE,PXA_UI_EVENT_FLAG_RELIABLE,now_us(NULL),controller,sizeof(controller))==PXA_STATUS_OK);
    dispatch_component_events(probe->host);
}

static void pump_probe(void *context) {
    probe_t *probe = context;
    product_host_t *host = probe->host;
    assert(now_us(NULL) - probe->started < UINT64_C(20000000));
    if (host && probe->pixel_play) {
        const uint64_t elapsed=now_us(NULL)-probe->started;
        if(probe->completed){
            if(!probe->save_started){
                product_focus_changed(host,false);probe->save_started=1;
                probe->finish_us=now_us(NULL)+UINT64_C(300000);
            }else if(now_us(NULL)>=probe->finish_us)host->exit_requested=1;
            return;
        }
        if(probe->play_stage==0 && elapsed>=UINT64_C(600000)) {
            tap(probe,148,122);probe->play_stage=1;
        } else if(probe->play_stage==1 && elapsed>=UINT64_C(1000000)) {
            tap(probe,148,103);probe->play_stage=2;
        } else if(probe->warmed && probe->inputs<12 && now_us(NULL)>=probe->next_input_us) {
            const uint64_t before=now_us(NULL);
            search(probe);++probe->inputs;
            fprintf(probe->output,"{\"phase\":\"input\",\"sample\":%u,\"cpu_us\":%llu}\n",probe->inputs,
                (unsigned long long)(now_us(NULL)-before));
            probe->next_input_us=probe->warm_us+probe->inputs*UINT64_C(240000);
        }
    }
    if (host && probe->lifecycle) {
        uint64_t elapsed = now_us(NULL)-probe->started;
        if (probe->lifecycle_stage==0 && elapsed>=UINT64_C(800000)) {
            assert(host->music_token && !host->music_paused);
            probe->paused_instance=host->music_token;
            product_focus_changed(host, false); probe->lifecycle_stage=1;
        } else if (probe->lifecycle_stage==1 && elapsed>=UINT64_C(1000000)) {
            assert(host->clock_period_ms==0 && host->music_paused);
            probe->paused_frames=host->raster_telemetry.rendered_frames;
            probe->lifecycle_stage=2;
        } else if (probe->lifecycle_stage==2 && elapsed>=UINT64_C(1400000)) {
            assert(host->raster_telemetry.rendered_frames==probe->paused_frames);
            assert(host->music_token==probe->paused_instance);
            product_focus_changed(host, true); probe->lifecycle_stage=3;
        } else if (probe->lifecycle_stage==3 && elapsed>=UINT64_C(1800000)) {
            assert(host->clock_period_ms && !host->music_paused);
            assert(host->music_token==probe->paused_instance);
            assert(host->raster_telemetry.rendered_frames>probe->paused_frames);
            fprintf(probe->output,"{\"phase\":\"lifecycle\",\"passed\":true}\n");
            probe->lifecycle_stage=4;
        }
    }
    if (!host || probe->warmed || !host->surface_display_buffer ||
        !host->raster_telemetry.rendered_frames ||
        now_us(NULL) - probe->started < UINT64_C(3000000)) return;
    snapshot(probe, host, "warm");
    if(probe->pixel_play)assert(probe->play_stage==2 && host->raster_telemetry.last_draw_list_bytes>1800);
    probe->previous = host->raster_telemetry.rendered_frames;
    probe->warm_frames = probe->previous;
    probe->warm_visible = host->raster_telemetry.visible_frames;
    probe->warm_us = now_us(NULL);
    probe->next_input_us=probe->warm_us;
    probe->warmed = 1;
    if (probe->static_frame) {
        snapshot(probe, host, "end");
        probe->completed = 1;
        host->exit_requested = 1;
    }
}

int main(int argc, char **argv) {
    assert(argc>=5 && argc<=8);
    probe_t probe = {0}; options_t options = {0};
    probe.output = fopen(argv[4], "w"); assert(probe.output);
    probe.started = now_us(NULL);
    for(int i=5;i<argc;++i) {
        if(!strcmp(argv[i],"static"))probe.static_frame=1;
        else if(!strcmp(argv[i],"pixel-play"))probe.pixel_play=1;
        else if(!strcmp(argv[i],"controller"))probe.controller_input=1;
        else {assert(!strcmp(argv[i],"lifecycle"));probe.lifecycle=1;}
    }
    options.package_path = argv[1]; options.publisher_key = argv[2];
    options.state_root = argv[3];
    options.width = 296; options.height = 240; options.locale = "zh-CN";
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    assert(run_product_simulator(&options, NULL, NULL, pump_probe, &probe,
        NULL, &probe, bind_probe, NULL, NULL, NULL) == 0);
    assert(probe.completed && !probe.host);
    assert(!probe.pixel_play || (probe.inputs==12 && probe.save_started));
    assert(!probe.lifecycle || probe.lifecycle_stage==4);
    assert(!fclose(probe.output));
    return 0;
}
