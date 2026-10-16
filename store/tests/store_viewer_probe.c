/* Deterministic production AOT + Host/LVGL/assets test. Only HTTP is replaced. */
#define _POSIX_C_SOURCE 200809L
#undef NDEBUG
#define PXSYS_PRODUCT_RUNNER_LIBRARY 1
#define pxsys_desktop_net_backend viewer_net_backend
#include PXA_PRODUCT_RUNNER
#include <assert.h>

static const char catalog[] = "{\"payload\":{\"items\":[{\"app_id\":\"demo\",\"name\":\"预览测试\",\"version\":\"1.0\",\"size\":65536}],\"next_cursor\":null}}";
static const char detail[] = "{\"payload\":{\"item\":{\"app_id\":\"demo\",\"name\":\"预览测试\",\"version\":\"1.0\",\"size\":65536,\"screenshots\":["
    "{\"url\":\"/media/apps/1/1\",\"width\":120,\"height\":240},"
    "{\"url\":\"/media/apps/1/2\",\"width\":240,\"height\":120},"
    "{\"url\":\"/media/apps/1/3\",\"width\":240,\"height\":240}]}}}";
static const char *json_body;
static size_t json_offset;
static bool pending;
static bool hold_network;
static bool fail_network;
static unsigned search_requests;
static char last_url[512];
static lv_obj_t *editing_input;
static pxsys_desktop_net_notify_fn notify_ready;
static void *notify_context;
static unsigned char *png[3];
static size_t png_size[3];
static unsigned exchanges, peak_exchanges, image_requests;
typedef struct {unsigned image;size_t offset;} exchange_t;
static pxa_status_t start(void *c,const pxa_net_request_t *r,uint64_t *op) {
    (void)c;assert(!pending);pending=true;json_offset=0;
    char url[512];assert(r->url.size<sizeof(url));memcpy(url,r->url.data,r->url.size);url[r->url.size]=0;
    strcpy(last_url,url);if(strstr(url,"&q="))++search_requests;
    json_body=strstr(url,"/api/v2/apps/")?detail:catalog;*op=1;notify_ready(notify_context);return 0;
}
static pxa_status_t poll(void *c,uint64_t op,pxa_net_response_t *r) {
    (void)c;assert(op==1 && pending);pending=false;
    if(hold_network){pending=true;return PXA_STATUS_WOULD_BLOCK;}
    r->struct_size=sizeof(*r);r->status_code=fail_network?500:200;fail_network=false;r->body_stream=(void *)json_body;
    r->content_type=(pxa_bytes_t){(const uint8_t *)"application/json",16};
    r->body_length=strlen(json_body);r->flags=PXA_NET_RESPONSE_BODY_PRESENT|PXA_NET_RESPONSE_BODY_LENGTH_KNOWN;return 0;
}
static void cancel(void *c,uint64_t op) {(void)c;(void)op;pending=false;}
static pxa_status_t read_body(void *c,void *s,uint8_t *out,size_t cap,size_t *size) {
    (void)c;assert(s==json_body);*size=strlen(json_body)-json_offset;if(*size>cap)*size=cap;
    memcpy(out,json_body+json_offset,*size);json_offset+=*size;return 0;
}
static void close_body(void *c,void *s) {(void)c;(void)s;}
static pxa_status_t open_exchange(void *c,const pxa_net_request_t *r,void **out) {
    (void)c;char url[512];assert(r->url.size<sizeof(url));memcpy(url,r->url.data,r->url.size);url[r->url.size]=0;
    const char *slash=strrchr(url,'/');assert(slash);int image=atoi(slash+1)-1;assert(image>=0 && image<3);
    exchange_t *e=calloc(1,sizeof(*e));assert(e);e->image=(unsigned)image;*out=e;
    ++image_requests;++exchanges;if(exchanges>peak_exchanges)peak_exchanges=exchanges;
    notify_ready(notify_context);return 0;
}
static int32_t exchange_io(void *c,void *context,uint32_t op,uint8_t *out,size_t size) {
    (void)c;exchange_t *e=context;
    if(op==3)return PXA_READY_READ|PXA_READY_HEADERS|(e->offset==png_size[e->image]?PXA_READY_END:0);
    if(op==5)return 16;
    if(op==4){assert(size>=16);memset(out,0,16);pxa_write_u16(out,200);pxa_write_u16(out+2,1);pxa_write_u64(out+8,png_size[e->image]);return 16;}
    assert(op==1);size_t n=png_size[e->image]-e->offset;if(n>size)n=size;
    memcpy(out,png[e->image]+e->offset,n);e->offset+=n;return (int32_t)n;
}
static void close_exchange(void *c,void *e) {(void)c;assert(exchanges);--exchanges;free(e);}
int viewer_net_backend(pxa_net_backend_t *out,pxsys_desktop_net_notify_fn notify,void *context) {
    notify_ready=notify;notify_context=context;memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);
    out->start=start;out->poll=poll;out->cancel=cancel;out->read_body=read_body;out->close_body=close_body;
    out->open_exchange=open_exchange;out->exchange_io=exchange_io;out->close_exchange=close_exchange;return 1;
}
typedef struct {product_host_t *host;const char *directory;unsigned step,debug_step;uint64_t began,changed;int32_t detail_scroll;bool done;} probe_t;
static void bind_probe(void *c,void (*focus)(void *,bool),void (*exit_app)(void *),void *host) {
    (void)focus;(void)exit_app;((probe_t *)c)->host=host;
}
static lv_obj_t *label(lv_obj_t *root,const char *text) {
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),text))return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){lv_obj_t *r=label(lv_obj_get_child(root,i),text);if(r)return r;}return NULL;
}
static lv_obj_t *image(lv_obj_t *root) {
    if(lv_obj_check_type(root,&lv_image_class))return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){lv_obj_t *r=image(lv_obj_get_child(root,i));if(r)return r;}return NULL;
}
static lv_obj_t *editor(lv_obj_t *root) {
    if(lv_obj_check_type(root,&lv_textarea_class))return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){lv_obj_t *r=editor(lv_obj_get_child(root,i));if(r)return r;}return NULL;
}
static void click(probe_t *p,uint32_t node);
static void search_text(probe_t *p,const char *text,bool submit) {
    click(p,60);lv_obj_t *input=editor(p->host->content_parent);assert(input);
    assert(lv_textarea_get_one_line(input));
    lv_textarea_set_text(input,text);dispatch_component_events(p->host);
    if(submit){lv_obj_send_event(input,LV_EVENT_READY,NULL);dispatch_component_events(p->host);}
}
static void click(probe_t *p,uint32_t node) {
    assert(!pxa_ui_queue_event(p->host->ui,p->host->active_component,PXA_UI_PRIMARY_SURFACE,node,
        PXA_UI_EVENT_ACTION,PXA_UI_EVENT_FLAG_RELIABLE,now_us(NULL),NULL,0));dispatch_component_events(p->host);
}
static void point_node(probe_t *p,uint32_t node,unsigned phase,int32_t x,int32_t y,uint64_t when) {
    uint8_t data[12]={0};data[1]=(uint8_t)phase;pxa_write_u32(data+4,(uint32_t)x);pxa_write_u32(data+8,(uint32_t)y);
    assert(!pxa_ui_queue_event(p->host->ui,p->host->active_component,PXA_UI_PRIMARY_SURFACE,node,
        PXA_UI_EVENT_POINTER,PXA_UI_EVENT_FLAG_RELIABLE,when,data,sizeof(data)));dispatch_component_events(p->host);
}
static void point(probe_t *p,unsigned phase,int32_t x,int32_t y,uint64_t when) {
    point_node(p,1620,phase,x,y,when);
}
static void snapshot(probe_t *p,const char *name) {
    lv_obj_update_layout(p->host->content_parent);lv_refr_now(lv_display_get_default());
    lv_draw_buf_t *shot=lv_snapshot_take(p->host->content_parent,LV_COLOR_FORMAT_ARGB8888);assert(shot);
    char path[2048];snprintf(path,sizeof(path),"%s/%s.bgra",p->directory,name);FILE *f=fopen(path,"wb");
    assert(f && fwrite(shot->data,1,shot->header.stride*shot->header.h,f)==shot->header.stride*shot->header.h && !fclose(f));
    snprintf(path,sizeof(path),"%s/%s.json",p->directory,name);f=fopen(path,"w");assert(f);
    fprintf(f,"{\"width\":%u,\"height\":%u,\"stride\":%u}\n",shot->header.w,shot->header.h,shot->header.stride);fclose(f);lv_draw_buf_destroy(shot);
}
static void check_controls(probe_t *p) {
    lv_obj_t *root=p->host->content_parent,*button=lv_obj_get_parent(label(root,"放大"));assert(button);
    lv_obj_t *controls=lv_obj_get_parent(button);lv_area_t parent,previous={0};lv_obj_get_coords(controls,&parent);
    for(unsigned i=0;i<lv_obj_get_child_count(controls);++i){lv_area_t a;lv_obj_get_coords(lv_obj_get_child(controls,i),&a);
        assert(a.x1>=parent.x1 && a.x2<=parent.x2 && a.y1>=parent.y1 && a.y2<=parent.y2);
        if(i)assert(a.x1>previous.x2);previous=a;}
}
static void check_install_action(probe_t *p) {
    lv_obj_t *root=p->host->content_parent,*text=label(root,"安装");assert(text);
    lv_obj_t *button=lv_obj_get_parent(text),*footer=lv_obj_get_parent(button);
    assert(lv_obj_get_child_count(footer)==1 && lv_obj_get_child(footer,0)==button);
    assert(!label(root,"下载") && !label(root,"更多") && !label(root,"仅下载，稍后安装"));
}
static void check_update_action(probe_t *p) {
    click(p,15);click(p,81);lv_obj_update_layout(p->host->content_parent);
    lv_obj_t *root=p->host->content_parent,*text=label(root,"\xef\x80\xa1");
    if(text) {
        const lv_font_t *font=lv_obj_get_style_text_font(text,0);lv_font_glyph_dsc_t glyph={0};
        assert(font && font->get_glyph_dsc(font,&glyph,0xf021,0) && glyph.box_w && glyph.box_h);
        int top=font->line_height-font->base_line-glyph.box_h-glyph.ofs_y;
        assert(top>=0 && top+glyph.box_h<=lv_obj_get_height(text));
    } else text=label(root,"检查更新");
    assert(text);lv_area_t a,b;lv_obj_get_coords(text,&a);lv_obj_get_coords(lv_obj_get_parent(text),&b);
    assert(a.x1>=b.x1 && a.x2<=b.x2 && a.y1>=b.y1 && a.y2<=b.y2);
    snapshot(p,"installed");
}
static void pump_probe(void *context) {
    probe_t *p=context;product_host_t *h=p->host;if(p->done || !h || !h->active_component)return;
    uint64_t now=now_us(NULL);assert(now-p->began<UINT64_C(30000000));
    if(now-p->changed<UINT64_C(180000))return;lv_obj_update_layout(h->content_parent);
    if(now-p->began>UINT64_C(1000000) && p->debug_step!=p->step+1) {
        char name[32];snprintf(name,sizeof(name),"debug-%u",p->step);snapshot(p,name);
        fprintf(stderr,"Viewer probe step=%u image_requests=%u\n",p->step,image_requests);p->debug_step=p->step+1;
    }
    if(p->step==0){if(!label(h->content_parent,"预览测试"))return;click(p,200);}
    else if(p->step==1){if(!image(h->content_parent))return;
        check_install_action(p);snapshot(p,"detail");
        point_node(p,1611,0,80,10,now);point_node(p,1611,1,80,60,now+1000);point_node(p,1611,2,80,60,now+2000);click(p,1611);
        check_install_action(p);assert(!label(h->content_parent,"截图 1 / 3"));}
    else if(p->step==2){check_install_action(p);
        lv_obj_t *scroll=lv_obj_get_parent(lv_obj_get_parent(image(h->content_parent)));
        lv_obj_scroll_to_y(scroll,40,LV_ANIM_OFF);dispatch_component_events(h);p->detail_scroll=lv_obj_get_scroll_y(scroll);
        click(p,1611);}
    else if(p->step==3){assert(label(h->content_parent,"截图 1 / 3"));check_controls(p);snapshot(p,"fit");click(p,1627);}
    else if(p->step==4){assert(label(h->content_parent,"2×"));snapshot(p,"zoom");
        lv_obj_t *viewport=lv_obj_get_parent(lv_obj_get_parent(image(h->content_parent)));assert(lv_obj_is_scrollable(viewport));
        pxa_ui_memory_snapshot_t before,after;assert(!pxa_ui_memory_snapshot(h->ui,h->active_component,&before));
        point(p,0,100,80,now);for(unsigned i=0;i<60;++i){lv_obj_scroll_to(viewport,(int)i,(int)i,LV_ANIM_OFF);point(p,1,100-(int)i,80,now+i*1000);}
        point(p,2,40,80,now+61000);assert(lv_obj_get_scroll_y(viewport)>0);
        assert(!pxa_ui_memory_snapshot(h->ui,h->active_component,&after));assert(after.commit_count==before.commit_count);
        snapshot(p,"pan");click(p,1629);}
    else if(p->step==5){if(!image(h->content_parent))return;assert(label(h->content_parent,"截图 2 / 3"));check_controls(p);snapshot(p,"second");
        point(p,0,130,70,now);point(p,1,70,72,now+10000);point(p,2,20,72,now+30000);}
    else if(p->step==6){if(!image(h->content_parent))return;assert(label(h->content_parent,"截图 3 / 3"));snapshot(p,"third");
        click(p,1629);assert(label(h->content_parent,"截图 3 / 3"));
        point(p,0,80,70,now);point(p,2,80,70,now+1000);point(p,0,80,70,now+100000);point(p,2,80,70,now+101000);}
    else if(p->step==7){assert(label(h->content_parent,"2×"));click(p,1627);assert(label(h->content_parent,"3×"));click(p,1627);assert(label(h->content_parent,"放大"));click(p,15);}
    else if(p->step==8){check_install_action(p);
        lv_obj_t *scroll=lv_obj_get_parent(lv_obj_get_parent(image(h->content_parent)));
        assert(abs(lv_obj_get_scroll_y(scroll)-p->detail_scroll)<=2);
        snapshot(p,"returned");
        assert(peak_exchanges==1);check_update_action(p);
        click(p,63);hold_network=true;search_text(p,"天气",true);}
    else if(p->step==9){if(search_requests<1)return;
        assert(strstr(last_url,"&q=%E5%A4%A9%E6%B0%94"));
        assert(!editor(h->content_parent));
        search_text(p,"浏览器",false);snapshot(p,"search");
        click(p,56);assert(!editor(h->content_parent));assert(search_requests==1);
        hold_network=false;notify_ready(notify_context);}
    else if(p->step==10){if(search_requests<2 || pending)return;
        assert(search_requests==2 && strstr(last_url,"&q=%E6%B5%8F%E8%A7%88%E5%99%A8"));
        search_text(p,"weather",false);click(p,56);}
    else if(p->step==11){if(search_requests<3 || pending)return;
        assert(search_requests==3 && strstr(last_url,"&q=weather"));
        assert(!editor(h->content_parent));snapshot(p,"search-results");
        hold_network=true;search_text(p,"first",true);}
    else if(p->step==12){if(search_requests<4)return;
        search_text(p,"draft",false);editing_input=editor(h->content_parent);
        hold_network=false;notify_ready(notify_context);}
    else if(p->step==13){if(pending)return;
        assert(search_requests==4 && editor(h->content_parent)==editing_input);
        assert(!strcmp(lv_textarea_get_text(editing_input),"draft"));
        hold_network=true;lv_textarea_set_text(editing_input,"failure");dispatch_component_events(h);
        lv_obj_send_event(editing_input,LV_EVENT_READY,NULL);dispatch_component_events(h);}
    else if(p->step==14){if(search_requests<5)return;
        search_text(p,"weather",false);click(p,56);
        assert(search_requests==5);fail_network=true;hold_network=false;notify_ready(notify_context);}
    else {if(search_requests<6 || pending)return;
        assert(search_requests==6 && strstr(last_url,"&q=weather"));
        assert(!editor(h->content_parent));
pxa_ui_memory_snapshot_t ui;assert(!pxa_ui_memory_snapshot(h->ui,h->active_component,&ui));
        char path[2048];snprintf(path,sizeof(path),"%s/report.json",p->directory);FILE *f=fopen(path,"w");assert(f);
        fprintf(f,"{\"passed\":true,\"search_requests\":6,\"queued_failure_recovery\":true,\"editor_preserved\":true,\"pan_commits\":0,\"peak_http_exchanges\":%u,\"image_requests\":%u,\"ui_nodes\":%zu,\"ui_current\":%zu,\"ui_peak\":%zu}\n",peak_exchanges,image_requests,ui.node_count,ui.current_bytes,ui.peak_bytes);fclose(f);

        fprintf(stderr,"Search: native submit, magnifier, queued UTF-8, preserved editor and failure recovery passed\n");
        p->done=true;h->exit_requested=true;return;}
    ++p->step;p->changed=now;
}
int main(int argc,char **argv) {
    assert(argc==6);setenv("SDL_VIDEODRIVER","dummy",1);setenv("SDL_AUDIODRIVER","dummy",1);
    for(unsigned i=0;i<3;++i){char path[2048];snprintf(path,sizeof(path),"%s/%u.png",argv[4],i+1);FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);png_size[i]=(size_t)ftell(f);rewind(f);png[i]=malloc(png_size[i]);assert(png[i]);assert(fread(png[i],1,png_size[i],f)==png_size[i]);fclose(f);}
    options_t options={0};options.package_path=argv[1];options.publisher_key=argv[2];options.state_root=argv[5];
    options.width=480;options.height=480;options.density_dpi=305;options.display_shape=1;options.locale="zh-CN";
    options.corner_radius=40;
    if(getenv("STORE_PROBE_DPI"))options.density_dpi=(unsigned)atoi(getenv("STORE_PROBE_DPI"));
    if(getenv("STORE_PROBE_WIDTH"))options.width=(unsigned)atoi(getenv("STORE_PROBE_WIDTH"));
    if(getenv("STORE_PROBE_HEIGHT"))options.height=(unsigned)atoi(getenv("STORE_PROBE_HEIGHT"));
    if(getenv("STORE_PROBE_SHAPE"))options.display_shape=(unsigned)atoi(getenv("STORE_PROBE_SHAPE"));
    if(getenv("STORE_PROBE_RADIUS"))options.corner_radius=(unsigned)atoi(getenv("STORE_PROBE_RADIUS"));
    probe_t p={0};p.directory=argv[3];p.began=now_us(NULL);
    assert(!run_product_simulator(&options,NULL,NULL,pump_probe,&p,NULL,&p,bind_probe,NULL,NULL,NULL));
    assert(p.done && !p.host && !exchanges);for(unsigned i=0;i<3;++i)free(png[i]);return 0;
}
