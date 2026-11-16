#include "sources.hpp"
#include "jpeg_compat.hpp"
#include "progressive_jpeg.hpp"
#include "webp_compat.hpp"
#include <cstring>
#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/http.hpp>
#include <pxa/fs.hpp>
#include <pxa/ui_widgets.hpp>
#include <pxa/ui_refresh.hpp>
#include <cmath>
using namespace pxa;
using namespace pxa::ui;
struct Reader {
    static constexpr pxa::game::LoopOptions loop_options{20000,20,1};
    enum Screen {shelf_screen,discover,source_screen,contents,reading,help,page_jump};
    enum Act {shelf_tab,discover_tab,source_tab,search,previous_list,next_list,open_book,open_chapter,
        go_back,language_change,resume,collect,previous_page,next_page,zoom,retry,cancel,
        show_help,choose_source,source_previous,source_next,clear_search,read_book,jump_page,jump_submit,jump_step,toggle_order};
    struct Callback {Reader* app=nullptr;Act act{};int arg=0;void operator()()const{app->action(act,arg);}};
    struct Controls {
        static constexpr Capacity capacity{128,0,32};
        Reader* app;std::array<Callback,32> callbacks{};
        template<class Owner> bool render(Owner&,std::uint32_t);
    };
    Context* ctx=nullptr;
    DisplayMetrics metrics;
    CanvasRef canvas;CanvasCommands<768> draw;
    State<std::uint32_t> revision{0};
    State<std::string> input{""};TextInputRef input_ref;
    State<TextInputBox> input_box{TextInputBox{{},false,Font::body,0x20382fff}};
    Screen screen=shelf_screen,prior=shelf_screen;
    std::vector<manga::Book> shelf,results;
    std::vector<manga::Chapter> chapters,groups;
    unsigned group_index=0;
    bool descending=false;
    std::vector<std::string> pages,originals;
    manga::Book book;
    std::string custom="copymanga",query,status="正在读取书架…",failure;
    unsigned source_start=0,language=0,offset=0,total=0,list_start=0,page=0,zoom_mode=1;
    bool input_empty=true;
    bool busy=false,dirty=true,ui_dirty=true,ready=false,save_pending=false,save_busy=false,storage_ok=true,bars=false,dragging=false,exit_pending=false;
    std::uint64_t now_us=0,bar_deadline=0;
    float pan_x=0,pan_y=0,scale=1,down_x=0,down_y=0,last_x=0,last_y=0;
    std::optional<Asset> image;
    Permission permission;
    std::array<std::byte,4096> buffer{};
    auto view(){return Stack(Canvas(canvas).on_pointer([this](const CanvasPointer& e){pointer(e);}),
        Refresh(revision,[this]{return Controls{this};}),
        TextInput(input,input_ref).box(input_box).max_bytes(1024).single_line().on_submit([this]{(void)input_ref.hide_keyboard();action(screen==page_jump?jump_submit:search);})).fill().fill_height();}
    void changed(){dirty=true;ui_dirty=true;}
    void message(std::string s){status=std::move(s);changed();}
    void start(Task<void> task,bool foreground=true){auto r=(foreground?ctx->foreground_tasks():ctx->tasks()).start(std::move(task));if(!r)error(r.error());}
    void error(Error e){busy=false;
        failure=e==Error::denied?"请求被拒绝，请检查权限或漫画源":
            e==Error::limit_exceeded?"图片或数据超过上限，请换章":
            e==Error::protocol_error?"漫画源数据格式错误，请重试":
            e==Error::unsupported?"图片编码暂不支持，可换章":
            e==Error::timed_out?"网络超时，请重试":
            e==Error::busy?"漫画源请求过多，请稍后重试":"网络或漫画源不可用，请重试";
        (void)ctx->log().write(LogLevel::warning,"MANGA error="+std::to_string(int(e)));message(failure);
    }
    Result<void> on_start(Context& c,std::span<const std::byte> config){ctx=&c;auto m=decode_start_display(config);if(!m)return std::unexpected(m.error());metrics=*m;
        c.foreground_tasks().on_error(this,[](void* p,Error e) noexcept {static_cast<Reader*>(p)->error(e);});
        c.tasks().on_error(this,[](void* p,Error e) noexcept {auto a=static_cast<Reader*>(p);a->save_busy=false;a->storage_ok=false;a->save_pending=false;a->message("保存失败 ("+std::to_string(int(e))+")");});
        auto w=c.window().fullscreen(WindowBarMode::hidden,WindowBarMode::hidden);if(!w)return w;start(load_saved(),false);return {};}
    Result<bool> on_event(Context&,const Event& e){if(e.is<DisplayMetrics>()){auto m=decode_display_metrics(e);if(m){bool resized=metrics.width!=m->width||metrics.height!=m->height;metrics=*m;if(resized)fit();changed();}return true;}
        if(e.is<PermissionRevoked>()){stop();permission.reset();failure="网络权限已撤销";message(failure);return true;}return false;}
    void on_background(Context&){stop();(void)input_ref.hide_keyboard();dragging=false;save();}
    void on_foreground(Context&){changed();if(ready&&screen==reading&&!image&&!busy)begin(pages.empty()?load_pages():load_image());}
    BackAction on_back(){if(screen==shelf_screen){if(save_busy||save_pending){exit_pending=true;message("正在保存…");return BackAction::stay;}return BackAction::close;}action(go_back);return BackAction::stay;}
    void stop(){ctx->foreground_tasks().cancel();busy=false;failure.clear();changed();}
    void begin(Task<void> task){stop();busy=true;message("正在加载…");start(std::move(task));}
    float density()const{return std::max(.25f,float(metrics.density_q16)/65536.f);}
    float ui_factor()const{return metrics.height>=480?std::max(1.f,density()/.78125f):1.f;}
    CanvasRegion physical_area()const {int l=metrics.safe.left,r=metrics.safe.right,t=metrics.safe.top,b=metrics.safe.bottom;
        if(metrics.system_bars_known){l=std::max(l,int(metrics.system_bars.left));r=std::max(r,int(metrics.system_bars.right));t=std::max(t,int(metrics.system_bars.top));b=std::max(b,int(metrics.system_bars.bottom));}
        if(metrics.shape==2){l=std::max(l,int(metrics.width*.15));r=std::max(r,int(metrics.width*.15));t=std::max(t,int(metrics.height*.15));b=std::max(b,int(metrics.height*.15));}
        return {l+4,t+4,std::max(1,int(metrics.width)-l-r-8),std::max(1,int(metrics.height)-t-b-8)};}
    CanvasRegion area()const {auto r=physical_area();float f=ui_factor();return {int(r.x/f),int(r.y/f),int(r.width/f),int(r.height/f)};}
    CanvasRegion viewport()const {return {0,0,int(metrics.width),int(metrics.height)};}
    CanvasRegion logical(CanvasRegion r)const {float d=density();int left=int(std::floor(r.x/d)),top=int(std::floor(r.y/d));return {left,top,std::max(1,int(std::ceil((r.x+r.width)/d))-left),std::max(1,int(std::ceil((r.y+r.height)/d))-top)};}
    unsigned list_y()const{return screen==shelf_screen?64:screen==contents?100:104;}
    unsigned row_height()const{return screen==contents?36:40;}
    unsigned row_count()const {auto a=area();return std::clamp((a.height-int(list_y())-30)/int(row_height()),1,12);}
    unsigned source_rows()const{return std::clamp((area().height-76)/52,1,8);}
    unsigned batch_size()const {auto source=manga::source_info(custom);return screen==discover&&source?source->batch:20;}
    bool remote_list()const{return screen==discover||(screen==contents&&(book.source.empty()||book.source=="copymanga"));}
    std::string active_source()const{return std::string(manga::source_name(screen==contents?book.source:custom));}
    void fit(){pan_x=pan_y=0;if(!image)return;auto v=viewport();auto d=image->descriptor();
        float width=float(v.width)/d.width,height=float(v.height)/d.height;
        scale=zoom_mode==0?std::min(width,height):width*(zoom_mode==2?2.f:1.f);clamp();dirty=true;}
    void clamp(){if(!image)return;auto v=viewport();auto d=image->descriptor();pan_x=std::clamp(pan_x,0.f,std::max(0.f,d.width*scale-v.width));pan_y=std::clamp(pan_y,0.f,std::max(0.f,d.height*scale-v.height));}
    void pointer(const CanvasPointer& e){if(screen!=reading)return;const float x=e.x*density(),y=e.y*density();
        if(e.action()==PointerPhase::down){down_x=last_x=x;down_y=last_y=y;dragging=true;return;}
        if(!dragging)return;
        if(e.action()==PointerPhase::move&&zoom_mode){pan_x-=x-last_x;pan_y-=y-last_y;clamp();dirty=true;if(bars&&(std::abs(x-down_x)>10||std::abs(y-down_y)>10)){bars=false;changed();}}
        if(e.action()==PointerPhase::up){dragging=false;float dx=x-down_x,dy=y-down_y;
            if(std::abs(dx)<10&&std::abs(dy)<10){bars=!bars;bar_deadline=now_us+6000000;changed();}
            else if(zoom_mode!=2&&std::abs(dx)>35&&std::abs(dx)>std::abs(dy))action(dx<0?next_page:previous_page);
        }if(e.action()==PointerPhase::cancel)dragging=false;last_x=x;last_y=y;}
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
            {"User-Agent",(copy_browser||site_browser||chinese_browser)?"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36 PXA-Manga/0.5":"PXA-Manga/0.5"},
            {"Referer",referrer.empty()?"https://www.mangacopy.com/":std::string_view(referrer)},
            {"platform","1"},{"version","2026.10.16"}}};
        const auto headers=std::span{fields}.first(copy_request?5:referrer.empty()?2:3);
        static constexpr std::array<std::string_view,3> wanted{{"content-type","location","x-wp-total"}};
        for(int n=0;n<4;++n){auto ex=co_await ctx->http().open(HttpRequest{.url=url,.headers=headers,.wanted_headers=wanted,.options={.buffer_bytes=4096,.header_bytes=1024,.max_body_bytes=limit,.timeout_ms=limit>524288?60000u:25000u}},permission);
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
    Task<void> load_chapters(){
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
    Task<void> load_pages(){
        using enum manga::SourceKind;auto kind=manga::source_kind(book.source);
        auto url=kind==mangadex?api("/at-home/server/"+book.chapter):book.chapter;
        auto text=co_await json(url,kind==mangadex,kind==copy?196608:(kind==katana||kind==pill||kind==town||manga::chinese_source(kind))?262144:98304);if(!text)co_return std::unexpected(text.error());
        bool ok=kind==copy?manga::copy_pages(*text,pages):kind==katana?manga::katana_pages(*text,pages):kind==pill?manga::pill_pages(*text,pages):kind==town?manga::town_pages(*text,book.chapter,pages):manga::chinese_source(kind)?manga::chinese_pages(*text,kind,pages):manga::pages(manga::Json(*text),pages,true,url);
        if(!ok)co_return std::unexpected(Error::protocol_error);
        originals.clear();if(kind==town){originals.resize(pages.size());auto first=manga::town_image(*text);if(!first)co_return std::unexpected(Error::protocol_error);originals[0]=std::move(*first);}if(kind==mangadex)manga::pages(manga::Json(*text),originals,true,url,true);
        page=std::min<unsigned>(book.page,pages.size()-1);co_return co_await load_image();
    }
    Task<void> load_image(){image.reset();dirty=true;
        // Canvas retains its last image independently of the Asset handle.
        // Replace that frame before requesting another decoded image so two
        // full-size pages cannot overlap in the Host resource budget.
        if(canvas.mounted()){draw.reset();auto box=logical(viewport());draw.rect(box.x,box.y,box.width,box.height,0x141916ff);auto cleared=canvas.present(draw);if(!cleared)co_return std::unexpected(cleared.error());}
        std::string image_url=pages[page];
        if(manga::source_kind(book.source)==manga::SourceKind::town){
            if(page>=originals.size())co_return std::unexpected(Error::protocol_error);
            if(originals[page].empty()){auto text=co_await json(pages[page],false,262144);if(!text)co_return std::unexpected(text.error());auto url=manga::town_image(*text);if(!url)co_return std::unexpected(Error::protocol_error);originals[page]=std::move(*url);}
            image_url=originals[page];
        }
        Result<Asset> a=std::unexpected(Error::io_error);
        auto image_path=std::string_view(image_url).substr(0,image_url.find('?'));bool known_webp=image_path.ends_with(".webp");
        if(known_webp)a=std::unexpected(Error::unsupported);
        for(unsigned attempt=0;!known_webp&&attempt<2;++attempt){auto r=co_await request(image_url,524288,manga::referrer(book));
            if(r)a=co_await ctx->assets().decode_image(std::move(r->body),{.max_width=512,.max_height=2048,.max_pixels=262144,.max_encoded_bytes=524288,.formats=ImageFormats::png|ImageFormats::jpeg});
            else a=std::unexpected(r.error());if(a||!transient(a.error()))break;
        }
        if(!a&&(a.error()==Error::unsupported||a.error()==Error::limit_exceeded)){
            if(book.source.empty()&&page<originals.size())a=co_await convert_original(originals[page]);
            else if(!book.source.empty())a=co_await convert_original(image_url);
        }
        if(!a)co_return std::unexpected(a.error());image.emplace(std::move(*a));busy=false;failure.clear();if(bars)bar_deadline=now_us+6000000;fit();book.page=page;book.language=language;book.pages=pages.size();
        auto at=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});if(at!=shelf.end())*at=book;else if(shelf.size()<24)shelf.insert(shelf.begin(),book);
        save();message("第 "+std::to_string(page+1)+" / "+std::to_string(pages.size())+" 页");co_return Result<void>{};}
    Task<Asset> convert_original(std::string url){
        message("正在处理原图…");
        auto response=co_await request(std::move(url),4*1024*1024,manga::referrer(book));if(!response)co_return std::unexpected(response.error());
        auto file=co_await ctx->fs().open("manga-source.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!file)co_return std::unexpected(file.error());
        unsigned received=0;
        for(;;){auto n=co_await response->body.read_some(buffer);if(!n)co_return std::unexpected(n.error());if(!*n)break;received+=*n;if(received>4*1024*1024)co_return std::unexpected(Error::limit_exceeded);
            std::size_t at=0;while(at<*n){auto written=file->write(std::span{buffer}.subspan(at,*n-at));if(!written||!*written)co_return std::unexpected(written?Error::io_error:written.error());at+=*written;}}
        auto closed=file->close();if(!closed)co_return std::unexpected(closed.error());
        file=co_await ctx->fs().open("manga-source.tmp");if(!file)co_return std::unexpected(file.error());
        auto converter=std::make_unique<manga::JpegCompat>();
        auto read=[](void* p,std::span<std::byte> out)->std::size_t{auto n=static_cast<File*>(p)->read(out);return n?*n:0;};
        std::unique_ptr<manga::ProgressiveJpeg> progressive;std::unique_ptr<manga::WebpCompat> webp;
        if(!converter->begin(read,&*file)){
            auto rewound=co_await file->seek(0);if(!rewound)co_return std::unexpected(rewound.error());
            std::array<std::byte,12> signature{};auto n=file->read(signature);if(!n)co_return std::unexpected(n.error());
            bool is_webp=*n==12&&std::memcmp(signature.data(),"RIFF",4)==0&&std::memcmp(signature.data()+8,"WEBP",4)==0;
            rewound=co_await file->seek(0);if(!rewound)co_return std::unexpected(rewound.error());
            if(is_webp){
                webp=std::make_unique<manga::WebpCompat>();if(!webp->begin(read,&*file))co_return std::unexpected(webp->limited?Error::limit_exceeded:Error::unsupported);
                while(!webp->done()){if(!webp->next())co_return std::unexpected(webp->limited?Error::limit_exceeded:Error::protocol_error);auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}
                converter=std::make_unique<manga::JpegCompat>();if(!converter->begin_rgb(webp->pixels,webp->width,webp->height,3))co_return std::unexpected(Error::limit_exceeded);
            }else{
                manga::JpegHeader header;if(!manga::jpeg_header(read,&*file,header))co_return std::unexpected(Error::unsupported);
                if(std::uint64_t(header.width)*header.height>1048576)co_return std::unexpected(Error::limit_exceeded);
                rewound=co_await file->seek(0);if(!rewound)co_return std::unexpected(rewound.error());
                progressive=std::make_unique<manga::ProgressiveJpeg>();
                if(!progressive->load(read,&*file))co_return std::unexpected(progressive->limited?Error::limit_exceeded:Error::unsupported);
                converter=std::make_unique<manga::JpegCompat>();
                if(!converter->begin_rgb(progressive->pixels,progressive->width,progressive->height,progressive->channels))co_return std::unexpected(Error::limit_exceeded);
            }
        }
        while(!converter->done()){if(!converter->next_row())co_return std::unexpected(Error::protocol_error);auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}
        progressive.reset();webp.reset();
        auto png=converter->png();converter.reset();closed=file->close();if(!closed)co_return std::unexpected(closed.error());
        auto removed=co_await ctx->fs().remove("manga-source.tmp");if(!removed)co_return std::unexpected(removed.error());
        file=co_await ctx->fs().open("manga-image.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!file)co_return std::unexpected(file.error());
        auto bytes=std::as_bytes(std::span{png});while(!bytes.empty()){auto n=file->write(bytes.first(std::min<std::size_t>(4096,bytes.size())));if(!n||!*n)co_return std::unexpected(n?Error::io_error:n.error());bytes=bytes.subspan(*n);auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}
        std::vector<std::uint8_t>{}.swap(png);closed=file->close();if(!closed)co_return std::unexpected(closed.error());
        file=co_await ctx->fs().open("manga-image.tmp");if(!file)co_return std::unexpected(file.error());
        auto decoded=co_await ctx->assets().decode_image(std::move(*file),{.max_width=512,.max_height=2048,.max_pixels=262144,.max_encoded_bytes=524288,.formats=ImageFormats::png});
        removed=co_await ctx->fs().remove("manga-image.tmp");if(!removed)co_return std::unexpected(removed.error());
        co_return std::move(decoded);
    }
    std::string shelf_status()const{return shelf.empty()?"书架为空，去发现漫画":std::to_string(shelf.size())+" 部漫画 · "+(!storage_ok?"未保存":save_busy||save_pending?"保存中":"已保存");}
    void save(){if(ready&&storage_ok)save_pending=true;}
    Task<void> save_state(){save_busy=true;
        while(save_pending){save_pending=false;auto text=manga::save(shelf,language,custom);if(text.size()>32768)co_return std::unexpected(Error::limit_exceeded);
            auto f=co_await ctx->fs().open("manga-state.tmp",OpenMode::write|OpenMode::create|OpenMode::truncate);if(!f)co_return std::unexpected(f.error());
            auto bytes=std::as_bytes(std::span{text.data(),text.size()});while(!bytes.empty()){auto n=f->write(bytes.first(std::min<size_t>(1024,bytes.size())));if(!n||!*n)co_return std::unexpected(n?Error::io_error:n.error());bytes=bytes.subspan(*n);auto y=co_await ctx->clock().yield();if(!y)co_return std::unexpected(y.error());}
            auto closed=f->close();if(!closed)co_return std::unexpected(closed.error());auto moved=co_await ctx->fs().replace("manga-state.tmp","manga-state.json");if(!moved)co_return std::unexpected(moved.error());
        }save_busy=false;if(screen==shelf_screen)message(shelf_status());else changed();co_return Result<void>{};}
    Task<void> load_saved(){auto f=co_await ctx->fs().open("manga-state.json");
        if(f){std::string text;for(;;){auto n=f->read(buffer);if(!n||text.size()+*n>32768){storage_ok=false;break;}if(!*n)break;text.append(reinterpret_cast<const char*>(buffer.data()),*n);}auto c=f->close();
            if(!c||!storage_ok||!manga::restore(text,shelf,language,custom)){storage_ok=false;message("书架记录损坏，已保留原文件");}else if(manga::Json(text).get("version").number()<3)save_pending=true;}
        else if(f.error()!=Error::not_found){storage_ok=false;message("无法读取书架，进度暂不保存");}
        auto cleaned=co_await ctx->fs().remove("manga-source.tmp");(void)cleaned;cleaned=co_await ctx->fs().remove("manga-image.tmp");(void)cleaned;
        ready=true;if(storage_ok)message(shelf_status());co_return Result<void>{};}
    void action(Act act,int arg=0){if(!ready)return;if(screen==reading)bar_deadline=now_us+6000000;
        if(act==cancel){stop();message("已取消，可重试");failure="已取消";return;}
        if(act==show_help){prior=screen;stop();screen=help;changed();return;}
        if(act==go_back){if(screen==page_jump){screen=reading;input.set("");(void)input_ref.hide_keyboard();bar_deadline=now_us+6000000;message("第 "+std::to_string(page+1)+" / "+std::to_string(pages.size())+" 页");return;}if(screen==help){screen=prior;bar_deadline=now_us+6000000;changed();if(screen==reading&&!image)begin(pages.empty()?load_pages():load_image());return;}stop();image.reset();pages.clear();
            if(screen==reading){screen=contents;if(chapters.empty()){list_start=offset=0;begin(load_chapters());}else message(std::to_string(total)+" 个章节");}
            else{screen=shelf_screen;list_start=0;message(shelf_status());}return;}
        if(act==source_previous||act==source_next){auto rows=source_rows();if(act==source_previous)source_start-=std::min(source_start,rows);else if(source_start+rows<manga::sources.size())source_start+=rows;changed();return;}
        if(act==choose_source){if(arg<0||unsigned(arg)>=manga::sources.size())return;custom=manga::sources[arg].id;save();action(discover_tab);return;}
        if(act==toggle_order){if(busy||screen!=contents)return;descending=!descending;offset=list_start=0;begin(load_chapters());return;}
        if(act==jump_page){if(busy||pages.empty())return;screen=page_jump;input.set(std::to_string(page+1));message("共 "+std::to_string(pages.size())+" 页");return;}
        if(act==jump_step){int value=int(manga::comic_number(input.get()));value=std::clamp(value+(arg<0?-1:1),1,int(pages.size()));input.set(std::to_string(value));message("共 "+std::to_string(pages.size())+" 页");return;}
        if(act==jump_submit){auto value=manga::comic_number(input.get());if(!value||value>pages.size()){message("请输入 1–"+std::to_string(pages.size())+" 之间的页码");return;}screen=reading;input.set("");(void)input_ref.hide_keyboard();bars=true;bar_deadline=now_us+6000000;if(value-1!=page){page=value-1;begin(load_image());}else message("第 "+std::to_string(page+1)+" / "+std::to_string(pages.size())+" 页");return;}
        if(act==clear_search){input.set("");query.clear();offset=0;begin(load_catalog());return;}
        if(act==shelf_tab||act==discover_tab||act==source_tab){stop();image.reset();pages.clear();list_start=offset=0;
            screen=act==shelf_tab?shelf_screen:act==discover_tab?discover:source_screen;query.clear();input.set("");
            if(screen==source_screen){for(unsigned i=0;i<manga::sources.size();++i)if(manga::sources[i].id==custom)source_start=(i/source_rows())*source_rows();}
            if(screen==discover)begin(load_catalog());else message(screen==source_screen?"当前源："+std::string(manga::source_name(custom)):shelf_status());return;}
        if(act==search){if(busy)return;query=input.get();offset=0;begin(load_catalog());return;}
        if(act==language_change){if(busy)return;if(screen==contents&&book.source=="copymanga"){if(groups.size()>1){group_index=(group_index+1)%groups.size();offset=total=0;begin(load_chapters());}return;}if((screen==discover&&!custom.empty())||(screen==contents&&!book.source.empty()))return;language=(language+1)%3;offset=0;save();if(screen==contents)begin(load_chapters());else begin(load_catalog());return;}
        if(act==retry){if(busy)return;failure.clear();if(screen==discover)begin(load_catalog());else if(screen==contents)begin(load_chapters());else if(screen==reading)begin(pages.empty()?load_pages():load_image());return;}
        if(busy)return;
        if(act==open_book||act==read_book){auto& list=screen==discover?results:shelf;if(arg<0||unsigned(arg)>=list.size())return;book=list[arg];auto saved=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});if(saved!=shelf.end())book=*saved;
            language=book.chapter.empty()?language:book.language;chapters.clear();groups.clear();group_index=0;total=0;list_start=offset=0;if(act==read_book&&!book.chapter.empty()){screen=reading;bars=false;zoom_mode=1;pages.clear();begin(load_pages());}else{screen=contents;begin(load_chapters());}return;}
        if(act==collect){auto p=std::find_if(shelf.begin(),shelf.end(),[&](auto& b){return manga::same(b,book);});if(p==shelf.end()){if(shelf.size()>=24){message("书架已满（24 部）");return;}shelf.push_back(book);message("已加入书架");}else{shelf.erase(p);message("已从书架移除");}save();return;}
        if(act==previous_list||act==next_list){auto count=screen==shelf_screen?shelf.size():screen==discover?results.size():chapters.size();auto rows=row_count();
            if(act==next_list){if(list_start+rows<count)list_start+=rows;else if(remote_list()&&offset+batch_size()<total){offset+=batch_size();begin(screen==discover?load_catalog():load_chapters());return;}}
            else if(list_start>=rows)list_start-=rows;else if(offset){offset-=std::min(offset,batch_size());begin(screen==discover?load_catalog():load_chapters());return;}changed();return;}
        if(act==open_chapter||act==resume){if(act==open_chapter){if(arg<0||unsigned(arg)>=chapters.size())return;book.chapter=chapters[arg].id;book.chapter_title=chapters[arg].title;book.page=0;}if(book.chapter.empty())return;if(act==resume)language=book.language;
            screen=reading;bars=false;zoom_mode=1;pages.clear();begin(load_pages());return;}
        if(act==zoom){zoom_mode=(zoom_mode+1)%3;fit();changed();return;}
        if(act==previous_page||act==next_page){if(pages.empty())return;if((act==previous_page&&page==0)||(act==next_page&&page+1==pages.size())){message(act==next_page?"本章读完，请返回目录选择下一章":"已是本章第一页");return;}
            page+=act==next_page?1:-1;begin(load_image());return;}
    }
    void on_frame(Context&,pxa::game::FrameTick tick){if(input_empty!=input.get().empty()){input_empty=input.get().empty();ui_dirty=true;}now_us=tick.timestamp_us;if(screen==reading&&bars&&!busy&&failure.empty()&&now_us>=bar_deadline){bars=false;changed();}if(save_pending&&!save_busy)start(save_state(),false);
        if(exit_pending&&!save_busy&&!save_pending){exit_pending=false;(void)ctx->window().request_back();}
        if(ui_dirty){ui_dirty=false;revision.update([](auto n){return n+1;});auto a=area();float d=density()/ui_factor();bool edit=screen==discover||screen==page_jump;
            input_box.set(TextInputBox{{int((a.x+8)/d),int((a.y+40)/d),int(std::max(1,a.width-16-(screen==discover?52:screen==page_jump?80:0))/d),int(28/d)},edit,Font::label,0x20382fff});}
        if(!dirty||!canvas.mounted())return;dirty=false;draw.reset();auto background=logical(viewport());draw.rect(background.x,background.y,background.width,background.height,screen==reading?0x141916ff:0xf6f5f0ff);
        
        if(screen==reading&&image){auto v=viewport();auto d=image->descriptor();int w=std::max(1,int(d.width*scale)),h=std::max(1,int(d.height*scale));
            draw.clip(logical(v)).image(logical({v.x+std::max(0,(v.width-w)/2)-int(pan_x),v.y+std::max(0,(v.height-h)/2)-int(pan_y),w,h}),*image,ImageFit::stretch).pop_clip();}
        auto r=canvas.present(draw);if(!r)error(r.error());}
};
#include "reader_ui.inc"
PXA_APPLICATION(Reader)
