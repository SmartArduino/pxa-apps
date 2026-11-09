// SPDX-License-Identifier: MIT
#include "location.hpp"
#include "design.hpp"
#include "camera.hpp"
#include "heading.hpp"
#include <wifi_location.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_layout.hpp>
#include <ctime>
using namespace pxa;
using namespace pxa::ui;
using namespace pxa::ui::literals;
namespace nav = navigation;
#ifndef PXA_NAV_API_ORIGIN
#define PXA_NAV_API_ORIGIN "https://restapi.amap.com"
#endif
namespace {
constexpr std::string_view Api = PXA_NAV_API_ORIGIN;
std::uint64_t now_us() { timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return std::uint64_t(t.tv_sec)*1000000+t.tv_nsec/1000; }
std::uint64_t now_ms() { return now_us()/1000; }
struct NavigationApp {
    Context* ctx = nullptr;
    Environment<DisplayMetrics> env;
    TaskScope jobs, location_jobs, map_jobs, settings_jobs;
    Permission net, wifi;
    navui::Painter paint;
    navui::Palette colors;
    navui::Layout layout;
    std::uint32_t canvas_node=0;
    State<TextInputBox> query_box{TextInputBox{}},key_box{TextInputBox{}},city_box{TextInputBox{}},endpoint_box{TextInputBox{}};
    TextInputRef query_editor,key_editor,city_editor,endpoint_editor;
    unsigned screen=0,return_screen=0,settings_tab=0;
    bool ui_dirty=true,dragging=false,pointer_down=false,in_tick=false;
    unsigned clock_period=0;
    unsigned route_revision=0,notice_revision=0;
    unsigned map_generation=0, location_epoch=0;
    bool location_pending=false, map_pending=false;
    bool location_retry=false,retry_as_start=false;
    unsigned scan_retries=0,location_job_epoch=0;
    std::uint64_t location_retry_due=0;
    std::optional<nav::Viewport> requested_map;
    nav::Viewport image_camera;
    bool bookmark_dirty=false,bookmark_pending=false;
    std::uint64_t bookmark_due=0;
    nav::LocationGate location_gate;
    std::string location_error,saved_view;
    bool map_failed=false;
    bool force_frame=true;
    bool compass_only_dirty=false;
    unsigned pressed_action=0;
    int down_x=0,down_y=0,pan_x=0,pan_y=0,drag_origin_x=0,drag_origin_y=0,list_scroll=0;
    std::uint16_t pointer_id=0;
    std::uint64_t toast_until=0;
    std::optional<WindowMetrics> window_metrics;
    State<std::string> notice{"加载设置…"}, key{""}, city{""}, endpoint{""}, query{""};
    State<bool> idle{false};
    std::string active_key, active_endpoint, active_city;
    std::optional<nav::Place> start, end;
    std::vector<nav::Place> results;
    nav::Mode mode = nav::Mode::walking;
    nav::Route route;
    nav::RouteDrawing route_drawing;
    nav::Tracker tracker;
    nav::Guidance progress;
    std::optional<nav::Fix> fix;
    std::optional<Asset> image;
    nav::Viewport camera;
    unsigned step_index=0, source=0;
    bool live=false, demo=false, initialized=false, search_start=false, wifi_open=false, following=true;
    std::size_t demo_segment=0;
    double demo_t=0;
    std::uint64_t next_fix=0, last_map=0, last_reroute=0, last_demo=0;
    int mw=280, mh=160;
    void message(std::string s) { notice.set(std::move(s));++notice_revision;toast_until=now_ms()+4200;ui_dirty=true; }
    void show(unsigned page) {
        (void)query_editor.hide_keyboard();(void)key_editor.hide_keyboard();
        (void)city_editor.hide_keyboard();(void)endpoint_editor.hide_keyboard();
        screen=page;list_scroll=0;pan_x=pan_y=0;ui_dirty=true;resize_map();draw();
    }
    void clock() {
        const unsigned period=ui_dirty?16:(live&&demo)?33:compass_streaming()?50:!idle.get()?100:500;
        if(clock_period!=period){clock_period=period;(void)ctx->clock().set_period(period);}
    }
    void remember_view(){if(demo)return;bookmark_dirty=nav::bookmark(camera)!=saved_view;if(!bookmark_dirty)return;bookmark_due=now_ms()+2000;}
    Task<void> store_view(){
        const auto value=nav::bookmark(camera);bookmark_dirty=false;
        std::array<std::byte,128> packet{};
        auto r=co_await ctx->storage().set("map.view",std::as_bytes(std::span{value.data(),value.size()}),packet);
        bookmark_pending=false;if(r)saved_view=value;if(!r){bookmark_dirty=true;bookmark_due=now_ms()+10000;}
        co_return Result<void>{};
    }
    void stop() {++location_epoch;location_retry=false;live=false;ui_dirty=true;if(ctx)clock();}
    void invalidate_route() {
        stop();++map_generation;requested_map.reset();route={};++route_revision;if(fix&&fix->simulated)fix.reset();progress={};demo=false;
        ui_dirty=true;
    }
    void done() {idle.set(true);ui_dirty=true;if(ctx)clock();}
    void run(Task<void> job) {
        if(!idle.get())return;
        idle.set(false);ui_dirty=true;clock();
        if(!jobs.start(std::move(job))){done();message("操作暂不可用，请重试");}
    }
    Task<void> load() {
        std::array<std::byte,512> bytes{};
        auto get = [&](std::size_t n){return std::string(reinterpret_cast<const char*>(bytes.data()),n);};
        auto k=co_await ctx->storage().get("amap.key",bytes);if(k) active_key=get(*k);
        auto e=co_await ctx->storage().get("location.url",bytes);if(e) active_endpoint=get(*e);
        auto c=co_await ctx->storage().get("search.city",bytes);if(c) active_city=get(*c);
        auto s=co_await ctx->storage().get_value<std::uint32_t>("location.source");if(s&&*s<=1)source=*s;
        key.set(active_key);endpoint.set(active_endpoint);city.set(active_city);
        camera.center={114.052,22.541};camera.zoom=16;
        auto view=co_await ctx->storage().get("map.view",bytes);if(view)(void)nav::restore_bookmark(get(*view),camera);
        saved_view=nav::bookmark(camera);
        initialized=true;done();notice.set("");toast_until=0;ui_dirty=true;
        draw();(void)ctx->log().write(LogLevel::info,"Navigation: ready");
        if(!active_key.empty())request_map(camera);
        draw();
        co_return Result<void>{};
    }
    Task<void> save() {
        if (!key.get().empty() && !nav::valid_key(key.get())) {message("请输入32位高德 Web 服务 Key");done();co_return Result<void>{};}
        if (!endpoint.get().empty() && !endpoint.get().starts_with("https://") && !endpoint.get().starts_with("http://")) {
            message("位置转发地址需以 http:// 或 https:// 开头");done();co_return Result<void>{};
        }
        std::array<std::byte,768> packet{};
        const auto k=key.get(),e=endpoint.get(),c=city.get();
        auto a=co_await ctx->storage().set("amap.key",std::as_bytes(std::span{k.data(),k.size()}),packet);
        auto b=co_await ctx->storage().set("location.url",std::as_bytes(std::span{e.data(),e.size()}),packet);
        auto d=co_await ctx->storage().set("search.city",std::as_bytes(std::span{c.data(),c.size()}),packet);
        auto s=co_await ctx->storage().set_value("location.source",std::uint32_t(source));
        if(a&&b&&d&&s) {active_key=k;active_endpoint=e;active_city=c;++location_epoch;location_retry=false;location_jobs.cancel();location_pending=false;net.reset();wifi.reset();wifi_open=false;location_gate.reset();message("设置已保存");show(return_screen);}
        else message("设置保存失败，请重试");
        done();
        if(a&&b&&d&&s&&!active_key.empty()&&map_screen())request_map(camera);
        co_return Result<void>{};
    }
    Task<void> permission() {
        if (!net) {
            auto p=co_await ctx->permissions().acquire("net.client","web");
            if(!p) co_return std::unexpected(p.error());net=std::move(*p);
        }
        co_return Result<void>{};
    }
    Task<std::string> fetch(std::string url, std::string body={},bool silent=false) {
        if(url.size()>512){message("请求过长，请缩短搜索词");co_return std::unexpected(Error::limit_exceeded);}
        auto grant=co_await permission();if(!grant)co_return std::unexpected(grant.error());
        const NetHeaderView headers[]={{"User-Agent","PXA-Navigation/0.6.1 (+https://github.com/SmartArduino/pxa-apps)"},{"Content-Type","application/json"}};
        auto opened=co_await ctx->http().open({.url=url,.method=body.empty()?HttpMethod::get:HttpMethod::post,
            .headers=headers,.body=std::as_bytes(std::span{body.data(),body.size()}),.options={.max_body_bytes=nav::MaxBody,.timeout_ms=15000}},net);
        if(!opened)co_return std::unexpected(opened.error());
        auto response=co_await std::move(*opened).response();if(!response)co_return std::unexpected(response.error());
        if(response->status_code!=200&&!silent){message("网络服务返回 HTTP "+nav::unsigned_text(response->status_code));co_return std::unexpected(Error::io_error);}
        std::string output;std::array<std::byte,2048> chunk;
        for(;;){auto n=co_await response->body.read_some(chunk);if(!n)co_return std::unexpected(n.error());if(!*n)break;
            if(output.size()+*n>nav::MaxBody)co_return std::unexpected(Error::limit_exceeded);
            output.append(reinterpret_cast<const char*>(chunk.data()),*n);}
        co_return output;
    }
    void begin_search(bool origin) {
        if (!idle.get()) return;
        search_start=origin;return_screen=screen;query.set("");results.clear();
        show(4);notice.set("");toast_until=0;
    }
    void choose(unsigned index) {
        if(!idle.get()||index>=results.size())return;
        invalidate_route();
        if(search_start)start=results[index];else end=results[index];
        show(5);toast_until=0;
    }
    Task<void> search() {
        results.clear();
        if(auto p=nav::coordinate(query.get())) results.push_back({nav::coordinate(*p),"手动输入的高德坐标（GCJ-02）",*p});
        else if(active_key.empty())message("请先在设置中填写高德 Key");
        else if(query.get().empty())message("请输入地点名称");
        else {
            message("正在搜索…");
            auto body=co_await fetch(nav::search_url(Api,active_key,query.get(),active_city));std::string error;
            if(body&&!nav::places(*body,results,error))message(error);
            else if(!body)message("地点搜索失败，请检查网络和 Key");
        }
        if(!results.empty())toast_until=0;done();co_return Result<void>{};
    }
    void update_step() {
        if(route.steps.empty())return;
        step_index=std::min<unsigned>(step_index,route.steps.size()-1);
    }
    void apply(nav::Fix f) {
        fix=f;
        if(!route.points.empty()){
            progress=tracker.update(route,f,now_ms());
            if(progress.arrived)stop();
            else if(progress.usable&&!progress.off_route){
                if(screen!=2)step_index=progress.step;
                update_step();
            }
        }
        ui_dirty=true;draw();
    }
    Task<void> basemap(nav::Viewport requested) {
        if(demo||active_key.empty())co_return Result<void>{};
        const auto request_generation=++map_generation;
        auto grant=co_await permission();if(!grant)co_return std::unexpected(grant.error());
        auto response=co_await ctx->http().get(nav::map_url(Api,active_key,requested),net,{.max_body_bytes=262144,.timeout_ms=15000});
        if(!response)co_return std::unexpected(response.error());
        if(response->status_code!=200)co_return std::unexpected(Error::io_error);
        if(request_generation!=map_generation||demo)co_return Result<void>{};
        auto decoded=co_await ctx->assets().decode_image(std::move(response->body),{
            .max_width=std::uint16_t(requested.width),.max_height=std::uint16_t(requested.height),
            .max_pixels=std::uint32_t(requested.width*requested.height),.max_encoded_bytes=262144});
        if(!decoded)co_return std::unexpected(decoded.error());
        if(decoded->descriptor().width!=requested.width||decoded->descriptor().height!=requested.height)co_return std::unexpected(Error::protocol_error);
        if(request_generation!=map_generation||demo)co_return Result<void>{};
        image.emplace(std::move(*decoded));image_camera=requested;last_map=now_ms();ui_dirty=true;draw();co_return Result<void>{};
    }
    Task<void> plan(bool reroute=false) {
        if(active_key.empty()||!start||!end){message("请配置高德 Key 并选择起终点");done();co_return Result<void>{};}
        message(reroute?"偏航，正在重新规划…":"正在规划路线…");
        auto body=co_await fetch(nav::route_url(Api,active_key,start->point,end->point,mode));
        nav::Route result;std::string error;
        if(!body||!nav::route(*body,result,error)){message(body?error:"路线请求失败，请检查网络");done();co_return Result<void>{};}
        route=std::move(result);++route_revision;demo=false;following=true;image.reset();if(fix&&fix->simulated)fix.reset();tracker.reset(route);progress={};step_index=0;
        if(fix)progress=tracker.update(route,*fix,now_ms());
        update_step();
        show(1);camera=fit_camera();
        done();request_map(camera);message("路线已加载");co_return Result<void>{};
    }
    void request_location(bool as_start=false,bool background=false,bool retry=false){
        if(location_pending&&location_job_epoch!=location_epoch){location_jobs.cancel();location_pending=false;}
        if(location_pending){if(!background)message("正在获取位置…");return;}
        if(!background&&!retry){following=true;scan_retries=0;location_retry=false;message("正在获取位置…");}
        location_pending=true;location_job_epoch=location_epoch;
        if(!location_jobs.start(locate(as_start,background))){location_pending=false;message("暂时无法获取位置，请重试");}
    }
    Task<void> locate(bool as_start=false,bool background=false) {
        const auto epoch=location_epoch;
        auto finish=[&]{location_pending=false;};
        auto notify=[&](std::string text){if(!background||location_error!=text){location_error=text;message(std::move(text));}};
        const auto selected_source=source;const auto selected_endpoint=active_endpoint;
        std::uint64_t observed=now_ms();std::string payload;
        if(selected_source==0){
            if(!wifi_open){auto opened=co_await ext::wifi_location::open(*ctx);if(!opened){notify("当前固件没有 Wi-Fi 定位扩展，请更新固件或使用网络转发");finish();co_return Result<void>{};}wifi_open=true;}
            if(!wifi){auto p=co_await ctx->permissions().acquire(ext::wifi_location::permission,ext::wifi_location::scope);if(!p){notify("Wi-Fi 扫描未授权");finish();co_return Result<void>{};}wifi=std::move(*p);}
            auto sample=co_await ext::wifi_location::snapshot(*ctx,wifi);
            if(!sample){
                if(sample.error()==Error::would_block){
                    next_fix=now_ms()+2000;
                    if(!background&&scan_retries++<12){location_retry=true;retry_as_start=as_start;location_retry_due=now_ms()+500;}
                    else notify("Wi-Fi 扫描尚未完成，请稍后重试");
                }else notify("没有足够可定位热点");
                finish();co_return Result<void>{};
            }
            observed=sample->timestamp_us/1000;
            if(sample->count<2||now_ms()<observed||now_ms()-observed>30000){notify("Wi-Fi 扫描结果已过期");finish();co_return Result<void>{};}
            payload=std::move(sample->json);
        }else if(selected_endpoint.empty()){notify("请在设置中填写网络位置转发地址");finish();co_return Result<void>{};}
        auto body=co_await fetch(selected_source==0?"https://api.beacondb.net/v1/geolocate":selected_endpoint,payload,true);std::string error;
        auto loc=body?nav::location(*body,observed,selected_source==0,error):std::nullopt;
        if(!loc){notify(body?error:selected_source==0?"定位失败，当前区域可能没有免费定位覆盖":"位置转发不可用，请检查服务是否运行");finish();co_return Result<void>{};}
        if(epoch!=location_epoch||(background&&!live)||!location_gate.eligible(*loc,now_ms()-observed)){finish();co_return Result<void>{};}
        if(loc->wgs84){
            if(active_key.empty()){notify("网络位置已获取；叠加高德地图需填写 Key 以转换坐标");finish();co_return Result<void>{};}
            auto converted=co_await fetch(std::string(Api)+"/v3/assistant/coordinate/convert?key="+active_key+"&coordsys=gps&locations="+nav::coordinate(loc->fix.point),{},true);
            const auto root=converted?nav::document(*converted,error):media::json::Value{};
            auto p=root?nav::coordinate(nav::text(root.get("locations"))):std::nullopt;
            if(!p){notify("坐标转换失败，请检查高德 Key");finish();co_return Result<void>{};}loc->fix.point=*p;
        }
        if(epoch!=location_epoch||(background&&!live)){finish();co_return Result<void>{};}
        if(!location_gate.accept(*loc,now_ms()-observed)){finish();co_return Result<void>{};}
        location_error.clear();apply(loc->fix);
        if(background&&!live){finish();co_return Result<void>{};}
        if(!live&&loc->fix.accuracy<=80&&(as_start||!start))start=nav::Place{"当前位置","",loc->fix.point};
        if(loc->fix.accuracy>80)notify("定位范围较大，暂不判断转向");
        else if(!live){if(following)request_map(focus_camera(loc->fix.point));notify("已找到你的位置");}
        if(live&&idle.get()&&progress.usable&&progress.off_route&&now_ms()-last_reroute>60000){
            last_reroute=now_ms();start=nav::Place{"当前位置","",loc->fix.point};
            idle.set(false);ui_dirty=true;clock();
            auto r=co_await plan(true);(void)r;finish();co_return Result<void>{};
        }
        if(live&&following&&fix&&now_ms()-last_map>15000){
            auto p=screen_point(fix->point);
            if(p.lon<safe.x+30||p.lon>safe.x+safe.w-80||p.lat<safe.y+(layout.compact?80:104)||p.lat>layout.map_h-34){request_map(focus_camera(fix->point));}
        }
        finish();co_return Result<void>{};
    }
    void start_demo() {
        if(!idle.get())return;
        stop();++map_generation;requested_map.reset();bookmark_dirty=false;demo=true;following=true;route=nav::demo_route();++route_revision;tracker.reset(route);image.reset();fix.reset();progress={};
        demo_segment=0;demo_t=0;step_index=0;last_demo=0;
        message("模拟位置与示意道路，仅供功能体验");update_step();
        live=true;show(1);camera=fit_camera();clock();draw();
    }
    void toggle_live() {
        if(!idle.get())return;
        if(live){stop();message("导航已停止");return;}
        if(route.points.empty()){message("请先规划路线或体验模拟导航");return;}
        live=true;following=true;next_fix=0;resize_map();clock();message(demo?"模拟导航已继续":progress.usable?"导航已开始":"导航已开始，等待有效位置");
    }
    void tick() {
        in_tick=true;jobs.reap();location_jobs.reap();map_jobs.reap();settings_jobs.reap();compass_jobs.reap();const auto now=now_ms();
        refresh_compass();update_compass_visual(now_us());
        if(live&&demo&&now!=last_demo){
            const auto elapsed=last_demo?std::min(0.25,(now-last_demo)/1000.0):0.1;last_demo=now;
            const auto a=route.points[demo_segment],b=route.points[demo_segment+1];
            apply({{a.lon+(b.lon-a.lon)*demo_t,a.lat+(b.lat-a.lat)*demo_t},5,now,true});
            demo_t+=elapsed*.12;if(demo_t>1){demo_t=0;if(demo_segment+2<route.points.size())++demo_segment;else demo_t=1;}
        }else if(live&&!demo&&idle.get()&&now>=next_fix){next_fix=now+(source?1000:15000);
            request_location(false,true);}
        if(location_retry&&!location_pending&&idle.get()&&now>=location_retry_due){location_retry=false;request_location(retry_as_start,false,true);}
        if(initialized&&!demo&&bookmark_dirty&&!bookmark_pending&&idle.get()&&!pointer_down&&now>=bookmark_due){bookmark_pending=true;if(!settings_jobs.start(store_view()))bookmark_pending=false;}
        if(live&&!demo&&fix&&progress.usable&&now-fix->time_ms>30000){progress.usable=false;compass_only_dirty=false;ui_dirty=true;}
        if(toast_until&&now>=toast_until){toast_until=0;ui_dirty=true;}
        if(!idle.get())ui_dirty=true;
        if(ui_dirty)flush_draw();
        in_tick=false;clock();
    }
    void zoom_map(int delta) {
        following=false;auto v=camera;v.zoom=std::clamp(v.zoom+delta,1,17);
        if(v.zoom!=camera.zoom)request_map(v);
    }
#include "compass_controls.inc"
#include "key_controls.inc"
#include "interface.inc"
};
}
PXA_APPLICATION(NavigationApp)
