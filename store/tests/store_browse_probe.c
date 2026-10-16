/* Signed AOT + real Host/UI/LVGL, deterministic catalog, no image/network
 * timing in the scroll comparison. Build with build_browse_probe.py. */
#define _POSIX_C_SOURCE 200809L
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define PXSYS_PRODUCT_RUNNER_LIBRARY 1
#define pxsys_desktop_net_backend store_probe_net_backend
#include PXA_PRODUCT_RUNNER

static char catalog[16000];
static size_t offset;
static int pending;
static int paging;
static unsigned pages;
static uint64_t response_ready_us;
static unsigned loading_checks, completed_pages;
static uint64_t body_closed_us;
static void make_page(void) {
    size_t at=(size_t)snprintf(catalog,sizeof(catalog),"{\"payload\":{\"items\":[");
    for(unsigned i=0;i<4;++i) {
        unsigned id=pages*4+i;
        at+=(size_t)snprintf(catalog+at,sizeof(catalog)-at,
            "%s{\"app_id\":\"demo-%u\",\"name\":\"应用 %02u\","
            "\"version\":\"1.0.0\",\"kind\":\"app\",\"size\":65536}",i?",":"",id,id);
    }
    ++pages;
    if(pages<10)snprintf(catalog+at,sizeof(catalog)-at,"],\"next_cursor\":%u}}",pages*4);
    else snprintf(catalog+at,sizeof(catalog)-at,"],\"next_cursor\":null}}");
}
static pxsys_desktop_net_notify_fn notify_ready;
static void *notify_context;
static pxa_status_t start(void *c,const pxa_net_request_t *r,uint64_t *op) {
    (void)c;(void)r;assert(!pending);if(paging)make_page();pending=1;offset=0;*op=1;
    response_ready_us=now_us(NULL)+(paging?UINT64_C(600000):0);
    notify_ready(notify_context);return PXA_STATUS_OK;
}
static pxa_status_t poll(void *c,uint64_t op,pxa_net_response_t *r) {
    (void)c;assert(op==1 && pending);
    if(now_us(NULL)<response_ready_us)return PXA_STATUS_WOULD_BLOCK;
    pending=0;
    r->struct_size=sizeof(*r);r->status_code=200;
    r->content_type=(pxa_bytes_t){(const uint8_t *)"application/json",16};
    r->body_stream=catalog;r->body_length=strlen(catalog);
    r->flags=PXA_NET_RESPONSE_BODY_PRESENT|PXA_NET_RESPONSE_BODY_LENGTH_KNOWN;
    return PXA_STATUS_OK;
}
static void cancel(void *c,uint64_t op) {(void)c;(void)op;pending=0;}
static pxa_status_t read_body(void *c,void *stream,uint8_t *out,size_t cap,size_t *size) {
    (void)c;assert(stream==catalog);
    size_t remaining=strlen(catalog)-offset;*size=remaining<cap?remaining:cap;
    memcpy(out,catalog+offset,*size);offset+=*size;return PXA_STATUS_OK;
}
static void close_body(void *c,void *s) {(void)c;assert(s==catalog);++completed_pages;body_closed_us=now_us(NULL);}
int store_probe_net_backend(pxa_net_backend_t *out,pxsys_desktop_net_notify_fn notify,void *context) {
    notify_ready=notify;notify_context=context;memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);
    out->start=start;out->poll=poll;out->cancel=cancel;out->read_body=read_body;out->close_body=close_body;return 1;
}
typedef struct {product_host_t *host;uint64_t began;const char *directory;int finished,flow;uint64_t last_scroll;uint32_t density_q16;} probe_t;
static void bind_probe(void *context,void (*focus)(void *,bool),void (*exit_app)(void *),void *runner) {
    (void)focus;(void)exit_app;((probe_t *)context)->host=runner;
}
static lv_obj_t *find_list(lv_obj_t *root) {
    if(lv_obj_is_scrollable(root) && lv_obj_get_child_count(root)>=(paging?2u:12u))return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i) {
        lv_obj_t *found=find_list(lv_obj_get_child(root,i));if(found)return found;
    }
    return NULL;
}
static lv_obj_t *find_label(lv_obj_t *root, const char *text) {
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),text))return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i) {
        lv_obj_t *found=find_label(lv_obj_get_child(root,i),text);if(found)return found;
    }
    return NULL;
}
static void verify_navigation(product_host_t *host, uint32_t density_q16) {
    static const char *labels[]={"应用","游戏","已安装","下载"};
    static const uint32_t codepoints[]={0xf015,0xf04b,0xf00c,0xf019};
    const lv_font_t *expected=pxa_lvgl_ui_icon_font(density_q16);
    for(unsigned i=0;i<4;++i) {
        lv_obj_t *caption=find_label(host->content_parent,labels[i]);assert(caption);
        lv_obj_t *button=lv_obj_get_parent(caption),*icon=lv_obj_get_child(button,0);
        assert(icon!=caption && lv_obj_check_type(icon,&lv_label_class));
        const lv_font_t *font=lv_obj_get_style_text_font(icon,0);assert(font==expected);
        lv_font_glyph_dsc_t glyph={0};assert(font->get_glyph_dsc(font,&glyph,codepoints[i],0));
        lv_area_t a,b,parent;lv_obj_get_coords(icon,&a);lv_obj_get_coords(caption,&b);lv_obj_get_coords(button,&parent);
        assert(a.y2<b.y1 && a.x1>=parent.x1 && a.x2<=parent.x2 &&
               a.y1>=parent.y1 && b.y2<=parent.y2 && b.x1>=parent.x1 && b.x2<=parent.x2);
        int top=font->line_height-font->base_line-glyph.box_h-glyph.ofs_y;
        assert(top>=0 && top+glyph.box_h<=lv_area_get_height(&a));
    }
}
static int compare_u64(const void *a,const void *b) {
    const uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y);
}
static void pump_probe(void *context) {
    probe_t *test=context;product_host_t *host=test->host;
    if(test->finished)return;
    if(pending && now_us(NULL)>=response_ready_us)notify_ready(notify_context);
    assert(now_us(NULL)-test->began<UINT64_C(30000000));
    if(!host || !host->active_component || !host->ui)return;
    if(now_us(NULL)-test->began<UINT64_C(2500000))return;
    lv_obj_update_layout(host->content_parent);
    lv_obj_t *list=find_list(host->content_parent);if(!list)return;
    if(paging && pending && pages>1) {
        lv_obj_t *footer=lv_obj_get_child(list,-1);
        assert(!lv_obj_is_hidden(footer));
        assert(!lv_obj_is_clickable(footer));
        lv_obj_t *label=lv_obj_get_child(footer,0);assert(label);
        assert(!strcmp(lv_label_get_text(label),"加载中"));
        ++loading_checks;
    }
    if(paging && pages<10) {
        if(now_us(NULL)-test->last_scroll>UINT64_C(100000)) {
            lv_obj_scroll_to_y(list,lv_obj_get_scroll_y(list)+60,LV_ANIM_OFF);
            dispatch_component_events(host);
            test->last_scroll=now_us(NULL);
        }
        return;
    }
    if(paging && (pending || completed_pages<10 ||
                  now_us(NULL)-body_closed_us<UINT64_C(200000)))return;
    if(paging) {
        assert(loading_checks>0);
        lv_obj_t *footer=lv_obj_get_child(list,-1);
        assert(lv_obj_is_hidden(footer));
        assert(!lv_obj_is_clickable(footer));
    }
    verify_navigation(host,test->density_q16);
    pxa_ui_memory_snapshot_t ui;pxa_wamr_memory_snapshot_t wasm;pxa_memory_stats_t memory;
    assert(!pxa_ui_memory_snapshot(host->ui,host->active_component,&ui));
    assert(!pxa_wamr_engine_memory_snapshot(host->engine,&wasm));
    assert(!pxa_memory_budget_stats(&host->resource_budget,0,&memory));
    lv_area_t bounds;lv_obj_get_coords(list,&bounds);
    if(test->flow) {
        lv_obj_t *parent=lv_obj_get_parent(list);
        for(unsigned i=0;i<lv_obj_get_child_count(parent);++i) {
            lv_obj_t *sibling=lv_obj_get_child(parent,i);if(sibling==list)continue;
            lv_area_t area;lv_obj_get_coords(sibling,&area);
            assert(area.y2<bounds.y1 || area.y1>bounds.y2);
        }
    }
    const unsigned samples=240,warm=30;uint64_t timings[240];
    for(unsigned i=0;i<samples+warm;++i) {
        const uint64_t began=now_us(NULL);
        lv_obj_scroll_to_y(list,(int)(i%90)*2,LV_ANIM_OFF);
        dispatch_component_events(host);
        lv_refr_now(lv_display_get_default());
        if(i>=warm)timings[i-warm]=now_us(NULL)-began;
    }
    pxa_ui_memory_snapshot_t after;
    assert(!pxa_ui_memory_snapshot(host->ui,host->active_component,&after));
    // A scroll with no new data must not rebuild any part of the surface.
    assert(after.commit_count==ui.commit_count);
    uint64_t total=0;for(unsigned i=0;i<samples;++i)total+=timings[i];
    qsort(timings,samples,sizeof(timings[0]),compare_u64);
    char path[2048];snprintf(path,sizeof(path),"%s/report.json",test->directory);
    FILE *file=fopen(path,"w");assert(file);
    fprintf(file,"{\"pages\":%u,\"loading_checks\":%u,\"ui_nodes\":%zu,\"ui_current\":%zu,\"ui_peak\":%zu,"
        "\"linear_current\":%llu,\"linear_peak\":%llu,\"artifact_buffer\":%llu,"
        "\"wamr_current\":%u,\"wamr_peak\":%u,"
        "\"shared_current\":%zu,\"shared_peak\":%zu,"
        "\"list_height_px\":%d,\"scroll_commits\":%llu,\"samples\":%u,"
        "\"scroll_mean_us\":%.2f,\"scroll_median_us\":%llu,\"scroll_p95_us\":%llu}\n",
        pages,loading_checks,ui.node_count,ui.current_bytes,ui.peak_bytes,
        (unsigned long long)wasm.linear_current_bytes,(unsigned long long)wasm.linear_peak_bytes,
        (unsigned long long)wasm.artifact_buffer_bytes,wasm.current_bytes,wasm.peak_bytes,
        memory.charged[0]+memory.charged[1],memory.peak[0]+memory.peak[1],
        (int)lv_area_get_height(&bounds),(unsigned long long)(after.commit_count-ui.commit_count),
        samples,(double)total/samples,(unsigned long long)timings[samples/2],
        (unsigned long long)timings[samples*95/100]);assert(!fclose(file));
    test->finished=1;host->exit_requested=1;
}
int main(int argc,char **argv) {
    assert(argc==6);setenv("SDL_VIDEODRIVER","dummy",1);setenv("SDL_AUDIODRIVER","dummy",1);
    size_t at=(size_t)snprintf(catalog,sizeof(catalog),"{\"payload\":{\"items\":[");
    for(unsigned i=0;i<12;++i)at+=(size_t)snprintf(catalog+at,sizeof(catalog)-at,
        "%s{\"app_id\":\"demo-%u\",\"name\":\"应用 %02u\",\"version\":\"1.0.0\","
        "\"kind\":\"app\",\"summary\":\"固定目录滚动测试\",\"size\":65536}",i?",":"",i,i);
    assert(at<sizeof(catalog));strcat(catalog,"] ,\"next_cursor\":null}}");
    paging=!strcmp(argv[4],"paged");
    options_t options={0};options.package_path=argv[1];options.publisher_key=argv[2];
    options.width=412;options.height=412;options.density_dpi=284;options.display_shape=2;options.locale="zh-CN";
    probe_t test={0};test.directory=argv[3];test.began=now_us(NULL);test.flow=!strcmp(argv[4],"flow");
    options.state_root=argv[5];
    if(paging){options.width=480;options.height=480;options.density_dpi=305;options.display_shape=0;}
    if(getenv("STORE_PROBE_DPI"))options.density_dpi=(unsigned)atoi(getenv("STORE_PROBE_DPI"));
    if(getenv("STORE_PROBE_WIDTH"))options.width=(unsigned)atoi(getenv("STORE_PROBE_WIDTH"));
    if(getenv("STORE_PROBE_HEIGHT"))options.height=(unsigned)atoi(getenv("STORE_PROBE_HEIGHT"));
    if(getenv("STORE_PROBE_SHAPE"))options.display_shape=(unsigned)atoi(getenv("STORE_PROBE_SHAPE"));
    test.density_q16=(uint32_t)(((uint64_t)options.density_dpi*65536u+80u)/160u);
    assert(!run_product_simulator(&options,NULL,NULL,pump_probe,&test,NULL,&test,bind_probe,NULL,NULL,NULL));
    assert(test.finished && !test.host);return 0;
}
