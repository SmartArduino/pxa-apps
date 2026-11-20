#include "draw.hpp"
#include <pxa/app.hpp>

#include <array>
#include <cstdio>
#include <optional>

namespace tide {
namespace {

constexpr Color ink = rgb(0x102429), panel = rgb(0x19343a), light = rgb(0x25464a);
constexpr Color border = rgb(0x497172), gold = rgb(0xf2cd87), ivory = rgb(0xf5ebd3);
constexpr Color mist = rgb(0xb9cebe), quiet = rgb(0x86a7a3), vermilion = rgb(0xdf825e);
constexpr Color mint = rgb(0x97d5b2), ocean = rgb(0x173d46);
constexpr Text ship_names[] = {"轻帆艇", "远洋卡拉维尔", "三桅商船"};
constexpr Text tabs[] = {"海图","市场","船坞","港务","日志"};

enum Screen { Chart, Market, Shipyard, Office, Journal };
enum Overlay {
    None, Routes, StoryPage, Intel, Help, Atlas, EndingPage, TaleList, TalePage, SeaLog,
    Preflight, RouteStops, PortPrices, EventReport, TowRequest, PortDesk, EscortPage, DuelInvitation, ProvisionsPage,
    ExpeditionList, ExpeditionPage, Wardroom, OfficerPage, RumorList, RumorPage
};
enum NavIcon { MapIcon, ScaleIcon, ShipIcon, LedgerIcon, JournalIcon, CoinsIcon, HullIcon,
               FoodIcon, CompassIcon, CursorIcon, AnchorIcon, GlassIcon };
enum Action {
    Begin=1, OpenChart, OpenMarket, OpenShipyard, OpenOffice, OpenJournal,
    RouteList, Close, ZoomIn, ZoomOut, Center, Sail, Pause, Trim, Retreat, NextStop,
    BuyOne, BuyFive, SellOne, SellFive, Accept, Deliver, ReadStory, Survey,
    ChooseFirst, ChooseSecond, Recruit, ShowHelp, ShowAtlas, ShowEnding,
    Previous, Next, ShowIntel, FightFirst, FightSecond, FightThird, FightFourth, WaitDay,
    ShowTales, PortTaleAction, TaleShare, TaleSell, TaleMark, ShowSeaLog,
    ShowPreflight, ShowStops, ConfirmSail, TopUp, ShowPortPrices, ResumeAfterEvent, ShowTow, ConfirmTow,
    OpenDesk, ShowEscort, AcceptEscort, DeliverPublic, DeliverPrivate, MarkEscort,
    ShowDuel, BeginDuel, InvestPort, StartHelm, SteerLeft, SteerRight, DockHelm,
    OpenProvisions, BuyFood, BuyWater, UnloadFood, UnloadWater,
    ShowExpeditions, OpenArchive, OpenObservation, CopyClue, AskWitness, SponsorVoyage, FundVoyage,
    MarkExpedition, ObserveFirst, ObserveSecond, WaitToObserve, FileExpedition, ReadNotes,
    ShowOfficers, OfficerTalk, OfficerDuty, OfficerHire, OfficerMark,
    ShowRumors, RecordRumor, MarkRumor, StartExplore,
    DockService=100, GoodsRow=200, Destination=300, AtlasEntry=400, TaleEntry=500, SeaEntry=600,
    ExpeditionEntry=700, OfficerEntry=800, RumorEntry=900
};

template<class... Args>
auto format(const char* message, Args... args) {
    struct Buffer {
        char contents[160]{};
        operator Text() const { return contents; }
    } out;
    std::snprintf(out.contents,sizeof(out.contents),message,args...);
    return out;
}

struct Button {
    Rect rect;
    int action;
};

struct EncounterOutcome {
    Text description;
    int hull=0, stores=0, water=0, gold=0, cargo=0, day=0;
};

struct VoyageApp {
    static constexpr pxa::game::LoopOptions loop_options{
        .simulation_step_us=50000, .frame_period_ms=72, .maximum_updates=1};
    static constexpr unsigned DayMs=480, HelmPulseMs=220;

    Game game;
    EncounterOutcome outcome;
    std::array<char,96> trade_receipt{};
    // Loading completes before gameplay can start. Subsequent saves use one
    // borrowed packet at a time, keeping the chart out of coroutine slots.
    std::array<std::byte,SaveBytes+72> storage_packet{};
    pxa::Context* context = nullptr;
    std::optional<pxa::Surface> surface;
    pxa::WindowMetrics metrics{};
    pxa::ui::DisplayMetrics input_display{};
    pxa::input::Controller controller;
    pxa::input::PointerCapture pointer_capture;
    std::array<Button,48> buttons{};
    int button_count=0, focused=0;
    Rect safe{}, body{}, map{};
    int scale=1, surface_x=0, surface_y=0;
    int selected=madeira, good=Wine, page=0, atlas_item=0, tale_item=0, sea_item=-1;
    int expedition_item=0;
    int officer_item=Pilot;
    int rumor_item=0, pinned_rumor=-1;
    HarborApproach approach;
    Point approach_position{-1,-1};
    int approach_port=-1;
    std::optional<Point> scout_course;
    bool officer_talk=false;
    float chart_x=0, chart_y=0, factor=1;
    int zoom=3, press_x=0, press_y=0, last_x=0, last_y=0;
    bool map_press=false, panning=false, follow=true, pad_focus=false, intel_from_preflight=false;
    bool title=true, loaded=false, had_save=false, opening=false, reconfigure=true;
    bool foreground=false, dirty=true, save_needed=false, save_active=false, save_failed=false;
    unsigned now=0, last_day=0, last_step=0, notice_until=0, flash_until=0, saved_until=0;
    unsigned surface_retry=0, duel_flash_until=0;
    int duel_animation=0;
    unsigned long long next_frame=1;
    Screen screen=Chart;
    Overlay modal=None, activity_back=None, rumor_back=None;
    bool expedition_from_book=false, show_witness=false;

    auto view() {
        return pxa::ui::Canvas().input_only().on_pointer(
            [this](const pxa::ui::CanvasPointer& event) { pointer(event); });
    }

    pxa::Result<void> on_start(pxa::Context& ctx, std::span<const std::byte> config) {
        context = &ctx;
        if (auto display=pxa::ui::decode_start_display(config)) input_display=*display;
        (void)ctx.window().configure({
            .edge_to_edge=true,
            .status_bar_mode=pxa::WindowBarMode::hidden,
            .navigation_bar_mode=pxa::WindowBarMode::hidden});
        return ctx.tasks().start(load());
    }
    pxa::Task<void> load() {
        auto bytes=std::span{storage_packet}.first(SaveBytes);
        auto pending = co_await context->storage().get("voyage-v1", bytes);
        had_save=pending && *pending<=bytes.size() && decode(game.s,std::span{bytes}.first(*pending));
        if (had_save) game.restore();
        else if ((pending || pending.error() != pxa::Error::not_found)) {
            game.notice = "存档读取失败";
            notify();
        }
        selected=game.s.destination>=0 ? game.s.destination :
                 game.s.origin==lisbon && !game.s.helm ? madeira : game.s.origin;
        center_at(game.position());
        loaded=true;
        dirty=true;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> write_snapshot() {
        const auto bytes=encode(game.s);
        return context->storage().set("voyage-v1",bytes,storage_packet);
    }
    pxa::Task<void> save() {
        auto result=co_await write_snapshot();
        save_active=false;
        save_failed=!result;
        if (result) saved_until=now+1600;
        else save_needed=true;
        dirty=true;
        co_return result;
    }
    void request_save() {
        if (save_needed && !save_active && (!save_failed || now-last_step > 2000)) {
            save_needed=false;
            save_active=true;
            last_step=now;
            if (!context->tasks().start(save())) {
                save_active=false;
                save_needed=true;
                save_failed=true;
            }
        }
    }

    void on_foreground(pxa::Context&) { foreground=true; last_day=0; dirty=true; }
    void on_background(pxa::Context&) {
        foreground=false;
        pointer_capture.reset();
        controller.reset();
        if (!title && game.sea()) save_needed=true;
        request_save();
    }
    pxa::Result<bool> on_event(pxa::Context&, const pxa::Event& event) {
        if (event.service!=3 || event.opcode!=0x8002 || event.token!=0) return false;
        auto display=pxa::ui::decode_display_metrics(event);
        if (!display) return std::unexpected(display.error());
        input_display=*display;
        pointer_capture.reset();
        return true;
    }
    void on_window_changed(const pxa::WindowMetrics& value) {
        reconfigure |= value.pixel_width != metrics.pixel_width || value.pixel_height != metrics.pixel_height;
        metrics = value;
        dirty = true;
    }
    pxa::Task<void> open_surface() {
        if (!metrics.pixel_width || !metrics.pixel_height) {
            auto value = co_await context->window().snapshot();
            if (!value) { opening=false; co_return std::unexpected(value.error()); }
            if (value->revision >= metrics.revision) on_window_changed(*value);
        }
        const auto width = metrics.pixel_width, height = metrics.pixel_height;
        const auto area = width * height;
        const int divisor = area > 220000 ? 2 : 1;
        const auto w = static_cast<std::uint16_t>(width/divisor);
        const auto h = static_cast<std::uint16_t>(height/divisor);
        surface.reset();
        auto created = co_await context->surface().create_mapped({
            .width=w, .height=h, .buffers=2, .max_buffer_bytes=1024*1024});
        if (!created) {
            surface_retry=now+1800;
            (void)context->log().write(pxa::LogLevel::error,"Tide Pact surface allocation failed");
            opening=false;
            co_return std::unexpected(created.error());
        }
        auto config = co_await created->configure({.width=w,.height=h});
        if (config) {
            scale=divisor;
            surface_x=(width-w*scale)/2;
            surface_y=(height-h*scale)/2;
            surface.emplace(std::move(*created));
        }
        reconfigure = width != metrics.pixel_width || height != metrics.pixel_height;
        opening=false;
        dirty=true;
        co_return config;
    }

    void center_at(Point p) {
        chart_x=float(p.x); chart_y=float(p.y);
        dirty=true;
    }
    void set_zoom(int n) {
        zoom=std::clamp(n,0,7);
        if (!zoom) center_at({ChartWidth/2,ChartHeight/2});
        dirty=true;
    }
    void notify() {
        notice_until = now + 3100;
        dirty=true;
    }
    void changed() {
        save_needed=true;
        notify();
    }
    int target() const { return game.s.chapter < Chapters ? Story[game.s.chapter].place : -1; }
    bool story_here() const { return !game.sea() && target() == game.s.origin; }

    static bool chart_water(Point p) {
        if (p.x<0 || p.y<0 || p.x>=ChartWidth || p.y>=ChartHeight) return false;
        // make_chart packs each terrain class as four palette shades.
        const auto kind=Image{art::chart}.index(p.x,p.y)/4;
        return kind!=4 && kind!=5 && kind!=7;
    }

    void on_frame(pxa::Context&, pxa::game::FrameTick tick) {
        if (!foreground) return;
        now=unsigned(tick.timestamp_us / 1000);
        if ((reconfigure || !surface) && !opening && now>=surface_retry) {
            opening=true;
            if (!context->tasks().start(open_surface())) opening=false;
        }
        if (!surface || opening) return;

        if (!title && game.sea() && !game.paused && !game.encounter()) {
            if (!last_day) last_day=now;
            if (now-last_day >= (game.s.helm ? HelmPulseMs : DayMs)) {
                const int old_day=game.s.day;
                const int old_hull=game.s.hull;
                const auto old_marks=game.s.chart_marks;
                if (game.s.helm) game.helm_step(chart_water);
                else game.advance();
                last_day=now;
                if ((game.s.day!=old_day && (game.s.helm || game.s.day%4==0)) ||
                    !game.sea() || game.encounter() || game.paused || game.s.chart_marks!=old_marks)
                    save_needed=true;
                if (modal==ExpeditionPage && game.s.day!=old_day) dirty=true;
                if (game.paused || game.s.hull<old_hull) notify();
                if (!game.sea()) {
                    modal=None; screen=Chart; selected=game.s.origin;
                    notify();
                } else if (game.encounter()) {
                    modal=None; screen=Chart; focused=0; notify();
                }
            }
            if (screen==Chart && modal==None) dirty=true;
        } else last_day=now;
        if (notice_until && now >= notice_until) { notice_until=0; dirty=true; }
        if (saved_until && now >= saved_until) { saved_until=0; dirty=true; }
        if (game.s.weather==Privateer) dirty=true;
        if (duel_flash_until) {
            dirty=true;
            if (now>=duel_flash_until) duel_flash_until=0;
        }
        if (flash_until) {
            dirty=true;
            if (now >= flash_until) flash_until=0;
        }
        request_save();
        if (!dirty) return;
        auto frame=surface->acquire();
        if (!frame) return;
        auto pixels=frame->rgb565();
        if (!pixels) return;
        Draw draw(*pixels);
        render(draw);
        if (frame->present(next_frame)) { ++next_frame; dirty=false; }
    }

    void layout(Draw& d) {
        const auto& inset=metrics.safe_insets;
        int sx=std::max(8,(int(inset.left)-surface_x+scale-1)/scale+3);
        int sy=std::max(8,(int(inset.top)-surface_y+scale-1)/scale+3);
        int rx=std::max(8,(int(inset.right)-surface_x+scale-1)/scale+3);
        int by=std::max(8,(int(inset.bottom)-surface_y+scale-1)/scale+3);
        safe={sx,sy,d.clip.w-sx-rx,d.clip.h-sy-by};
        body={safe.x,safe.y+39,safe.w,safe.h-39-36};
    }

    void skin_box(Draw& d, Rect r, int tile, int edge) {
        constexpr int tile_size=72, source_edge=8;
        edge=std::min({edge, r.w/3, r.h/3});
        if (edge<1) return;
        const int x[]{r.x,r.x+edge,r.right()-edge,r.right()};
        const int y[]{r.y,r.y+edge,r.bottom()-edge,r.bottom()};
        const int uv[]{0,source_edge,tile_size-source_edge,tile_size};
        for (int j=0; j<3; ++j)
            for (int i=0; i<3; ++i)
                d.image({art::skin},{tile*tile_size+uv[i],uv[j],uv[i+1]-uv[i],uv[j+1]-uv[j]},
                        {x[i],y[j],x[i+1]-x[i],y[j+1]-y[j]});
    }
    void meter(Draw& d, Rect r, int value, int maximum, bool voyage) {
        skin_box(d,r,1,3);
        const Rect fill=r.inset(2);
        if (fill.w<1 || fill.h<1) return;
        d.fill(fill,ink);
        const int width=fill.w*std::clamp(value,0,maximum)/std::max(1,maximum);
        if (width) {
            d.image({art::skin},{voyage ? 192 : 144,0,48,12},{fill.x,fill.y,width,fill.h});
            d.fill({fill.x+width-1,fill.y,1,fill.h},voyage?gold:mint);
        }
    }
    int miles_done(const Passage& path) const {
        int miles=game.s.progress;
        for (int i=0;i<game.s.leg;++i) miles+=Legs[path.edge[i]].nautical_miles;
        return miles;
    }

    void nav_icon(Draw& d, int icon, Rect r) {
        d.image({art::navigation},{icon%4*42,icon/4*42,42,42},r,true);
    }
    void goods_icon(Draw& d, int icon, Rect r) {
        d.image({art::goods},{icon%4*48,icon/4*48,48,48},r,true);
    }
    void port_scene(Draw& d, Rect area, int port) {
        if (port<0 || port>=PortCount || area.w<1 || area.h<1) return;
        constexpr int tw=188, th=105;
        const int width=std::min(tw,th*area.w/area.h);
        const int height=std::min(th,tw*area.h/area.w);
        d.image({art::ports},{Zone[port]*tw+(tw-width)/2,(th-height)/2,width,height},area);
    }
    void button(Draw& d, Rect r, Text text, int action, bool accent=false, bool inactive=false,
                Text price={}) {
        bool chosen=pad_focus && button_count==focused && !inactive;
        skin_box(d,r,1,3);
        if (inactive) d.darken(r,7);
        if (chosen || accent) {
            d.fill({r.x+3,r.bottom()-4,r.w-6,1},chosen ? mint : rgb(0x87734e));
            const int icon=chosen ? 12 : 9;
            if (r.w-measure(text)>icon+6)
                nav_icon(d,CursorIcon,{r.x+3,r.y+(r.h-icon)/2,icon,icon});
        }
        const auto color=inactive ? quiet : chosen ? mint : accent ? gold : ivory;
        if (price.empty()) d.center(r,text,color);
        else {
            d.center({r.x,r.y+1,r.w,14},text,color);
            d.center({r.x,r.y+15,r.w,14},price,inactive ? quiet : mist);
        }
        if (!inactive && button_count<int(buttons.size())) buttons[button_count++]={r,action};
    }
    void pair(Draw& d, Rect r, Text a, int aa, Text b, int bb, bool accent=false) {
        const int half=(r.w-5)/2;
        button(d,{r.x,r.y,half,r.h},a,aa,accent);
        button(d,{r.x+half+5,r.y,r.w-half-5,r.h},b,bb);
    }
    void header(Draw& d) {
        skin_box(d,{safe.x,safe.y,safe.w,34},0,5);
        auto stat=[&](int x, int icon, int value, Color c) {
            nav_icon(d,icon,{x,safe.y+4,14,14});
            const auto text=format("%d",value);
            d.label(x+16,safe.y+4,text,c);
            return x+20+measure(text);
        };
        int x=stat(safe.x+5,CoinsIcon,game.s.gold,gold);
        x=stat(x,HullIcon,game.s.hull,ivory);
        const auto date=format("第%d日",game.s.day+1);
        const int date_x=safe.right()-6-measure(date);
        const auto stores=format("%d/%d",game.s.stores,int(game.s.water));
        const auto compact=format("粮%d 水%d",game.s.stores,int(game.s.water));
        const bool fits=x+20+measure(stores)<=date_x;
        if (fits) {
            nav_icon(d,FoodIcon,{x,safe.y+4,14,14});
            d.label(x+16,safe.y+4,stores,ivory);
        }
        d.label(date_x,safe.y+4,date,mist);
        const int status_width=safe.w-(fits?80:measure(compact)+15);
        if (save_failed && !save_active)
            d.label(safe.x+6,safe.y+18,"存档失败，正在重试",vermilion,status_width);
        else if (modal!=EventReport && notice_until && !game.notice.empty())
            d.label(safe.x+6,safe.y+18,game.notice,mint,status_width);
        else {
            const auto& p=Places[game.s.origin];
            d.label(safe.x+6,safe.y+18,game.sea()?"航行中":p.name,quiet,status_width);
            if (saved_until && fits) d.label(safe.right()-108,safe.y+18,"已存档",quiet);
        }
        if (fits) meter(d,{safe.right()-60,safe.y+20,55,9},game.s.hull,100,false);
        else d.label(safe.right()-6-measure(compact),safe.y+18,compact,mint);
    }
    void footer(Draw& d) {
        skin_box(d,{safe.x,safe.bottom()-30,safe.w,30},0,5);
        for (int i=0;i<5;++i) {
            const int left=safe.x+i*safe.w/5, right=safe.x+(i+1)*safe.w/5;
            const Rect r{left,safe.bottom()-29,right-left,29};
            const bool focused_tab=pad_focus && button_count==focused;
            const bool active=screen==i;
            if (active) {
                d.fill({r.x+3,r.y+3,r.w-6,r.h-6},rgb(0x25484a));
                d.fill({r.x+r.w/2-7,r.bottom()-3,14,1},rgb(0xcfb477));
            }
            if (focused_tab)
                nav_icon(d,CursorIcon,{r.x+r.w/2-5,r.y-6,10,10});
            const Color label=focused_tab ? mint : active ? gold : ivory;
            if (r.w>=53) {
                const int total=17+measure(tabs[i]);
                const int x=r.x+(r.w-total)/2;
                nav_icon(d,i,{x,r.y+5,16,16});
                d.label(x+17,r.y+7,tabs[i],label);
            } else {
                nav_icon(d,i,{r.x+r.w/2-7,r.y+1,14,14});
                d.label(r.x+(r.w-measure(tabs[i]))/2,r.y+14,tabs[i],label);
            }
            buttons[button_count++]={r,OpenChart+i};
        }
    }

    void start_screen(Draw& d) {
        d.cover({art::harbor},d.clip);
        d.darken(d.clip,5);
        const Image logo{art::wordmark};
        const int logo_w=std::min(360,safe.w-24);
        const int logo_h=logo.height()*logo_w/logo.width();
        const int logo_y=safe.y+std::max(24,safe.h/4);
        d.image(logo,{0,0,logo.width(),logo.height()},
                {safe.x+(safe.w-logo_w)/2,logo_y,logo_w,logo_h},true);
        int width=std::min(216,safe.w-12);
        button(d,{safe.x+(safe.w-width)/2,safe.bottom()-42,width,36},
               !loaded ? "读取航海日志" : had_save ? "继续航行" : "启航",Begin,true,!loaded);
    }

    Point map_point(Point p) const {
        return {map.x+map.w/2+int(std::lround((p.x-chart_x)*factor)),
                map.y+map.h/2+int(std::lround((p.y-chart_y)*factor))};
    }
    void atlas_background(Draw& d) {
        factor=std::min(float(map.w)/ChartWidth,float(map.h)/ChartHeight)*
               (zoom==0 ? 1.0f : std::pow(1.52f,float(zoom)));
        chart_x=std::clamp(chart_x,0.f,float(ChartWidth));
        chart_y=std::clamp(chart_y,0.f,float(ChartHeight));
        const auto tl=map_point({0,0}), br=map_point({ChartWidth,ChartHeight});
        const Rect chart_area{tl.x,tl.y,br.x-tl.x,br.y-tl.y};
        d.fill(map,ocean);
        constexpr auto uncharted=rgb(0x293c3c), grid=rgb(0x374b49);
        d.fill(chart_area,uncharted);
        const int left=std::max(0,int((chart_x-map.w/(2*factor))/ChartCell)-1);
        const int right=std::min(ChartColumns-1,int((chart_x+map.w/(2*factor))/ChartCell)+1);
        const int top=std::max(0,int((chart_y-map.h/(2*factor))/ChartCell)-1);
        const int bottom=std::min(ChartRows-1,int((chart_y+map.h/(2*factor))/ChartCell)+1);
        for (int x=(left+3)/4*4;x<=right;x+=4)
            d.fill({map_point({x*ChartCell,0}).x,tl.y,1,chart_area.h},grid);
        for (int y=(top+3)/4*4;y<=bottom;y+=4)
            d.fill({tl.x,map_point({0,y*ChartCell}).y,chart_area.w,1},grid);

        // Keep the texture transform fixed while clipping the blit to explored spans.
        const auto clip=d.clip;
        for (int y=top;y<=bottom;++y) {
            int x=left;
            while (x<=right) {
                if (!game.chart_cell(x,y)) { ++x; continue; }
                const int first=x++;
                while (x<=right && game.chart_cell(x,y)) ++x;
                const auto a=map_point({first*ChartCell,y*ChartCell});
                const auto b=map_point({std::min(x*ChartCell,ChartWidth),
                                        std::min((y+1)*ChartCell,ChartHeight)});
                d.clip=d.clipped({a.x,a.y,b.x-a.x,b.y-a.y});
                if (d.clip.w && d.clip.h) d.image({art::chart},{0,0,ChartWidth,ChartHeight},chart_area);
                d.clip=clip;
            }
        }
    }
    void draw_wake(Draw& d) {
        constexpr auto wake_color=rgb(0x61aabd);
        for (int i=1;i<game.s.wake_count;++i)
            d.line(map_point(game.wake_point(i-1)),map_point(game.wake_point(i)),wake_color);
    }
    void draw_rumor_marks(Draw& d) {
        for (int id=0;id<RumorCount;++id) {
            const auto& rumor=Rumors[id];
            if (!game.heard_rumor(id) || game.charted(rumor.site)) continue;
            const auto point=map_point(rumor.area);
            const int radius=std::max(6,int(24*factor));
            const Color color=id==pinned_rumor?gold:quiet;
            const Point north{point.x,point.y-radius}, south{point.x,point.y+radius};
            const Point east{point.x+radius,point.y}, west{point.x-radius,point.y};
            d.line(north,east,color,true);
            d.line(east,south,color,true);
            d.line(south,west,color,true);
            d.line(west,north,color,true);
            d.label(point.x-3,point.y-7,"?",color);
        }
    }
    void draw_routes(Draw& d, const Passage& planned, bool active) {
        for (int l=0;l<planned.count;++l) {
            const auto& leg=Legs[planned.edge[l]];
            const auto color = active && l < game.s.leg ? quiet : gold;
            for (int i=1;i<leg.count;++i) {
                const auto a=map_point(Waypoints[leg.offset+i-1]), b=map_point(Waypoints[leg.offset+i]);
                if ((a.x<map.x && b.x<map.x) || (a.x>=map.right() && b.x>=map.right()) ||
                    (a.y<map.y && b.y<map.y) || (a.y>=map.bottom() && b.y>=map.bottom())) continue;
                d.line(a,b,color,true);
            }
        }
    }
    void draw_map(Draw& d, int landmark, int berth) {
        const auto before=d.clip;
        d.clip=map;
        float fraction=game.sea() && !game.paused && !game.encounter() ? float(now-last_day)/DayMs : 0.f;
        const auto ship=game.position(std::clamp(fraction,0.f,1.f));
        if (game.sea() && follow) center_at(ship);
        atlas_background(d);
        const auto path=game.sea() ? game.route : passage(game.s.origin,selected);
        draw_routes(d,path,game.sea());
        draw_wake(d);
        draw_rumor_marks(d);
        if (landmark>=0) {
            const auto location=game.position();
            if (approach_port!=landmark || approach_position.x!=location.x || approach_position.y!=location.y) {
                approach=game.harbor_approach(landmark,chart_water);
                approach_port=landmark;
                approach_position=location;
            }
            for (int i=1;i<approach.count;++i)
                d.line(map_point(approach.points[i-1]),map_point(approach.points[i]),gold,true);
        }

        struct Geo { int lon,lat; Text label; bool land; };
        constexpr Geo names[] = {
            {-25,15,"大西洋",false}, {-5,24,"撒哈拉",true},
            {14,39,"地中海",false}, {3,-2,"几内亚湾",false},
            {46,5,"印度洋",false}, {16,-10,"非洲",true},
            {20,51,"欧洲",true}, {42,-21,"莫桑比克海峡",false},
            {-23,0,"赤道",false}, {-19,65,"冰岛",true}, {18,63,"斯堪的纳维亚",true}
        };
        std::array<Rect,64> placed{};
        int labels=0;
        const Rect compass{map.right()-32,map.y+4,26,26};
        placed[labels++]=compass;
        auto draw_label=[&](Point p, Text text, Color color, bool coast, bool force) {
            Rect r{p.x,p.y,measure(text)+4,16};
            if (r.x<map.x+1 || r.y<map.y+1 || r.right()>map.right()-1 || r.bottom()>map.bottom()-1) return;
            if (!force) {
                for (int i=0;i<labels;++i)
                    if (r.x<placed[i].right() && r.right()>placed[i].x &&
                        r.y<placed[i].bottom() && r.bottom()>placed[i].y) return;
            }
            if (labels<int(placed.size())) placed[labels++]=r;
            if (coast) {
                constexpr auto shadow=rgb(0x25423d);
                d.label(p.x+1,p.y+1,text,shadow);
                d.label(p.x+3,p.y+1,text,shadow);
                d.label(p.x+2,p.y+2,text,shadow);
                d.label(p.x+2,p.y,text,shadow);
            }
            d.label(p.x+2,p.y+1,text,color);
        };
        if (landmark>=0 && berth<0 && approach.count>1) {
            const auto a=approach.points[0], b=approach.points[1];
            const int bearing=direction_to({b.x-a.x,b.y-a.y});
            const auto message=format("%s%s",approach.direct?"锚地向":"绕浅滩向",Bearings[bearing].data());
            draw_label({map.x+4,map.bottom()-19},message,gold,true,false);
        }
        for (int i=0;i<PlaceCount;++i) {
            if (!game.charted(i)) continue;
            const auto p=map_point(Places[i].map);
            if (!map.contains(p)) continue;
            const bool landed=game.s.visited&(1U<<i);
            auto c=i==target()?gold:i>=PortCount?mint:landed?ivory:quiet;
            d.diamond(p,i==selected ? 6 : i==target() ? 4 : 3,ink);
            d.diamond(p,i==selected ? 4 : 2,c);
        }
        for (const int i : {selected,target(),game.s.origin}) {
            if (!game.charted(i)) continue;
            const auto p=map_point(Places[i].map);
            const auto label=Places[i].name;
            int left=p.x+7;
            if (left+measure(label)+4>map.right()) left=p.x-7-measure(label)-4;
            const int top=std::clamp(p.y-17,map.y+2,map.bottom()-17);
            if (p.y<map.y || p.y>=map.bottom()) continue;
            draw_label({left,top},label,i==selected ? gold : ivory,true,false);
        }
        for (auto& n:names) {
            const Point source{(n.lon+45)*7,(ChartNorth-n.lat)*8};
            if (!game.chart_cell(source.x/ChartCell,source.y/ChartCell)) continue;
            auto p=map_point(source);
            draw_label({p.x-measure(n.label)/2,p.y},n.label,n.land ? rgb(0x293e37) : mist,false,false);
        }
        if (zoom>=3) for (int i=0;i<PlaceCount;++i) {
            if (!game.charted(i) || i==selected || i==target() || i==game.s.origin) continue;
            const auto p=map_point(Places[i].map);
            if (map.contains(p)) draw_label({p.x+4,p.y-14},Places[i].name,ivory,true,false);
        }
        if (game.sea()) {
            auto p=map_point(ship);
            int frame=(now/115)%12;
            const auto heading=game.position(.9f);
            const bool west=game.s.helm?Directions[game.s.heading].x<0:heading.x<ship.x;
            d.image({art::caravel},{frame*36,0,36,27},{p.x-18,p.y-20,36,27},true,west);
            if (game.s.helm) {
                const auto bearing=Directions[game.s.heading];
                const Point end{p.x+bearing.x,p.y+bearing.y};
                d.line(p,end,mint);
                d.diamond(end,2,mint);
            }
        } else {
            auto p=map_point(Places[game.s.origin].map);
            d.line({p.x-3,p.y+5},{p.x+3,p.y+5},mint);
        }
        if (landmark>=0) {
            const auto mark=map_point(Anchor[landmark]);
            d.diamond(mark,5,ink);
            d.diamond(mark,3,berth>=0?mint:gold);
            d.dot(mark.x,mark.y,ink);
        }
        nav_icon(d,CompassIcon,compass);
        d.clip=before;
        d.frame(map,border);
    }

    void chart(Draw& d) {
        if (game.encounter()) { encounter_screen(d); return; }
        const bool wide=body.w>=body.h+70;
        const int pane=std::clamp(body.w/3,95,170);
        Rect info{};
        if (wide) {
            map={body.x,body.y,body.w-pane-5,body.h-29};
            info={map.right()+5,body.y,pane,body.h};
        } else {
            const int card=game.s.helm?86:74;
            map={body.x,body.y,body.w,body.h-card-31};
            info={body.x,map.bottom()+31,body.w,card};
        }
        if (map.h<35) map.h=35;
        const int berth=game.s.helm?game.nearby_anchorage(chart_water):-1;
        const int landmark=berth>=0?berth:game.s.helm?game.visible_landmark():-1;
        draw_map(d,landmark,berth);

        const int icon_w=28, y=map.bottom()+3;
        button(d,{map.x,y,icon_w,24},"+",ZoomIn);
        button(d,{map.x+icon_w+3,y,icon_w,24},"-",ZoomOut);
        button(d,{map.x+2*(icon_w+3),y,36,24},landmark>=0 && zoom<5?"近岸":"定位",Center);
        button(d,{map.x+2*(icon_w+3)+39,y,map.w-2*(icon_w+3)-39,24},
               game.s.helm ? "靠泊" : game.sea() ? (game.s.stop_next ? "取消停靠" : "下站停靠") : "目的地",
               game.s.helm ? DockHelm : game.sea() ? NextStop : RouteList,game.s.stop_next || berth>=0,
               game.s.helm && berth<0);
        if (game.s.helm) { helm_panel(d,info,wide,landmark); return; }

        skin_box(d,info,0,5);
        const int p=game.sea()?game.s.destination:selected;
        const auto route=game.sea()?game.route:passage(game.s.origin,p);
        const auto forecast=game.sea()?PassageForecast{}:game.forecast(route);
        const bool shortage=forecast.provisions>game.s.stores || forecast.water>game.s.water;
        const bool local_study=game.expedition_here()>=0;
        const bool scouting=scout_course.has_value() && !game.sea();
        const int route_info=route.count ? ShowPreflight : scouting ? ShowRumors : ShowIntel;
        const Text route_label=route.count ? "航前" : scouting ? "传闻" : "地理";
        const Text visit=scouting ? "出海" : story_here() ? "会面" : local_study ? "考察" :
                         p>=PortCount ? "测绘" : "探索";
        const int visit_action=story_here() ? ReadStory : local_study ? OpenObservation :
                               p>=PortCount ? Survey : StartExplore;
        const int visit_command=scouting?StartExplore:visit_action;
        const auto aim=scout_course.value_or(Anchor[p]);
        const auto course=format("拟向%s",Bearings[direction_to(
                {aim.x-Anchor[p].x,aim.y-Anchor[p].y})].data());
        d.label(info.x+5,info.y+4,scouting?(pinned_rumor>=0?"传闻海域":"自由出海"):Places[p].name,gold,info.w-10);
        if (wide) {
            if (!game.sea()) d.label(info.x+5,info.y+22,scouting?Text(course):Places[p].region,quiet,info.w-10);
            if (game.sea()) {
                const int wind=game.wind(game.s.leg);
                const auto progress=format("%d/%d  %s",game.s.leg+1,game.route.count,
                                           wind>5 ? "顺风" : wind<-5 ? "逆风" : "风平");
                d.label(info.x+5,info.y+22,progress,mist);
                meter(d,{info.x+5,info.y+39,info.w-10,12},miles_done(route),route.miles,true);
                button(d,{info.x+4,info.bottom()-80,info.w-8,23},
                    game.s.reefed ? "张帆" : "缩帆",Trim);
                button(d,{info.x+4,info.bottom()-52,info.w-8,23},
                    game.paused ? "继续" : "停船",Pause,true);
                button(d,{info.x+4,info.bottom()-26,info.w-8,22},"掉头",Retreat,false,!game.paused);
            } else {
                if (route.count) {
                    d.label(info.x+5,info.y+42,format("%d 海里",route.miles),mist,info.w-10);
                    const auto duration=format("约 %d 日",forecast.days);
                    d.label(info.x+5,info.y+59,shortage ? Text("水粮不足") : Text(duration),
                            shortage?vermilion:mint,info.w-10);
                } else if (story_here())
                    d.label(info.x+5,info.y+47,"有新剧情",mint,info.w-10);
                button(d,{info.x+4,info.bottom()-54,info.w-8,23},route_label,route_info);
                button(d,{info.x+4,info.bottom()-27,info.w-8,23},route.count?"启航":visit,
                       route.count?Sail:visit_command,true);
            }
        } else {
            if (game.sea()) {
                d.label(info.x+5,info.y+22,format("航段 %d/%d  %s",game.s.leg+1,
                    game.route.count,game.wind(game.s.leg)>5 ? "顺风" : game.wind(game.s.leg)<-5 ? "逆风" : "风平"),
                    mist,info.w-10);
                meter(d,{info.right()-80,info.y+22,73,12},miles_done(route),route.miles,true);
                const int w=(info.w-14)/3;
                button(d,{info.x+4,info.y+43,w,25},game.s.reefed ? "张帆" : "缩帆",Trim);
                button(d,{info.x+7+w,info.y+43,w,25},game.paused ? "继续" : "停船",Pause,true);
                button(d,{info.x+10+w*2,info.y+43,w,25},"掉头",Retreat,false,!game.paused);
            } else {
                const auto summary=format("%d海里  %d日",route.miles,forecast.days);
                const Text detail=shortage ? Text("水粮不足") : Text(summary);
                d.label(info.x+5,info.y+22,route.count ? detail : scouting?Text(course):Places[p].region,
                        shortage?vermilion:mist,info.w-10);
                pair(d,{info.x+4,info.y+43,info.w-8,25},route.count?"启航":visit,
                     route.count?Sail:visit_command,route_label,route_info,true);
            }
        }
    }

    void helm_panel(Draw& d, Rect info, bool wide, int landmark) {
        skin_box(d,info,0,5);
        const auto state=game.conditions();
        const auto bearing=format("航向%s",Bearings[game.s.heading].data());
        constexpr Text compact_weather[]{"晴","热","雨","阵"};
        const auto breeze=format("%s→%s%d级",
                                  (wide?compact_weather[state.weather]:SeaWeatherNames[state.weather]).data(),
                                  Bearings[state.direction].data(),state.strength);
        d.label(info.x+5,info.y+4,landmark>=0?Places[landmark].name:Text("自由掌舵"),gold,info.w-10);
        if (wide) {
            d.label(info.x+5,info.y+22,bearing,ivory,info.w-10);
            d.label(info.x+5,info.y+41,breeze,mist,info.w-10);
            pair(d,{info.x+4,info.bottom()-78,info.w-8,21},"左舵",SteerLeft,"右舵",SteerRight);
            button(d,{info.x+4,info.bottom()-52,info.w-8,22},game.s.reefed?"张帆":"缩帆",Trim);
            button(d,{info.x+4,info.bottom()-26,info.w-8,22},game.paused?"继续":"停船",Pause,true);
        } else {
            d.label(info.x+5,info.y+24,bearing,ivory,info.w-10);
            d.label(info.x+5,info.y+43,breeze,mist,info.w-10);
            const Text names[]{"左舵","右舵",game.s.reefed?"张帆":"缩帆",game.paused?"继续":"停船"};
            const int actions[]{SteerLeft,SteerRight,Trim,Pause};
            const int step=(info.w-7)/4;
            for (int i=0;i<4;++i)
                button(d,{info.x+4+i*step,info.bottom()-25,step-3,21},names[i],actions[i],i==3);
        }
    }

    int market_rows() const { return std::clamp((body.h-72)/19,2,8); }

    void market(Draw& d) {
        if (!game.port()) {
            no_port(d,"靠港后才能交易");
            return;
        }
        const auto& s=game.s;
        const int rows=market_rows();
        const int pages=(GoodsCount+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        const int x=body.x, y=body.y, w=body.w;
        skin_box(d,body,0,6);
        d.label(x+5,y+2,format("船舱 %d/%d",game.load(),game.capacity()),gold);
        const int right=x+w-4;
        if (w>=235) d.label(right-139,y+2,"购入 / 卖出",quiet);
        button(d,{right-44,y+1,42,17},"比价",ShowPortPrices);
        for (int i=0; i<rows && page*rows+i<GoodsCount; ++i) {
            const int g=page*rows+i;
            const Rect item{x+3,y+19+i*19,w-6,18};
            const bool foc=pad_focus && button_count==focused;
            d.fill(item,foc ? rgb(0x3b5550) : g==good ? light : (i%2 ? rgb(0x1e3b3f) : panel));
            if (foc || g==good) nav_icon(d,CursorIcon,{item.x+1,item.y+4,10,10});
            goods_icon(d,g,{item.x+13,item.y,18,18});
            d.label(item.x+34,item.y+2,GoodsNames[g],g==good ? gold : ivory);
            d.label(item.x+83,item.y+2,format("%d",s.cargo[g]),mint);
            const auto price=format("%d / %d",game.quote(g,false),game.quote(g,true));
            d.label(right-measure(price)-5,item.y+2,price,ivory);
            buttons[button_count++]={item,GoodsRow+g};
        }
        const auto note=format("选中 %s  库存 %d",GoodsNames[good].data(),game.stock(good));
        d.label(x+4,body.bottom()-51,note,mist,w-(pages>1?72:10));
        if (pages>1) {
            button(d,{right-56,body.bottom()-54,25,23},"<",Previous,false,page==0);
            button(d,{right-27,body.bottom()-54,25,23},">",Next,false,page+1==pages);
        }
        const int step=w/4;
        for (int i=0;i<4;++i) {
            const bool sell=i>=2;
            const int batch=i%2==0 ? 1 : 5;
            const auto offer=game.trade_quote(good,sell,batch);
            const auto label=format("%s%d",sell?"卖":"买",offer.units ? offer.units : batch);
            const auto price=format("%d%s",offer.gold,step>=48?"金":"");
            button(d,{x+i*step,body.bottom()-31,step-3,30},label,BuyOne+i,i==0,!offer.units,
                   offer.units ? Text(price) : Text("—"));
        }
    }

    void no_port(Draw& d, Text reason) {
        skin_box(d,body,0,6);
        d.center({body.x,body.y,body.w,body.h-27},reason,mist);
        button(d,{body.x+5,body.bottom()-30,body.w-10,25},"返回海图",OpenChart,true);
    }
    void shipyard(Draw& d) {
        if (!game.port()) { no_port(d,"靠港后才能整备船只"); return; }
        const auto& s=game.s;
        skin_box(d,body,0,6);
        d.label(body.x+5,body.y+4,ship_names[s.ship],gold,body.w-82);
        button(d,{body.right()-71,body.y+2,67,20},"副官",ShowOfficers,s.first_mate!=NoOfficer);
        d.label(body.x+5,body.y+22,format("船员 %d/%d  士气 %d  火炮 %d",s.crew,
            game.crew_limit(),s.spirits,s.guns),mist,body.w-10);
        int height=std::clamp((body.h-36)/4-3,20,34);
        const auto fill=format("补给 %d金",game.ration_cost());
        const auto repair=format("维修 %d金",game.repair_cost());
        const auto guns=format("加炮 %d金",145+s.guns*120);
        const auto upgrade=format("换船 %d金",s.ship==0?800:1850);
        const Text titles[]{fill,repair,"休整 24金","雇水手",s.ship<2?Text(upgrade):"已是商船",
                            s.guns<3?Text(guns):"火炮已满","应急信贷","食物淡水"};
        for (int i=0;i<8;++i) {
            int x=body.x+(i%2)*(body.w/2+2);
            button(d,{x,body.y+37+(i/2)*(height+3),body.w/2-3,height},titles[i],
                   i==7?OpenProvisions:DockService+i,i==0,(i==4 && s.ship==2)||(i==5 && s.guns==3));
        }
    }
    void office(Draw& d) {
        if (game.sea()) { no_port(d,"到港或抵达测绘点后办理"); return; }
        const auto& p=Places[game.s.origin];
        const bool valid_contract=game.s.contract>=0 && game.s.day<=game.s.deadline;
        skin_box(d,body,0,6);
        const int archive=game.archive_here();
        const bool observation=game.expedition_here()>=0;
        int banner=34;
        if (game.port()) {
            banner=std::clamp(body.h-(archive>=0?119:100)-(valid_contract?7:0),35,190);
            Rect display{body.x+4,body.y+4,body.w-8,banner-2};
            port_scene(d,display,game.s.origin);
            d.darken({display.x,display.y,display.w,std::min(34,display.h)},9);
        }
        d.label(body.x+9,body.y+6,p.name,gold,body.w-18);
        d.label(body.x+9,body.y+22,p.region,ivory,body.w-18);
        const int y=body.y+banner+8;
        if (game.port()) {
            const int dest=valid_contract ? game.s.contract : game.proposal();
            const int item=valid_contract ? game.s.contract_good : game.proposal_good();
            const auto offer=format("委托  %s ×3  →  %s",GoodsNames[item].data(),Places[dest].name.data());
            d.wrap({body.x+8,y,body.w-16,32},offer,ivory);
            const int contract_y=y+(measure(offer)<=body.w-16?16:31);
            if (valid_contract && contract_y+14<=body.bottom()-(archive>=0?75:51))
                d.label(body.x+8,contract_y,format("第 %d 日前  赏金 %d",game.s.deadline+1,game.reward(dest)),mist,body.w-10);
            const int split=body.w/2;
            const int local_tale=game.tale_here();
            if (archive>=0)
                button(d,{body.x+3,body.bottom()-75,body.w-6,21},
                       game.s.expeditions[archive].stage==HasEvidence?"档案室 · 交考察记录":"档案室",
                       OpenArchive,true);
            button(d,{body.x+3,body.bottom()-51,split-6,21},"港口商会",OpenDesk);
            if (local_tale>=0)
                button(d,{body.x+split+2,body.bottom()-51,body.w-split-5,21},
                       Tales[local_tale].title,PortTaleAction,true);
            button(d,{body.x+3,body.bottom()-26,split-6,24},
                valid_contract ? "交付委托" : "接行会委托",valid_contract ? Deliver : Accept,false,
                valid_contract && game.s.origin!=dest);
            button(d,{body.x+split+2,body.bottom()-26,body.w-split-5,24},
                story_here() ? "会面" : "等候一日",story_here() ? ReadStory : WaitDay,story_here());
        } else {
            d.wrap({body.x+5,y,body.w-10,body.h-70},p.note,mist);
            pair(d,{body.x+4,body.bottom()-28,body.w-8,25},"测绘",Survey,
                 story_here() ? "读蜡封信" : game.tow_needed() ? "请求拖带" : observation ? "考察" : "海图",
                 story_here()?ReadStory:game.tow_needed()?ShowTow:observation?OpenObservation:OpenChart,true);
            if (story_here() && game.tow_needed())
                button(d,{body.x+5,body.bottom()-52,75,21},"请求拖带",ShowTow);
        }
    }
    void journal(Draw& d) {
        const auto& s=game.s;
        const int half=(body.w-13)/2;
        skin_box(d,body,0,6);
        d.label(body.x+5,body.y+2,format("声望 %d  信任 %d  测绘 %d/%d",s.renown,s.trust,
                s.charts,PlaceCount-PortCount),gold,body.w-10);
        d.label(body.x+5,body.y+23,"主线",quiet);
        if (s.ending) {
            d.label(body.x+58,body.y+22,s.ending==1 ? "航图为众人所有" : "商号的独家合约",ivory,body.w-63);
            pair(d,{body.x+4,body.y+64,body.w-8,25},"重读结局",ShowEnding,"考察簿",ShowExpeditions,true);
        } else {
            d.label(body.x+58,body.y+23,Story[s.chapter].title,ivory,body.w-63);
            d.label(body.x+5,body.y+44,format("下一站  %s",Places[target()].name.data()),mist,body.w-10);
            button(d,{body.x+4,body.y+64,half,25},
                   story_here() ? "会面" : "在海图上标记",story_here()?ReadStory:Destination+target(),true,game.sea());
            button(d,{body.x+9+half,body.y+64,body.w-13-half,25},"考察簿",ShowExpeditions);
        }
        if (s.contract>=0 && body.h>=177)
            d.label(body.x+5,body.y+99,format("委托  %s / %s",
                GoodsNames[s.contract_good].data(),Places[s.contract].name.data()),mist,body.w-10);
        pair(d,{body.x+4,body.bottom()-53,body.w-8,23},"港口故事",ShowTales,"海上纪事",ShowSeaLog);
        pair(d,{body.x+4,body.bottom()-27,body.w-8,23},"发现图鉴",ShowAtlas,"操作帮助",ShowHelp);
    }
    void officer_portrait(Draw& d, int officer, Rect viewport) {
        d.image({art::officers},{(officer-1)*96,0,96,96},viewport);
        d.frame(viewport,border);
    }
    void officer_list(Draw& d) {
        page_title(d,"副官名册");
        const int row_h=std::clamp((body.h-34)/(OfficerLimit-1),29,46);
        for (int id=Pilot;id<OfficerLimit;++id) {
            const auto& person=Officers[id];
            const bool on_duty=game.s.first_mate==id;
            const bool focused_row=pad_focus && button_count==focused;
            const Rect item{body.x+6,body.y+28+(id-1)*row_h,body.w-12,row_h-3};
            skin_box(d,item,on_duty?1:0,4);
            if (focused_row) d.frame(item,mint);
            const int size=item.h-4;
            officer_portrait(d,id,{item.x+3,item.y+2,size,size});
            const Text state=on_duty?"在任":game.has_officer(id)?"候任":Places[person.port].name;
            const int badge=measure(state);
            d.label(item.x+size+8,item.y+(item.h-14)/2,person.name,on_duty?gold:ivory,
                    item.w-size-badge-17);
            d.label(item.right()-6-badge,item.y+(item.h-14)/2,state,on_duty?mint:mist);
            buttons[button_count++]={item,OfficerEntry+id};
        }
        if (body.h>28+(OfficerLimit-1)*row_h+22)
            d.label(body.x+7,body.bottom()-20,"靠港可换任一位副官",quiet,body.w-14);
    }
    void officer_page(Draw& d) {
        const auto& person=Officers[officer_item];
        const bool aboard=game.has_officer(officer_item);
        const bool on_duty=game.s.first_mate==officer_item;
        page_title(d,person.name);
        const int size=std::min({96,body.w/3,body.h-63});
        const Rect portrait{body.x+7,body.y+29,size,size};
        officer_portrait(d,officer_item,portrait);
        const int x=portrait.right()+7, y=body.y+29, w=body.right()-7-x;
        d.label(x,y,person.specialty,gold,w);
        const Text status=on_duty?"在任副官":aboard?"随船候任":Places[person.port].name;
        d.label(x,y+17,status,on_duty?mint:mist,w);
        const int text_y=y+35;
        d.wrap({x,text_y,w,body.bottom()-32-text_y},
               officer_talk?person.greeting:person.effect,ivory,15);

        const Rect actions{body.x+7,body.bottom()-28,body.w-14,24};
        const int half=(actions.w-5)/2;
        const auto fee=format("招募 %d金",game.hiring_fee(officer_item));
        const auto destination=format("去%s",Places[person.port].name.data());
        const bool local=game.s.origin==person.port;
        const Text action=aboard?(on_duty?"卸任":"任命副官"):local?Text(fee):Text(destination);
        button(d,{actions.x,actions.y,half,actions.h},action,aboard?OfficerDuty:local?OfficerHire:OfficerMark,
               true, !aboard && local && game.s.gold<game.hiring_fee(officer_item));
        button(d,{actions.x+half+5,actions.y,actions.w-half-5,actions.h},
               officer_talk?"查看专长":"交谈",OfficerTalk);
    }
    void encounter_screen(Draw& d) {
        const auto& s=game.s;
        const auto weather=s.weather;
        if (game.duel()) { duel_screen(d); return; }
        const auto arena=combat_backdrop(d,{art::storm},weather==Privateer);
        const int x=body.x+6, w=body.w-12;
        const auto& scene=Encounters[weather];
        d.label(x,body.y+5,game.convoy_battle() && weather==Privateer ?
                "银铃号护航 · 断铃号" : scene.title,gold,w);
        if (weather==Privateer) {
            d.label(x,body.y+23,format("船距%d  敌船%d  %s",s.range,s.enemy,
                     s.needs_reload?"待装填":s.aim?"已瞄准":"可开炮"),ivory,w);
            const int wind=game.wind(s.leg);
            const auto threat=format("%s  %s",EnemyOrders[game.naval_intent()].data(),
                                     wind>5?"顺风":wind<-5?"逆风":"风平");
            d.label(x,body.y+40,threat,mist,w);
            const int n=(now/115)%12;
            const int ship_y=arena.y==body.y?body.y+53:arena.bottom()-36;
            d.image({art::caravel},{n*36,0,36,27},{x+6,ship_y,43,32},true);
            d.image({art::caravel},{n*36,0,36,27},{x+43+s.range*(w-92)/4,ship_y,43,32},true,true);
            if (flash_until) {
                const int f=std::clamp(7-int((flash_until-now)/74),0,7);
                d.image({art::broadside},{f*32,0,32,32},{x+31,ship_y-4,36,36},true);
            }
            combat_buttons(d,SailOrders);
            return;
        } else {
            const Text copy=weather==FalseBeacon && s.tales[Lamps]==Shared ?
                "渔会公开的潮表就在桌上。这回可以直接核对灯位，不用等到天亮。" : scene.message;
            d.wrap({x,body.y+29,w,body.h-66},copy,ivory);
        }
        const int gap=3, step=w/3;
        const auto price=format("买水%d金",game.water_price());
        for (int i=0;i<3;++i) {
            const auto label=weather==ReliefShip && i==1 ? Text(price) : scene.choices[i];
            button(d,{x+i*step,body.bottom()-33,step-gap,28},label,FightFirst+i,i==0,
                   !game.encounter_requirement(i).empty());
        }
    }

    Rect combat_backdrop(Draw& d, Image art, bool show_arena) {
        const auto previous=d.clip;
        d.clip=body;
        d.cover(art,body);
        d.darken(body,9);
        Rect arena=body;
        if (show_arena && body.h>body.w+36) {
            const int width=body.w-10;
            const int height=width*art.height()/art.width();
            const int space=body.h-132;
            arena={body.x+5,body.y+78+std::max(0,(space-height)/2),width,height};
            d.cover(art,arena);
            d.darken(arena,3);
            d.frame(arena,border);
        }
        d.clip=previous;
        d.frame(body,border);
        return arena;
    }
    void combat_buttons(Draw& d, const Text (&names)[4]) {
        const int width=(body.w-17)/2;
        for (int i=0;i<4;++i) {
            button(d,{body.x+6+(i%2)*(width+5),body.bottom()-50+(i/2)*24,width,22},
                   names[i],FightFirst+i,i==0,!game.encounter_requirement(i).empty());
        }
    }
    void duel_screen(Draw& d) {
        const bool ashore=game.s.weather==DockDuel;
        const auto arena=combat_backdrop(d,{ashore?art::cadiz_duel:art::boarding},true);
        const int x=body.x+6, w=body.w-12, y=body.y;
        d.label(x,y+5,ashore?"加的斯的旧誓":"接舷决斗",gold,w);
        const auto yours=format("体力 %d",game.s.vigor);
        const auto theirs=format("%s %d",ashore?"费罗":"对手",game.s.rival);
        const int half=(w-6)/2;
        d.label(x,y+24,yours,ivory,half);
        d.label(x+half+6,y+24,theirs,ivory,half);
        meter(d,{x,y+40,half,9},game.s.vigor,game.vigor_limit(),false);
        meter(d,{x+half+6,y+40,half,9},game.s.rival,game.rival_limit(),true);
        if (duel_flash_until) {
            const int frame=std::clamp(5-int((duel_flash_until-now)/70),0,5);
            const int offset=(duel_animation*6+frame)*48;
            const int effect_y=arena.y==y?y+49:arena.y+arena.h/2-18;
            d.image({art::duel_fx},{offset,0,48,36},{x+w/2-24,effect_y,48,36},true);
        }
        d.label(x,y+61,DuelIntent[game.duel_intent()],gold,w);
        combat_buttons(d,DuelOrders);
    }

    void page_title(Draw& d, Text name) {
        skin_box(d,body,0,6);
        d.label(body.x+6,body.y+5,name,gold,body.w-61);
        button(d,{body.right()-49,body.y+3,45,23},"返回",Close);
    }
    void set_modal(Overlay layer) {
        modal=layer;
        focused=layer==StoryPage || layer==EndingPage || layer==Routes || layer==TalePage || layer==EventReport ? 1 : 0;
        dirty=true;
    }
    int list_rows() const { return std::clamp((body.h-55)/23,2,10); }
    void row(Draw& d, int index, Text name, Text meta, int action, bool highlight=false, int top=28) {
        const Rect r{body.x+4,body.y+top+index*23,body.w-8,21};
        const bool marked=pad_focus && button_count==focused;
        d.fill(r,marked?rgb(0x5b6047):(index%2?light:ink));
        if (marked) nav_icon(d,CursorIcon,{r.x+2,r.y+4,12,12});
        const auto mw=measure(meta);
        const int left=marked?17:5;
        d.label(r.x+left,r.y+4,name,highlight ? gold : ivory,r.w-mw-left-8);
        if (mw) d.label(r.right()-mw-4,r.y+4,meta,mist);
        if (action) buttons[button_count++]={r,action};
    }
    void pager(Draw& d, int pages) {
        if (pages<=1) return;
        const int y=body.bottom()-24;
        button(d,{body.x+5,y,45,20},"上页",Previous,false,page==0);
        d.center({body.x+52,y,body.w-104,20},format("%d / %d",page+1,pages),mist);
        button(d,{body.right()-50,y,45,20},"下页",Next,false,page+1>=pages);
    }
    void route_list(Draw& d) {
        page_title(d,"目的地");
        button(d,{body.right()-129,body.y+3,37,23},"传闻",ShowRumors);
        button(d,{body.right()-89,body.y+3,37,23},"出海",StartExplore);
        std::array<int,PlaceCount> order{};
        int count=0;
        if (target()>=0 && target()!=game.s.origin) order[count++]=target();
        order[count++]=game.s.origin;
        for (int i=0;i<PlaceCount;++i)
            if (game.charted(i) && i!=game.s.origin && i!=target()) order[count++]=i;
        const int rows=list_rows(), pages=(count+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int p=order[page*rows+i];
            const auto path=passage(game.s.origin,p);
            auto meta=format("%d海里",path.miles);
            row(d,i,Places[p].name,p==game.s.origin ? "所在处" : p==target() ? "主线" : Text(meta),
                Destination+p,p==target());
        }
        pager(d,pages);
    }
    int supply_target(int need) const {
        return need+2*game.daily_rations();
    }
    void preflight(Draw& d) {
        page_title(d,"航前计划");
        const auto path=passage(game.s.origin,selected);
        const auto forecast=game.forecast(path);
        const int x=body.x+7, w=body.w-14;
        d.label(x,body.y+30,Places[selected].name,gold,w);
        d.label(x,body.y+48,format("%d海里  预计%d日",path.miles,forecast.days),ivory,w);
        const int surplus=std::min(game.s.stores-forecast.provisions,int(game.s.water)-forecast.water);
        const int ship_ok=game.s.hull>12;
        const bool ready=path.count && ship_ok && game.s.stores>=game.daily_rations() &&
                         game.s.water>=game.daily_water(Fair);
        d.label(x,body.y+65,format("粮 %d/%d  水 %d/%d",forecast.provisions,game.s.stores,
                                 forecast.water,int(game.s.water)),surplus<0?vermilion:mint,w);
        if (body.h>=160 && game.s.first_mate!=NoOfficer)
            d.label(x,body.y+84,format("当值副官  %s",Officers[game.s.first_mate].name.data()),quiet,w);
        const int third=(w-8)/3;
        button(d,{x,body.bottom()-54,third,23},"停靠点",ShowStops);
        button(d,{x+third+4,body.bottom()-54,third,23},"地理",ShowIntel);
        button(d,{x+(third+4)*2,body.bottom()-54,third,23},"掌舵",StartHelm,false,!ready);
        const int half=(w-5)/2;
        const int cost=game.resupply_cost(supply_target(forecast.provisions),supply_target(forecast.water));
        const auto supplement=format("补粮 %d金",cost);
        const auto repairs=format("维修 %d金",game.repair_cost());
        if (game.tow_needed()) button(d,{x,body.bottom()-27,half,23},"请求拖带",ShowTow);
        else if (!game.port()) button(d,{x,body.bottom()-27,half,23},"返海图",Close);
        else if (!ship_ok && game.s.gold>=game.repair_cost())
            button(d,{x,body.bottom()-27,half,23},repairs,DockService+1);
        else if (!ship_ok || (cost>game.s.gold && !ready))
            button(d,{x,body.bottom()-27,half,23},"去船坞",OpenShipyard);
        else button(d,{x,body.bottom()-27,half,23},cost?Text(supplement):"水粮已备",TopUp,false,
                    !cost || cost>game.s.gold);
        button(d,{x+half+5,body.bottom()-27,w-half-5,23},
               surplus<0 ? "仍要启航" : "启航",ConfirmSail,true,!ready);
    }
    void route_stops(Draw& d) {
        page_title(d,"航段与停靠点");
        const auto path=passage(game.s.origin,selected);
        const auto forecast=game.forecast(path);
        const int visible=list_rows(), pages=std::max(1,(path.count+visible-1)/visible);
        page=std::clamp(page,0,pages-1);
        for (int j=0;j<visible && page*visible+j<path.count;++j) {
            const int i=page*visible+j;
            const auto& leg=Legs[path.edge[i]];
            const int port=path.reverse[i]?leg.from:leg.to;
            const bool known=game.charted(port);
            const auto eta=format("%d日  %s",forecast.arrival_days[i],!known?"待查":port<PortCount?"可补给":"测绘点");
            row(d,j,known?Places[port].name:Text("未标记水域"),eta,known?Destination+port:0,port==selected);
        }
        pager(d,pages);
    }
    void port_prices(Draw& d) {
        const auto title=format("%s比价",GoodsNames[good].data());
        page_title(d,title);
        d.label(body.x+6,body.y+28,"已到访港口的当日买价 / 卖价",quiet,body.w-12);
        std::array<int,PortCount> order{};
        int count=0;
        for (int port=0;port<PortCount;++port)
            if (game.s.visited & (1U<<port)) order[count++]=port;
        std::sort(order.begin(),order.begin()+count,[&](int a,int b) {
            return game.price_at(a,good,true)>game.price_at(b,good,true);
        });
        const int rows=std::clamp((body.h-74)/23,1,9);
        const int pages=std::max(1,(count+rows-1)/rows);
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int port=order[page*rows+i];
            const auto prices=format("%d / %d",game.price_at(port,good,false),game.price_at(port,good,true));
            row(d,i,Places[port].name,prices,Destination+port,port==game.s.origin,47);
        }
        if (pages>1) pager(d,pages);
        else d.label(body.x+6,body.bottom()-22,"抵达后价格可能变化",quiet,body.w-12);
    }
    void provisions_page(Draw& d) {
        page_title(d,"食物与淡水");
        const int x=body.x+7, w=body.w-14, half=(w-5)/2;
        const int food=game.s.stores, water=game.s.water;
        const int food_cap=game.food_capacity(), water_cap=game.water_capacity();
        d.label(x,body.y+28,format("补给舱 %d/%d",food+water,2*game.max_stores()),mist,w);
        d.label(x,body.y+46,format("食物 %d/%d",food,food_cap),ivory,w);
        d.label(x,body.y+64,format("淡水 %d/%d",water,water_cap),ivory,w);
        if (w>=208) {
            const int bar=w/3;
            meter(d,{x+w-bar,body.y+50,bar,8},food,food_cap,false);
            meter(d,{x+w-bar,body.y+68,bar,8},water,water_cap,true);
        }
        button(d,{x,body.bottom()-54,half,20},"卸食物",UnloadFood,false,!food);
        button(d,{x+half+5,body.bottom()-54,w-half-5,20},"卸淡水",UnloadWater,false,!water);
        const auto food_cost=game.resupply_cost(food+10,water);
        const auto water_cost=game.resupply_cost(food,water+10);
        button(d,{x,body.bottom()-32,half,30},format("食物 +%d",std::min(10,food_cap-food)),
               BuyFood,true,!food_cost || game.s.gold<food_cost,format("%d金",food_cost));
        button(d,{x+half+5,body.bottom()-32,w-half-5,30},format("淡水 +%d",std::min(10,water_cap-water)),
               BuyWater,true,!water_cost || game.s.gold<water_cost,format("%d金",water_cost));
    }

    void event_report(Draw& d) {
        page_title(d,"遭遇结果");
        const int x=body.x+7, w=body.w-14;
        d.wrap({x,body.y+29,w,30},outcome.description,ivory,14);
        const Text names[]{"船体","食物","淡水","金币","货物","耗时"};
        const int changes[]{outcome.hull,outcome.stores,outcome.water,outcome.gold,outcome.cargo,outcome.day};
        int item=0;
        for (int i=0;i<6;++i) {
            if (!changes[i]) continue;
            const auto caption=format("%s %+d%s",names[i].data(),changes[i],i==5?"日":"");
            d.label(x+(w/2)*(item%2),body.y+59+16*(item/2),caption,
                    changes[i]>0 && i!=5 ? mint : gold,w/2-3);
            ++item;
        }
        button(d,{x,body.bottom()-27,w,24},game.sea()?"继续航行":"返回海图",ResumeAfterEvent,true);
    }
    void tow_request(Draw& d) {
        page_title(d,"请求拖带");
        d.wrap({body.x+7,body.y+31,body.w-14,body.h-61},
               "救助船会花一日把你拖到沿航路最近的港口。收费最多六十五金，货物照常运回。",
               ivory);
        button(d,{body.x+7,body.bottom()-27,body.w-14,24},"叫来救助船",ConfirmTow,true);
    }

    void port_desk(Draw& d) {
        page_title(d,"港口商会");
        const bool invested=game.guild_stake(game.s.origin);
        const auto cost=format("入股 %d金",game.guild_cost());
        row(d,0,"本港商栈",invested?"已入股":Text(cost),InvestPort,invested);
        d.label(body.x+7,body.y+52,"入股后在本港买货约省6%",mist,body.w-14);
        if (game.s.origin==lisbon || game.s.origin==madeira)
            row(d,2,"银铃护航","航海故事",ShowEscort,true);
        else if (game.s.origin==cadiz)
            row(d,2,"加的斯的旧誓",game.s.cadiz_victory?"已完成":"码头决斗",ShowDuel,true);
        else if (game.s.origin==tunis)
            row(d,2,"优素福","招募领航",Recruit,true);
        button(d,{body.x+6,body.bottom()-28,body.w-12,24},"打听传闻",ShowRumors);
    }
    void escort_page(Draw& d) {
        page_title(d,"银铃护航");
        const int x=body.x+7, w=body.w-14;
        const auto stage=game.s.escort;
        const int origin=game.s.origin;
        if (stage>=EscortPublished) {
            const Text copy=stage==EscortPublished?
                "修船人从公开的船契上认出了伪造的印章。银铃号从此在丰沙尔鸣铃入港，每次都等最后一艘渔船靠岸。" :
                "商号付了三百四十金。银铃号能继续做买卖，码头上有关那枚印章的传闻仍没有停。";
            d.wrap({x,body.y+34,w,body.h-39},copy,ivory,16);
            return;
        }
        if (stage==EscortOpen) {
            d.wrap({x,body.y+33,w,body.h-66},
                "伊内丝请你从里斯本护送银铃号去丰沙尔。有人拿着伪造的船契，想把载着木匠和修船料的小商船扣下。",
                ivory,15);
        } else if (stage==EscortSailing) {
            d.wrap({x,body.y+33,w,body.h-66},
                "银铃号在里斯本等你。从里斯本驶上通往丰沙尔的航段，就能与商船会合。若护航失散，可以回里斯本重试。",
                ivory,15);
        } else {
            const auto head=stage==EscortWonDuel?
                "断铃号船长交出了完整船契。"
                : "炮战过后，水手从漂箱里找到了盖错印的船契。";
            const auto copy=format("%s在丰沙尔公开它，木匠们就能作证。商号愿意出更多钱私下买走。",head);
            d.wrap({x,body.y+33,w,body.h-66},copy,ivory,15);
        }
        const Rect actions{x,body.bottom()-28,w,24};
        if (game.port() && origin==madeira && stage>=EscortWonGuns)
            pair(d,actions,"公开船契",DeliverPublic,"私下交付",DeliverPrivate,true);
        else if (game.port() && origin==lisbon && stage==EscortOpen)
            button(d,actions,"接下护航",AcceptEscort,true);
        else {
            const Text label=stage>=EscortWonGuns || (stage==EscortSailing && origin==lisbon) ?
                "标记丰沙尔" : "标记里斯本";
            button(d,actions,label,MarkEscort,true,game.sea());
        }
    }
    void duel_invitation(Draw& d) {
        page_title(d,"加的斯的旧誓");
        const int x=body.x+7, w=body.w-14;
        const Text copy=game.s.cadiz_victory ?
            "费罗把那枚旧印交给了裁判，并把避开突刺的步法教给你。往后的接舷决斗，你能承受更多次交锋。" :
            "旧印能证明费罗父亲的清白。费罗要先看看你能否保护证人。格挡克突刺，佯攻破格挡，突刺截佯攻。";
        d.wrap({x,body.y+33,w,body.h-66},copy,ivory,15);
        if (!game.s.cadiz_victory) {
            const bool local=game.port() && game.s.origin==cadiz;
            button(d,{x,body.bottom()-28,w,24},local?"接受比试":"标记加的斯",
                   local?BeginDuel:Destination+int(cadiz),true,game.sea());
        }
    }

    void story(Draw& d) {
        const auto& c=Story[game.s.chapter];
        page_title(d,c.title);
        const int portrait=std::clamp(body.h-98,34,74);
        d.image({art::portraits},{c.portrait*86,0,86,86},{body.x+5,body.y+28,portrait,portrait});
        d.frame({body.x+5,body.y+28,portrait,portrait},border);
        const int left=body.x+portrait+11;
        d.label(left,body.y+25,c.speaker,gold);
        const Text copy=game.s.chapter==1 && game.s.tales[Lamps]==Shared ?
            "多亏你的潮表，渔会知道昨夜哪盏灯有问题。你兄长留了半张货单给我。搜失踪渔船的话，抄表的船一日就能来回。" :
            c.message;
        d.wrap({left,body.y+43,body.right()-left-5,body.h-72},copy,ivory,15);
        pair(d,{body.x+4,body.bottom()-28,body.w-8,25},
             game.s.chapter==1 && game.s.tales[Lamps]==Shared ? "搜寻渔船 +1日" : c.first,
             ChooseFirst,c.second,ChooseSecond,true);
    }
    void tale_list(Draw& d) {
        page_title(d,"港口故事");
        const int visible=list_rows();
        const int pages=(TaleCount+2+visible-1)/visible;
        page=std::clamp(page,0,pages-1);
        for (int j=0;j<visible && page*visible+j<TaleCount+2;++j) {
            const int i=page*visible+j;
            if (i==TaleCount) {
                const Text harbor=game.s.escort>=EscortPublished?"已完成":
                                  game.s.escort>=EscortWonGuns?"丰沙尔":"里斯本";
                row(d,j,"银铃护航",harbor,ShowEscort,game.s.escort==EscortSailing);
                continue;
            }
            if (i==TaleCount+1) {
                row(d,j,"加的斯的旧誓",game.s.cadiz_victory?"已完成":"加的斯",ShowDuel);
                continue;
            }
            const auto stage=game.s.tales[i];
            const int port=game.tale_stop(i);
            row(d,j,Tales[i].title,port>=0 ? Places[port].name : "已完成",TaleEntry+i,stage==InProgress);
        }
        pager(d,pages);
    }
    void tale_page(Draw& d) {
        const auto& tale=Tales[tale_item];
        const auto stage=game.s.tales[tale_item];
        page_title(d,tale.title);
        const int x=body.x+7, w=body.w-14;
        if (stage>=Shared) {
            const Text outcome=stage==Shared ? tale.shared_end : tale.sold_end;
            d.wrap({x,body.y+34,w,body.h-39},outcome,ivory);
            return;
        }
        const bool beginning=stage==Unmet;
        const int port=game.tale_stop(tale_item);
        if (port!=game.s.origin || game.sea()) {
            d.label(x,body.y+32,Places[port].name,gold,w);
            if (!beginning) d.wrap({x,body.y+52,w,body.h-83},tale.opening,ivory);
            if (game.charted(port))
                button(d,{x,body.bottom()-28,w,25},"标记到访港口",TaleMark,true,game.sea());
            return;
        }
        const int portrait=beginning ? tale.start_portrait : tale.finish_portrait;
        d.image({art::portraits},{portrait*86,0,86,86},{x,body.y+28,19,19});
        d.label(x+24,body.y+29,beginning ? tale.starter : tale.listener,gold,w-24);
        const auto requirement=beginning ? Text{} : game.tale_requirement(tale_item);
        if (!requirement.empty()) d.label(x+w-56,body.y+29,tale_item==Chartmakers ? "缺木料" : "缺水粮",mist,56);
        const Text copy=tale_item==Lamps && !beginning && game.s.beacon!=BeaconCharted ?
            "莱昂可花一日访问归港的渔船，核对灯讯。你可让渔会公布潮表，或将它作价二百二十金卖给船行。" :
            beginning ? tale.opening : tale.delivery;
        d.wrap({x,body.y+47,w,body.h-77},copy,ivory,14);
        const Rect actions{x,body.bottom()-28,w,25};
        if (beginning) pair(d,actions,tale.accept,TaleShare,"稍后再说",Close,true);
        else {
            const int half=(w-5)/2;
            button(d,{x,actions.y,half,actions.h},tale.share,TaleShare,true,!requirement.empty());
            button(d,{x+half+5,actions.y,w-half-5,actions.h},tale.sell,TaleSell);
        }
    }
    void sea_log(Draw& d) {
        page_title(d,"海上纪事");
        const int x=body.x+7, w=body.w-14;
        if (sea_item>=0) {
            const auto entry=read_sea(game.s.sea_log[sea_item]);
            const auto& scene=Encounters[entry.kind];
            d.label(x,body.y+31,scene.title,gold,w);
            d.label(x,body.y+52,format("第 %d 日",entry.day+1),mist,w);
            d.wrap({x,body.y+73,w,body.h-76},scene.results[entry.choice],ivory);
            return;
        }
        if (!game.s.sea_log[0]) {
            d.label(x,body.y+34,"本航程尚无记载",mist,w);
            return;
        }
        const int count=int(game.s.sea_log.size());
        const int rows=list_rows(), pages=(count+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int index=page*rows+i;
            const auto entry=read_sea(game.s.sea_log[index]);
            if (!entry.kind) break;
            const auto date=format("第 %d 日",entry.day+1);
            row(d,i,Encounters[entry.kind].title,date,SeaEntry+index);
        }
        pager(d,pages);
    }
    void intel(Draw& d) {
        int p=modal==Atlas?atlas_item:selected;
        if (p<0 || p>=PlaceCount) p=game.s.origin;
        const auto& l=Places[p];
        page_title(d,l.name);
        auto coord=format("%c %d.%02d   %c %d.%02d",l.latitude<0?'S':'N',
            std::abs(l.latitude)/100,std::abs(l.latitude)%100,l.longitude<0?'W':'E',
            std::abs(l.longitude)/100,std::abs(l.longitude)%100);
        d.label(body.x+6,body.y+29,coord,gold,body.w-12);
        d.wrap({body.x+6,body.y+53,body.w-12,body.h-57},l.note,ivory);
    }
    void atlas_list(Draw& d) {
        page_title(d,"发现图鉴");
        std::array<int,PlaceCount-PortCount> entries{};
        int count=0;
        for (int p=PortCount;p<PlaceCount;++p)
            if (game.charted(p)) entries[count++]=p;
        if (!count) {
            d.label(body.x+6,body.y+38,"出海后逐步收录测绘水域",mist,body.w-12);
            return;
        }
        const int rows=list_rows(), pages=(count+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int p=entries[page*rows+i];
            bool seen=game.s.surveyed & (1U<<p);
            row(d,i,Places[p].name,seen?"已测绘":"待测绘",AtlasEntry+p,seen);
        }
        pager(d,pages);
    }
    void rumor_list(Draw& d) {
        page_title(d,"航海传闻");
        std::array<int,RumorCount> entries{};
        int count=0;
        for (int id=0;id<RumorCount;++id)
            if (game.port() && Rumors[id].port==game.s.origin) entries[count++]=id;
        for (int id=0;id<RumorCount;++id)
            if (game.heard_rumor(id) && (!game.port() || Rumors[id].port!=game.s.origin)) entries[count++]=id;
        if (!count) {
            d.label(body.x+6,body.y+38,"这里没有新的航路消息",mist,body.w-12);
            return;
        }
        const int rows=list_rows(), pages=(count+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int id=entries[page*rows+i];
            const auto& rumor=Rumors[id];
            const bool found=game.charted(rumor.site);
            row(d,i,game.heard_rumor(id)||found?Places[rumor.site].name:rumor.speaker,
                found?"已定位":game.heard_rumor(id)?"已抄录":"新线索",RumorEntry+id,found);
        }
        pager(d,pages);
    }
    void rumor_page(Draw& d) {
        const auto& rumor=Rumors[rumor_item];
        const bool found=game.charted(rumor.site), heard=game.heard_rumor(rumor_item);
        page_title(d,heard||found?Places[rumor.site].name:rumor.speaker);
        const int x=body.x+6, w=body.w-12;
        d.label(x,body.y+29,found?"已记入海图":heard?"只知道大致海域":"可抄下航路传闻",found?mint:mist,w);
        d.wrap({x,body.y+48,w,body.h-78},rumor.clue,ivory,15);
        if (!heard && !found)
            button(d,{x,body.bottom()-28,w,24},"抄录传闻",RecordRumor,true);
        else
            button(d,{x,body.bottom()-28,w,24},found?"定位海图":"圈出大致海域",MarkRumor,true,game.sea());
    }
    void expedition_list(Draw& d) {
        page_title(d,"考察簿");
        const int rows=list_rows(), pages=(ExpeditionCount+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<ExpeditionCount;++i) {
            const int id=page*rows+i;
            const auto& study=Expeditions[id];
            const auto& record=game.s.expeditions[id];
            const Text detail=record.stage>=FiledReport?"已入藏":
                record.stage==HasEvidence?"待交付":
                record.stage==AtSeaSearch?"考察中":
                record.stage==ReadClue?"待出发":Places[study.archive].name;
            row(d,i,study.title,detail,ExpeditionEntry+id,record.stage>=HasEvidence);
        }
        if (pages>1) pager(d,pages);
    }
    void expedition_page(Draw& d) {
        const auto& study=Expeditions[expedition_item];
        const auto& record=game.s.expeditions[expedition_item];
        const bool at_archive=game.archive_here()==expedition_item;
        const bool on_site=game.expedition_here()==expedition_item;
        const bool witness=record.flags&HeardWitness;
        const int x=body.x+7, w=body.w-14, half=(w-5)/2;
        const int bottom=body.bottom();
        const int y=body.y;
        const Rect actions{x,bottom-28,w,24};
        page_title(d,study.title);
        if (on_site || record.stage>=HasEvidence) {
            const bool large=body.h>=174 || w>=290;
            const int art_w=large?std::min(w/3,126):46;
            const int art_y=y+25, art_h=art_w/2;
            d.image({art::discoveries},{expedition_item*160,0,160,80},
                    {x+w-art_w,art_y,art_w,art_h});
            d.frame({x+w-art_w,art_y,art_w,art_h},border);
            if (on_site) {
                const auto block=game.observation_block(expedition_item);
                d.label(x,y+26,block.empty() ? Text("可开始观测") : block,
                        block.empty()?mint:vermilion,w-art_w-4);
                const int text_y=y+(large?46:50);
                d.wrap({x,text_y,large?w-art_w-6:w,bottom-56-text_y},study.scene,ivory,15);
                const auto sea_gap=game.s.expeditions[expedition_item].rival_day-game.s.day;
                if (large)
                    d.label(x,bottom-74,sea_gap>0?Text("仍可争取首报"):Text("可带回复测记录"),
                            sea_gap>0?mint:quiet,w-art_w-5);
                button(d,{x,bottom-53,w,21},"锚地等一日",WaitToObserve);
                button(d,{x,actions.y,half,actions.h},study.options[0],ObserveFirst,false,!block.empty());
                button(d,{x+half+5,actions.y,w-half-5,actions.h},study.options[1],ObserveSecond,false,!block.empty());
            } else {
                const bool first=record.flags&FirstObserver;
                const auto bill=format("%s · 尾款%d金",first?"首报":"复测",game.expedition_purse(expedition_item));
                const Text status=record.stage==HasEvidence?Text(bill):first?"首份记录":"复测记录";
                d.label(x,y+26,status,first?mint:mist,w-art_w-4);
                const int text_y=y+(large?48:50);
                d.wrap({x,text_y,large?w-art_w-6:w,bottom-30-text_y},study.evidence,ivory,15);
                if (record.stage==HasEvidence) {
                    const auto home=format("返回%s",Places[study.archive].name.data());
                    button(d,actions,at_archive?Text("交付记录"):Text(home),
                           at_archive?FileExpedition:MarkExpedition,true,!at_archive && game.sea());
                } else d.label(x,bottom-24,"已入藏",quiet,w);
            }
            return;
        }

        if (record.stage==UnknownClue) {
            d.label(x,y+27,format("%s · %s",Places[study.archive].name.data(),study.archivist.data()),mist,w);
            d.wrap({x,y+49,w,body.h-81},"抄录手记，便能筹措考察经费。其他船也在打听这份手记，赶先完成观测可获首报赏金。",ivory);
            const auto mark_archive=format("标记%s",Places[study.archive].name.data());
            button(d,actions,at_archive?Text("抄录手记"):game.charted(study.archive)?Text(mark_archive):"尚无航路",
                   at_archive?CopyClue:MarkExpedition,true,!at_archive && (game.sea() || !game.charted(study.archive)));
            return;
        }
        const int remaining=record.rival_day-game.s.day;
        d.label(x,y+27,remaining>0?Text(format("首报余 %d 日",remaining)):Text("对手已经先行"),
                remaining>0?mint:vermilion,w);
        if (witness || at_archive)
            button(d,{x+w-58,y+24,58,21},show_witness?"手记":"口述",
                   witness?ReadNotes:AskWitness);
        const bool deciding=record.stage==ReadClue && at_archive;
        const bool mark=record.stage==ReadClue || witness;
        const int notes_y=y+44;
        d.wrap({x,notes_y,w,bottom-(deciding?46:mark?31:5)-notes_y},
               show_witness && witness?study.witness:study.book,ivory,15);
        if (deciding) {
            d.label(x,bottom-46,"预支 / 首报交稿金",quiet,w);
            button(d,{x,bottom-31,half,29},"领资助",SponsorVoyage,true,false,
                   format("%d / %d",study.advance,study.purse-study.advance*3/2));
            button(d,{x+half+5,bottom-31,w-half-5,29},"自费出海",FundVoyage,false,false,
                   format("0 / %d",study.purse));
        } else if (mark) {
            const int point=record.stage==ReadClue?study.archive:study.site;
            button(d,actions,format("在海图标记%s",Places[point].name.data()),MarkExpedition,true,
                   game.sea() || !game.charted(point));
        }
    }
    void help(Draw& d) {
        page_title(d,"船长手册");
        constexpr Text instructions[]={
            "点港口或菱形选择目的地。拖动海图，用加减调整比例。手柄方向键选项，A 确认，B 返回。海图上 START 可启航或停船。",
            "跨港贩货赚取差价。库存和行情会变，连续买入会抬高价格。船坞可加粮、修船、招募及换船。",
            "出发前看预计天数。船员越多耗粮越快。海上可缩帆、停船、掉头或在下站靠泊。遇到私掠船时，火炮齐射后需装填，机动可以贴近并瞄准。距一且有六名船员时可以接舷。",
            "金色菱形标明下一处主线。到青色测绘点后记入图鉴。交易和航行中会自动保存，重开可续航。",
            "决斗时先看对方起手。格挡克突刺，佯攻破格挡，突刺截佯攻。",
            "在目的地页点出海，无需先选港口。点海面或用左右舵改航向。未踏查的海图随航行展开。港口商会可打听传闻。",
            "去港务的档案室抄录手记。可出资或领预支，也可询问船工。抵达水域后点考察，按线索观测，再回原港交付。错过首报仍可交复测。"
        };
        page=std::clamp(page,0,int(std::size(instructions))-1);
        d.wrap({body.x+6,body.y+32,body.w-12,body.h-58},instructions[page],ivory);
        pager(d,std::size(instructions));
    }
    void ending(Draw& d) {
        page_title(d,game.s.ending==1?"众人的海图":"一份副本");
        d.wrap({body.x+6,body.y+36,body.w-12,body.h-68},Endings[game.s.ending-1],ivory);
        button(d,{body.x+5,body.bottom()-28,body.w-10,23},"继续自由航行",Close,true);
    }
    void render(Draw& d) {
        layout(d);
        d.fill(d.clip,ink);
        button_count=0;
        if (title) { start_screen(d); return; }
        header(d);
        switch (modal) {
        case Routes: route_list(d); break;
        case StoryPage: story(d); break;
        case Intel: intel(d); break;
        case Atlas: if (atlas_item) intel(d); else atlas_list(d); break;
        case Help: help(d); break;
        case EndingPage: ending(d); break;
        case TaleList: tale_list(d); break;
        case TalePage: tale_page(d); break;
        case SeaLog: sea_log(d); break;
        case Preflight: preflight(d); break;
        case RouteStops: route_stops(d); break;
        case PortPrices: port_prices(d); break;
        case EventReport: event_report(d); break;
        case TowRequest: tow_request(d); break;
        case PortDesk: port_desk(d); break;
        case EscortPage: escort_page(d); break;
        case DuelInvitation: duel_invitation(d); break;
        case ProvisionsPage: provisions_page(d); break;
        case ExpeditionList: expedition_list(d); break;
        case ExpeditionPage: expedition_page(d); break;
        case Wardroom: officer_list(d); break;
        case OfficerPage: officer_page(d); break;
        case RumorList: rumor_list(d); break;
        case RumorPage: rumor_page(d); break;
        case None:
            switch (screen) {
            case Chart: chart(d); break;
            case Market: market(d); break;
            case Shipyard: shipyard(d); break;
            case Office: office(d); break;
            case Journal: journal(d); break;
            }
            break;
        }
        if (!game.encounter() || modal!=None) footer(d);
        else d.center({safe.x,safe.bottom()-30,safe.w,25},"选择如何应对",mist);
        focused=std::clamp(focused,0,std::max(0,button_count-1));
    }

    void select(int p) {
        if (!game.charted(p)) { game.notice="还不知道确切泊地"; notify(); return; }
        scout_course.reset();
        pinned_rumor=-1;
        selected=p;
        screen=Chart;
        set_modal(None);
        follow=false;
        if (!zoom) zoom=3;
        center_at(Places[p].map);
    }
    void activate(int action) {
        dirty=true;
        if (action>=RumorEntry) {
            rumor_item=action-RumorEntry;
            set_modal(RumorPage);
            return;
        }
        if (action>=OfficerEntry) {
            officer_item=action-OfficerEntry;
            officer_talk=false;
            set_modal(OfficerPage);
            return;
        }
        if (action>=ExpeditionEntry) {
            expedition_item=action-ExpeditionEntry;
            show_witness=false;
            expedition_from_book=true;
            set_modal(ExpeditionPage);
            return;
        }
        if (action>=SeaEntry) {
            sea_item=action-SeaEntry;
            focused=0;
            return;
        }
        if (action>=TaleEntry) {
            tale_item=action-TaleEntry;
            set_modal(TalePage);
            return;
        }
        if (action>=AtlasEntry) {
            atlas_item=action-AtlasEntry;
            focused=0;
            return;
        }
        if (action>=Destination) {
            if (!game.sea()) select(action-Destination);
            return;
        }
        if (action>=GoodsRow) { good=action-GoodsRow; return; }
        if (action>=DockService) { game.service(action-DockService); changed(); return; }
        if (action>=OpenChart && action<=OpenJournal) {
            screen=Screen(action-OpenChart);
            page=0;
            set_modal(None);
            return;
        }
        switch (action) {
        case Begin:
            if (!loaded) break;
            title=false;
            if (!had_save) { selected=lisbon; set_modal(StoryPage); }
            break;
        case RouteList: page=0; set_modal(Routes); break;
        case ShowRumors: rumor_back=modal; page=0; set_modal(RumorList); break;
        case RecordRumor:
            if (game.hear_rumor(rumor_item)) changed();
            break;
        case MarkRumor: {
            const auto& rumor=Rumors[rumor_item];
            if (game.charted(rumor.site)) select(rumor.site);
            else if (!game.sea() && game.heard_rumor(rumor_item)) {
                scout_course=rumor.area;
                pinned_rumor=rumor_item;
                selected=game.s.origin;
                screen=Chart;
                set_modal(None);
                follow=false;
                center_at(rumor.area);
            }
            break;
        }
        case StartExplore:
            if (game.start_helm(-1)) {
                if (scout_course) {
                    const auto here=game.position();
                    game.s.heading=direction_to({scout_course->x-here.x,scout_course->y-here.y});
                }
                scout_course.reset();
                screen=Chart; set_modal(None); follow=true; last_day=now;
                changed();
            } else notify();
            break;
        case Close: back(); break;
        case ZoomIn: set_zoom(zoom+1); break;
        case ZoomOut:
            set_zoom(zoom-1);
            if (!zoom) follow=false;
            break;
        case Center:
            if (game.s.helm && game.visible_landmark()>=0 && zoom<5) set_zoom(5);
            else if (!zoom) zoom=3;
            follow=true; center_at(game.position());
            break;
        case Sail: {
            const auto plan=game.forecast(passage(game.s.origin,selected));
            if (plan.provisions>game.s.stores || plan.water>game.s.water || game.s.hull<=12) {
                set_modal(Preflight);
                break;
            }
            [[fallthrough]];
        }
        case ConfirmSail:
            if (game.depart(selected)) {
                set_modal(None);
                follow=true;
                last_day=now;
                changed();
            } else notify();
            break;
        case ShowPreflight: set_modal(Preflight); break;
        case ShowStops: page=0; set_modal(RouteStops); break;
        case TopUp: {
            const auto forecast=game.forecast(passage(game.s.origin,selected));
            if (game.resupply(supply_target(forecast.provisions),supply_target(forecast.water))) changed();
            else notify();
            break;
        }
        case OpenProvisions: if (game.port()) set_modal(ProvisionsPage); break;
        case ShowOfficers: if (game.port()) set_modal(Wardroom); break;
        case OfficerTalk: officer_talk=!officer_talk; break;
        case OfficerHire:
            if (game.hire_officer(officer_item)) changed();
            else notify();
            break;
        case OfficerDuty:
            if (game.assign_officer(game.s.first_mate==officer_item?NoOfficer:officer_item)) changed();
            break;
        case OfficerMark:
            if (game.port()) select(Officers[officer_item].port);
            break;
        case BuyFood: case BuyWater:
            if (game.resupply(game.s.stores+(action==BuyFood?10:0),
                              game.s.water+(action==BuyWater?10:0))) changed();
            else notify();
            break;
        case UnloadFood: case UnloadWater:
            if (game.unload_provisions(action==UnloadWater)) changed();
            break;
        case StartHelm:
            if (game.start_helm(selected)) {
                screen=Chart; set_modal(None); follow=true; last_day=now;
                changed();
            } else notify();
            break;
        case SteerLeft: case SteerRight:
            if (game.s.helm) {
                game.s.heading=(game.s.heading+(action==SteerLeft?7:1))%8;
                changed();
            }
            break;
        case DockHelm:
            if (game.dock_helm(chart_water)) { select(game.s.origin); changed(); }
            else notify();
            break;
        case ShowTow: set_modal(TowRequest); break;
        case ConfirmTow:
            if (game.request_tow()) { select(game.s.origin); changed(); }
            break;
        case ShowPortPrices: page=0; set_modal(PortPrices); break;
        case NextStop:
            game.s.stop_next=1-game.s.stop_next;
            game.notice=game.s.stop_next?"将在下一处靠泊":"继续前往原目的地";
            changed();
            break;
        case Pause:
            game.paused=!game.paused;
            last_day=now;
            break;
        case Trim: game.s.reefed=1-game.s.reefed; save_needed=true; break;
        case Retreat:
            game.turn_back(); selected=game.sea()?game.s.destination:game.s.origin;
            last_day=now;
            follow=true;
            changed();
            break;
        case BuyOne: case BuyFive: case SellOne: case SellFive: {
            const bool sell=action>=SellOne;
            const auto trade=game.trade(good,sell,action==BuyFive || action==SellFive?5:1);
            if (trade.units) {
                std::snprintf(trade_receipt.data(),trade_receipt.size(),"%s %d件  %s%d金",
                              sell?"卖出":"买入",trade.units,sell?"+":"-",trade.gold);
                game.notice=trade_receipt.data();
                changed();
            } else notify();
            break;
        }
        case Accept: game.take_contract(); changed(); break;
        case Deliver: game.deliver(); changed(); break;
        case ReadStory: if (story_here()) set_modal(StoryPage); break;
        case Survey: game.survey(); changed(); break;
        case ChooseFirst: case ChooseSecond: {
            const int chapter=game.s.chapter;
            game.chapter_choice(action==ChooseSecond);
            changed();
            if (game.s.chapter!=chapter) {
                if (game.s.ending) { screen=Journal; set_modal(EndingPage); }
                else select(target());
            }
            break;
        }
        case Recruit: game.navigator(); changed(); break;
        case WaitDay: game.wait_at_port(); changed(); break;
        case ShowHelp: page=0; set_modal(Help); break;
        case ShowAtlas: page=0; atlas_item=0; set_modal(Atlas); break;
        case ShowEnding: if (game.s.ending) set_modal(EndingPage); break;
        case ShowIntel:
            intel_from_preflight=modal==Preflight;
            set_modal(Intel);
            break;
        case ShowTales: page=0; set_modal(TaleList); break;
        case PortTaleAction:
            if (const int tale=game.tale_here(); tale>=0) {
                tale_item=tale;
                set_modal(TalePage);
            }
            break;
        case TaleShare: case TaleSell:
            if (game.choose_tale(tale_item,action==TaleShare)) {
                changed();
                if (game.s.tales[tale_item]==InProgress) select(game.tale_stop(tale_item));
            } else notify();
            break;
        case TaleMark:
            if (const int port=game.tale_stop(tale_item); port>=0 && !game.sea()) select(port);
            break;
        case ShowSeaLog: page=0; sea_item=-1; set_modal(SeaLog); break;
        case OpenDesk: if (game.port()) set_modal(PortDesk); break;
        case ShowExpeditions: page=0; set_modal(ExpeditionList); break;
        case OpenArchive: case OpenObservation:
            if (const int id=action==OpenArchive?game.archive_here():game.expedition_here(); id>=0) {
                expedition_item=id;
                expedition_from_book=show_witness=false;
                set_modal(ExpeditionPage);
            }
            break;
        case CopyClue: if (game.read_expedition(expedition_item)) changed(); break;
        case AskWitness:
            if (game.consult_witness(expedition_item)) { show_witness=true; changed(); }
            break;
        case ReadNotes: show_witness=!show_witness; break;
        case SponsorVoyage: case FundVoyage:
            if (game.launch_expedition(expedition_item,action==SponsorVoyage)) changed();
            break;
        case MarkExpedition:
            if (!game.sea()) {
                const auto& study=Expeditions[expedition_item];
                const auto stage=game.s.expeditions[expedition_item].stage;
                select(stage==AtSeaSearch?study.site:study.archive);
            }
            break;
        case ObserveFirst: case ObserveSecond:
            game.observe(expedition_item,action==ObserveSecond);
            changed();
            break;
        case WaitToObserve:
            if (game.wait_to_observe(expedition_item)) changed();
            else notify();
            break;
        case FileExpedition: if (game.file_expedition(expedition_item)) changed(); break;
        case ShowEscort:
            activity_back=modal;
            set_modal(EscortPage);
            break;
        case ShowDuel:
            activity_back=modal;
            set_modal(DuelInvitation);
            break;
        case InvestPort:
            if (game.invest()) changed();
            else notify();
            break;
        case AcceptEscort:
            if (game.accept_escort()) { select(madeira); changed(); }
            break;
        case MarkEscort:
            if (!game.sea()) select(game.s.escort>=EscortWonGuns ||
                (game.s.escort==EscortSailing && game.s.origin==lisbon) ? madeira : lisbon);
            break;
        case DeliverPublic: case DeliverPrivate:
            if (game.deliver_escort(action==DeliverPublic)) changed();
            break;
        case BeginDuel:
            if (game.start_dock_duel()) {
                screen=Chart;
                set_modal(None);
                changed();
            }
            break;
        case Previous: page=std::max(0,page-1); break;
        case Next: ++page; break;
        case FightFirst: case FightSecond: case FightThird: case FightFourth: {
            const auto& s=game.s;
            const int hull=s.hull, stores=s.stores, water=s.water, gold=s.gold, cargo=game.load(), day=s.day;
            if (s.weather==Privateer && action==FightSecond) flash_until=now+570;
            if (game.duel() && action!=FightFourth) {
                duel_animation=action-FightFirst;
                duel_flash_until=now+419;
            }
            game.encounter_action(action-FightFirst);
            if (!game.encounter()) {
                outcome={game.notice,s.hull-hull,s.stores-stores,s.water-water,s.gold-gold,game.load()-cargo,s.day-day};
                if (game.sea()) game.paused=true;
                else {
                    selected=s.origin;
                    center_at(game.position());
                }
                set_modal(EventReport);
            }
            last_day=now;
            changed();
            break;
        }
        case ResumeAfterEvent:
            game.paused=false;
            last_day=now;
            set_modal(None);
            break;
        }
    }

    bool back() {
        if (title) return false;
        if (modal!=None) {
            if (modal==Atlas && atlas_item) { atlas_item=0; dirty=true; }
            else if (modal==TalePage && screen==Journal) set_modal(TaleList);
            else if (modal==SeaLog && sea_item>=0) { sea_item=-1; focused=0; dirty=true; }
            else if (modal==RouteStops || (modal==Intel && intel_from_preflight)) set_modal(Preflight);
            else if (modal==EscortPage || modal==DuelInvitation) set_modal(activity_back);
            else if (modal==ExpeditionPage && expedition_from_book) set_modal(ExpeditionList);
            else if (modal==OfficerPage) set_modal(Wardroom);
            else if (modal==RumorPage) set_modal(RumorList);
            else if (modal==RumorList) { page=0; set_modal(rumor_back); }
            else if (modal==PortPrices) { page=good/market_rows(); set_modal(None); }
            else set_modal(None);
        } else if (screen!=Chart) { screen=Chart; focused=0; dirty=true; }
        else if (game.sea() && !game.encounter()) { game.paused=true; dirty=true; }
        else if (game.encounter()) return true;
        else { title=true; had_save=had_save || next_frame>1; dirty=true; }
        return true;
    }
    pxa::BackAction on_back() { return back()?pxa::BackAction::stay:pxa::BackAction::close; }

    void focus_move(int dx,int dy) {
        if (!button_count) return;
        if (!pad_focus) { pad_focus=true; dirty=true; return; }
        auto origin=buttons[focused].rect;
        int best=10000000, choice=-1;
        for (int i=0;i<button_count;++i) {
            const auto r=buttons[i].rect;
            const int x=(r.x+r.right()-origin.x-origin.right())/2;
            const int y=(r.y+r.bottom()-origin.y-origin.bottom())/2;
            int ahead=x*dx+y*dy;
            if (ahead<=0) continue;
            const int cost=ahead+3*std::abs(y*dx-x*dy);
            if (cost<best) { best=cost; choice=i; }
        }
        if (choice<0) choice=(focused+(dx+dy>0?1:button_count-1))%button_count;
        focused=choice;
        dirty=true;
    }
    void on_controller(pxa::Context&, const pxa::input::ControllerState& e) {
        using namespace pxa::input;
        if (!controller.update(e)) return;
        if (controller.pressed(controller_up)) focus_move(0,-1);
        if (controller.pressed(controller_down)) focus_move(0,1);
        if (controller.pressed(controller_left)) focus_move(-1,0);
        if (controller.pressed(controller_right)) focus_move(1,0);
        if (controller.pressed(controller_a) && button_count) {
            pad_focus=true;
            activate(buttons[focused].action);
        }
        if (controller.pressed(controller_b)) back();
        if (game.s.helm && screen==Chart && modal==None && !game.encounter()) {
            if (controller.pressed(controller_x)) activate(SteerLeft);
            if (controller.pressed(controller_y)) activate(SteerRight);
        }
        if (controller.pressed(controller_start) && !title && modal==None &&
            screen==Chart && !game.encounter())
            activate(game.sea() ? Pause : selected!=game.s.origin ? Sail : story_here() ? ReadStory :
                     game.expedition_here()>=0 ? OpenObservation : StartExplore);
    }
    void tap(Point p) {
        for (int i=0;i<button_count;++i) {
            if (buttons[i].rect.contains(p)) { activate(buttons[i].action); return; }
        }
        if (title || modal!=None || screen!=Chart || game.encounter() || !map.contains(p)) return;
        const int x=int(std::lround(chart_x+(p.x-map.x-map.w/2)/factor));
        const int y=int(std::lround(chart_y+(p.y-map.y-map.h/2)/factor));
        if (game.s.helm) {
            const auto here=game.position();
            if (std::abs(x-here.x)+std::abs(y-here.y)>=2) {
                game.s.heading=static_cast<std::uint8_t>(direction_to({x-here.x,y-here.y}));
                game.notice="已调整航向";
                follow=true;
                changed();
            }
            return;
        }
        if (game.sea()) return;
        int nearest=-1, best=14*14;
        for (int i=0;i<PlaceCount;++i) {
            if (!game.charted(i)) continue;
            auto point=map_point(Places[i].map);
            const int dx=point.x-p.x, dy=point.y-p.y;
            const int d=dx*dx+dy*dy;
            if (map.contains(point) && d<best) { nearest=i; best=d; }
        }
        if (nearest>=0) {
            selected=nearest;
            scout_course.reset();
            pinned_rumor=-1;
            dirty=true;
        } else if (x>=0 && x<ChartWidth && y>=0 && y<ChartHeight) {
            selected=game.s.origin;
            scout_course=Point{x,y};
            pinned_rumor=-1;
            dirty=true;
        }
    }
    void pointer(const pxa::ui::CanvasPointer& e) {
        using pxa::input::PointerPhase;
        const auto device_pixel=[&](int logical) {
            return pxa::ui::canvas_to_surface_coordinate(logical,input_display);
        };
        const Point p{(device_pixel(e.x)-surface_x)/scale,
                      (device_pixel(e.y)-surface_y)/scale};
        if (e.action()==PointerPhase::down) {
            if (!pointer_capture.try_capture(e)) return;
            pad_focus=false;
            press_x=last_x=p.x; press_y=last_y=p.y;
            map_press=!title && screen==Chart && modal==None && !game.encounter() && map.contains(p);
            panning=false;
            dirty=true;
        } else if (pointer_capture.owns(e)) {
            if (e.action()==PointerPhase::move && map_press) {
                panning |= std::abs(p.x-press_x)+std::abs(p.y-press_y)>5;
                if (panning) {
                    chart_x-=(p.x-last_x)/factor;
                    chart_y-=(p.y-last_y)/factor;
                    follow=false;
                    last_x=p.x; last_y=p.y;
                    dirty=true;
                }
            }
            if (e.ended()) {
                pointer_capture.release(e);
                if (e.action()==PointerPhase::up && !panning &&
                    std::abs(p.x-press_x)+std::abs(p.y-press_y)<14) tap(p);
            }
        }
    }
};

} // namespace
} // namespace tide

PXA_GAME(tide::VoyageApp)
