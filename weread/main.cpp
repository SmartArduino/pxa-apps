#include <pxa/libcpp_diagnostics.hpp>
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/canvas.hpp>
#include <pxa/ui_display.hpp>
#include <pxa/ui_geometry.hpp>
#include <pxa/ui_volume_keys.hpp>
#include <pxa/ui_gesture.hpp>
#include "protocol.hpp"
#include "reading.hpp"
#include "inline_image.hpp"
#include "state.hpp"
#include "shelf_snapshot.hpp"
#include "bookmarks.hpp"
#include "search.hpp"
#include "image_page.hpp"
#include "agent.hpp"
#include "notes.hpp"
#include "diagnostics.hpp"
#include "reader_input.hpp"
#include "reader_gzip.hpp"
#ifdef WEREAD_STACK_PROBE
#include "tests/stack_probe.hpp"
#endif
#include <chrono>
#include <cmath>

using namespace pxa::ui;
namespace wr=weread;
struct WeReadApp;
struct ReadingPointer {WeReadApp* app;void operator()(const CanvasPointer&) const;};

struct WeReadApp {
    static constexpr pxa::game::LoopOptions loop_options{16000,16,1};
    enum class Screen : std::uint8_t {login,shelf,reading,menu,settings,about,contents,search,unavailable,details,bookmarks,picture};
    enum class Action {refresh,open,previous,next,settings,home,back,logout,about,larger,smaller,theme,remember,exit,menu,contents,chapter,leading,volume,images,search,search_go,search_open,details,details_read,details_tab,bookmarks,bookmark_add,bookmark_open,bookmark_remove};
    struct Details {Screen parent;wr::Book book;std::string body;std::size_t offset=0;std::unique_ptr<wr::agent::NoteNavigation> notes;bool key_setup=false;};
    std::unique_ptr<Details> details;
    struct BookmarkPanel {wr::LocalBookmarks data;std::size_t first=0;std::optional<unsigned> pending_remove;};
    std::unique_ptr<BookmarkPanel> bookmark_panel;
    struct Hit {PixelRect box;Action action;int index=0;};
    pxa::Context* context=nullptr;
    DisplayMetrics metrics;
    CanvasRef canvas;
    State<bool> volume_capture{false};
    CanvasCommands<4000> draw;
    CanvasCommands<4000> scene_draw;
    Screen scene_screen=Screen::login;
    bool scene_valid=false,scene_night=false;
    std::optional<CanvasRegion> presented_wait;
    std::string presented_wait_text;
    pxa::Permission permission;
    wr::Session session;
    wr::SavedState saved;
    wr::Login login;
    wr::Shelf shelf;
    wr::Search search;
    State<std::string> query{""};
    TextInputRef search_input;
    State<TextInputBox> search_box{TextInputBox{}};
    std::vector<std::uint32_t> search_history;
    std::uint32_t search_cursor=0;
    std::size_t search_start=0;
    std::string search_keyword;
    wr::Reading reader;
    std::string reader_account;
    std::chrono::steady_clock::time_point reader_loaded{};
    wr::ReadingPage reading_page;
    wr::PageImages page_images;
    struct Picture {
        std::uint16_t index;
        std::optional<wr::PageImages::Entry> preview;
        unsigned decode_budget=262144;
        int zoom=1,pan_x=0,pan_y=0,last_x=0,last_y=0;
        bool dragging=false;
    };
    std::optional<Picture> picture;
    PressGesture picture_press;
    std::optional<std::uint16_t> held_image;
    std::vector<std::byte> character_map;
    std::vector<std::size_t> page_history;
    std::size_t reading_offset=0,contents_start=0,settings_start=0;
    std::optional<pxa::Asset> qr;
    Screen screen=Screen::login,return_screen=Screen::shelf;
    std::string status="正在准备…";
    std::array<Hit,24> hits{};
    std::size_t hit_count=0,list_start=0,server_start=0;
    int server_key=-1;
    struct ShelfCursor {std::size_t start;int key;};
    std::vector<ShelfCursor> shelf_history;
    std::array<std::byte,1024> receive{};
    pxa::input::PointerCapture pointer;
    int press_x=0,press_y=0;
    bool moved=false,dirty=true,busy=false,night=false,initializing=true,covers_active=false;
    bool auth_dirty=false,save_pending=false,save_running=false,login_after_save=false,close_after_save=false,reauth_required=false;
    bool shelf_syncing=false,shelf_local=false;
    std::uint64_t save_at=0;
    unsigned font_dp=18;
    std::uint64_t now=0,retry_at=0;
    std::uint32_t page=0;

    auto view(){return CaptureVolumeKeys(Stack(Canvas(canvas).on_pointer(ReadingPointer{this}),
        TextInput(query,search_input).box(search_box).max_bytes(192).single_line().on_submit([this]{
            (void)search_input.hide_keyboard();if(screen==Screen::search)action(Action::search_go);
        })).fill().fill_height(),volume_capture,
        [this](const pxa::input::VolumeKeyEvent& key){
            if(key.key==pxa::input::VolumeKey::down)turn(1);
            else if(key.key==pxa::input::VolumeKey::up)turn(-1);
        });}
    int dp(float n)const{return std::max(1,int(n*metrics.density_q16/65536.f+.5f));}
    int logical(int n)const{return int(std::floor(double(n)*65536/metrics.density_q16));}
    CanvasRegion region(PixelRect r)const {
        int x=logical(r.x),y=logical(r.y);
        return {x,y,std::max(1,int(std::ceil(double(r.x+r.width)*65536/metrics.density_q16))-x),
            std::max(1,int(std::ceil(double(r.y+r.height)*65536/metrics.density_q16))-y)};
    }
    PixelRect safe()const{return safe_rectangle(metrics);}
    std::uint32_t background()const{return night?0x161c22ff:0xf5f7faff;}
    std::uint32_t ink()const{return night?0xe5eaf0ff:0x243142ff;}
    std::uint32_t muted()const{return night?0x9ba7b7ff:0x6d7b8dff;}
    std::uint32_t surface()const{return night?0x232d38ff:0xffffffff;}
    static constexpr auto blue=0x2879eaff;
    bool showing_login()const{
        return screen==Screen::login||((screen==Screen::settings||screen==Screen::about)&&return_screen==Screen::login);
    }
    void log(std::string_view message){(void)context->log().write(pxa::LogLevel::info,message);}
    void log(std::string_view message,std::initializer_list<wr::Metric> metrics){
        if(auto line=wr::diagnostic(message,metrics))log(line->view());
    }
    void notice(std::string value){status=std::move(value);dirty=true;}
    bool waiting()const{return login_after_save||close_after_save||(busy&&!(showing_login()&&login.phase==wr::LoginPhase::polling));}
    PixelRect wait_box()const{
        const auto r=safe();const int gap=std::max(4,dp(8)),head=std::min(dp(44),r.height/5);
        if(screen==Screen::reading){const auto box=reading_box();return {box.x,box.y+box.height,box.width,std::min(dp(24),r.y+r.height-box.y-box.height)};}
        int width=r.width-2*gap;
        if(screen==Screen::shelf)width-=2*(head+gap);
        if(screen==Screen::details)width-=head+dp(52)+gap;
        if(screen==Screen::login)width-=head+gap;
        if(screen==Screen::bookmarks&&bookmark_panel)width-=std::min(dp(52),r.width/3)+gap;
        return {r.x+gap,r.y,std::max(1,width),head};
    }
    auto show_wait(){
        if(canvas.mounted()&&context->foreground())render();
        return context->clock().yield();
    }
    void fail(std::string_view operation,pxa::Error e) {
        busy=false;
        if(screen==Screen::login){login.phase=wr::LoginPhase::failed;qr.reset();}
        notice(std::string(operation)+"未完成，请重试 ("+wr::number(int(e))+")");
        (void)context->window().toast(status);
        const auto tasks=pxa::task_pool_stats();
        log("WEREAD ERROR",{{"code",int(e)},{"phase",int(login.phase)},{"task_peak",tasks.peak_slots},{"task_failures",tasks.allocation_failures}});
    }
    void start(pxa::Task<void> task,bool foreground=true){
        auto r=foreground?context->foreground_tasks().start(std::move(task)):context->tasks().start(std::move(task));
        if(!r)fail("任务",r.error());
    }
    pxa::Result<void> on_start(pxa::Context& ctx,std::span<const std::byte> config) {
#ifdef WEREAD_STACK_PROBE
        stack_probe::initialize();
#endif
        context=&ctx;page_images.configure(ctx,permission);
        if(auto d=decode_start_display(config))metrics=*d;
        auto capabilities=decode_start_ui_capabilities(config);
        if(!capabilities || !capabilities->supports(UiFeature::sized_text))return std::unexpected(pxa::Error::unsupported);
        (void)ctx.window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::transient);
        auto error=[](void* self,pxa::Error e)noexcept {static_cast<WeReadApp*>(self)->fail("连接",e);};
        ctx.tasks().on_error(this,error);ctx.foreground_tasks().on_error(this,error);
        start(restore());return {};
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event){
        if(event.is<DisplayMetrics>())if(auto d=decode_display_metrics(event)){metrics=*d;pointer.reset();picture_press.reset();held_image.reset();page_images.reset();if(picture)picture->preview.reset();scene_valid=false;dirty=true;}
        if(event.is<pxa::PermissionRevoked>() && pxa::decode_permission_revoked(event)) {
            context->foreground_tasks().cancel();page_images.reset();permission.reset();busy=false;scene_valid=false;
            qr.reset();login.clear();notice("网络权限已关闭，可在系统设置中重新授权");
        }
        return false;
    }
    void on_background(pxa::Context&) {
        (void)search_input.hide_keyboard();pointer.reset();busy=false;page_images.reset();bookmark_panel.reset();scene_valid=false;
        picture_press.reset();held_image.reset();if(picture){picture->dragging=false;picture->preview.reset();}
        remember_position();save(true);
        if(showing_login()){qr.reset();login.clear();notice("已暂停，轻点重新获取二维码");}
    }
    void on_foreground(pxa::Context&){dirty=true;if(screen==Screen::bookmarks)start(load_bookmarks());}
    pxa::BackAction on_back(){
        if(screen==Screen::picture){close_picture();return pxa::BackAction::stay;}
        if(shelf_syncing)context->foreground_tasks().cancel();
        if(busy&&!showing_login()){context->foreground_tasks().cancel();busy=false;}
        if(screen==Screen::login || screen==Screen::shelf){
            if(auth_dirty||save_pending||save_running){close_after_save=true;notice("正在保存阅读位置…");save(true);return pxa::BackAction::stay;}
            return pxa::BackAction::close;
        }
        if(screen==Screen::search){(void)search_input.hide_keyboard();screen=Screen::shelf;dirty=true;return pxa::BackAction::stay;}
        if(screen==Screen::details){leave_details();return pxa::BackAction::stay;}
        if(screen==Screen::bookmarks){bookmark_panel.reset();screen=Screen::menu;dirty=true;return pxa::BackAction::stay;}
        if(screen==Screen::reading)screen=Screen::menu;
        else if(screen==Screen::menu || screen==Screen::contents)screen=Screen::reading;
        else if(screen==Screen::settings||screen==Screen::unavailable)screen=return_screen;
        else if(screen==Screen::about)screen=Screen::settings;
        dirty=true;return pxa::BackAction::stay;
    }
    void on_frame(pxa::Context&,pxa::game::FrameTick tick){
#ifdef WEREAD_STACK_PROBE
        stack_probe::sample(*context);
#endif
        now=tick.timestamp_us;
        if(held_image&&picture_press.advance(now,PressOptions{.slop=dp(8)}))show_picture(*held_image);
        if(page_images.poll()){
            for(std::size_t i=0;!covers_active&&i<reader.images.size();++i)if(!reader.images[i].explicit_aspect)
                if(const auto* entry=page_images.find(i);entry&&entry->asset){
                    const auto d=entry->asset->descriptor();
                    reader.images[i].aspect_q16=std::uint32_t(std::clamp<std::uint64_t>((std::uint64_t(d.source_width)<<16)/d.source_height,1024,4194304));
                    reader.images[i].explicit_aspect=true;
                }
            dirty=true;
        }
        if(login.phase==wr::LoginPhase::polling && login.deadline && now>=login.deadline){
            log("WEREAD QR EXPIRED");
            context->foreground_tasks().cancel();busy=false;qr.reset();login.phase=wr::LoginPhase::expired;
            notice("二维码已过期，请重新获取");
        }
        if(retry_at && now>=retry_at && login.phase==wr::LoginPhase::polling){retry_at=0;start(poll_login());}
        if(save_pending&&!save_running&&now>=save_at)start(persist(),false);
        if(dirty && canvas.mounted()){render();}
    }
    pxa::Task<wr::ReaderInput> request(std::string path,std::string body={},std::size_t limit=8192,std::uint32_t timeout=20000,
        wr::agent::Target target=wr::agent::Target::web,std::string_view bearer={},bool compressed=true) {
        if(!wr::agent::route_allowed(target,path)||(target==wr::agent::Target::gateway&&!wr::agent::valid_key(bearer)))
            co_return std::unexpected(pxa::Error::invalid_argument);
        if(!permission){
            auto p=co_await context->permissions().acquire("net.client","web");
            if(!p)co_return std::unexpected(p.error());permission=std::move(*p);
        }
        // The typed target fixes both origin and credential policy. Never copy
        // Cookie to the agent gateway or Bearer credentials to web/image URLs.
        const bool web=target==wr::agent::Target::web;
        const bool reader_page=web&&path.starts_with("/wrwebsimplenjlogic/reader");
        auto url=std::string(web?wr::origin:wr::agent::gateway_origin)+path;
        const auto cookie=web?session.header():std::string{};
        const auto authorization=web?std::string{}:"Bearer "+std::string(bearer);
        const auto referer=std::string(wr::origin)+((!web||path==wr::agent::key_path)?"/r/weread-skills":
            path.starts_with("/api/")?"/":std::string(wr::service_path)+"/login");
        std::array<pxa::NetHeaderView,8> headers{{{"User-Agent",wr::user_agent},{"Accept","application/json, text/html;q=0.8"},
            {"Accept-Encoding",reader_page&&compressed?"gzip":"identity"},{"Referer",referer},{"Origin",wr::origin},
            {"Content-Type","application/json;charset=UTF-8"}}};
        std::size_t header_count=6;
        headers[header_count++]=web?pxa::NetHeaderView{"Cookie",cookie}:pxa::NetHeaderView{"Authorization",authorization};
        if(web&&path==wr::agent::key_path){
            // This GET has no request body. Reuse Content-Type's position for
            // X-Vid and stay within the SDK's eight-header admission budget.
            headers[5]={"X-Vid",session.vid};
            if(auto i=std::ranges::find(session.cookies,"wr_skey",&wr::Session::Cookie::name);i!=session.cookies.end())
                headers[header_count++]={"X-Skey",i->value};
        }
        static constexpr std::array<std::string_view,4> wanted{"set-cookie","location","content-type","content-encoding"};
#ifdef WEREAD_HTTP_TRACE
        const bool trace_reader=path.starts_with("/wrwebsimplenjlogic/reader");
        auto trace_error=[&](unsigned stage,pxa::Error error,std::size_t bytes=0){
            log("WEREAD HTTP FAILURE",{{"reader",trace_reader},{"stage",stage},{"code",int(error)},{"bytes",bytes},{"limit",limit}});
        };
#endif
        const auto transport_limit=
#ifdef WEREAD_HTTP_TRACE
            trace_reader?std::max<std::size_t>(limit,8388608):
#endif
            limit;
        // Large chapter responses used to pay one Guest/Host transfer per
        // KiB. Allocate a full wire-sized buffer only for these requests.
        std::unique_ptr<std::array<std::byte,4064>> bulk;
        if(limit>131072)bulk=std::make_unique<std::array<std::byte,4064>>();
        const auto input=bulk?std::span<std::byte>{*bulk}:std::span<std::byte>{receive};
        auto opened=co_await context->http().open({.url=url,.method=body.empty()?pxa::HttpMethod::get:pxa::HttpMethod::post,
            .headers=std::span(headers).first(header_count),.wanted_headers=wanted,.body=std::as_bytes(std::span{body.data(),body.size()}),
            .options={.buffer_bytes=bulk?4096u:1024u,.header_bytes=4096,.max_body_bytes=transport_limit,.timeout_ms=timeout}},permission);
        if(!opened){
#ifdef WEREAD_HTTP_TRACE
            trace_error(1,opened.error());
#endif
            co_return std::unexpected(opened.error());
        }
        auto response=co_await std::move(*opened).response();
        if(!response){
#ifdef WEREAD_HTTP_TRACE
            trace_error(2,response.error());
#endif
            co_return std::unexpected(response.error());
        }
#ifdef WEREAD_HTTP_TRACE
        log("WEREAD HTTP HEAD",{{"reader",trace_reader},{"status",response->status_code},{"length_known",bool(response->body_length)},
            {"body_bytes",response->body_length.value_or(0)},{"limit",limit}});
#endif
        // Renewal is transactional: rejected/truncated bodies cannot publish
        // their Set-Cookie values. Allocate a copy only on this uncommon path.
        std::optional<wr::Session> renewed;
        if(web&&path==wr::agent::renewal_path)renewed.emplace(session);
        bool received_cookie=false;
        if(web)for(auto h:response->headers.values("set-cookie")){
            (renewed?*renewed:session).receive(h);received_cookie=true;}
        if(renewed)received_cookie=false;
        if(received_cookie&&session.header()!=cookie){auth_dirty=true;if(!session.vid.empty())save();}
        const bool reading_request=path.starts_with("/wrwebsimplenjlogic/reader")||path.starts_with("/wrwebsimplenjlogic/api/catalogloadmore?");
        if((!web||path==wr::agent::key_path||reading_request)&&(response->status_code==401||response->status_code==403))co_return std::unexpected(pxa::Error::denied);
        if(web&&reading_request&&response->status_code>=300&&response->status_code<400){
            auto location=response->headers.first("location").value_or("");
            if(location.starts_with(wr::origin))location.remove_prefix(wr::origin.size());
            if(location.starts_with("/web/login")||location.starts_with("/wrwebsimplenjlogic/login"))
                co_return std::unexpected(pxa::Error::denied);
        }
        if(response->status_code==429)co_return std::unexpected(pxa::Error::busy);
        if(response->status_code<200 || response->status_code>=300)co_return std::unexpected(pxa::Error::protocol_error);
        const auto encoding=response->headers.first("content-encoding").value_or("");
        const bool gzip=reader_page&&compressed&&wr::html::equal(encoding,"gzip");
        if(!encoding.empty()&&!wr::html::equal(encoding,"identity")&&!gzip)co_return std::unexpected(pxa::Error::protocol_error);
        const auto retained_limit=gzip?std::min<std::size_t>(limit,262144):limit;
        wr::ReaderInput out;
        if(response->body_length&&*response->body_length>limit){
#ifdef WEREAD_HTTP_TRACE
            trace_error(5,pxa::Error::limit_exceeded);
#endif
            co_return std::unexpected(pxa::Error::limit_exceeded);
        }
        if(gzip&&response->body_length&&*response->body_length>retained_limit)co_return std::unexpected(pxa::Error::unsupported);
        if(!out.reserve(response->body_length.value_or(1024),retained_limit))co_return std::unexpected(pxa::Error::resource_limit);
        std::size_t wire_bytes=0;
        for(;;){
            auto n=co_await response->body.read_some(input);
            if(!n){
#ifdef WEREAD_HTTP_TRACE
                trace_error(3,n.error(),out.size());
#endif
                co_return std::unexpected(n.error());
            }
            if(!*n)break;
            if(*n>transport_limit-wire_bytes)co_return std::unexpected(pxa::Error::limit_exceeded);
            wire_bytes+=*n;
            if(gzip&&*n>retained_limit-out.size())co_return std::unexpected(pxa::Error::unsupported);
            if(*n>limit-out.size()){
#ifdef WEREAD_HTTP_TRACE
                trace_error(4,pxa::Error::limit_exceeded,out.size());
#endif
                co_return std::unexpected(pxa::Error::limit_exceeded);
            }
            if(!out.append({reinterpret_cast<const char*>(input.data()),*n},retained_limit))
                co_return std::unexpected(pxa::Error::resource_limit);
        }
        if(gzip){
            response={};bulk.reset();
            auto decoded=co_await decode_reader_gzip(std::move(out),limit);
            if(!decoded)co_return std::unexpected(decoded.error());out=std::move(*decoded);
        }
        if(reader_page)log("WEREAD READER RESPONSE",{{"wire_bytes",wire_bytes},{"decoded_bytes",out.size()},{"gzip",gzip}});
        if(renewed&&wr::agent::renewal_confirmed(wr::Json(out))){
            const auto id=std::ranges::find(renewed->cookies,"wr_vid",&wr::Session::Cookie::name);
            if(id!=renewed->cookies.end()&&id->value!=session.vid)co_return std::unexpected(pxa::Error::protocol_error);
            if(renewed->header()!=cookie){session=std::move(*renewed);auth_dirty=true;save();}
        }
        co_return std::move(out);
    }
    pxa::Task<wr::ReaderInput> decode_reader_gzip(wr::ReaderInput encoded,std::size_t limit){
        // Count/validate first, then allocate the exact decoded size once.
        // Incremental realloc of a megabyte response can require two large
        // live blocks and exhaust the 2 MiB heap despite ample decoded budget.
        wr::ReaderInput out;std::size_t expected=0;
        for(unsigned pass=0;pass<2;++pass){
            if(pass&&!out.reserve(expected,limit))co_return std::unexpected(pxa::Error::resource_limit);
            auto decoder=std::make_unique<wr::ReaderGzip>();auto part=encoded.view();
            auto slice_end=std::chrono::steady_clock::now()+std::chrono::milliseconds(16);
            while(!part.empty()||decoder->pending()){
                const auto error=decoder->step(part,pass?&out:nullptr,limit);
                if(error!=wr::ReaderGzip::Error::none)co_return std::unexpected(error==wr::ReaderGzip::Error::limit?pxa::Error::limit_exceeded:
                    error==wr::ReaderGzip::Error::memory?pxa::Error::resource_limit:pxa::Error::protocol_error);
                if(std::chrono::steady_clock::now()>=slice_end){
                    if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
                    slice_end=std::chrono::steady_clock::now()+std::chrono::milliseconds(16);
                }
            }
            if(!decoder->complete()||(pass&&decoder->size()!=expected))co_return std::unexpected(pxa::Error::protocol_error);
            expected=decoder->size();
        }
        co_return std::move(out);
    }
    pxa::Task<wr::ReaderInput> reading_request(std::string path,std::size_t limit){
        for(unsigned attempt=0;attempt<2;++attempt){
            auto raw=co_await request(path,{},limit);
            if(!raw&&raw.error()==pxa::Error::unsupported&&path.starts_with("/wrwebsimplenjlogic/reader")){
                // Poorly compressed/large wire bodies would overlap the full
                // decoded response. Retry this GET once without compression.
                log("WEREAD READER IDENTITY FALLBACK");
                raw=co_await request(path,{},limit,20000,wr::agent::Target::web,{},false);
            }
            const bool expired=raw?wr::login_expired(wr::Json(*raw)):raw.error()==pxa::Error::denied;
            if(!expired)co_return std::move(raw);
            if(attempt)break;
            notice("正在续期登录…");
            auto renewal=co_await request(std::string(wr::agent::renewal_path),"{\"rq\":\"%2Fweb%2Fbook%2Fread\",\"ql\":false}",4096,10000);
            if(!renewal||!wr::agent::renewal_confirmed(wr::Json(*renewal)))break;
            log("WEREAD READING SESSION RENEWED");
        }
        co_return std::unexpected(pxa::Error::denied);
    }
    void reading_failed(pxa::Error error){
        busy=false;
        std::string_view message=error==pxa::Error::denied?"登录或阅读授权已失效，请刷新书架或重新登录":
            error==pxa::Error::busy?"请求过于频繁，请稍后重试":
            error==pxa::Error::timed_out?"连接超时，已保留原页，可重新选择章节":
            error==pxa::Error::limit_exceeded?"章节响应超出处理限额，已保留原页":
            error==pxa::Error::resource_limit?"暂时资源不足，已保留原页，请稍后重试":
            "章节加载未完成，已保留原页，可重试或重新打开图书";
        notice(std::string(message));(void)context->window().toast(message);
        log("WEREAD READING FAILED",{{"code",int(error)}});
    }
    pxa::Task<void> write_file(std::string path,std::string text,bool atomic=true) {
        const auto temp=atomic?path+".tmp":path;
        auto f=co_await context->fs().open(temp,pxa::OpenMode::write|pxa::OpenMode::create|pxa::OpenMode::truncate);
        if(!f)co_return std::unexpected(f.error());
        std::span bytes=std::as_bytes(std::span{text.data(),text.size()});
        while(!bytes.empty()){
            auto n=f->write(bytes.first(std::min<std::size_t>(4064,bytes.size())));
            if(!n&&n.error()==pxa::Error::would_block){
                if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
                continue;
            }
            if(!n || !*n)co_return std::unexpected(n?pxa::Error::io_error:n.error());
            bytes=bytes.subspan(*n);
            if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
        }
        if(auto c=f->close();!c)co_return std::unexpected(c.error());
        if(atomic)co_return co_await context->fs().replace(temp,path);
        co_return pxa::Result<void>{};
    }
    pxa::Task<std::string> read_file(std::string path,std::size_t maximum){
        auto file=co_await context->fs().open(path);
        if(!file)co_return std::unexpected(file.error());
        std::string text;auto buffer=std::make_unique<std::array<std::byte,4064>>();
        for(;;){
            auto n=file->read(*buffer);
            if(!n&&n.error()==pxa::Error::would_block){
                if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
                continue;
            }
            if(!n)co_return std::unexpected(n.error());if(!*n)break;
            if(*n>maximum-text.size())co_return std::unexpected(pxa::Error::limit_exceeded);
            text.append(reinterpret_cast<const char*>(buffer->data()),*n);
            if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
        }
        if(auto result=file->close();!result)co_return std::unexpected(result.error());
        co_return text;
    }
    pxa::Task<void> load_bookmarks(){
        busy=true;notice("正在读取本地书签…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        auto next=std::make_unique<BookmarkPanel>();
        next->data.account=session.vid;next->data.book=reader.book_id;
        if(!next->data.valid_identity()){busy=false;notice("当前图书暂不能保存书签");co_return pxa::Result<void>{};}
        const auto path=next->data.path();
        // A cancelled atomic write may leave a draft. Never treat it as saved.
        (void)co_await context->fs().remove(path+".tmp");
        auto raw=co_await read_file(path,wr::LocalBookmarks::max_bytes);
        busy=false;
        if((!raw&&raw.error()!=pxa::Error::not_found)||(raw&&!next->data.decode(*raw))){
            notice("书签数据暂不可用，未更改原文件");log("WEREAD BOOKMARKS UNAVAILABLE");co_return pxa::Result<void>{};
        }
        bookmark_panel=std::move(next);notice("本地书签，不参与云端同步");
        log("WEREAD BOOKMARKS READY",{{"count",bookmark_panel->data.items.size()}});
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> edit_bookmark(std::optional<unsigned> remove={}){
        if(!bookmark_panel||bookmark_panel->data.account!=session.vid||bookmark_panel->data.book!=reader.book_id)
            co_return std::unexpected(pxa::Error::invalid_argument);
        // Edit on demand, publish the new model only after atomic file replace.
        auto next=std::make_unique<BookmarkPanel>(*bookmark_panel);
        if(remove){
            if(*remove>=next->data.items.size())co_return std::unexpected(pxa::Error::invalid_argument);
            next->data.items.erase(next->data.items.begin()+*remove);
        }else{
            wr::LocalBookmark mark{reader.current,wr::bookmark_label(reader.chapter_title,std::string_view(reader.text).substr(reading_offset)),
                reader.chapter_uid,reader.section,std::uint32_t(reading_offset),wr::bookmark_text_hash(reader.text)};
            const auto index=next->data.add(std::move(mark));
            if(!index){notice("书签已满或当前位置暂不能保存");co_return pxa::Result<void>{};}
            next->first=*index/rows()*rows();
        }
        auto text=next->data.encode();
        if(!text){notice("书签数据较多，请先删除不需要的书签");co_return pxa::Result<void>{};}
        busy=true;notice(remove?"正在删除书签…":"正在保存书签…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        auto written=co_await write_file(next->data.path(),std::move(*text));
        busy=false;
        if(!written){fail("保存书签",written.error());co_return pxa::Result<void>{};}
        next->pending_remove.reset();
        if(next->first>=next->data.items.size())next->first=next->data.items.empty()?0:(next->data.items.size()-1)/rows()*rows();
        bookmark_panel=std::move(next);notice(remove?"书签已删除":"已保存本地书签");
        log("WEREAD BOOKMARKS SAVED",{{"count",bookmark_panel->data.items.size()}});
        co_return pxa::Result<void>{};
    }
    void remember_position(){
        if(reader.book_id.empty()||reader.current.empty())return;
        saved.remember({reader.book_id,reader.current,reader.chapter_uid,reader.section,std::uint32_t(reading_offset),1});
    }
    void save(bool immediately=false){
        if(initializing)return;
        saved.preferences.font=font_dp;saved.preferences.night=night;saved.preferences.remember=session.remember;
        save_pending=true;save_at=immediately?0:now+2000000;
        if(immediately&&!save_running)start(persist(),false);
    }
    pxa::Task<void> persist(){
        if(save_running)co_return pxa::Result<void>{};
        save_running=true;
        struct Guard {WeReadApp& app;~Guard(){app.save_running=false;}} guard{*this};
        if(login_after_save||close_after_save)
            if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        bool success=true;
        while(save_pending){
            save_pending=false;
            // Write authentication only when it changes, never on page turns.
            // A single writer serializes authentication and preferences. A
            // logout cannot race an older atomic session publication.
            if(std::exchange(auth_dirty,false)){
                if(!session.remember||session.vid.empty()){
                    (void)co_await context->fs().remove("shelf-v1.json");
                    (void)co_await context->fs().remove("shelf-v1.json.tmp");
                    auto removed=co_await context->fs().remove("session.json");
                    if(!removed&&removed.error()!=pxa::Error::not_found){auth_dirty=true;success=false;break;}
                    auto partial=co_await context->fs().remove("session.json.tmp");
                    if(!partial&&partial.error()!=pxa::Error::not_found){auth_dirty=true;success=false;break;}
                }else if(auto auth=co_await write_file("session.json",session.encode());!auth){auth_dirty=true;success=false;break;}
            }
            auto encoded=saved.encode();
            if(encoded.size()>wr::SavedState::max_bytes){success=false;break;}
            if(auto state=co_await write_file("state.json",std::move(encoded));!state){success=false;break;}
        }
        if(!success){close_after_save=false;login_after_save=false;notice("保存未完成，请检查可用存储空间");
            (void)context->window().toast(status);co_return pxa::Result<void>{};}
        if(login_after_save){login_after_save=false;start(begin_login());}
        if(close_after_save){close_after_save=false;(void)context->window().request_back();}
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> restore() {
        (void)co_await context->fs().remove("login-qr.png");
        if(auto state=co_await read_file("state.json",wr::SavedState::max_bytes))saved.decode(*state);
        font_dp=saved.preferences.font;night=saved.preferences.night;session.remember=saved.preferences.remember;
        if(session.remember){if(auto auth=co_await read_file("session.json",8192))
            if(session.decode(*auth))saved.bind_account(session.vid);}
        else (void)co_await context->fs().remove("session.json");
        initializing=false;
        if(!session.cookies.empty()){
            screen=Screen::shelf;bool cached=false;
            if(auto raw=co_await read_file("shelf-v1.json",wr::ShelfSnapshot::max_bytes))
                if(auto candidate=wr::ShelfSnapshot::decode(*raw,session.vid)){
                    for(auto& book:candidate->books)book.cover=wr::cover_image_url(book.cover);
                    shelf=std::move(*candidate);cached=shelf_local=true;notice("本地书架 · 正在同步…");
                    log("WEREAD SHELF CACHE READY",{{"count",shelf.books.size()}});
                }
            start(load_shelf(0,-1,false,cached));
        }
        else start(begin_login());
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> decode_qr(std::string uri) {
        auto payload=wr::inline_image::payload(uri);
        if(!payload)co_return std::unexpected(pxa::Error::protocol_error);
        // Maximum QR payload is 3 KiB. No Guest pixel buffer is ever allocated.
        std::string bytes(3072,'\0');
        auto count=wr::inline_image::decode(*payload,std::as_writable_bytes(std::span{bytes.data(),bytes.size()}));
        if(count!=payload->size()/4*3-(payload->ends_with("==")?2:payload->ends_with('=')?1:0))co_return std::unexpected(pxa::Error::protocol_error);
        bytes.resize(count);
        if(auto written=co_await write_file("login-qr.png",std::move(bytes),false);!written)co_return std::unexpected(written.error());
        auto f=co_await context->fs().open("login-qr.png");
        if(!f)co_return std::unexpected(f.error());
        auto image=co_await context->assets().decode_image(std::move(*f),{.max_width=256,.max_height=256,.max_pixels=65536,.max_encoded_bytes=3072,.formats=pxa::ImageFormats::png});
        (void)co_await context->fs().remove("login-qr.png");
        if(!image)co_return std::unexpected(image.error());
        qr=std::move(*image);dirty=true;co_return pxa::Result<void>{};
    }
    pxa::Task<void> begin_login() {
        scene_valid=false;
        busy=true;qr.reset();login.clear();retry_at=0;screen=Screen::login;notice("正在获取登录二维码…");
        login.phase=wr::LoginPhase::uid;login.key=wr::number(now%1000);
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        auto uid=co_await request("/wrwebsimplenjlogic/api/getuid?platform=desktop");
        if(!uid){login.phase=wr::LoginPhase::failed;fail("获取二维码",uid.error());co_return pxa::Result<void>{};}
        login.uid=wr::scalar(wr::Json(*uid).get("uid"),128);
        if(login.uid.empty())co_return std::unexpected(pxa::Error::protocol_error);
        login.phase=wr::LoginPhase::qr;
        const auto target="https://weread.qq.com/web/confirm?pf=2&uid="+wr::percent(login.uid);
        auto response=co_await request("/wrwebsimplenjlogic/api/qrcode?platform=desktop&url="+wr::percent(target));
        if(!response)co_return std::unexpected(response.error());
        auto data=wr::Json(*response).get("data").string(4096);
        if(!data || wr::Json(*response).get("succ").number()!=1)co_return std::unexpected(pxa::Error::protocol_error);
        if(auto decoded=co_await decode_qr(std::move(*data));!decoded)co_return std::unexpected(decoded.error());
        // Local fixtures can finish before the first frame tick. Anchor the
        // expiry to the Host clock rather than an uninitialized frame time.
        if(auto time=co_await context->clock().now())now=*time;
        login.phase=wr::LoginPhase::polling;login.deadline=now+180000000;
        notice("请用微信扫码，并在手机上确认");log("WEREAD QR READY");
        start(poll_login());co_return pxa::Result<void>{};
    }
    pxa::Task<void> poll_login() {
        log("WEREAD POLL START");
        while(now<login.deadline){
            auto raw=co_await request("/wrwebsimplenjlogic/api/getlogininfo?platform=desktop",
                "{\"uid\":"+wr::quote(login.uid)+",\"cgiKey\":"+wr::quote(login.key)+"}",8192,60000);
            if(!raw){notice("连接暂时中断，正在重新等待确认…");
                retry_at=now+1500000;co_return pxa::Result<void>{};}
            wr::Json j(*raw);if(!j.valid())co_return std::unexpected(pxa::Error::protocol_error);
            log("WEREAD POLL RESPONSE");
            auto body=login.confirmation(j);
            if(!body){
                // A completed long poll without confirmation is still pending.
                // Never submit a login, discard a valid QR, or spin on an
                // ambiguous response. Explicit expiry and our deadline win.
                if(wr::login_expired(j)){login.phase=wr::LoginPhase::expired;busy=false;qr.reset();notice("二维码已过期，请重新获取");}
                else{notice("请用微信扫码，并在手机上确认");retry_at=now+1500000;}
                co_return pxa::Result<void>{};
            }
            login.phase=wr::LoginPhase::commit;notice("扫码已确认，正在登录…");
            if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
            // A login POST is never replayed after an ambiguous network result.
            auto committed=co_await request("/wrwebsimplenjlogic/api/weblogin?platform=desktop",std::move(*body));
            log("WEREAD COMMIT RESPONSE");
            if(!committed)co_return std::unexpected(committed.error());
            wr::Json result(*committed);
            if(!result.valid() || wr::scalar(result.get("accessToken")).empty() || wr::scalar(result.get("refreshToken")).empty())
                co_return std::unexpected(pxa::Error::protocol_error);
            session.vid=wr::scalar(result.get("vid"),64);
            if(session.vid.empty() || !wr::clean(session.vid) || session.cookies.empty())co_return std::unexpected(pxa::Error::protocol_error);
            saved.bind_account(session.vid);reauth_required=false;
            login.clear();login.phase=wr::LoginPhase::complete;qr.reset();screen=Screen::shelf;list_start=server_start=0;
            saved.preferences.remember=session.remember;auth_dirty=true;save_pending=true;
            (void)co_await persist();
            log("WEREAD LOGIN COMPLETE");start(load_shelf());co_return pxa::Result<void>{};
        }
        busy=false;qr.reset();login.phase=wr::LoginPhase::expired;notice("二维码已过期，请重新获取");
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> load_shelf(std::size_t offset=0,int key=-1,bool backward=false,bool cached=false) {
        shelf_syncing=true;
        struct Guard {WeReadApp& app;~Guard(){app.shelf_syncing=false;}} guard{*this};
        busy=!cached;notice(cached?"本地书架 · 正在同步…":"正在同步书架…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        pxa::Result<wr::ReaderInput> raw=std::unexpected(pxa::Error::protocol_error);
        // One retry site keeps the request lifecycle identical. Only this
        // idempotent shelf GET may be replayed after confirmed renewal.
        for(unsigned attempt=0;attempt<2;++attempt){
            raw=co_await request("/wrwebsimplenjlogic/api/shelfloadmore?idx="+wr::number(offset)+"&kkIdx="+wr::number(key)+"&platform=desktop",{},131072);
            if(!raw||!wr::login_expired(wr::Json(*raw))||attempt)break;
            notice("正在续期登录…");
            auto renewal=co_await request(std::string(wr::agent::renewal_path),"{\"rq\":\"%2Fweb%2Fbook%2Fread\",\"ql\":false}",4096,10000);
            if(!renewal||!wr::agent::renewal_confirmed(wr::Json(*renewal)))break;
            log("WEREAD SESSION RENEWED");
        }
        if(!raw){
            if(cached){notice("本地书架 · 同步未完成，可稍后刷新");log("WEREAD SHELF CACHE RETAINED");}
            else fail("加载书架",raw.error());
            co_return pxa::Result<void>{};
        }
        if(wr::login_expired(wr::Json(*raw))){
            busy=false;reauth_required=true;notice("登录已过期，请点击重新登录");
            log("WEREAD LOGIN EXPIRED");co_return pxa::Result<void>{};
        }
        wr::Shelf candidate;bool parsed=candidate.parse(*raw);
        if(!parsed && offset==0){
            // Some releases server-render the initial shelf. Never publish a
            // partial parse or advance cursors until a whole response validates.
            auto html=co_await request("/wrwebsimplenjlogic/shelf",{},262144);
            if(html){wr::Hydration hydration;if(auto data=hydration.decode(*html))
                parsed=candidate.parse(wr::Json(*data).get("state").get("shelf").raw());}
        }
        busy=false;
        if(!parsed || (offset&&candidate.books.empty())){
            if(wr::Json(*raw).get("sessionTimeout").number()==1)notice("登录已过期，请在设置中重新登录");
            else notice("书架响应暂不可用，请重试");
            (void)context->window().toast(status);
            log("WEREAD SHELF UNAVAILABLE");co_return pxa::Result<void>{};
        }
        if(offset==0&&!backward)shelf_history.clear();
        else if(backward){if(!shelf_history.empty())shelf_history.pop_back();}
        else if(offset!=server_start){
            if(shelf_history.size()==100)shelf_history.erase(shelf_history.begin());
            shelf_history.push_back({server_start,server_key});
        }
        for(auto& book:candidate.books)book.cover=wr::cover_image_url(book.cover);
        const bool stable=server_start==offset&&wr::ShelfSnapshot::same_books(shelf,candidate);
        if(!stable){page_images.reset();covers_active=false;}
        shelf=std::move(candidate);server_start=offset;server_key=key;shelf_local=false;
        if(!cached||!stable)list_start=backward&&!shelf.books.empty()?(shelf.books.size()-1)/rows()*rows():0;
        screen=Screen::shelf;notice(shelf.books.empty()?"书架还没有图书":"共 "+wr::number(shelf.total)+" 本图书");
        log("WEREAD SHELF READY",{{"count",shelf.books.size()}});
        if(!offset&&session.remember)
            if(auto snapshot=wr::ShelfSnapshot::encode(shelf,session.vid)){
                if(auto result=co_await write_file("shelf-v1.json",std::move(*snapshot));result)log("WEREAD SHELF CACHE SAVED");
            }
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> load_search(std::uint32_t cursor,bool previous=false){
        busy=true;notice("正在搜索…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        auto raw=co_await request("/api/store/search?keyword="+wr::percent(search_keyword)+"&count=20&scope=17&maxIdx="+
            wr::number(cursor)+(cursor?"&sid="+wr::percent(search.sid):""),{},131072);
        busy=false;
        if(!raw){fail("搜索",raw.error());co_return pxa::Result<void>{};}
        wr::Search candidate;
        if(!candidate.parse(*raw)||candidate.cursor<cursor){notice("搜索响应暂不可用，请重试");
            (void)context->window().toast(status);co_return pxa::Result<void>{};}
        if(!cursor&&!previous)search_history.clear();
        else if(previous){if(!search_history.empty())search_history.pop_back();}
        else {if(search_history.size()==100)search_history.erase(search_history.begin());search_history.push_back(search_cursor);}
        for(auto& book:candidate.books)book.cover=wr::cover_image_url(book.cover);
        page_images.reset();covers_active=false;search=std::move(candidate);search_cursor=cursor;
        search_start=previous&&!search.books.empty()?(search.books.size()-1)/search_rows()*search_rows():0;
        notice(search.books.empty()?"没有找到相关图书":"共 "+wr::number(search.total)+" 项结果");
        log("WEREAD SEARCH READY",{{"count",search.books.size()}});co_return pxa::Result<void>{};
    }
    pxa::Task<void> open_search(std::string book){
        busy=true;notice("正在读取图书目录…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        auto raw=co_await reading_request("/wrwebsimplenjlogic/api/catalogloadmore?type=3&bookId="+wr::percent(book)+"&rangeStart=0&rangeEnd=0",131072);
        if(!raw){fail("打开图书",raw.error());co_return pxa::Result<void>{};}
        wr::Json data(*raw);std::string param;
        if(data.valid()&&data.get("succ").number()==1)data.get("readerUrlParmas").each([&](wr::Json item){param=wr::scalar(item.get("param"),1024);return param.empty();},300);
        if(param.empty()){busy=false;notice("本书暂时没有可用阅读入口");(void)context->window().toast(status);co_return pxa::Result<void>{};}
        // Ask the official service for an authorized chapter URL. No local
        // reproduction of opaque reader URL signing/encryption is necessary.
        start(load_reader("?bc="+wr::percent(param)));co_return pxa::Result<void>{};
    }
    pxa::Task<std::string> project_reader(const wr::ReaderInput& html,std::optional<std::uint32_t> window={},std::optional<wr::Hydration::CatalogHint> hint={},std::size_t catalog_rows=300){
        std::optional<std::string> state;unsigned parse_stage=0;bool capacity_limited=false;
        log("WEREAD READER PARSE",{{"bytes",html.size()}});
        {
            wr::Hydration hydration;
            // A small parser step is a cancellation boundary, not a mandatory
            // Host round trip. Fill a short CPU slice before yielding so large
            // directories do not pay a scheduler turn per inexpensive row.
            // Reader and catalog loading both suspend image work. Spend one
            // frame per CPU slice; cancellation is still checked between them.
            const auto slice=std::chrono::milliseconds(16);
            auto slice_end=std::chrono::steady_clock::now()+slice;
            bool prepared=false;
            {
                auto preparation=hydration.preparation(html.view(),html.capacity());
                while(!preparation.done()){
                    preparation.step();
                    if(!preparation.done()&&std::chrono::steady_clock::now()>=slice_end){
                        if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
                        slice_end=std::chrono::steady_clock::now()+slice;
                    }
                }
                prepared=preparation.result().has_value();
            }
            if(prepared)log("WEREAD READER PREPARED");
            if(prepared){
                if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
                slice_end=std::chrono::steady_clock::now()+slice;
                auto projection=hydration.reader_projection(std::max(html.size(),html.capacity()),window,hint,catalog_rows);
                while(!projection.done()){
                    projection.step(16);
                    if(!projection.done()&&std::chrono::steady_clock::now()>=slice_end){
                        if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
                        slice_end=std::chrono::steady_clock::now()+slice;
                    }
                }
                state=std::move(projection).result();
            }
            parse_stage=hydration.stage;capacity_limited=hydration.capacity_limited;
        }
        // Release the borrowed parser/index state before constructing the new
        // reading model. Cancellation destroys the whole job with its HTML.
        if(!state){busy=false;notice(capacity_limited?"本节数据较大，已保留原页，请选择其他章节":"正文数据格式暂不支持，请返回书架重试");(void)context->window().toast(status);log("WEREAD READER HYDRATION ERROR",{{"stage",parse_stage}});co_return std::unexpected(pxa::Error::protocol_error);}
        co_return std::move(*state);
    }
    pxa::Task<void> load_reader(std::string query,bool last=false,std::optional<wr::Resume> resume={},std::optional<std::uint32_t> bookmark_hash={},std::optional<std::uint32_t> expected_chapter={}) {
        const auto began=std::chrono::steady_clock::now();
        page_images.suspend();
        struct ImagesGuard {wr::PageImages& images;~ImagesGuard(){images.resume();}} images_guard{page_images};
        remember_position();
        busy=true;notice(expected_chapter?"正在跳转章节…":"正在读取章节…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        auto html=co_await reading_request("/wrwebsimplenjlogic/reader"+query,wr::Hydration::max_input_bytes);
        if(!html){reading_failed(html.error());co_return pxa::Result<void>{};}
        const auto downloaded=std::chrono::steady_clock::now();
        notice("正在解析章节…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        std::optional<wr::Hydration::CatalogHint> hint;
        if(expected_chapter&&!reader.catalog.items.empty())hint=wr::Hydration::CatalogHint{reader.book_id,*expected_chapter,reader.total_chapters,reader.catalog.last_chapter};
        // Opening a book needs the nearby directory, not 300 expanded rows.
        // Keep full-size subsequent windows and the existing jump reuse path.
        auto state=co_await project_reader(*html,{},hint,!hint&&reader.catalog.items.empty()?64:300);
        if(!state)co_return pxa::Result<void>{};
        log("WEREAD READER PROJECTED",{{"bytes",state->size()}});
        wr::Reading candidate;
        const bool reuse=hint&&wr::Json(*state).get("catalogReused").raw()=="true";
        if(!candidate.parse(wr::Json(*state),!reuse)){reading_failed(pxa::Error::protocol_error);co_return pxa::Result<void>{};}
        if(expected_chapter&&!candidate.unavailable&&!candidate.requires_purchase&&
           (candidate.book_id!=reader.book_id||candidate.chapter_uid!=*expected_chapter||
            !wr::Json(*state).get("curChapter").get("chapterUid").number()||candidate.current.empty()||candidate.section!=0)){
            busy=false;notice("章节入口已变化，已保留原页，请重新打开图书");
            (void)context->window().toast(status);log("WEREAD CHAPTER TARGET MISMATCH");co_return pxa::Result<void>{};
        }
        if(bookmark_hash&&(!resume||resume->book!=candidate.book_id||resume->chapter!=candidate.chapter_uid||resume->section!=candidate.section)){
            busy=false;notice("书签章节暂不可用，已保留原页");log("WEREAD BOOKMARK CHAPTER UNAVAILABLE");(void)context->window().toast(status);co_return pxa::Result<void>{};
        }
        log("WEREAD READER MODEL READY");
        std::string_view body=candidate.text;
        if(body.empty())if(auto rendered=wr::html::element(html->view(),"readerContentRenderContainer")){
            body=*rendered;
        }
        // The projected DTO is independent. Normalize directly from the
        // response view, avoiding a second copy of server-rendered body HTML.
        std::string{}.swap(*state);
        if(candidate.unavailable || candidate.requires_purchase){
            if(candidate.requires_purchase&&candidate.book_id==reader.book_id)
                for(auto& chapter:reader.catalog.items)if(chapter.uid==candidate.chapter_uid)chapter.locked=true;
            busy=false;notice(std::string(candidate.denied_message()));
            return_screen=screen;screen=Screen::unavailable;
            log(candidate.requires_purchase&&!candidate.content_error?"WEREAD READER AUTHORIZATION REQUIRED":"WEREAD READER CONTENT UNAVAILABLE");
            co_return pxa::Result<void>{};
        }
        if(character_map.empty()){
            character_map.resize(41984);
            for(std::size_t at=0;at<character_map.size();){
                // Use the SDK's full 4064-byte asset reply. Read directly into
                // the final mapping; no extra transfer buffer or prefetch cache.
                auto part=co_await context->assets().read("assets/weread-cjk.bin",at,std::span{character_map}.subspan(at,std::min<std::size_t>(4064,character_map.size()-at)));
                if(!part || !part->bytes || part->total_bytes!=character_map.size()){
                    character_map.clear();busy=false;notice("阅读字形数据缺失，请重新安装应用");co_return pxa::Result<void>{};}
                at+=part->bytes;
            }
        }
        bool image_limit=false,text_limit=false;
        candidate.text=wr::reader_text(body,character_map,&candidate.images,&image_limit,&text_limit);
        *html={};
        log("WEREAD READER TEXT READY");
        if(image_limit){busy=false;notice("本节图片数据较多，已保留原页");(void)context->window().toast(status);co_return pxa::Result<void>{};}
        if(text_limit){reading_failed(pxa::Error::limit_exceeded);co_return pxa::Result<void>{};}
        if(candidate.text.empty()){busy=false;notice("本节没有可显示正文");(void)context->window().toast(status);co_return pxa::Result<void>{};}
        if(reuse){candidate.catalog=std::move(reader.catalog);log("WEREAD CATALOG REUSED");}
        for(auto& chapter:candidate.catalog.items)if(chapter.uid==candidate.chapter_uid)chapter.locked=false;
        page_images.reset();reader=std::move(candidate);reader_account=session.vid;reader_loaded=std::chrono::steady_clock::now();reading_offset=0;page_history.clear();page=0;
        bool bookmark_changed=bookmark_hash&&(*bookmark_hash!=wr::bookmark_text_hash(reader.text)||resume->offset>=reader.text.size());
        if(resume&&resume->book==reader.book_id&&resume->chapter==reader.chapter_uid&&resume->section==reader.section&&resume->offset<reader.text.size()){
            reading_offset=bookmark_changed?0:resume->text_version?resume->offset:wr::migrate_text_offset(resume->offset,reader.images);
            reading_offset=std::min(reading_offset,reader.text.size());
            while(reading_offset && (static_cast<unsigned char>(reader.text[reading_offset])&0xc0)==0x80)--reading_offset;
        }
        screen=Screen::reading;return_screen=Screen::reading;busy=false;bookmark_panel.reset();
        if(last){
            for(std::size_t count=0;;++count){reflow();if(reading_page.end>=reader.text.size() || reading_page.end<=reading_offset)break;
                if(page_history.size()==512)page_history.erase(page_history.begin());page_history.push_back(reading_offset);
                reading_offset=reading_page.end;++page;
                if(count%8==7)if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
            }
        }
        notice("轻点右侧翻页 · 中间打开菜单");
        if(bookmark_changed){log("WEREAD BOOKMARK TEXT CHANGED");(void)context->window().toast("正文已更新，已定位到本节开头");}
        remember_position();save();
        log("WEREAD READER READY",{{"bytes",reader.text.size()},{"chapters",reader.catalog.items.size()}});
        const auto ready=std::chrono::steady_clock::now();
        log("WEREAD READER TIMING",{{"download_ms",std::chrono::duration_cast<std::chrono::milliseconds>(downloaded-began).count()},
            {"prepare_ms",std::chrono::duration_cast<std::chrono::milliseconds>(ready-downloaded).count()},{"reused",reuse}});
        co_return pxa::Result<void>{};
    }
    bool open_loaded_book(std::string_view book){
        const auto* position=saved.find(book);
        const bool hit=!reauth_required&&!session.vid.empty()&&reader_account==session.vid&&reader.book_id==book&&!reader.text.empty()&&
            std::chrono::steady_clock::now()-reader_loaded<=std::chrono::minutes(10)&&position&&
            position->param==reader.current&&position->chapter==reader.chapter_uid&&position->section==reader.section;
        if(!hit){
            // A shelf/search open can release the old model. Keeping it while
            // allocating a large new response fragments the 2 MiB Guest heap.
            // Chapter jumps still retain their old page until a successful load.
            remember_position();reader={};reader_account.clear();reader_loaded={};reading_offset=0;page_history.clear();
            return false;
        }
        page_images.reset();covers_active=false;bookmark_panel.reset();
        screen=return_screen=Screen::reading;busy=false;notice("轻点右侧翻页 · 中间打开菜单");
        log("WEREAD READER MEMORY HIT");return true;
    }
    pxa::Task<void> load_catalog(int direction){
        if(busy)co_return pxa::Result<void>{};busy=true;notice("正在加载目录…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        page_images.suspend();
        struct ImagesGuard {wr::PageImages& images;~ImagesGuard(){images.resume();}} images_guard{page_images};
        auto offset=reader.catalog.local_window(direction);
        const auto metadata_path=[&]{return "/wrwebsimplenjlogic/api/catalogloadmore?type="+wr::number(direction>0?2:1)+
            "&bookId="+wr::percent(reader.book_id)+"&rangeStart="+wr::number(reader.catalog.wire_begin)+"&rangeEnd="+wr::number(reader.catalog.wire_end);};
        auto path=offset?"/wrwebsimplenjlogic/reader?bc="+wr::percent(reader.current)+"&sect="+wr::number(reader.section):metadata_path();
        auto raw=co_await reading_request(std::move(path),offset?wr::Hydration::max_input_bytes:131072);
        if(!raw&&raw.error()==pxa::Error::resource_limit&&offset&&reader.catalog.wire_known){
            // The full reader can exceed the remaining contiguous heap while
            // retaining the current chapter. The official catalog endpoint
            // uses chapter-index cursors and needs a much smaller response.
            log("WEREAD CATALOG METADATA FALLBACK");offset.reset();
            raw=co_await reading_request(metadata_path(),131072);
        }
        if(!raw){reading_failed(raw.error());co_return pxa::Result<void>{};}
        if(offset){
            // A short initial window can precede a larger one. Stop a backward
            // window at the current beginning, so Previous cannot jump ahead.
            auto state=co_await project_reader(*raw,*offset,{},direction<0?reader.catalog.begin-*offset:300);
            if(!state)co_return pxa::Result<void>{};
            wr::Catalog candidate;
            if(!candidate.initial(wr::Json(*state))||candidate.items.empty()){
                busy=false;notice("目录已更新，请重新打开目录");co_return pxa::Result<void>{};
            }
            reader.catalog=std::move(candidate);
        }else if(!reader.catalog.page(wr::Json(*raw),direction)){
            busy=false;notice("目录响应暂不可用，请重试");(void)context->window().toast(status);co_return pxa::Result<void>{};
        }
        busy=false;
        contents_start=direction>0?0:(reader.catalog.items.size()-1)/rows()*rows();dirty=true;
        log("WEREAD CATALOG READY",{{"count",reader.catalog.items.size()},{"offset",reader.catalog.begin},{"chapter_end",reader.catalog.wire_end}});co_return pxa::Result<void>{};
    }
    PixelRect reading_box() const {
        auto r=safe();int gap=dp(8),head=std::min(dp(32),r.height/6),foot=std::min(dp(24),r.height/7);
        return {r.x+gap,r.y+head,r.width-2*gap,r.height-head-foot};
    }
    void reflow(){auto box=reading_box();const int pixels=std::clamp(dp(font_dp),8,128),line=pixels*int(saved.preferences.leading)/100;
        reading_page=wr::paginate_document(reader.text,reader.images,reading_offset,box.width,box.height,pixels,std::max(pixels,line),saved.preferences.images);}
    void turn(int direction){
        if(busy||screen!=Screen::reading)return;reflow();
        if(direction>0 && reading_page.end<reader.text.size() && reading_page.end>reading_offset){
            if(page_history.size()==512)page_history.erase(page_history.begin());
            page_images.clear_page();page_history.push_back(reading_offset);reading_offset=reading_page.end;++page;dirty=true;
            remember_position();save();
        }else if(direction<0 && !page_history.empty()){
            page_images.clear_page();reading_offset=page_history.back();page_history.pop_back();if(page)--page;dirty=true;
            remember_position();save();
        }else if(direction<0&&reading_offset){start(find_previous_page());}
        else if(auto query=reader.adjacent(direction);!query.empty())start(load_reader(std::move(query),direction<0));
        else notice(direction>0?"已到末尾":"已到开头");
    }
    pxa::Task<void> find_previous_page(){
        busy=true;notice("正在定位上一页…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        const auto target=reading_offset;auto box=reading_box();
        const auto pixels=std::clamp(dp(font_dp),8,128);
        std::size_t at=0;page_history.clear();
        for(std::size_t i=0;at<target;++i){
            auto p=wr::paginate_document(reader.text,reader.images,at,box.width,box.height,pixels,pixels*int(saved.preferences.leading)/100,saved.preferences.images);
            if(p.end>=target||p.end<=at)break;
            if(page_history.size()==512)page_history.erase(page_history.begin());
            page_history.push_back(at);at=p.end;
            if(i%8==7)if(auto y=co_await context->clock().yield();!y)co_return std::unexpected(y.error());
        }
        page_images.reset();reading_offset=at;busy=false;remember_position();save();dirty=true;
        co_return pxa::Result<void>{};
    }
    void leave_details(){if(details){screen=details->parent;details.reset();dirty=true;}}
    void details_failed(std::string_view message,bool key_setup=false){
        busy=false;notice(std::string(message));
        if(details){
            if(details->key_setup!=key_setup)details->offset=0;
            details->key_setup=key_setup;
            if(!key_setup&&details->body.empty()){details->body=details->book.title;details->body+="\n\n";details->body+=message;}
        }
        (void)context->window().toast(message);
        if(key_setup)log("WEREAD SKILL SETUP REQUIRED");
        log(details&&details->notes?"WEREAD NOTES UNAVAILABLE":"WEREAD DETAILS UNAVAILABLE");
    }
    pxa::Task<void> load_details(std::string book,std::int64_t cursor=0,int direction=0){
        const auto kind=details&&details->notes?std::optional(details->notes->kind):std::nullopt;
        busy=true;notice(kind?"正在获取个人笔记…":"正在获取图书信息…");
        if(auto shown=co_await show_wait();!shown)co_return std::unexpected(shown.error());
        std::optional<std::string> key;
        {
            auto raw=co_await request(std::string(wr::agent::key_path),{},4096);
            if(!raw){const bool denied=raw.error()==pxa::Error::denied;
                details_failed(denied?wr::agent::key_setup_notice:"连接未完成，请重试",denied);co_return pxa::Result<void>{};}
            key=wr::agent::existing_key(wr::Json(*raw));
            if(!key){const auto error=wr::agent::status(wr::Json(*raw));
                const bool setup=error==wr::agent::Status::ok||error==wr::agent::Status::authorization;
                details_failed(setup?wr::agent::key_setup_notice:wr::agent::message(error),setup);co_return pxa::Result<void>{};}
        }
        if(details&&details->key_setup){details->key_setup=false;details->offset=0;dirty=true;}
        if(kind){
            auto raw=co_await request(std::string(wr::agent::gateway_path),wr::agent::payload(
                *kind==wr::agent::NoteKind::highlights?wr::agent::Api::highlights:wr::agent::Api::thoughts,book,cursor),
                wr::agent::note_response_limit,20000,wr::agent::Target::gateway,*key);
            if(!raw){details_failed(raw.error()==pxa::Error::denied?wr::agent::message(wr::agent::Status::authorization):"笔记连接未完成，请刷新重试");co_return pxa::Result<void>{};}
            auto document=wr::agent::note_page(*raw,book,*kind,cursor);
            if(!document){
                const auto error=wr::agent::status(wr::Json(*raw));
                details_failed(error==wr::agent::Status::ok?"笔记数据暂不可用，请刷新重试":wr::agent::message(error));
                co_return pxa::Result<void>{};
            }
            if(details&&screen==Screen::details&&details->book.id==book&&details->notes&&details->notes->kind==*kind){
                auto& nav=*details->notes;
                if(document->more&&((direction>0&&document->next==nav.cursor)||std::ranges::find(nav.history,document->next)!=nav.history.end())){
                    details_failed("笔记翻页游标无效，请刷新重试");co_return pxa::Result<void>{};
                }
                if(direction>0)nav.history.push_back(nav.cursor);
                else if(direction<0&&!nav.history.empty())nav.history.pop_back();
                nav.cursor=cursor;nav.next=document->next;nav.more=document->more;
                details->body=std::move(document->body);details->offset=0;busy=false;dirty=true;
                log("WEREAD NOTES READY");
            }
            co_return pxa::Result<void>{};
        }
        std::optional<std::string> document;
        {
            auto raw=co_await request(std::string(wr::agent::gateway_path),wr::agent::payload(wr::agent::Api::book_info,book),32768,20000,
                wr::agent::Target::gateway,*key);
            if(!raw){details_failed(raw.error()==pxa::Error::denied?wr::agent::message(wr::agent::Status::authorization):"图书信息连接未完成，请重试");co_return pxa::Result<void>{};}
            document=wr::agent::book_document(*raw,book);
            if(!document){auto error=wr::agent::status(wr::Json(*raw));details_failed(wr::agent::message(error==wr::agent::Status::ok?wr::agent::Status::malformed:error));co_return pxa::Result<void>{};}
        }
        // The metadata body is released before fetching progress. These calls
        // neither report reading time nor modify local/cloud reading positions.
        if(auto raw=co_await request(std::string(wr::agent::gateway_path),wr::agent::payload(wr::agent::Api::book_progress,book),8192,20000,
            wr::agent::Target::gateway,*key)){
            if(auto line=wr::agent::progress_line(*raw,book))*document+="\n\n"+*line;
            else *document+="\n\n云端进度暂不可用";
        }else *document+="\n\n云端进度暂不可用";
        if(details&&screen==Screen::details&&details->book.id==book&&!details->notes){
            details->body=std::move(*document);details->offset=0;busy=false;dirty=true;
            log("WEREAD DETAILS READY",{{"bytes",details->body.size()}});
        }
        co_return pxa::Result<void>{};
    }
    void show_details(wr::Book book){
        if(busy||book.id.empty())return;
        (void)search_input.hide_keyboard();page_images.reset();covers_active=false;
        details=std::make_unique<Details>(Details{screen,std::move(book),{}});
        screen=Screen::details;dirty=true;start(load_details(details->book.id));
    }
    PixelRect details_box()const{auto r=safe();const auto head=std::min(dp(44),r.height/5),foot=std::min(dp(40),r.height/5),gap=std::min(std::max(4,dp(8)),std::max(4,r.height/20)),tabs=std::min(dp(32),r.height/6);
        return {r.x+gap,r.y+head+tabs+2*gap,r.width-2*gap,std::max(1,r.height-head-tabs-foot-3*gap)};}
    int details_pixels()const{return std::clamp(dp(15),8,std::max(8,std::min(128,details_box().height*2/3)));}
    std::string_view details_text()const{return !details?std::string_view{}:details->key_setup?wr::agent::key_setup_guide:std::string_view(details->body);}
    wr::ReadingPage details_page(std::size_t offset)const{
        const auto box=details_box();const int pixels=details_pixels();
        return wr::paginate_document(details_text(), {},offset,box.width,box.height,pixels,pixels*3/2);
    }
    void action(Action a,int index=0) {
        if(screen==Screen::picture){
            if(a==Action::back){close_picture();return;}
            if(picture&&(a==Action::larger||a==Action::smaller)){
                const auto zoom=std::clamp(picture->zoom+(a==Action::larger?1:-1),1,6);
                picture->pan_x=picture->pan_x*zoom/picture->zoom;
                picture->pan_y=picture->pan_y*zoom/picture->zoom;
                picture->zoom=zoom;dirty=true;return;
            }
        }
        if(shelf_syncing&&!busy){context->foreground_tasks().cancel();log("WEREAD SHELF SYNC INTERRUPTED");}
        if(a==Action::details_tab&&details&&index==(!details->notes?0:details->notes->kind==wr::agent::NoteKind::highlights?1:2))return;
        if(busy&&!showing_login()&&(a==Action::back||a==Action::settings||a==Action::home||a==Action::menu||a==Action::details_read||a==Action::details_tab)){
            context->foreground_tasks().cancel();busy=false;
        }
        if(a==Action::details_tab&&details&&index>=0&&index<=2){
            if(index){
                details->notes=std::make_unique<wr::agent::NoteNavigation>();
                details->notes->kind=index==1?wr::agent::NoteKind::highlights:wr::agent::NoteKind::thoughts;
            }else details->notes.reset();
            details->body.clear();details->offset=0;details->key_setup=false;dirty=true;start(load_details(details->book.id));return;
        }
        if(a==Action::bookmarks&&!busy&&!reader.text.empty()){
            screen=Screen::bookmarks;dirty=true;start(load_bookmarks());return;
        }
        if(screen==Screen::bookmarks){
            if(a==Action::back){bookmark_panel.reset();screen=Screen::menu;dirty=true;return;}
            if(!bookmark_panel||busy)return;
            auto& panel=*bookmark_panel;
            if(a==Action::bookmark_add){start(edit_bookmark());return;}
            if(a==Action::bookmark_remove&&index>=0&&std::size_t(index)<panel.data.items.size()){
                if(panel.pending_remove==unsigned(index))start(edit_bookmark(unsigned(index)));
                else {panel.pending_remove=unsigned(index);dirty=true;}
                return;
            }
            if(a==Action::bookmark_open&&index>=0&&std::size_t(index)<panel.data.items.size()){
                const auto& mark=panel.data.items[index];
                if(mark.chapter==reader.chapter_uid&&mark.section==reader.section&&mark.param==reader.current){
                    const bool changed=mark.body_hash!=wr::bookmark_text_hash(reader.text)||mark.offset>=reader.text.size();
                    reading_offset=changed?0:mark.offset;
                    while(reading_offset&&(static_cast<unsigned char>(reader.text[reading_offset])&0xc0)==0x80)--reading_offset;
                    bookmark_panel.reset();page_images.reset();page_history.clear();page=0;screen=Screen::reading;
                    remember_position();save();dirty=true;
                    if(changed){log("WEREAD BOOKMARK TEXT CHANGED");(void)context->window().toast("正文已更新，已定位到本节开头");}
                }else{
                    auto location=mark.resume(panel.data.book);
                    auto query=location.query();
                    start(load_reader(std::move(query),false,std::move(location),mark.body_hash));
                }
                return;
            }
            if(a==Action::previous||a==Action::next){
                panel.pending_remove.reset();
                if(a==Action::previous&&panel.first>=rows())panel.first-=rows();
                if(a==Action::next&&panel.first+rows()<panel.data.items.size())panel.first+=rows();
                dirty=true;return;
            }
        }
        if(a==Action::details){
            if(screen==Screen::shelf&&index>=0&&std::size_t(index)<shelf.books.size())show_details(shelf.books[index]);
            else if(screen==Screen::search&&index>=0&&std::size_t(index)<search.books.size())show_details(search.books[index]);
            else if(screen==Screen::menu)show_details({reader.book_id,reader.title,{},{},reader.current});return;
        }
        if(a==Action::details_read&&details){
            auto selected=std::move(details->book);const auto parent=details->parent;leave_details();
            if(parent==Screen::menu){screen=Screen::reading;dirty=true;return;}
            if(open_loaded_book(selected.id))return;
            if(auto* resume=saved.find(selected.id))start(load_reader(resume->query(),false,*resume));
            else if(!selected.param.empty())start(load_reader("?bc="+wr::percent(selected.param)));
            else start(open_search(std::move(selected.id)));return;
        }
        if(screen==Screen::details&&details&&(a==Action::previous||a==Action::next)){
            if(busy)return;
            const auto end=details_page(details->offset).end;
            if(a==Action::next&&end>details->offset&&end<details_text().size())details->offset=end;
            else if(a==Action::previous&&details->offset){
                std::size_t at=0;
                while(at<details->offset){const auto page=details_page(at);if(page.end>=details->offset||page.end<=at)break;at=page.end;}
                details->offset=at;
            }else if(details->notes&&!details->key_setup){
                const auto& nav=*details->notes;
                if(a==Action::next&&nav.more){
                    if(nav.history.size()>=256){notice("本次笔记翻页较多，请切换标签重新载入");return;}
                    start(load_details(details->book.id,nav.next,1));
                }else if(a==Action::previous&&!nav.history.empty())start(load_details(details->book.id,nav.history.back(),-1));
            }
            dirty=true;return;
        }
        if(a==Action::search){search_start=0;screen=Screen::search;dirty=true;return;}
        if(a==Action::search_go&&!busy){
            auto keyword=wr::trim(query.get());if(keyword.empty()){notice("请输入书名或作者");return;}
            (void)search_input.hide_keyboard();search_keyword=keyword;start(load_search(0));return;}
        if(a==Action::search_open&&!busy&&index>=0&&std::size_t(index)<search.books.size()){
            (void)search_input.hide_keyboard();
            if(open_loaded_book(search.books[index].id))return;
            if(auto* resume=saved.find(search.books[index].id))start(load_reader(resume->query(),false,*resume));
            else start(open_search(search.books[index].id));return;}
        if(a==Action::previous&&screen==Screen::search&&!busy){
            if(search_start>=search_rows()){search_start-=search_rows();dirty=true;}
            else if(!search_history.empty())start(load_search(search_history.back(),true));return;}
        if(a==Action::next&&screen==Screen::search&&!busy){
            if(search_start+search_rows()<search.books.size()){search_start+=search_rows();dirty=true;}
            else if(search.cursor<search.total)start(load_search(search.cursor));return;}
        if(a==Action::refresh){context->foreground_tasks().cancel();busy=false;
            if(screen==Screen::details&&details){start(load_details(details->book.id,details->notes?details->notes->cursor:0));return;}
            if(screen==Screen::login)start(begin_login());
            else if(reauth_required){
                remember_position();session.clear();auth_dirty=true;login_after_save=true;
                page_images.reset();reader={};shelf={};shelf_history.clear();page_history.clear();reading_offset=0;
                screen=Screen::login;notice("正在重新登录…");save(true);
            }else start(load_shelf());return;}
        if(a==Action::settings){if(screen!=Screen::about && screen!=Screen::settings)return_screen=screen;settings_start=0;screen=Screen::settings;dirty=true;return;}
        if(a==Action::about){screen=Screen::about;dirty=true;return;}
        if(a==Action::back&&screen==Screen::search){(void)search_input.hide_keyboard();screen=Screen::shelf;dirty=true;return;}
        if(a==Action::back&&screen==Screen::details){leave_details();return;}
        if(a==Action::back){screen=screen==Screen::menu||screen==Screen::contents?Screen::reading:screen==Screen::about?Screen::settings:return_screen;dirty=true;return;}
        if(a==Action::home){remember_position();save(true);page_images.reset();screen=Screen::shelf;dirty=true;return;}
        if(a==Action::theme){night=!night;save();dirty=true;return;}
        if(a==Action::larger){page_images.reset();font_dp=std::min(28u,font_dp+2);page_history.clear();save();dirty=true;return;}
        if(a==Action::smaller){page_images.reset();font_dp=std::max(12u,font_dp-2);page_history.clear();save();dirty=true;return;}
        if(a==Action::menu){screen=Screen::menu;dirty=true;return;}
        if(a==Action::contents){auto at=std::ranges::find(reader.catalog.items,reader.chapter_uid,&wr::ChapterLink::uid);
            contents_start=at==reader.catalog.items.end()?0:std::size_t(at-reader.catalog.items.begin())/rows()*rows();
            screen=Screen::contents;dirty=true;return;}
        if(a==Action::chapter && index>=0 && std::size_t(index)<reader.catalog.items.size() && !busy){
            const auto& chapter=reader.catalog.items[index];
            if(chapter.uid==reader.chapter_uid&&chapter.param==reader.current&&!reader.section&&!reader.text.empty()){
                reading_offset=0;page_history.clear();page=0;page_images.clear_page();screen=Screen::reading;
                remember_position();save();dirty=true;log("WEREAD CURRENT CHAPTER LOCAL");return;
            }
            if(!chapter.param.empty())start(load_reader("?bc="+wr::percent(chapter.param)+"&sect=0",false,{},{},chapter.uid));
            else{notice("本章节暂无阅读入口，请重新打开图书");(void)context->window().toast(status);}return;}
        if((a==Action::previous || a==Action::next) && screen==Screen::reading){turn(a==Action::next?1:-1);return;}
        if(a==Action::previous && screen==Screen::contents&&!busy){
            if(contents_start>=rows()){contents_start-=rows();dirty=true;}
            else if(reader.catalog.can_load(-1))start(load_catalog(-1));return;}
        if(a==Action::next && screen==Screen::contents&&!busy){
            if(contents_start+rows()<reader.catalog.items.size()){contents_start+=rows();dirty=true;}
            else if(reader.catalog.can_load(1))start(load_catalog(1));return;}
        if(a==Action::previous && screen==Screen::settings){auto n=settings_capacity();settings_start=settings_start>=n?settings_start-n:((7-1)/n)*n;dirty=true;return;}
        if(a==Action::next && screen==Screen::settings){settings_start=settings_start+settings_capacity()<7?settings_start+settings_capacity():0;dirty=true;return;}
        if(a==Action::leading){page_images.reset();saved.preferences.leading=saved.preferences.leading>=200?120:saved.preferences.leading+10;
            page_history.clear();save();dirty=true;return;}
        if(a==Action::images){page_images.reset();saved.preferences.images=!saved.preferences.images;page_history.clear();save();dirty=true;return;}
        if(a==Action::volume){saved.preferences.volume_keys=!saved.preferences.volume_keys;save();dirty=true;return;}
        if(a==Action::remember){session.remember=!session.remember;auth_dirty=true;
            save(true);dirty=true;return;}
        if(a==Action::logout){context->foreground_tasks().cancel();busy=false;session.clear();auth_dirty=true;shelf={};shelf_history.clear();server_start=list_start=0;server_key=-1;qr.reset();
            details.reset();bookmark_panel.reset();
            page_images.reset();reader={};saved.progress.clear();saved.account.clear();reading_offset=0;page_history.clear();
            login_after_save=true;screen=Screen::login;notice("正在退出账号…");save(true);dirty=true;return;}
        if(a==Action::previous && screen==Screen::shelf&&!busy){
            if(list_start>=rows()){list_start-=rows();dirty=true;}
            else if(!shelf_history.empty()){const auto cursor=shelf_history.back();start(load_shelf(cursor.start,cursor.key,true));}return;}
        if(a==Action::next && screen==Screen::shelf && !busy){
            if(list_start+rows()<shelf.books.size()){list_start+=rows();dirty=true;}
            else if(server_start+shelf.books.size()<shelf.total){start(load_shelf(server_start+shelf.books.size(),shelf.kk));}return;}
        if(a==Action::open && !busy && index>=0 && std::size_t(index)<shelf.books.size()){
            if(open_loaded_book(shelf.books[index].id))return;
            if(shelf.books[index].param.empty()){notice("本书没有可用阅读入口");return;}
            if(auto* resume=saved.find(shelf.books[index].id)){start(load_reader(resume->query(),false,*resume));return;}
            start(load_reader("?bc="+wr::percent(shelf.books[index].param)));return;
        }
    }
    std::size_t settings_capacity()const{auto r=safe();const int head=std::min(dp(44),r.height/5),foot=std::min(dp(40),r.height/5);
        return r.height-head-foot>=4*(dp(34)+std::max(4,dp(8)))?4:2;}
    std::size_t search_rows()const {auto r=safe();return std::clamp((r.height-dp(140))/std::max(dp(48),32),1,10);}
    std::size_t rows()const {auto r=safe();return std::clamp((r.height-dp(100))/std::max(dp(48),32),1,10);}
    PixelRect picture_box()const{
        const auto r=safe();const auto gap=dp(8);
        const auto head=std::min(dp(44),r.height/5),foot=std::min(dp(40),r.height/5);
        return {r.x+gap,r.y+head+gap,std::max(1,r.width-2*gap),std::max(1,r.height-head-foot-2*gap)};
    }
    const pxa::Asset* picture_asset()const{
        if(const auto* entry=page_images.find(picture->index);entry&&entry->asset)return &*entry->asset;
        return picture->preview&&picture->preview->asset?&*picture->preview->asset:nullptr;
    }
    PixelRect picture_rectangle(){
        const auto box=picture_box();
        auto aspect=reader.images[picture->index].aspect_q16;
        if(const auto* asset=picture_asset()){
            const auto d=asset->descriptor();
            aspect=std::uint32_t((std::uint64_t(d.width)<<16)/std::max(1u,unsigned(d.height)));
        }
        int width=box.width,height=int(std::int64_t(width)*65536/std::max(1u,aspect));
        if(height>box.height){height=box.height;width=int(std::int64_t(height)*aspect/65536);}
        width=std::max(1,width)*picture->zoom;height=std::max(1,height)*picture->zoom;
        const auto max_x=std::max(0,(width-box.width+1)/2),max_y=std::max(0,(height-box.height+1)/2);
        picture->pan_x=std::clamp(picture->pan_x,-max_x,max_x);
        picture->pan_y=std::clamp(picture->pan_y,-max_y,max_y);
        return {box.x+(box.width-width)/2+picture->pan_x,box.y+(box.height-height)/2+picture->pan_y,width,height};
    }
    void show_picture(std::uint16_t index){
        picture_press.reset();held_image.reset();pointer.reset();
        if(screen!=Screen::reading||index>=reader.images.size()||reader.images[index].url.empty()||!saved.preferences.images)return;
        picture=Picture{index};picture->preview=page_images.take(index);
        if(picture->preview){const auto d=picture->preview->asset->descriptor();picture->decode_budget-=unsigned(d.width)*d.height;}
        page_images.reset();screen=Screen::picture;dirty=true;
        log("WEREAD PICTURE OPEN",{{"index",index},{"preview",bool(picture->preview)}});
    }
    void close_picture(){
        auto preview=std::move(picture->preview);
        picture.reset();picture_press.reset();held_image.reset();pointer.reset();page_images.reset();
        if(preview)page_images.remember(std::move(*preview));
        screen=Screen::reading;dirty=true;log("WEREAD PICTURE CLOSED");
    }
    void pointer_event(const CanvasPointer& source){
        auto e=source;
        e.x=canvas_to_surface_coordinate(e.x,metrics);
        e.y=canvas_to_surface_coordinate(e.y,metrics);
        if(e.action()==PointerPhase::down){
            if(waiting()&&wait_box().contains(e.x,e.y))return;
            if(!pointer.try_capture(e))return;
            press_x=e.x;press_y=e.y;moved=false;picture_press.reset();held_image.reset();
            if(screen==Screen::picture&&picture){
                picture->dragging=picture_box().contains(e.x,e.y);picture->last_x=e.x;picture->last_y=e.y;
            }else if(screen==Screen::reading&&!busy&&saved.preferences.images){
                const auto box=reading_box();
                for(std::size_t i=0;i<reading_page.count;++i){const auto& run=reading_page.lines[i];
                    if(run.is_image()&&run.image<reader.images.size()&&!reader.images[run.image].url.empty()&&
                       PixelRect{box.x+run.x,box.y+run.y,run.width,run.height}.contains(e.x,e.y)){
                        held_image=run.image;(void)picture_press.update(e,PressOptions{.slop=dp(8)});break;
                    }
                }
            }
            return;
        }
        if(!pointer.owns(e))return;
        if(held_image&&picture_press.update(e,PressOptions{.slop=dp(8)})==PressResult::long_press){show_picture(*held_image);return;}
        if(std::abs(e.x-press_x)>dp(8) || std::abs(e.y-press_y)>dp(8))moved=true;
        if(screen==Screen::picture&&picture&&picture->dragging&&e.action()==PointerPhase::move){
            picture->pan_x+=e.x-picture->last_x;picture->pan_y+=e.y-picture->last_y;
            picture->last_x=e.x;picture->last_y=e.y;dirty=true;
        }
        if(e.action()==PointerPhase::cancel){pointer.reset();picture_press.reset();held_image.reset();if(picture)picture->dragging=false;return;}
        if(e.action()!=PointerPhase::up)return;
        pointer.reset();
        picture_press.reset();held_image.reset();
        if(screen==Screen::picture&&picture&&std::exchange(picture->dragging,false)){
            if(!moved)if(const auto* entry=page_images.find(picture->index);entry&&entry->failed)page_images.retry(picture->index);
            return;
        }
        if(moved){
            if((screen==Screen::shelf || screen==Screen::contents || screen==Screen::search || screen==Screen::bookmarks) && std::abs(e.y-press_y)>dp(28))action(e.y<press_y?Action::next:Action::previous);
            if(screen==Screen::reading && std::abs(e.x-press_x)>dp(28))turn(e.x<press_x?1:-1);return;
        }
        if(screen==Screen::reading){auto box=reading_box();if(box.contains(e.x,e.y)){
            for(std::size_t i=0;i<reading_page.count;++i){const auto& run=reading_page.lines[i];
                if(run.is_image()&&PixelRect{box.x+run.x,box.y+run.y,run.width,run.height}.contains(e.x,e.y))
                    if(const auto* entry=page_images.find(run.image);entry&&entry->failed&&!entry->url.empty()){page_images.retry(run.image);dirty=true;return;}
            }
            if(e.x<box.x+box.width/3)turn(-1);else if(e.x>box.x+box.width*2/3)turn(1);else action(Action::menu);return;}}
        for(std::size_t i=0;i<hit_count;++i)if(hits[i].box.contains(e.x,e.y) && hits[i].box.contains(press_x,press_y)){
            if(hits[i].action==Action::open||hits[i].action==Action::search_open){
                const auto edge=hits[i].box.x+hits[i].box.width-dp(36);
                if(e.x>=edge&&press_x>=edge){action(Action::details,hits[i].index);break;}
                if(e.x>=edge||press_x>=edge)break;
            }
            action(hits[i].action,hits[i].index);break;
        }
    }
    void rectangle(PixelRect r,std::uint32_t color,int radius=0){if(r.width<=0 || r.height<=0)return;
        auto b=region(r);draw.rect(b.x,b.y,b.width,b.height,color,std::max(0,logical(radius)));}
    void text(PixelRect r,std::string_view value,int pixels,std::uint32_t color,int align=0){
        if(r.width<=0 || r.height<=0)return;
        pixels=std::clamp(pixels,8,128);std::size_t at=0;int width=0;
        while(at<value.size()){
            const auto c=wr::rune(value.substr(at));int advance=c.value==' '?pixels/2:c.value>=33&&c.value<127&&c.value!='W'&&c.value!='M'&&c.value!='@'?(pixels*3+3)/4:pixels;
            if(width+advance>r.width)break;width+=advance;at+=c.bytes;
        }
        draw.text_sized(region(r),color,value.substr(0,at),pixels,align);
    }
    bool button_surface(PixelRect r,Action action,int index=0,bool primary=false){
        if(hit_count==hits.size()||r.width<=0||r.height<=0)return false;
        hits[hit_count++]={r,action,index};rectangle(r,primary?blue:surface(),dp(12));return true;
    }
    // Icons share hit/background handling and omit empty text commands.
    void button(PixelRect r,std::string_view value,Action a,int index=0,bool primary=false){
        if(!button_surface(r,a,index,primary))return;
        int size=std::min(dp(15),r.height*3/5);
        const int padding=std::min(dp(6),std::max(1,r.width/10));
        // Short action labels must fit as a whole on dense/round displays.
        // Long book titles keep the normal size and use single-line clipping.
        const bool fit_counter=a==Action::refresh&&screen==Screen::shelf;
        unsigned count=0,units=0;
        for(unsigned char c:value){
            if((c&0xc0)==0x80)continue;
            if(++count>4&&!fit_counter)break;
            units+=c==' '?2:c>=33&&c<127&&c!='W'&&c!='M'&&c!='@'?3:4;
        }
        if((count<=4||fit_counter)&&units)size=std::min(size,(r.width-2*padding)*4/int(units));
        size=std::max(8,size);
        text({r.x+padding,r.y+(r.height-size*5/4)/2,r.width-2*padding,size*5/4+2},value,size,primary?0xffffffff:ink(),1);
    }
    void icon(PixelRect r,Action action){
        if(!button_surface(r,action))return;
        const int cx=r.x+r.width/2,cy=r.y+r.height/2,n=std::max(3,std::min(dp(7),std::min(r.width,r.height)/4));
        auto stroke=[&](int x1,int y1,int x2,int y2){
            draw.line(logical(x1),logical(y1),logical(x2),logical(y2),ink(),std::max(1,logical(dp(2))));};
        if(action==Action::previous||action==Action::next){
            int direction=action==Action::next?1:-1;
            stroke(cx-direction*n/2,cy-n,cx+direction*n/2,cy);stroke(cx+direction*n/2,cy,cx-direction*n/2,cy+n);
        }else if(action==Action::settings){
            for(int i=-1;i<=1;++i){int y=cy+i*n;int x=cx+(i==0?-n/3:n/3);
                stroke(cx-n,y,cx+n,y);rectangle({x-dp(1),y-dp(3),dp(3),dp(6)},ink(),dp(1));}
        }else if(action==Action::search){
            draw.arc(logical(cx-n/4),logical(cy-n/4),std::max(1,logical(n*3/4)),0,360,ink(),std::max(1,logical(dp(2))));
            stroke(cx+n/3,cy+n/3,cx+n,cy+n);
        }else if(action==Action::refresh){
            draw.arc(logical(cx),logical(cy),std::max(1,logical(n)),40,310,ink(),std::max(1,logical(dp(2))));
            int x=cx+n*64/100,y=cy-n*77/100;
            stroke(x,y,x-n*8/10,y);stroke(x,y,x,y-n*7/10);
        }
    }
    PixelRect cover_box(PixelRect card)const{
        const auto h=std::max(1,std::min(dp(52),card.height-dp(8))),w=std::max(1,std::min(dp(36),h*3/4));
        return {card.x+dp(8),card.y+(card.height-h)/2,w,h};
    }
    void update_covers(std::span<const wr::Book> books,std::size_t first,std::size_t count,PixelRect box){
        std::array<wr::PageImages::Request,10> wanted{};std::size_t n=0;
        for(auto i=first;i<books.size()&&n<std::min(count,wanted.size());++i)
            wanted[n++]={std::uint16_t(i),box.width,box.height,books[i].cover};
        covers_active=true;
        page_images.update(std::span(wanted).first(n),saved.preferences.images&&!busy);
    }
    void book_card(PixelRect card,const wr::Book& book,std::size_t index,Action action){
        if(hit_count<hits.size())hits[hit_count++]={card,action,int(index)};
        rectangle(card,surface(),dp(12));const auto cover=cover_box(card);
        rectangle(cover,0xe2ebfaff,dp(3));
        if(const auto* image=page_images.find(index);image&&image->asset)
            draw.image(region(cover),*image->asset,ImageFit::contain);
        else text(cover,"书",std::min(dp(16),cover.width*2/3),blue,1);
        const auto x=cover.x+cover.width+dp(8),width=card.x+card.width-x-dp(36);
        const bool author=!book.author.empty()&&card.height>=dp(52);
        const auto h=dp(author?36:22),y=card.y+(card.height-h)/2;
        text({x,y,width,dp(22)},book.title,dp(16),ink());
        if(author)text({x,y+dp(23),width,dp(14)},book.author,dp(12),muted());
        const int cx=card.x+card.width-dp(18),cy=card.y+card.height/2,n=std::max(3,dp(7));
        draw.arc(logical(cx),logical(cy),std::max(1,logical(n)),0,360,muted(),std::max(1,logical(dp(1))));
        rectangle({cx-dp(1),cy-dp(1),dp(2),dp(5)},muted(),dp(1));
        rectangle({cx-dp(1),cy-dp(4),dp(2),dp(2)},muted(),dp(1));
    }
    void bookmark_card(PixelRect card,const wr::LocalBookmark& mark,std::size_t index){
        if(!button_surface(card,Action::bookmark_open,int(index)))return;
        constexpr std::string_view separator=" · ";
        const auto split=mark.label.find(separator);
        const auto label=std::string_view(mark.label);
        const auto title=label.substr(0,split);
        const auto excerpt=split==label.npos?std::string_view{}:label.substr(split+separator.size());
        const int padding=std::min(dp(6),std::max(1,card.width/12));
        const int width=card.width-2*padding;
        const int pixels=std::max(8,std::min({dp(14),width/6,card.height/3}));
        const int heading=std::min(dp(12),pixels),leading=pixels*5/4+2,title_height=heading*5/4+2;
        const int gap=std::max(1,dp(2));
        const int top=card.y+(card.height-title_height-(excerpt.empty()?0:gap+leading))/2;
        std::array<char,12> number;
        const auto end=std::to_chars(number.data(),number.data()+number.size()-1,index+1).ptr;
        *end='.';
        const int indent=std::min(width/3,heading*2+gap);
        text({card.x+padding,top,indent,title_height},{number.data(),std::size_t(end-number.data()+1)},heading,muted());
        text({card.x+padding+indent,top,width-indent,title_height},title,heading,muted());
        if(!excerpt.empty())text({card.x+padding,top+title_height+gap,width,leading},excerpt,pixels,ink());
    }
    void render_scene(){
        if(covers_active&&screen!=Screen::shelf&&screen!=Screen::search){page_images.reset();covers_active=false;}
        volume_capture.set(screen==Screen::reading&&saved.preferences.volume_keys);
        if(screen!=Screen::search)search_box.set({});
        draw.reset();hit_count=0;
        rectangle({0,0,int(metrics.width),int(metrics.height)},background());
        auto r=safe();const int gap=std::max(4,dp(8)),head=std::min(dp(44),r.height/5),foot=std::min(dp(40),r.height/5);
        const int title_font=std::min(dp(22),head*2/3);
        if(screen!=Screen::reading)text({r.x+gap,r.y+(head-title_font*5/4)/2,r.width-2*gap-(screen==Screen::shelf?2*(head+gap):screen==Screen::details?head+dp(52)+gap:0),head},
            screen==Screen::shelf?"书架":screen==Screen::settings?"阅读设置":screen==Screen::about?"关于应用":screen==Screen::contents?"章节目录":screen==Screen::search?"搜索图书":screen==Screen::unavailable?"阅读提示":screen==Screen::details?"详情":screen==Screen::bookmarks?"本地书签":screen==Screen::picture?"查看图片":"微信读书",title_font,ink(),screen==Screen::login?1:0);
        if(screen==Screen::login){
            const int available=r.height-head-foot-gap;
            const int side=std::max(8,std::min({r.width-2*gap,available,dp(224)}));
            PixelRect box{r.x+(r.width-side)/2,r.y+head+(available-side)/2,side,side};
            rectangle(box,0xffffffff,dp(8));
            if(qr)draw.image(region(box),*qr,ImageFit::contain);
            else text({box.x+gap,box.y+side/2-dp(10),box.width-2*gap,dp(28)},busy?"正在连接…":"轻点下方重新获取",std::min(dp(14),side/12),muted(),1);
            const auto label=qr?std::string_view("微信扫码登录"):std::string_view(busy?"连接中 · 点击重试":"重新获取二维码");
            const int notice_y=box.y+box.height+gap;
            if(notice_y+dp(20)<r.y+r.height-foot)
                text({r.x,notice_y,r.width,dp(20)},status,dp(12),muted(),1);
            button({r.x,r.y+r.height-foot,r.width,foot},label,Action::refresh,0,!qr);
            icon({r.x+r.width-head,r.y,head,head-gap/2},Action::settings);
        }else if(screen==Screen::unavailable){
            const PixelRect box{r.x+gap,r.y+head+gap,r.width-2*gap,r.height-head-foot-2*gap};
            const int pixels=std::clamp(dp(15),8,std::max(8,box.width/6)),leading=pixels*5/4;
            const auto message=wr::paginate_document(status,{},0,box.width,box.height,pixels,leading);
            for(std::size_t i=0;i<message.count;++i){const auto& line=message.lines[i];
                text({box.x+line.x,box.y+line.y,line.width,line.height},std::string_view(status).substr(line.start,line.end-line.start),pixels,ink());}
            button({r.x,r.y+r.height-foot,r.width,foot},"返回",Action::back);
        }else if(screen==Screen::shelf){
            const int row_h=(r.height-head-foot-gap)/int(rows());
            if(!busy)update_covers(shelf.books,list_start,rows(),cover_box({r.x,r.y+head,r.width,row_h-gap}));
            for(std::size_t i=list_start;i<std::min(list_start+rows(),shelf.books.size());++i)
                book_card({r.x,r.y+head+int(i-list_start)*row_h,r.width,row_h-gap},shelf.books[i],i,Action::open);
            if(shelf.books.empty())text({r.x,r.y+head+gap,r.width,dp(50)},status,dp(15),muted(),1);
            icon({r.x+r.width-head,r.y,head,head-gap/2},Action::settings);
            icon({r.x+r.width-2*head-gap/2,r.y,head,head-gap/2},Action::search);
            const int w=std::min(dp(44),r.width/4),y=r.y+r.height-foot;
            icon({r.x,y,w,foot},Action::previous);
            icon({r.x+r.width-w,y,w,foot},Action::next);
            const auto page_label=reauth_required?std::string("重新登录"):shelf.books.empty()?std::string("刷新书架"):
                (shelf_local?std::string("本地 "):std::string{})+
                wr::number(server_start+list_start+(shelf.books.empty()?0:1))+(shelf_local?"/":" / ")+wr::number(shelf.total);
            button({r.x+w+gap,y,r.width-2*(w+gap),foot},page_label,Action::refresh);
        }else if(screen==Screen::search){
            const int input_h=std::min(dp(42),r.height/5),go=std::min(dp(56),r.width/3),y=r.y+head;
            rectangle({r.x,y,r.width,input_h},surface(),dp(10));
            search_box.set({region({r.x+gap,y+gap/2,r.width-go-2*gap,input_h-gap}),true,Font::body,ink()});
            button({r.x+r.width-go,y,go,input_h},"搜索",Action::search_go,0,true);
            const int top=y+input_h+gap,n=int(search_rows()),h=std::max(12,(r.y+r.height-foot-gap-top)/n);
            if(!busy)update_covers(search.books,search_start,n,cover_box({r.x,top,r.width,h-gap}));
            for(std::size_t i=search_start;i<std::min(search_start+n,search.books.size());++i)
                book_card({r.x,top+int(i-search_start)*h,r.width,h-gap},search.books[i],i,Action::search_open);
            if(search.books.empty())text({r.x,top,r.width,dp(40)},search_keyword.empty()?"输入书名或作者":std::string_view(status),dp(13),muted(),1);
            const int w=std::min(dp(44),r.width/4),bottom=r.y+r.height-foot;
            icon({r.x,bottom,w,foot},Action::previous);icon({r.x+r.width-w,bottom,w,foot},Action::next);
            button({r.x+w+gap,bottom,r.width-2*(w+gap),foot},"返回",Action::back);
        }else if(screen==Screen::details&&details){
            const int tabs=std::min(dp(32),r.height/6),tab_width=(r.width-2*gap)/3;
            const auto selected=!details->notes?0:details->notes->kind==wr::agent::NoteKind::highlights?1:2;
            for(int i=0;i<3;++i)button({r.x+i*(tab_width+gap),r.y+head,tab_width,tabs},i==0?"简介":i==1?"划线":"想法",Action::details_tab,i,i==selected);
            const auto box=details_box();const int pixels=details_pixels();
            const auto page=details_page(details->offset);
            for(std::size_t i=0;i<page.count;++i){const auto& line=page.lines[i];
                text({box.x+line.x,box.y+line.y,line.width,line.height},details_text().substr(line.start,line.end-line.start),pixels,ink());}
            if(details_text().empty())text(box,status,dp(13),muted(),1);
            button({r.x+r.width-head-dp(52)-gap/2,r.y,dp(52),head-gap/2},"阅读",Action::details_read);
            icon({r.x+r.width-head,r.y,head,head-gap/2},Action::refresh);
            const int w=std::min(dp(44),r.width/4),bottom=r.y+r.height-foot;
            icon({r.x,bottom,w,foot},Action::previous);icon({r.x+r.width-w,bottom,w,foot},Action::next);
            button({r.x+w+gap,bottom,r.width-2*(w+gap),foot},"返回",Action::back);
        }else if(screen==Screen::settings){
            const int count=int(settings_capacity());
            const int h=std::max(16,std::min(dp(42),(r.height-head-foot-gap*(count+1))/count)),y=r.y+head;
            const int begin=int(settings_start);
            for(int i=0;i<count&&begin+i<7;++i){int row=begin+i,at=y+i*(h+gap);
                if(row==0)button({r.x,at,r.width,h},night?"外观 · 深色":"外观 · 浅色",Action::theme);
                else if(row==1){button({r.x,at,(r.width-gap)/2,h},"字号 −",Action::smaller);
                    button({r.x+(r.width+gap)/2,at,(r.width-gap)/2,h},"字号 +",Action::larger);}
                else if(row==2)button({r.x,at,r.width,h},session.remember?"保持登录 · 开":"保持登录 · 关",Action::remember);
                else if(row==3){button({r.x,at,(r.width-gap)/2,h},reauth_required?"重新登录":"退出账号",reauth_required?Action::refresh:Action::logout);
                    button({r.x+(r.width+gap)/2,at,(r.width-gap)/2,h},"关于",Action::about);}
                else if(row==4)button({r.x,at,r.width,h},"行距 · "+wr::number(saved.preferences.leading)+"%",Action::leading);
                else if(row==5)button({r.x,at,r.width,h},saved.preferences.volume_keys?"音量键翻页 · 开":"音量键翻页 · 关",Action::volume);
                else if(row==6)button({r.x,at,r.width,h},saved.preferences.images?"加载图片 · 开":"加载图片 · 关",Action::images);
            }
            {
                int w=std::min(dp(44),r.width/4),bottom=r.y+r.height-foot;
                icon({r.x,bottom,w,foot},Action::previous);
                icon({r.x+r.width-w,bottom,w,foot},Action::next);
                button({r.x+w+gap,bottom,r.width-2*(w+gap),foot},"返回",Action::back);
            }
        }else if(screen==Screen::about){
            text({r.x,r.y+head+gap,r.width,dp(30)},"为 PXA 制作的独立阅读客户端",dp(15),ink());
            text({r.x,r.y+head+dp(42),r.width,dp(30)},"非腾讯官方出品 · 0.1.0 开发版",dp(12),muted());
            button({r.x,r.y+r.height-foot,r.width,foot},"返回",Action::settings);
        }else if(screen==Screen::picture&&picture){
            const auto box=picture_box();
            const wr::PageImages::Request wanted{picture->index,box.width*2,box.height*2,reader.images[picture->index].url};
            page_images.update(std::span{&wanted,1},true,picture->decode_budget);
            rectangle(box,surface());
            if(const auto* asset=picture_asset()){
                draw.clip(region(box));
                draw.image(region(picture_rectangle()),*asset,ImageFit::contain);
                draw.pop_clip();
            }else{
                const auto* pending=page_images.find(picture->index);
                text(box,pending&&pending->failed?"图片未载入 · 轻点重试":"正在加载图片…",dp(13),muted(),1);
            }
            const int control=std::min(dp(40),r.width/5),back=std::min(dp(64),r.width/3),bottom=r.y+r.height-foot;
            button({r.x,bottom,control,foot},"−",Action::smaller);
            button({r.x+control+gap,bottom,control,foot},"+",Action::larger);
            const int label_size=dp(12),label_height=label_size*5/4+2;
            const auto* entry=page_images.find(picture->index);
            const auto caption=entry&&entry->failed?std::string_view("加载失败"):entry&&!entry->asset?std::string_view("加载中"):std::string_view{};
            const int label_x=r.x+2*(control+gap),label_width=r.width-2*(control+gap)-back-gap;
            text({label_x,bottom+(foot-label_height)/2-(caption.empty()?0:dp(7)),label_width,label_height},wr::number(picture->zoom*100)+"%",label_size,muted(),1);
            if(!caption.empty())text({label_x,bottom+foot/2+dp(2),label_width,dp(14)},caption,dp(10),muted(),1);
            button({r.x+r.width-back,bottom,back,foot},"返回",Action::back);
        }else if(screen==Screen::reading){
            auto box=reading_box();reflow();
            text({box.x,r.y,box.width,box.y-r.y},reader.chapter_title,dp(12),muted());
            const int pixels=std::clamp(dp(font_dp),8,128);
            page_images.update(reading_page,reader.images,saved.preferences.images);
            for(std::size_t i=0;i<reading_page.count;++i){const auto& l=reading_page.lines[i];
                PixelRect run{box.x+l.x,box.y+l.y,l.width,l.height};
                if(!l.is_image())text(run,std::string_view(reader.text).substr(l.start,l.end-l.start),pixels,ink());
                else if(const auto* entry=page_images.find(l.image);entry&&entry->asset)draw.image(region(run),*entry->asset,ImageFit::contain);
                else {
                    rectangle(run,surface());
                    auto label=!saved.preferences.images?std::string_view("图片已关闭"):std::string_view("正在加载图片…");
                    if(const auto* entry=page_images.find(l.image);entry&&entry->failed)label=entry->url.empty()?"图片地址暂不支持":"图片未载入 · 轻点重试";
                    text(run,label,std::min(pixels,dp(12)),muted(),1);
                }
            }
            auto progress=wr::number(reader.text.empty()?0:reading_page.end*100/reader.text.size())+"% · 本节";
            if(!reader.access_label().empty())progress+=" · "+std::string(reader.access_label());
            text({box.x,box.y+box.height,box.width,dp(24)},progress,dp(11),muted(),2);
            hits[hit_count++]={{r.x,r.y,r.width,box.y-r.y},Action::menu};
        }else if(screen==Screen::menu){
            const int h=std::max(24,std::min(dp(42),(r.height-head-foot-3*gap)/3)),y=r.y+head;
            const int mark=std::min(dp(64),r.width/3);
            button({r.x,y,r.width-mark-gap,h},"继续阅读",Action::back);
            button({r.x+r.width-mark,y,mark,h},"书签",Action::bookmarks);
            button({r.x,y+h+gap,(r.width-gap)/2,h},"目录",Action::contents);
            button({r.x+(r.width+gap)/2,y+h+gap,(r.width-gap)/2,h},"设置",Action::settings);
            button({r.x,y+2*(h+gap),(r.width-gap)/2,h},"返回书架",Action::home);
            button({r.x+(r.width+gap)/2,y+2*(h+gap),(r.width-gap)/2,h},"图书详情",Action::details);
        }else if(screen==Screen::bookmarks){
            const int add=std::min(dp(52),r.width/3);
            if(bookmark_panel&&!busy)button({r.x+r.width-add,r.y,add,head-gap/2},"添加",Action::bookmark_add);
            if(bookmark_panel&&!bookmark_panel->data.items.empty()){
                const int h=(r.height-head-foot-gap)/int(rows()),remove=std::min(dp(44),r.width/4);
                for(auto i=bookmark_panel->first;i<std::min(bookmark_panel->first+rows(),bookmark_panel->data.items.size());++i){
                    const auto y=r.y+head+int(i-bookmark_panel->first)*h;
                    bookmark_card({r.x,y,r.width-remove-gap,h-gap},bookmark_panel->data.items[i],i);
                    button({r.x+r.width-remove,y,remove,h-gap},bookmark_panel->pending_remove==unsigned(i)?"确认":"删除",Action::bookmark_remove,int(i));
                }
            }else text({r.x,r.y+head+gap,r.width,r.height-head-foot-gap},busy?std::string_view(status):bookmark_panel?"暂无本地书签，点击添加保存当前页":std::string_view(status),std::min(dp(14),r.width/12),muted(),1);
            const int w=std::min(dp(44),r.width/4),y=r.y+r.height-foot;
            icon({r.x,y,w,foot},Action::previous);icon({r.x+r.width-w,y,w,foot},Action::next);
            button({r.x+w+gap,y,r.width-2*(w+gap),foot},"返回",Action::back);
        }else if(screen==Screen::contents){
            const int h=(r.height-head-foot-gap)/int(rows());
            for(std::size_t i=contents_start;i<std::min(contents_start+rows(),reader.catalog.items.size());++i){
                const auto& chapter=reader.catalog.items[i];
                const PixelRect box{r.x,r.y+head+int(i-contents_start)*h,r.width,h-gap};
                if(!button_surface(box,Action::chapter,int(i)))continue;
                const int pixels=std::min(dp(15),box.height*3/5),padding=dp(6);
                text({box.x+padding,box.y+(box.height-pixels*5/4)/2,box.width-2*padding-(chapter.locked?dp(28):0),pixels*5/4+2},chapter.title,pixels,ink(),1);
                if(chapter.locked){
                    const int x=box.x+box.width-dp(16),y=box.y+box.height/2;
                    rectangle({x-dp(4),y-dp(8),dp(8),dp(10)},ink(),dp(4));
                    rectangle({x-dp(2),y-dp(6),dp(4),dp(8)},surface(),dp(2));
                    rectangle({x-dp(6),y-dp(1),dp(12),dp(10)},ink(),dp(2));
                    rectangle({x-dp(1),y+dp(2),dp(2),dp(4)},surface(),dp(1));
                }
            }
            const int w=(r.width-2*gap)/3,y=r.y+r.height-foot;
            button({r.x,y,w,foot},"上一页",Action::previous);
            button({r.x+w+gap,y,w,foot},"下一页",Action::next);
            button({r.x+2*(w+gap),y,r.width-2*(w+gap),foot},"返回",Action::back);
        }
    }
    void render(){
        const bool pending=waiting();
        bool same_scene=scene_valid&&scene_screen==screen&&scene_night==night;
        if(pending&&same_scene)draw=scene_draw;
        else{
            render_scene();same_scene=same_scene&&draw.same_commands(scene_draw);
            scene_draw=draw;
        }
        std::optional<CanvasRegion> hint;
        if(pending){
            const auto box=wait_box();hint=region(box);
            if(same_scene&&presented_wait==hint&&presented_wait_text==status){dirty=false;return;}
            rectangle(box,background());
            const int pixels=std::max(8,std::min({dp(12),box.height*4/5,box.width/3})),leading=pixels*5/4;
            auto message=std::string_view(status);
            auto page=wr::paginate_document(message,{},0,box.width,box.height,pixels,leading);
            if(page.end<message.size()){
                message="加载中";page=wr::paginate_document(message,{},0,box.width,box.height,pixels,leading);
            }
            const int top=box.y+(box.height-int(page.count)*leading)/2;
            for(std::size_t i=0;i<page.count;++i){const auto& line=page.lines[i];
                text({box.x,top+int(i)*leading,box.width,leading},message.substr(line.start,line.end-line.start),pixels,ink(),1);}
        }else if(same_scene&&!presented_wait){dirty=false;return;}
        std::array<CanvasRegion,2> damage{};std::size_t count=0;
        if(same_scene){
            if(presented_wait)damage[count++]=*presented_wait;
            if(hint&&hint!=presented_wait)damage[count++]=*hint;
        }
        const bool clearing=presented_wait.has_value()&&!pending;
        if(auto result=canvas.present(draw,std::span(damage).first(count));result){
            scene_valid=true;scene_screen=screen;scene_night=night;presented_wait=hint;
            presented_wait_text=pending?status:std::string{};dirty=false;
            if(pending)log("WEREAD WAIT PRESENTED",{{"partial",same_scene},{"regions",count}});
            if(clearing)log("WEREAD WAIT CLEARED",{{"partial",same_scene},{"regions",count}});
        }else{scene_valid=false;dirty=false;log("WEREAD DRAW ERROR",{{"code",int(result.error())}});}
    }
};
void ReadingPointer::operator()(const CanvasPointer& e)const{app->pointer_event(e);}
PXA_APPLICATION(WeReadApp)
