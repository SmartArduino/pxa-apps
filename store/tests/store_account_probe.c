/* Production Store AOT + Host/Storage/LVGL. Only HTTP is synthetic; no real
 * website account or secret is used. Exercises simultaneous catalog/account
 * requests, short-code login, restored credentials and shared likes. */
#define _POSIX_C_SOURCE 200809L
#undef NDEBUG
#define PXSYS_PRODUCT_RUNNER_LIBRARY 1
#define pxsys_desktop_net_backend account_net_backend
#include PXA_PRODUCT_RUNNER
#include <assert.h>

#define ROOT "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define TOKEN "pxad_bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define DEVICE "pxac_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
static const char catalog[]="{\"payload\":{\"items\":[{\"app_id\":\"demo\",\"lineage_root_key_id\":\"" ROOT "\",\"name\":\"账号测试\",\"version\":\"1.0\",\"size\":65536}],\"next_cursor\":null}}";
static const char detail[]="{\"payload\":{\"item\":{\"app_id\":\"demo\",\"lineage_root_key_id\":\"" ROOT "\",\"name\":\"账号测试\",\"version\":\"1.0\",\"size\":65536}}}";
typedef struct {const char *text;size_t offset;unsigned code;} exchange_t;
static pxsys_desktop_net_notify_fn notify_ready;
static void *notify_context;
static unsigned exchanges, peak, likes_put, likes_delete, sessions, polls, authorizations;
static bool liked;
static pxa_status_t start(void *ctx,const pxa_net_request_t *r,uint64_t *op) {
    (void)ctx;char url[512];assert(r->url.size<sizeof(url));memcpy(url,r->url.data,r->url.size);url[r->url.size]=0;
    exchange_t *e=calloc(1,sizeof(*e));assert(e);e->code=200;
    if(strstr(url,"/device/authorize")){++authorizations;e->text="{\"device_code\":\"" DEVICE "\",\"user_code\":\"ABCD-EFGH\",\"interval\":5,\"expires_in\":600}";}
    else if(strstr(url,"/device/token")){++polls;e->text="{\"access_token\":\"" TOKEN "\",\"display_name\":\"测试用户\"}";}
    else if(strstr(url,"/device/session")){
        ++sessions;
        e->text=r->method==PXA_NET_METHOD_DELETE?"{\"revoked\":true}":"{\"display_name\":\"测试用户\"}";
    }else if(strstr(url,"/likes?")){
        if(r->method==PXA_NET_METHOD_PUT){liked=true;++likes_put;}
        else if(r->method==PXA_NET_METHOD_DELETE){liked=false;++likes_delete;}
        e->text=liked?"{\"count\":1,\"liked\":true}":"{\"count\":0,\"liked\":false}";
    }else e->text=strstr(url,"/api/v2/apps/")?detail:catalog;
    ++exchanges;if(exchanges>peak)peak=exchanges;
    *op=(uint64_t)(uintptr_t)e;notify_ready(notify_context);return 0;
}
static pxa_status_t poll(void *ctx,uint64_t op,pxa_net_response_t *r) {
    (void)ctx;exchange_t *e=(void *)(uintptr_t)op;assert(e);
    r->struct_size=sizeof(*r);r->status_code=e->code;r->body_stream=e;
    r->content_type=(pxa_bytes_t){(const uint8_t *)"application/json",16};r->body_length=strlen(e->text);
    r->flags=PXA_NET_RESPONSE_BODY_PRESENT|PXA_NET_RESPONSE_BODY_LENGTH_KNOWN;return 0;
}
static void cancel(void *ctx,uint64_t op){(void)ctx;assert(exchanges);--exchanges;free((void *)(uintptr_t)op);}
static pxa_status_t read_body(void *ctx,void *stream,uint8_t *out,size_t capacity,size_t *size){
    (void)ctx;exchange_t *e=stream;*size=strlen(e->text)-e->offset;if(*size>capacity)*size=capacity;
    memcpy(out,e->text+e->offset,*size);e->offset+=*size;return 0;
}
static void close_body(void *ctx,void *stream){(void)ctx;assert(exchanges);--exchanges;free(stream);}
int account_net_backend(pxa_net_backend_t *out,pxsys_desktop_net_notify_fn notify,void *ctx){
    notify_ready=notify;notify_context=ctx;memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);
    out->start=start;out->poll=poll;out->cancel=cancel;out->read_body=read_body;out->close_body=close_body;return 1;
}
typedef struct{product_host_t *host;const char *directory;unsigned step;uint64_t began,changed;bool done,restore;} probe_t;
static void bind_probe(void *ctx,void (*focus)(void *,bool),void (*exit_app)(void *),void *host){
    (void)focus;(void)exit_app;((probe_t *)ctx)->host=host;
}
static lv_obj_t *label(lv_obj_t *root,const char *text){
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),text))return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){lv_obj_t *r=label(lv_obj_get_child(root,i),text);if(r)return r;}return NULL;
}
static void click(probe_t *p,uint32_t id){
    assert(!pxa_ui_queue_event(p->host->ui,p->host->active_component,PXA_UI_PRIMARY_SURFACE,id,
        PXA_UI_EVENT_ACTION,PXA_UI_EVENT_FLAG_RELIABLE,now_us(NULL),NULL,0));dispatch_component_events(p->host);
}
static void capture(probe_t *p,const char *name){
    lv_obj_update_layout(p->host->content_parent);lv_refr_now(lv_display_get_default());
    lv_draw_buf_t *shot=lv_snapshot_take(p->host->content_parent,LV_COLOR_FORMAT_ARGB8888);assert(shot);
    char path[2048];snprintf(path,sizeof(path),"%s/%s.bgra",p->directory,name);FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(shot->data,1,shot->header.stride*shot->header.h,f)==shot->header.stride*shot->header.h);fclose(f);
    snprintf(path,sizeof(path),"%s/%s.json",p->directory,name);f=fopen(path,"w");assert(f);
    fprintf(f,"{\"width\":%u,\"height\":%u,\"stride\":%u}\n",shot->header.w,shot->header.h,shot->header.stride);fclose(f);lv_draw_buf_destroy(shot);
}
static void check_button(probe_t *p,const char *text){
    lv_obj_t *obj=label(p->host->content_parent,text);assert(obj);obj=lv_obj_get_parent(obj);
    assert(lv_obj_is_clickable(obj) && lv_obj_is_click_focusable(obj));
}
static void pump(void *ctx){
    probe_t *p=ctx;product_host_t *h=p->host;if(p->done||!h||!h->active_component)return;
    const uint64_t now=now_us(NULL);assert(now-p->began<UINT64_C(30000000));
    if(now-p->changed<UINT64_C(160000))return;
    lv_obj_update_layout(h->content_parent);
    if(p->step==0){if(!label(h->content_parent,"账号测试"))return;click(p,1800);}
    else if(p->step==1){
        if(p->restore){if(!label(h->content_parent,"测试用户"))return;assert(sessions && !authorizations);capture(p,"restored");click(p,1808);}
        else {check_button(p,"登录");capture(p,"guest");click(p,1808);}
    }else if(p->step==2){
        if(p->restore){assert(label(h->content_parent,"登录"));assert(!label(h->content_parent,"测试用户"));p->done=true;h->exit_requested=1;return;}
        if(!label(h->content_parent,"ABCD-EFGH"))return;
        assert(!label(h->content_parent,DEVICE) && !label(h->content_parent,TOKEN));
        check_button(p,"取消");capture(p,"code");
    }else if(p->step==3){
        if(!label(h->content_parent,"测试用户"))return;assert(polls==1);check_button(p,"退出登录");capture(p,"signed-in");click(p,63);click(p,200);
    }else if(p->step==4){if(!label(h->content_parent,"点赞  ·  0"))return;capture(p,"before-like");click(p,1813);}
    else if(p->step==5){if(!label(h->content_parent,"已点赞 · 取消  ·  1"))return;assert(likes_put==1);capture(p,"liked");click(p,1813);}
    else if(p->step==6){if(!label(h->content_parent,"点赞  ·  0"))return;assert(likes_delete==1);
        capture(p,"unliked");p->done=true;h->exit_requested=1;return;}
    ++p->step;p->changed=now;
}
int main(int argc,char **argv){
    assert(argc==5);setenv("SDL_VIDEODRIVER","dummy",1);setenv("SDL_AUDIODRIVER","dummy",1);
    options_t options={0};options.package_path=argv[1];options.publisher_key=argv[2];options.state_root=argv[4];
    options.width=296;options.height=240;options.density_dpi=130;options.locale="zh-CN";
    if(getenv("STORE_PROBE_WIDTH"))options.width=atoi(getenv("STORE_PROBE_WIDTH"));
    if(getenv("STORE_PROBE_HEIGHT"))options.height=atoi(getenv("STORE_PROBE_HEIGHT"));
    if(getenv("STORE_PROBE_DPI"))options.density_dpi=atoi(getenv("STORE_PROBE_DPI"));
    probe_t p={0};p.directory=argv[3];p.began=now_us(NULL);p.restore=getenv("STORE_PROBE_RESTORE")!=NULL;
    assert(!run_product_simulator(&options,NULL,NULL,pump,&p,NULL,&p,bind_probe,NULL,NULL,NULL));
    assert(p.done && !p.host && !exchanges);
    printf("Store account AOT/Host: login, likes, focus controls, storage restore=%u, peak HTTP=%u passed\n",p.restore,peak);
    return 0;
}
