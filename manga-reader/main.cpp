#include "sources.hpp"
#include "jpeg_compat.hpp"
#include "jpeg_rgb.hpp"
#include "progressive_jpeg.hpp"
#include "webp_compat.hpp"
#include "cache_model.hpp"
#include "reader_gesture.hpp"
#include "reader_work.hpp"
#include "directory_cache.hpp"
#include <cstring>
#include <cstdlib>
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/http.hpp>
#include <pxa/fs.hpp>
#include "image_source.hpp"
#include <pxa/profile_clock.hpp>
#include <pxa/ui_widgets.hpp>
#include <pxa/ui_refresh.hpp>
#include <pxa/ui_resource_pressure.hpp>
#include <cmath>
using namespace pxa;
using namespace pxa::ui;
struct Reader {
    static constexpr pxa::game::LoopOptions loop_options{16000,16,1};
    enum Screen {shelf_screen,discover,source_screen,contents,reading,help,page_jump,settings_screen};
    enum Act {shelf_tab,discover_tab,source_tab,search,previous_list,next_list,open_book,open_chapter,
        go_back,language_change,resume,collect,previous_page,next_page,zoom,retry,cancel,
        show_help,choose_source,source_previous,source_next,clear_search,read_book,jump_page,jump_submit,jump_step,toggle_order,show_settings,toggle_cache,clear_cache,reading_mode,retry_buffer,show_contents,refresh_contents};
    struct Callback {Reader* app=nullptr;Act act{};int arg=0;void operator()()const{app->enter_from_bottom=false;app->action(act,arg);}};
    struct Controls {
        static constexpr Capacity capacity{208,0,64};
        Reader* app;std::array<Callback,64> callbacks{};
        template<class Owner> bool render(Owner&,std::uint32_t);
    };
    Context* ctx=nullptr;
    DisplayMetrics metrics;
    CanvasRef canvas;CanvasCommands<1024> draw;
    State<std::uint32_t> revision{0};
    State<std::string> input{""};TextInputRef input_ref;
    State<TextInputBox> input_box{TextInputBox{{},false,Font::body,0x20382fff}};
    Screen screen=shelf_screen,prior=shelf_screen,settings_prior=shelf_screen;
    std::vector<manga::Book> shelf,results;
    std::vector<manga::Chapter> chapters,groups;
    unsigned group_index=0;
    bool descending=false;
    std::vector<std::string> pages,originals;
    manga::Book book;
    std::string custom="copymanga",query,status="正在读取书架…",failure;
    unsigned source_start=0,language=0,offset=0,total=0,list_start=0,page=0,zoom_mode=1,compatibility_pixels=1048576;
    bool input_empty=true;
    bool busy=false,dirty=true,ui_dirty=true,ready=false,save_pending=false,save_busy=false,storage_ok=true,bars=false,dragging=false,exit_pending=false;
    std::uint64_t now_us=0,bar_deadline=0,save_deadline=0,save_first=0;
    manga::TouchPair touches;manga::PinchAnchor pinch;float custom_zoom=1;
    manga::FrameWork frame_work;std::uint64_t last_input_us=0;
    bool interacting()const{return screen==reading&&(dragging||touches.count()||(last_input_us&&now_us-last_input_us<120000));}
    struct WorkYield {
        manga::FrameWork::Awaiter frame;ClockService::Yield clock;bool paced;
        explicit WorkYield(Reader& app):frame(app.frame_work.yield()),clock(app.ctx->clock().yield()),paced(app.interacting()){}
        bool await_ready()const{return false;}
        bool await_suspend(std::coroutine_handle<> handle){return paced?frame.await_suspend(handle):clock.await_suspend(handle);}
        Result<void> await_resume(){if(!paced)return clock.await_resume();if(frame.await_resume())return {};return std::unexpected(Error::resource_limit);}
    };
    WorkYield yield_work(){return WorkYield{*this};}
    float pan_x=0,pan_y=0,scale=1,down_x=0,down_y=0,last_x=0,last_y=0;
    std::optional<Asset> image;
    Permission permission;
    manga::PageCache cache;bool chapter_from_cache=false,refresh_chapter=false;unsigned rendered_page=0;bool enter_from_bottom=false;float overflow_y=0;
#include "reader_buffer.inc"
#include "reader_network.inc"
#include "reader_cache.inc"
#include "reader_directory.inc"
#include "reader_chapter.inc"
    std::array<std::byte,4096> buffer{};
    auto view(){return Stack(Canvas(canvas).on_pointer([this](const CanvasPointer& e){pointer(e);}),
        Refresh(revision,[this]{return Controls{this};}),
        TextInput(input,input_ref).box(input_box).max_bytes(1024).single_line().on_submit([this]{(void)input_ref.hide_keyboard();action(screen==page_jump?jump_submit:search);})).fill().fill_height();}
    void sync_clock(){std::uint64_t ns=0;if(profile_clock_ns(ns))now_us=std::max(now_us,ns/1000);}
    void changed(){dirty=true;ui_dirty=true;}
    void message(std::string s){status=std::move(s);changed();}
    void start(Task<void> task,bool foreground=true){auto r=(foreground?ctx->foreground_tasks():ctx->tasks()).start(std::move(task));if(!r)error(r.error());}
    void error(Error e){busy=false;if(screen==reading)bars=true;
        failure=e==Error::denied?"请求被拒绝，请检查权限或漫画源":
            e==Error::limit_exceeded?"图片超过限额，可跳过此页":
            (e==Error::resource_limit||e==Error::quota_exceeded)?"内存不足，请稍后重试或跳过":
            e==Error::protocol_error?"漫画源数据格式错误，请重试":
            e==Error::unsupported?"图片编码暂不支持，可跳过":
            e==Error::timed_out?"网络超时，请重试":
            e==Error::busy?"漫画源请求过多，请稍后重试":"网络或漫画源不可用，请重试";
        (void)ctx->log().write(LogLevel::warning,"MANGA error="+std::to_string(int(e)));message(failure);
    }
    Result<void> on_start(Context& c,std::span<const std::byte> config){ctx=&c;auto m=decode_start_display(config);if(!m)return std::unexpected(m.error());metrics=*m;
        // Grow linear memory before decoded Host images fragment PSRAM. Free
        // the block for normal allocations; Wasm keeps the mapped capacity.
        compatibility_pixels=image_pixels();auto reserve=std::malloc(std::size_t(compatibility_pixels)*3+1048576);
        if(reserve)std::free(reserve);else compatibility_pixels=std::min(compatibility_pixels,131072u);
        (void)c.log().write(LogLevel::info,"MANGA conversion_pixels="+std::to_string(compatibility_pixels));
        c.foreground_tasks().on_error(this,[](void* p,Error e) noexcept {static_cast<Reader*>(p)->error(e);});
        c.tasks().on_error(this,[](void* p,Error e) noexcept {auto a=static_cast<Reader*>(p);a->save_busy=false;a->storage_ok=false;a->save_pending=false;a->message("保存失败 ("+std::to_string(int(e))+")");});
        auto w=c.window().fullscreen(WindowBarMode::hidden,WindowBarMode::hidden);if(!w)return w;start(load_saved(),false);return {};}
    Result<bool> on_event(Context&,const Event& e){if(e.is<DisplayMetrics>()){auto m=decode_display_metrics(e);if(m){bool resized=metrics.width!=m->width||metrics.height!=m->height;metrics=*m;if(resized){touches.reset();dragging=false;fit();source_start=(source_start/source_rows())*source_rows();list_start=(list_start/row_count())*row_count();}changed();}return true;}
        if(e.is<ui::ResourcePressure>()){auto p=ui::decode_resource_pressure(e);if(p&&*p!=ui::ResourcePressure::normal){stop();memory_pressure(*p==ui::ResourcePressure::critical);}return true;}
        if(e.is<PermissionRevoked>()){stop();permission.reset();failure="网络权限已撤销";message(failure);return true;}return false;}
    void on_background(Context&){stop();(void)input_ref.hide_keyboard();dragging=false;touches.reset();save();save_deadline=0;if(save_pending&&!save_busy)start(save_state(),false);}
    void on_foreground(Context&){changed();if(ready&&screen==reading&&(!image||rendered_page!=page)&&!busy)begin(pages.empty()?load_pages():load_image());}
    BackAction on_back(){if(screen==shelf_screen){if(save_busy||save_pending){exit_pending=true;save_deadline=0;message("正在保存…");return BackAction::stay;}return BackAction::close;}action(go_back);return BackAction::stay;}
    void stop(){ctx->foreground_tasks().cancel();chapter_requested=chapter_loading=chapter_failed=false;clear_wire();pending_cache_key.clear();pending_cache_file.clear();image_finishing=false;prefetch_running=false;prefetch_page=-1;busy=false;failure.clear();changed();}
    void begin(Task<void> task){stop();busy=true;message("正在加载…");start(std::move(task));}
    unsigned image_width()const{return std::clamp(unsigned(metrics.width),320u,1024u);}
    unsigned image_pixels()const{return metrics.width>360?1048576u:262144u;}
    float density()const{return std::max(.25f,float(metrics.density_q16)/65536.f);}
    float ui_factor()const{return metrics.height>=480?std::max(1.f,std::min({density()/.78125f,metrics.width/320.f,metrics.height/240.f})):1.f;}
    CanvasRegion physical_area()const {int l=metrics.safe.left,r=metrics.safe.right,t=metrics.safe.top,b=metrics.safe.bottom;
        if(metrics.system_bars_known){l=std::max(l,int(metrics.system_bars.left));r=std::max(r,int(metrics.system_bars.right));t=std::max(t,int(metrics.system_bars.top));b=std::max(b,int(metrics.system_bars.bottom));}
        if(metrics.system_gestures_known){l=std::max(l,int(metrics.system_gestures.left));r=std::max(r,int(metrics.system_gestures.right));t=std::max(t,int(metrics.system_gestures.top));b=std::max(b,int(metrics.system_gestures.bottom));}
        if(metrics.shape==2){l=std::max(l,int(metrics.width*.15));r=std::max(r,int(metrics.width*.15));t=std::max(t,int(metrics.height*.15));b=std::max(b,int(metrics.height*.15));}
        return {l+4,t+4,std::max(1,int(metrics.width)-l-r-8),std::max(1,int(metrics.height)-t-b-8)};}
    CanvasRegion area()const {auto r=physical_area();float f=ui_factor();return {int(r.x/f),int(r.y/f),int(r.width/f),int(r.height/f)};}
    CanvasRegion viewport()const {return {0,0,int(metrics.width),int(metrics.height)};}
    CanvasRegion logical(CanvasRegion r)const {float d=density();int left=int(std::floor(r.x/d)),top=int(std::floor(r.y/d));return {left,top,std::max(1,int(std::ceil((r.x+r.width)/d))-left),std::max(1,int(std::ceil((r.y+r.height)/d))-top)};}
    unsigned list_y()const{return screen==shelf_screen?76:screen==contents?100:104;}
    bool dense_fonts()const{return density()>ui_factor()*1.08f;}
    unsigned row_height()const{return screen==contents?36:dense_fonts()?56:40;}
    unsigned columns()const{return (screen==shelf_screen||screen==discover)&&area().width>=480?2:1;}
    unsigned row_count()const {auto a=area();return std::clamp((a.height-int(list_y())-30)/int(row_height()),1,12)*columns();}
    unsigned source_cell()const{return dense_fonts()?56:52;}
    unsigned source_rows()const{return std::clamp((area().height-(dense_fonts()?70:76))/int(source_cell()),1,8);}
    unsigned batch_size()const {auto source=manga::source_info(custom);return screen==discover&&source?source->batch:20;}
    bool remote_list()const{return screen==discover||(screen==contents&&(book.source.empty()||book.source=="copymanga"));}
    std::string active_source()const{return std::string(manga::source_name(screen==contents?book.source:custom));}
    void fit(){pan_x=pan_y=0;if(!image)return;auto v=viewport();auto d=image->descriptor();
        float width=float(v.width)/d.width,height=float(v.height)/d.height;
        scale=zoom_mode==0?std::min(width,height):width*(zoom_mode==3?custom_zoom:zoom_mode==2?2.f:1.f);clamp();dirty=true;}
    void clamp(){if(!image)return;auto v=viewport();auto d=image->descriptor();pan_x=std::clamp(pan_x,0.f,std::max(0.f,d.width*scale-v.width));
        float low=0,high=d.height*scale-v.height;if(vertical()){const float z=strip_zoom();if(page){auto p=buffered_slot(page-1);if(p>=0)low=-strip_height(*buffered[p].asset)*z;}for(unsigned i=page+1;i<=strip_end();++i)high+=strip_height(*buffered[buffered_slot(i)].asset)*z;
            // A few tiny neighbors may not fill the viewport under pressure.
            // Reach the next loaded panel so the old large image can retire.
            if(buffered_slot(page+1)>=0&&strip_end()+1<pages.size())high=std::max(high,d.height*scale);}
        pan_y=std::clamp(pan_y,low,std::max(0.f,high));}
    void pointer(const CanvasPointer& e){if(screen!=reading||chapter_loading)return;sync_clock();last_input_us=now_us;const float x=e.x*density(),y=e.y*density();
        auto before=touches.count();bool mixed=touches.mixed;if(!touches.update(e.pointer_id,unsigned(e.phase),x,y))return;
        if(touches.count()==2&&image&&!busy){auto d=image->descriptor();auto v=viewport();float content_h=vertical()?strip_source_height():d.height;
            if(before<2)pinch.start(touches,scale,pan_x,pan_y,v.width,v.height,d.width,content_h);
            pinch.apply(touches,v.width,v.height,d.width,content_h,scale,pan_x,pan_y);custom_zoom=scale/(float(v.width)/d.width);zoom_mode=3;dragging=true;overflow_y=waiting_scroll=0;requested_page=-1;
            if(bars){bars=false;changed();}dirty=true;return;
        }
        if(before==2){normalize_strip();if(zoom_mode==3){remember_view();(void)ctx->log().write(LogLevel::info,"MANGA pinch_permille="+std::to_string(unsigned(custom_zoom*1000)));}auto p=touches.remaining();last_x=down_x=p.x;last_y=down_y=p.y;overflow_y=0;dragging=touches.count()!=0;dirty=true;if(!touches.count())touches.reset();return;}
        if(e.action()==PointerPhase::down){down_x=last_x=x;down_y=last_y=y;overflow_y=waiting_scroll=0;requested_page=-1;prefetch_retry=0;dragging=true;return;}
        if(!dragging){if(!touches.count())touches.reset();return;}
        if(e.action()==PointerPhase::move){auto old_x=pan_x,old_y=pan_y;pan_x-=x-last_x;auto wanted=pan_y+(vertical()?waiting_scroll:0)-(y-last_y);pan_y=wanted;clamp();if(vertical()){waiting_scroll=std::clamp(wanted-pan_y,-float(metrics.height),float(metrics.height));normalize_strip();if(waiting_scroll>0&&strip_end()+1==pages.size()){overflow_y+=waiting_scroll;if(overflow_y>=40&&!chapter_last)chapter_requested=true;waiting_scroll=0;}else if(waiting_scroll<0&&!page)waiting_scroll=0;else if(y>last_y)overflow_y=0;queue_strip_edge();}dirty|=int(old_x)!=int(pan_x)||int(old_y)!=int(pan_y);
            if(bars&&(std::abs(x-down_x)>10||std::abs(y-down_y)>10)){bars=false;changed();}}
        if(e.action()==PointerPhase::up){dragging=false;dirty=true;float dx=x-down_x,dy=y-down_y;
            if(!mixed){if(std::abs(dx)<10&&std::abs(dy)<10){bars=!bars;bar_deadline=now_us+6000000;changed();}
                else if(!vertical()&&!zoomed()&&std::abs(dx)>35&&std::abs(dx)>std::abs(dy))action(dx<0?next_page:previous_page);}
        }
        if(e.action()==PointerPhase::cancel){dragging=false;dirty=true;}last_x=x;last_y=y;if(!touches.count())touches.reset();
    }
    const char* lang()const{return language==0?"zh":language==1?"zh-hk":"en";}
    std::string api(std::string path)const {
#ifdef MANGA_API_BASE
        return std::string(MANGA_API_BASE)+path;
#else
        return "https://api.mangadex.org"+path;
#endif
    }
    Task<HttpResponse> request(std::string url,unsigned limit,std::string referrer={}){if(!manga::web(url))co_return std::unexpected(Error::invalid_argument);
        if(!permission){auto p=co_await ctx->permissions().acquire("net.client","web");if(!p)co_return std::unexpected(p.error());permission=std::move(*p);}
        const bool copy_request=url.starts_with("https://api.copy4000.com/");
        const bool copy_browser=copy_request||url.starts_with("https://www.mangacopy.com/")||referrer=="https://www.mangacopy.com/";
        const bool chinese_browser=url.starts_with("https://www.guazimanhua.com/")||url.starts_with("https://www.woimh.com/")||referrer=="https://www.guazimanhua.com/"||referrer=="https://www.woimh.com/";
        const bool site_browser=url.starts_with("https://mangapill.com/")||url.starts_with("https://www.mangatown.com/")||referrer=="https://mangapill.com/"||referrer=="https://www.mangatown.com/";
        const std::array<NetHeaderView,5> fields{{{"Accept-Encoding","identity"},
            {"User-Agent",(copy_browser||site_browser||chinese_browser)?"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36 PXA-Manga/0.7":"PXA-Manga/0.7"},
            {"Referer",referrer.empty()?"https://www.mangacopy.com/":std::string_view(referrer)},
            {"platform","1"},{"version","2026.10.16"}}};
        const auto headers=std::span{fields}.first(copy_request?5:referrer.empty()?2:3);
        static constexpr std::array<std::string_view,3> wanted{{"content-type","location","x-wp-total"}};
        for(int n=0;n<4;++n){auto ex=co_await ctx->http().open(HttpRequest{.url=url,.headers=headers,.wanted_headers=wanted,.options={.buffer_bytes=limit>524288?16384u:4096u,.header_bytes=1024,.max_body_bytes=limit,.timeout_ms=limit>524288?60000u:25000u}},permission);
            if(!ex)co_return std::unexpected(ex.error());auto r=co_await std::move(*ex).response();if(!r)co_return std::unexpected(r.error());
            if(r->status_code>=300&&r->status_code<400){auto loc=r->headers.first("location");if(!loc)co_return std::unexpected(Error::protocol_error);auto next=manga::resolve(url,*loc);if(!manga::web(next)||(url.starts_with("https:")&&!next.starts_with("https:")))co_return std::unexpected(Error::denied);url=std::move(next);continue;}
            if(r->status_code!=200)co_return std::unexpected(r->status_code==429?Error::busy:r->status_code==403?Error::denied:Error::unavailable);co_return std::move(*r);
        }co_return std::unexpected(Error::protocol_error);}
    static bool transient(Error e){return e==Error::io_error||e==Error::timed_out||e==Error::unavailable;}
    Task<std::string> json(std::string url,bool validate=true,unsigned maximum=98304,unsigned* count=nullptr){
        for(unsigned attempt=0;attempt<2;++attempt){
            auto r=co_await request(url,maximum);if(!r){if(!attempt&&transient(r.error()))continue;co_return std::unexpected(r.error());}
            if(count){auto value=r->headers.first("x-wp-total");*count=value?manga::Json(*value).number():0;}
            std::string out;std::optional<Error> failed;
            for(;;){auto n=co_await r->body.read_some(buffer);if(!n){failed=n.error();break;}if(!*n)break;
                if(out.size()+*n>maximum)co_return std::unexpected(Error::limit_exceeded);out.append(reinterpret_cast<const char*>(buffer.data()),*n);}
            if(failed){if(!attempt&&transient(*failed))continue;co_return std::unexpected(*failed);}
            if(validate&&!manga::Json(out).valid())co_return std::unexpected(Error::protocol_error);co_return out;
        }co_return std::unexpected(Error::io_error);
    }
    Task<void> load_catalog(){
        using enum manga::SourceKind;auto kind=manga::source_kind(custom);
        if(manga::chinese_source(kind)){
            auto number=std::to_string(offset/batch_size()+1);std::string url;
            if(kind==guazi)url=std::string(manga::guazi_base)+"/category.php?sort=hits&page="+number+(query.empty()?"":"&keyword="+manga::encoded(query));
            else url=std::string(manga::woi_base)+(query.empty()?"/booklist?page="+number:"/search?keyword="+manga::encoded(query));
            auto text=co_await json(url,false,524288);if(!text)co_return std::unexpected(text.error());bool more=false;
            if(!manga::chinese_books(*text,kind,results,more))co_return std::unexpected(Error::protocol_error);
            total=offset+batch_size()+(more?1:0);list_start=0;busy=false;message(results.empty()?"没有找到漫画":"中文漫画");co_return Result<void>{};
        }
        if(kind==pill||kind==town){
            auto number=std::to_string(offset/batch_size()+1);std::string url;
            if(kind==pill)url=std::string(manga::pill_base)+"/search?page="+number+(query.empty()?"&type=manga":"&q="+manga::encoded(query));
            else url=std::string(manga::town_base)+(query.empty()?"/directory/0-0-0-0-0-0/"+number+".htm":"/search?page="+number+"&name="+manga::encoded(query)+"&name_method=cw");
            auto text=co_await json(url,false,524288);if(!text)co_return std::unexpected(text.error());bool more=false;
            if(!manga::site_books(*text,kind,results,more))co_return std::unexpected(Error::protocol_error);
            total=offset+batch_size()+(more?1:0);list_start=0;busy=false;message(results.empty()?"没有找到漫画":"英文漫画");co_return Result<void>{};
        }
        if(kind==katana){
            auto number=std::to_string(offset/20+1);auto url=std::string(manga::katana_base)+(query.empty()?"/manga/page/"+number+"?order=popular":"/page/"+number+"?search="+manga::encoded(query)+"&search_by=book_name");
            auto text=co_await json(url,false,262144);if(!text)co_return std::unexpected(text.error());bool more=false;
            if(!manga::katana_books(*text,results,more))co_return std::unexpected(Error::protocol_error);
            total=offset+batch_size()+(more?1:0);list_start=0;busy=false;message(results.empty()?"没有找到漫画":"英文漫画");co_return Result<void>{};
        }
        std::string url;
        if(kind==mangadex)url=api("/manga?limit=12&offset="+std::to_string(offset)+"&contentRating[]=safe&availableTranslatedLanguage[]="+lang()+"&order[followedCount]=desc"+(query.empty()?"":"&title="+manga::encoded(query)));
        else if(kind==copy)url=std::string(manga::copy_api)+(query.empty()?"/comics?ordering=-popular&free_type=1":"/search/comic?platform=1&q_type=&q="+manga::encoded(query))+"&limit=12&offset="+std::to_string(offset);
        else co_return std::unexpected(Error::invalid_argument);
        auto text=co_await json(url);if(!text)co_return std::unexpected(text.error());manga::Json j(*text);
        if(kind==copy){if(!manga::copy_books(j,results))co_return std::unexpected(Error::protocol_error);total=j.get("results").get("total").number();}
        else{if(!manga::books(j,results,custom))co_return std::unexpected(Error::protocol_error);
            total=kind==mangadex?j.get("total").number():results.size();}
        list_start=0;busy=false;message(results.empty()?"暂无结果，试试其他搜索词":std::to_string(total)+" 部漫画");co_return Result<void>{};
    }
    Task<void> load_chapters_network(){
        using enum manga::SourceKind;auto kind=manga::source_kind(book.source);std::string url;
        if(manga::chinese_source(kind)){auto text=co_await json(book.id,false,1048576);if(!text)co_return std::unexpected(text.error());
            if(!manga::chinese_chapters(*text,book,chapters))co_return std::unexpected(Error::protocol_error);
            if(descending)std::reverse(chapters.begin(),chapters.end());total=chapters.size();list_start=0;busy=false;message(std::to_string(total)+" 个章节");co_return Result<void>{};}
        if(kind==pill||kind==town){auto text=co_await json(book.id,false,1048576);if(!text)co_return std::unexpected(text.error());
            if(!manga::site_chapters(*text,book,chapters))co_return std::unexpected(Error::protocol_error);
            if(descending)std::reverse(chapters.begin(),chapters.end());total=chapters.size();list_start=0;busy=false;message(std::to_string(total)+" 个章节");co_return Result<void>{};}
        if(kind==katana){auto text=co_await json(book.id,false,524288);if(!text)co_return std::unexpected(text.error());
            if(!manga::katana_chapters(*text,book.id,chapters))co_return std::unexpected(Error::protocol_error);
            if(descending)std::reverse(chapters.begin(),chapters.end());total=chapters.size();list_start=0;busy=false;message(std::to_string(total)+" 个章节");co_return Result<void>{};}
        if(kind==mangadex)url=api("/manga/"+book.id+"/feed?limit=20&offset="+std::to_string(offset)+"&translatedLanguage[]="+lang()+"&order[chapter]="+std::string(descending?"desc":"asc")+"&includeExternalUrl=0&contentRating[]=safe");
        else if(kind==copy){
            if(groups.empty()){
                auto details=co_await json(std::string(manga::copy_api)+"/comic2/"+manga::encoded(book.id)+"?platform=1");if(!details)co_return std::unexpected(details.error());
                manga::Json j(*details);if(j.get("code").number()!=200)co_return std::unexpected(Error::unavailable);
                bool ok=j.get("results").get("groups").members([&](const std::string& key,manga::Json g){auto id=g.get("path_word").string(192),name=g.get("name").string(192);if(id&&name&&manga::slug(*id))groups.push_back({*id,*name});return true;});
                if(!ok||groups.empty())co_return std::unexpected(Error::not_found);group_index=0;
                for(unsigned i=0;i<groups.size();++i)if(groups[i].id=="default")group_index=i;
            }
            auto base=std::string(manga::copy_api)+"/comic/"+manga::encoded(book.id)+"/group/"+manga::encoded(groups[group_index].id)+"/chapters?";
            if(descending&&!total){auto probe=co_await json(base+"limit=1&offset=0");if(!probe)co_return std::unexpected(probe.error());total=manga::Json(*probe).get("results").get("total").number();}
            auto count=descending?std::min(20u,total>offset?total-offset:0u):20u;
            auto position=descending?(total>offset+count?total-offset-count:0u):offset;
            url=base+"limit="+std::to_string(count)+"&offset="+std::to_string(position);
        }else co_return std::unexpected(Error::invalid_argument);
        auto text=co_await json(url);if(!text)co_return std::unexpected(text.error());manga::Json j(*text);
        if(kind==copy){if(!manga::copy_chapters(j,chapters,book.id))co_return std::unexpected(Error::protocol_error);total=j.get("results").get("total").number();if(descending)std::reverse(chapters.begin(),chapters.end());}
        else{if(!manga::chapters(j,chapters,kind==mangadex,url))co_return std::unexpected(Error::protocol_error);total=kind==mangadex?j.get("total").number():chapters.size();}
        list_start=0;busy=false;message(chapters.empty()?"暂无章节，可切换分组或语言":std::to_string(total)+" 个章节");co_return Result<void>{};
    }
    Task<void> load_pages(bool foreground=true){
        clear_buffers();prefetch_retry=0;
        auto cached=co_await chapter_cache();if(cached&&*cached){page=std::min<unsigned>(book.page,pages.size()-1);co_return co_await load_image(foreground);}chapter_from_cache=false;
        using enum manga::SourceKind;auto kind=manga::source_kind(book.source);
        auto url=kind==mangadex?api("/at-home/server/"+book.chapter):book.chapter;
        auto text=co_await json(url,kind==mangadex,kind==copy?196608:(kind==katana||kind==pill||kind==town||manga::chinese_source(kind))?262144:98304);if(!text)co_return std::unexpected(text.error());
        bool ok=kind==copy?manga::copy_pages(*text,pages):kind==katana?manga::katana_pages(*text,pages):kind==pill?manga::pill_pages(*text,pages):kind==town?manga::town_pages(*text,book.chapter,pages):manga::chinese_source(kind)?manga::chinese_pages(*text,kind,pages):manga::pages(manga::Json(*text),pages,true,url);
        if(!ok)co_return std::unexpected(Error::protocol_error);
        originals.clear();if(kind==town){originals.resize(pages.size());auto first=manga::town_image(*text);if(!first)co_return std::unexpected(Error::protocol_error);originals[0]=std::move(*first);}if(kind==mangadex)manga::pages(manga::Json(*text),originals,true,url,true);
        auto stored=co_await store_chapter();(void)stored;page=std::min<unsigned>(book.page,pages.size()-1);co_return co_await load_image(foreground);
    }
    Task<void> load_image(bool foreground=true){auto started=now_us;
        if(cache_prepare){cache_prepare=false;auto r=co_await reserve_cache_space();(void)r;}
        Result<Asset> a=std::unexpected(Error::not_found);auto resident=take_buffered(page);
        if(resident){a=std::move(*resident);(void)ctx->log().write(LogLevel::info,"MANGA buffer=hit");}else a=co_await fetch_page(page,foreground);
        if(!a)co_return std::unexpected(a.error());sync_clock();if(image&&rendered_page!=page&&!chapter_loading)remember(rendered_page,std::move(*image));image.emplace(std::move(*a));if(chapter_loading){chapter_committed=true;chapter_loading=false;chapter_last=false;}touches.reset();dragging=false;rendered_page=page;failure.clear();prefetch_retry=0;if(bars)bar_deadline=now_us+6000000;progress();fit();if(enter_from_bottom){pan_y=std::max(0.f,image->descriptor().height*scale-metrics.height);enter_from_bottom=false;}
        (void)ctx->log().write(LogLevel::info,"MANGA page_ready_us="+std::to_string(now_us-started));image_finishing=true;busy=false;changed();
        // Display first; keep the next producer away from this scratch file
        // until its optional persistent cache entry has been committed.
        auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());auto stored=co_await finish_image_cache();(void)stored;
        auto r=co_await ctx->fs().remove("manga-source.tmp");(void)r;r=co_await ctx->fs().remove("manga-image.tmp");(void)r;image_finishing=false;co_return Result<void>{};}
    Task<Asset> convert_download(bool foreground=true){auto started=now_us;std::uint64_t work_us=0;unsigned max_slice_us=0;
        auto batch=[&](auto done,auto step){auto tick=profile_short_tick();bool ok=true;for(unsigned n=0;n<64&&!done();++n){if(!step()){ok=false;break;}if(profile_short_us(tick,profile_short_tick())>=(foreground?6000u:interacting()?1000u:4000u))break;}auto us=profile_short_us(tick,profile_short_tick());work_us+=us;max_slice_us=std::max(max_slice_us,us);return ok;};
        if(foreground){
            clear_buffers();release_page();message("正在处理图片…");
            // Let the compositor retire its old image snapshot before the
            // Guest grows a contiguous RGB decode buffer on fragmented PSRAM.
            auto tick=profile_short_tick();for(unsigned pass=0;pass<256;++pass){auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());if(profile_short_us(tick,profile_short_tick())>=20000)break;}
        }manga::ImageSource source;if(!source_bytes.empty())source.memory=source_bytes;else{auto opened=co_await ctx->fs().open("manga-source.tmp");if(!opened)co_return std::unexpected(opened.error());source.file.emplace(std::move(*opened));}auto file=&source;
        auto converter=std::make_unique<manga::JpegCompat>();converter->max_width=image_width();converter->max_pixels=compatibility_pixels;
        auto read=[](void* p,std::span<std::byte> out)->std::size_t{auto n=static_cast<manga::ImageSource*>(p)->read(out);return n?*n:0;};
        std::unique_ptr<manga::ProgressiveJpeg> progressive;std::unique_ptr<manga::WebpCompat> webp;bool jpeg_output=true;
        if(!converter->begin(read,&*file)){
            auto rewound=co_await file->seek(0);if(!rewound)co_return std::unexpected(rewound.error());
            std::array<std::byte,12> signature{};auto n=file->read(signature);if(!n)co_return std::unexpected(n.error());
            bool is_webp=*n==12&&std::memcmp(signature.data(),"RIFF",4)==0&&std::memcmp(signature.data()+8,"WEBP",4)==0;
            rewound=co_await file->seek(0);if(!rewound)co_return std::unexpected(rewound.error());
            if(is_webp){
                webp=std::make_unique<manga::WebpCompat>();webp->max_width=image_width();webp->max_pixels=compatibility_pixels;if(!webp->begin(read,&*file))co_return std::unexpected(webp->limited?Error::limit_exceeded:Error::unsupported);
                while(!webp->done()){if(!batch([&]{return webp->done();},[&]{return webp->next();}))co_return std::unexpected(webp->limited?Error::limit_exceeded:Error::protocol_error);auto y=co_await yield_work();if(!y)co_return std::unexpected(y.error());}
                converter=std::make_unique<manga::JpegCompat>();converter->max_width=image_width();converter->max_pixels=compatibility_pixels;if(!converter->begin_rgb(webp->pixels,webp->width,webp->height,3))co_return std::unexpected(Error::limit_exceeded);
            }else{
                manga::JpegHeader header;if(!manga::jpeg_header(read,&*file,header))co_return std::unexpected(Error::unsupported);
                if(std::uint64_t(header.width)*header.height>1048576)co_return std::unexpected(Error::limit_exceeded);
                rewound=co_await file->seek(0);if(!rewound)co_return std::unexpected(rewound.error());
                progressive=std::make_unique<manga::ProgressiveJpeg>();
                if(!progressive->load(read,&*file))co_return std::unexpected(progressive->limited?Error::limit_exceeded:Error::unsupported);
                converter=std::make_unique<manga::JpegCompat>();converter->max_width=image_width();converter->max_pixels=compatibility_pixels;
                if(!converter->begin_rgb(progressive->pixels,progressive->width,progressive->height,progressive->channels))co_return std::unexpected(Error::limit_exceeded);
            }
        }
        while(!converter->done()){if(!batch([&]{return converter->done();},[&]{return converter->step();}))co_return std::unexpected(Error::protocol_error);auto y=co_await yield_work();if(!y)co_return std::unexpected(y.error());}
        if(webp){jpeg_output=webp->lossy();webp->release_decoder();}if(!converter->borrowed){progressive.reset();webp.reset();}
        auto decode_us=work_us;std::vector<std::uint8_t> png;
        if(jpeg_output){auto encoder=std::make_unique<manga::JpegRgb>();
            for(unsigned quality:{95u,90u,85u}){if(!encoder->begin(converter->rgb_data(),converter->width,converter->height,quality))co_return std::unexpected(Error::resource_limit);
                while(!encoder->done()){if(!batch([&]{return encoder->done();},[&]{return encoder->step();}))break;auto y=co_await yield_work();if(!y)co_return std::unexpected(y.error());}
                png=encoder->take();if(!png.empty())break;}
        }
        if(png.empty()){jpeg_output=false;
        for(unsigned attempt=0;attempt<5;++attempt){if(!converter->begin_png())co_return std::unexpected(Error::resource_limit);
            while(!converter->png_done()){if(!batch([&]{return converter->png_done();},[&]{return converter->next_png();}))break;auto y=co_await yield_work();if(!y)co_return std::unexpected(y.error());}
            png=converter->png_result();if(!png.empty())break;if(!converter->encoder->limited||!converter->shrink())co_return std::unexpected(Error::limit_exceeded);}
        }
        if(png.empty())co_return std::unexpected(Error::limit_exceeded);converted_encoded_bytes=png.size();
        sync_clock();(void)ctx->log().write(LogLevel::info,"MANGA converted_rgb="+std::to_string(converter->width)+"x"+std::to_string(converter->height)+" encoded_bytes="+std::to_string(png.size())+" jpeg="+std::to_string(jpeg_output)+" channel_bits="+std::to_string(converter->png_precision)+" work_us="+std::to_string(work_us)+" decode_us="+std::to_string(decode_us)+" encode_us="+std::to_string(work_us-decode_us)+" max_slice_us="+std::to_string(max_slice_us)+" elapsed_us="+std::to_string(now_us-started));
        converter.reset();progressive.reset();webp.reset();std::vector<std::byte>{}.swap(source_bytes);auto closed=file->close();if(!closed)co_return std::unexpected(closed.error());
        auto removed=co_await ctx->fs().remove("manga-source.tmp");if(!removed&&removed.error()!=Error::not_found)co_return std::unexpected(removed.error());
        auto output=co_await ctx->fs().open("manga-image.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!output)co_return std::unexpected(output.error());
        auto written=co_await write_chunks(*output,std::as_bytes(std::span{png}));if(!written)co_return std::unexpected(written.error());
        std::vector<std::uint8_t>{}.swap(png);closed=output->close();if(!closed)co_return std::unexpected(closed.error());
        co_return co_await decode_file("manga-image.tmp",foreground,ImageFormats::png|ImageFormats::jpeg);
    }
    std::string shelf_status()const{return shelf.empty()?"书架为空，去发现漫画":std::to_string(shelf.size())+" 部漫画 · "+(!storage_ok?"未保存":save_busy||save_pending?"保存中":"已保存");}
    void save(){if(ready&&storage_ok){if(!save_pending)save_first=now_us;save_pending=true;save_deadline=screen==reading?std::min(now_us+350000,save_first+1500000):0;}}
    Task<void> save_state(){save_busy=true;
        if(save_pending){save_pending=false;auto text=manga::save(shelf,language,custom);if(text.size()>32768)co_return std::unexpected(Error::limit_exceeded);
            auto f=co_await ctx->fs().open("manga-state.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!f)co_return std::unexpected(f.error());
            auto bytes=std::as_bytes(std::span{text.data(),text.size()});while(!bytes.empty()){auto n=f->write(bytes.first(std::min<size_t>(1024,bytes.size())));if(!n||!*n)co_return std::unexpected(n?Error::io_error:n.error());bytes=bytes.subspan(*n);auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}
            auto closed=f->close();if(!closed)co_return std::unexpected(closed.error());auto moved=co_await ctx->fs().replace("manga-state.tmp","manga-state.json");if(!moved)co_return std::unexpected(moved.error());
        }save_busy=false;(void)ctx->log().write(LogLevel::info,"MANGA progress_saved="+std::to_string(book.page));if(screen==shelf_screen)message(shelf_status());else if(screen==settings_screen&&book_settings()&&!save_pending)message("设置已保存");co_return Result<void>{};}
    Task<void> load_saved(){auto loaded_cache=co_await load_cache();(void)loaded_cache;loaded_cache=co_await load_directory_cache();(void)loaded_cache;auto f=co_await ctx->fs().open("manga-state.json");
        if(f){std::string text;for(;;){auto n=f->read(buffer);if(!n||text.size()+*n>32768){storage_ok=false;break;}if(!*n)break;text.append(reinterpret_cast<const char*>(buffer.data()),*n);}auto c=f->close();
            if(!c||!storage_ok||!manga::restore(text,shelf,language,custom,cache.mode)){storage_ok=false;message("书架记录损坏，已保留原文件");}else if(manga::Json(text).get("version").number()<4)save_pending=true;}
        else if(f.error()!=Error::not_found){storage_ok=false;message("无法读取书架，进度暂不保存");}
        auto cleaned=co_await ctx->fs().remove("manga-source.tmp");(void)cleaned;cleaned=co_await ctx->fs().remove("manga-image.tmp");(void)cleaned;
        ready=true;if(storage_ok)message(shelf_status());co_return Result<void>{};}
    void action(Act act,int arg=0){if(!ready)return;sync_clock();if(screen==reading)bar_deadline=now_us+6000000;
        if(act==retry_buffer){prefetch_retry=0;if(arg<0&&page)requested_page=page-1;else if(strip_notice==StripNotice::failed&&strip_end()+1<pages.size())requested_page=strip_end()+1;changed();return;}
        if(act==cancel){enter_from_bottom=false;stop();if(image){page=rendered_page;message(page_label());}else{bars=true;message("已取消，可重试");failure="已取消";}return;}
        if(act==show_settings){settings_prior=screen;stop();screen=settings_screen;message(cache.limit?"自动管理最近阅读过的页面":"空间不足或不可用，暂不新增缓存");changed();return;}
        if(act==toggle_cache){cache.enabled=!cache.enabled;cache_prepare=cache.enabled&&!cache.limit;begin(save_cache_settings());return;}
        if(act==reading_mode){if(!book_settings())return;book.reading_mode=(book.reading_mode+1)%3;zoom_mode=1;custom_zoom=1;fit();remember_view();message("正在保存…");return;}
        if(act==clear_cache){begin(clear_cache_files());return;}
        if(act==show_help){prior=screen;stop();screen=help;changed();return;}
        if(act==show_contents){if(screen!=reading)return;stop();contents_from_reader=true;screen=contents;
            if(chapters.empty()){list_start=offset=0;begin(load_chapters());}else message(std::to_string(total)+" 个章节");return;}
        if(act==refresh_contents){if(screen==contents&&!busy)begin(load_chapters(true));return;}
        if(act==go_back){if(screen==settings_screen){stop();screen=settings_prior;bars=true;bar_deadline=now_us+6000000;message(screen==reading?page_label():shelf_status());if(screen==reading&&(!image||rendered_page!=page))begin(pages.empty()?load_pages():load_image());return;}if(screen==page_jump){screen=reading;input.set("");(void)input_ref.hide_keyboard();bar_deadline=now_us+6000000;message("第 "+std::to_string(page+1)+" / "+std::to_string(pages.size())+" 页");return;}if(screen==help){screen=prior;bar_deadline=now_us+6000000;changed();if(screen==reading&&(!image||rendered_page!=page))begin(pages.empty()?load_pages():load_image());return;}
            if(screen==reading){action(show_contents);return;}
            if(screen==contents&&contents_from_reader){stop();contents_from_reader=false;if(language!=book.language){language=book.language;chapters.clear();groups.clear();group_index=total=list_start=offset=0;}screen=reading;bars=true;bar_deadline=now_us+6000000;message(page_label());if(!image||rendered_page!=page)begin(pages.empty()?load_pages():load_image());return;}
            stop();clear_buffers();image.reset();pages.clear();originals.clear();screen=shelf_screen;list_start=0;message(shelf_status());return;}
        if(act==source_previous||act==source_next){auto rows=source_rows();if(act==source_previous)source_start-=std::min(source_start,rows);else if(source_start+rows<manga::sources.size())source_start+=rows;changed();return;}
        if(act==choose_source){if(arg<0||unsigned(arg)>=manga::sources.size())return;custom=manga::sources[arg].id;save();action(discover_tab);return;}
        if(act==toggle_order){if(busy||screen!=contents)return;descending=!descending;offset=list_start=0;begin(load_chapters());return;}
        if(act==jump_page){if(busy||pages.empty())return;screen=page_jump;input.set(std::to_string(page+1));message("共 "+std::to_string(pages.size())+" 页");return;}
        if(act==jump_step){int value=int(manga::comic_number(input.get()));value=std::clamp(value+(arg<0?-1:1),1,int(pages.size()));input.set(std::to_string(value));message("共 "+std::to_string(pages.size())+" 页");return;}
        if(act==jump_submit){auto value=manga::comic_number(input.get());if(!value||value>pages.size()){message("请输入 1–"+std::to_string(pages.size())+" 之间的页码");return;}screen=reading;input.set("");(void)input_ref.hide_keyboard();bars=true;bar_deadline=now_us+6000000;if(value-1!=page){page=value-1;begin(load_image());}else if(!image||rendered_page!=page)begin(load_image());else message(page_label());return;}
        if(act==clear_search){input.set("");query.clear();offset=0;begin(load_catalog());return;}
        if(act==shelf_tab||act==discover_tab||act==source_tab){stop();clear_buffers();image.reset();pages.clear();originals.clear();contents_from_reader=false;list_start=offset=0;
            screen=act==shelf_tab?shelf_screen:act==discover_tab?discover:source_screen;query.clear();input.set("");
            if(screen==source_screen){for(unsigned i=0;i<manga::sources.size();++i)if(manga::sources[i].id==custom)source_start=(i/source_rows())*source_rows();}
            if(screen==discover)begin(load_catalog());else message(screen==source_screen?"当前源："+std::string(manga::source_name(custom)):shelf_status());return;}
        if(act==search){if(busy)return;query=input.get();offset=0;begin(load_catalog());return;}
        if(act==language_change){if(busy)return;if(screen==contents&&book.source=="copymanga"){if(groups.size()>1){group_index=(group_index+1)%groups.size();offset=total=0;begin(load_chapters());}return;}if((screen==discover&&!custom.empty())||(screen==contents&&!book.source.empty()))return;language=(language+1)%3;offset=0;save();if(screen==contents)begin(load_chapters());else begin(load_catalog());return;}
        if(act==retry){if(busy)return;if(chapter_failed){next_chapter();return;}failure.clear();if(screen==discover)begin(load_catalog());else if(screen==contents)begin(load_chapters());else if(screen==reading){if(chapter_from_cache){refresh_chapter=true;pages.clear();}begin(pages.empty()?load_pages():load_image());}return;}
        if(busy)return;
        if(act==open_book||act==read_book){auto& list=screen==discover?results:shelf;if(arg<0||unsigned(arg)>=list.size())return;book=list[arg];auto saved=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});if(saved!=shelf.end())book=*saved;
            chapter_last=false;restore_view();language=book.chapter.empty()?language:book.language;clear_buffers();image.reset();pages.clear();originals.clear();contents_from_reader=false;chapters.clear();groups.clear();group_index=0;total=0;list_start=offset=0;
            if(act==read_book){screen=reading;bars=false;if(book.chapter.empty()){descending=false;begin(read_first_chapter());}else begin(load_pages());}else{screen=contents;begin(load_chapters());}return;}
        if(act==collect){auto p=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});if(p==shelf.end()){if(shelf.size()>=24){message("书架已满（24 部）");return;}shelf.push_back(book);message("已加入书架");}else{shelf.erase(p);message("已从书架移除");}save();return;}
        if(act==previous_list||act==next_list){auto count=screen==shelf_screen?shelf.size():screen==discover?results.size():chapters.size();auto rows=row_count();
            if(act==next_list){if(list_start+rows<count)list_start+=rows;else if(remote_list()&&offset+batch_size()<total){offset+=batch_size();begin(screen==discover?load_catalog():load_chapters());return;}}
            else if(list_start)list_start-=std::min(list_start,rows);else if(offset){offset-=std::min(offset,batch_size());begin(screen==discover?load_catalog():load_chapters());return;}changed();return;}
        if(act==open_chapter||act==resume){if(act==resume&&contents_from_reader){action(go_back);return;}if(act==open_chapter){if(arg<0||unsigned(arg)>=chapters.size())return;chapter_last=false;book.chapter=chapters[arg].id;book.chapter_title=chapters[arg].title;book.page=0;}if(book.chapter.empty())return;if(act==resume)language=book.language;
            contents_from_reader=false;
            screen=reading;bars=false;restore_view();pages.clear();begin(load_pages());return;}
        if(act==zoom){zoom_mode=zoom_mode>=2?0:zoom_mode+1;touches.reset();fit();remember_view();changed();return;}
        if(act==previous_page||act==next_page){if(pages.empty())return;if(act==next_page&&page+1==pages.size()&&!chapter_last){next_chapter();return;}if((act==previous_page&&page==0)||(act==next_page&&page+1==pages.size())){enter_from_bottom=false;message(act==next_page?"没有下一章":"已是本章第一页");bars=true;return;}
            page+=act==next_page?1:-1;begin(load_image());return;}
    }
    void on_frame(Context&,pxa::game::FrameTick tick){if(input_empty!=input.get().empty()){input_empty=input.get().empty();ui_dirty=true;}now_us=std::max(now_us,tick.timestamp_us);if(screen==reading&&bars&&!busy&&failure.empty()&&now_us>=bar_deadline){bars=false;changed();}if(save_pending&&!save_busy&&now_us>=save_deadline&&!interacting())start(save_state(),false);
        if(chapter_requested&&!busy){chapter_requested=false;next_chapter();}
        if(needs_prefetch()){prefetch_running=true;start(prefetch_pages());}
        auto notice=strip_notice_state();if(notice!=strip_notice){strip_notice=notice;changed();}
        notice=strip_notice_state(true);if(notice!=previous_notice){previous_notice=notice;changed();}
        if(exit_pending&&!save_busy&&!save_pending){exit_pending=false;(void)ctx->window().request_back();}
        if(ui_dirty){ui_dirty=false;revision.update([](auto n){return n+1;});auto a=area();float d=density()/ui_factor();bool edit=screen==discover||screen==page_jump;
            input_box.set(TextInputBox{{int((a.x+8)/d),int((a.y+40)/d),int(std::max(1,a.width-16-(screen==discover?52:screen==page_jump?80:0))/d),int(28/d)},edit,Font::label,0x20382fff});}
        if(chapter_loading||!dirty||!canvas.mounted()){frame_work.advance();return;}dirty=false;draw.reset();auto background=logical(viewport());draw.rect(background.x,background.y,background.width,background.height,screen==reading?0x141916ff:0xf6f5f0ff);
        
        if(screen==reading&&image){auto v=viewport();auto d=image->descriptor();int w=std::max(1,int(d.width*scale)),h=std::max(1,int(d.height*scale));
            draw.clip(logical(v));bool strip=vertical();int xx=v.x+std::max(0,(v.width-w)/2)-int(pan_x),yy=(strip?0:std::max(0,(v.height-h)/2))-int(pan_y);
            draw.image(logical({xx,v.y+yy,w,h}),*image,ImageFit::stretch,255,dragging?ImageSampling::nearest:ImageSampling::smooth);
            if(strip){const float z=strip_zoom();if(page&&yy>0){auto p=buffered_slot(page-1);if(p>=0){auto& asset=*buffered[p].asset;int ph=std::max(1,int(strip_height(asset)*z));draw.image(logical({xx,yy-ph,w,ph}),asset,ImageFit::stretch,255,ImageSampling::nearest);}}
                yy+=h;for(unsigned i=page+1;i<=strip_end()&&yy<v.height;++i){auto& asset=*buffered[buffered_slot(i)].asset;int ph=std::max(1,int(strip_height(asset)*z));draw.image(logical({xx,yy,w,ph}),asset,ImageFit::stretch,255,dragging?ImageSampling::nearest:ImageSampling::smooth);yy+=ph;}}
            draw.pop_clip();}
        const bool source_arrows=screen==source_screen;
        const auto list_size=screen==shelf_screen?shelf.size():screen==discover?results.size():screen==contents?chapters.size():0;
        const bool list_arrows=(screen==shelf_screen||screen==discover||screen==contents)&&!busy&&failure.empty()&&list_size;
        if((screen==reading&&(bars||!failure.empty()))||source_arrows||list_arrows){
            const auto a=area();const float unit=ui_factor()/density();
            const bool reader=screen==reading;const int button_w=reader?38:36,button_h=reader?30:28;
            const int top=int((a.y+a.height-button_h)*unit),height=std::max(1,int(button_h*unit)),width=std::max(1,int(button_w*unit));
            unsigned start=source_arrows?std::min<unsigned>(source_start,((manga::sources.size()-1)/source_rows())*source_rows()):list_start;
            for(unsigned side=0;side<2;++side){
                const int left=int((side?a.x+a.width-button_w:a.x)*unit),cx=left+width/2,cy=top+height/2;
                const int dx=std::max(2,int(std::round(4*unit))),dy=std::max(3,int(std::round(7*unit))),direction=side?1:-1;
                bool enabled=reader?(side?!pages.empty()&&(page+1<pages.size()||!chapter_last):page>0):source_arrows?(side?start+source_rows()<manga::sources.size():start>0):
                    side?(list_start+row_count()<list_size||(remote_list()&&offset+batch_size()<total)):(list_start||offset);
                const auto color=!busy&&ready&&enabled?(reader?0xf5f6eeffu:0x243b31ffu):0x748178ffu;
                draw.rect(left,top,width,height,reader?0x14251edc:0xfffefaff,6);
                draw.line(cx-direction*dx,cy-dy,cx+direction*dx,cy,color,std::max(1,int(std::round(2*unit))));
                draw.line(cx+direction*dx,cy,cx-direction*dx,cy+dy,color,std::max(1,int(std::round(2*unit))));
            }
        }
        auto r=canvas.present(draw);if(!r)error(r.error());frame_work.advance();}
};
#include "reader_ui.inc"
PXA_APPLICATION(Reader)
