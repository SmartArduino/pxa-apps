#include "draw.hpp"
#include "game_audio.hpp"
#include "game_save.hpp"
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
constexpr int PricedButtonHeight=33;

enum Screen { Chart, Market, Shipyard, Office, Journal };
enum Plate { StandardPlate, ActionPlate, TabPlate, CurrentTabPlate };
enum FontChoice : std::uint8_t { AutomaticFont, SmallFont, LargeFont, FontChoices };
enum Overlay {
    None, Routes, StoryPage, Intel, Help, Atlas, EndingPage, TaleList, TalePage, SeaLog,
    Preflight, RouteStops, PortPrices, EventReport, TowRequest, PortDesk, EscortPage, DuelInvitation, ProvisionsPage,
    ExpeditionList, ExpeditionPage, Wardroom, OfficerPage, RumorList, RumorPage, AudioSettings,
    LanguageDesk, TideLedger, BargainPage, DriftStory, DriftRecovery, MerchantBoard
};
enum NavIcon { MapIcon, ScaleIcon, ShipIcon, LedgerIcon, JournalIcon, CoinsIcon, HullIcon,
               FoodIcon, CompassIcon, CursorIcon, AnchorIcon, GlassIcon };
enum VoyageIcon { SpeechIcon, PhrasebookIcon, LetterIcon, AstrolabeIcon, PennantIcon,
                  FlaskIcon, RationIcon, MooringIcon, SeaChartIcon, SettingsIcon, CrewIcon,
                  RepairIcon, ReefIcon, ResumeIcon, PauseIcon, SailIcon, HelmIcon, CargoIcon };
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
    ShowAudio, ToggleMusic, ToggleEffects, ToggleVoices,
    DockService=100, GoodsRow=200, Destination=300, AtlasEntry=400, TaleEntry=500, SeaEntry=600,
    ExpeditionEntry=700, OfficerEntry=800, RumorEntry=900,
    OpenLanguages=1000, SelectPortLanguage, SelectSecondLanguage, HireInterpreter, TakeLesson,
    OpenLedger, CopyLedger, MarkLedger, PublishLedger, ReturnLedger, ShowBargain, Negotiate,
    ShowDriftStory, TakeDriftLead, MarkDriftGround, OpenDriftRecovery, BoatForDrift,
    HookForDrift, WaitForDrift, ReturnDriftRoll, FileDriftRoll, CycleFont,
    ShowMerchants, MarkMerchantFirst, MarkMerchantSecond
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
        .simulation_step_us=50000, .frame_period_ms=48, .maximum_updates=1};
    static constexpr unsigned DayMs=480, HelmPulseMs=220;

    Game game;
    GameAudio audio;
    EncounterOutcome outcome;
    std::array<char,96> trade_receipt{};
    // Loading completes before gameplay can start. Subsequent saves use one
    // borrowed packet at a time, keeping the chart out of coroutine slots.
    VoyageSave voyage_save;
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
    Language lesson_language=Portuguese;
    HarborApproach approach;
    Point approach_position{-1,-1};
    int approach_port=-1;
    std::optional<Point> scout_course;
    bool officer_talk=false;
    float chart_x=0, chart_y=0, factor=1;
    Point previous_helm{};
    int zoom=3, press_x=0, press_y=0, last_x=0, last_y=0;
    bool map_press=false, panning=false, follow=true, pad_focus=false, intel_from_preflight=false;
    bool title=true, loaded=false, had_save=false, opening=false, reconfigure=true, landscape_requested=false;
    bool foreground=false, dirty=true, save_needed=false, save_active=false, save_failed=false;
    FontChoice font_choice=AutomaticFont;
    bool font_pending=false, font_writing=false;
    unsigned font_retry_at=0;
    unsigned now=0, last_day=0, last_step=0, notice_until=0, flash_until=0, saved_until=0;
    unsigned surface_retry=0, duel_flash_until=0;
    int duel_animation=0;
    unsigned long long next_frame=1;
    Screen screen=Chart;
    Overlay modal=None, activity_back=None, rumor_back=None, language_back=None;
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
        auto started=ctx.tasks().start(load());
        if (started) audio.start(ctx);
        return started;
    }
    pxa::Task<void> load() {
        const auto result=co_await voyage_save.load(context->storage(),game.s);
        had_save=bool(result);
        if (had_save) game.restore();
        else if (result.error()!=pxa::Error::not_found) {
            game.notice = "存档读取失败";
            notify();
        }
        selected=game.s.destination>=0 ? game.s.destination :
                 game.s.origin==lisbon && !game.s.helm ? madeira : game.s.origin;
        center_at(game.position());
        previous_helm={game.s.helm_x,game.s.helm_y};
        const auto preference=co_await context->storage().get_value<std::uint8_t>("display-font-v1");
        if (preference && *preference<FontChoices) font_choice=FontChoice(*preference);
        loaded=true;
        dirty=true;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> save_font(FontChoice choice) {
        const auto result=co_await context->storage().set_value("display-font-v1",std::uint8_t(choice));
        font_writing=false;
        if (!result) { font_pending=true; font_retry_at=now+1500; }
        co_return pxa::Result<void>{};
    }

    bool wide_display() const {
        return metrics.pixel_width>=700 && metrics.pixel_height>=360 &&
               metrics.pixel_width>metrics.pixel_height;
    }
    bool small_text() const {
        return font_choice==SmallFont || (font_choice==AutomaticFont && wide_display());
    }
    bool dense_layout() const { return small_text() && wide_display(); }
    pxa::Task<void> save() {
        auto result=co_await voyage_save.write(context->storage(),game.s);
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

    void on_foreground(pxa::Context&) { foreground=true; audio.foreground(true); last_day=0; dirty=true; }
    void on_background(pxa::Context&) {
        foreground=false;
        audio.foreground(false);
        pointer_capture.reset();
        controller.reset();
        if (!title && game.sea()) save_needed=true;
        request_save();
    }
    pxa::Result<bool> on_event(pxa::Context&, const pxa::Event& event) {
        if (audio.event(event)) return true;
        if (event.service!=3 || event.opcode!=0x8002 || event.token!=0) return false;
        auto display=pxa::ui::decode_display_metrics(event);
        if (!display) return std::unexpected(display.error());
        input_display=*display;
        pointer_capture.reset();
        dirty=true;
        return true;
    }
    void on_window_changed(const pxa::WindowMetrics& value) {
        reconfigure |= value.pixel_width != metrics.pixel_width || value.pixel_height != metrics.pixel_height;
        metrics = value;
        if (!landscape_requested && std::min(value.pixel_width,value.pixel_height)>=400) {
            landscape_requested=true;
            (void)context->window().request_orientation(pxa::OrientationPreference::landscape);
        }
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
        const bool battle=game.s.weather==Privateer || game.duel();
        audio.tick(tick.timestamp_us,battle?Score::Battle:!title && game.sea()?Score::Voyage:Score::Harbor);
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
                const bool manual=game.s.helm;
                if (manual) {
                    previous_helm={game.s.helm_x,game.s.helm_y};
                    game.helm_step(chart_water);
                }
                else game.advance();
                last_day=now;
                if ((game.s.day!=old_day && game.s.day%4==0) ||
                    !game.sea() || game.encounter() || game.paused || game.s.chart_marks!=old_marks)
                    save_needed=true;
                if (modal==ExpeditionPage && game.s.day!=old_day) dirty=true;
                if (game.paused || game.s.hull<old_hull) notify();
                if (manual && game.s.helm && game.paused) {
                    if (game.s.hull<old_hull && game.notice=="触及浅滩，转舵离岸") audio.cue(Sound::Shoal);
                    else if (game.nearby_anchorage(chart_water)>=0) audio.cue(Sound::AnchorCall);
                    else if (game.s.chart_marks!=old_marks) {
                        audio.cue(Sound::Sighting);
                        audio.cue(Sound::LandCall);
                    }
                }
                if (!game.sea()) {
                    audio.cue(Sound::Anchor);
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
        if (font_pending && !font_writing && now>=font_retry_at &&
            context->tasks().start(save_font(font_choice))) {
            font_pending=false;
            font_writing=true;
        }
        if (!dirty) return;
        auto frame=surface->acquire();
        if (!frame) return;
        auto pixels=frame->rgb565();
        if (!pixels) return;
        Draw draw(*pixels,small_text()?CompactFace:StandardFace);
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
        d.nine_slice({art::skin},{tile*72,0,72,72},r,8,edge);
    }
    void plate(Draw& d, Rect r, Plate style, int edge=4) {
        d.nine_slice({art::controls},{style*96,0,96,32},r,edge,edge,true);
    }
    bool pressed(Rect r) const {
        return pointer_capture.active() && !map_press && r.contains({press_x,press_y}) &&
               r.contains({last_x,last_y}) && std::abs(last_x-press_x)+std::abs(last_y-press_y)<14;
    }
    void focus_marker(Draw& d, Rect r) {
        const Point cue{r.x+r.w/2,r.y+1};
        d.diamond(cue,3,ink);
        d.diamond(cue,2,ivory);
    }
    void meter(Draw& d, Rect r, int value, int maximum, bool voyage) {
        plate(d,r,StandardPlate,2);
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
    void voyage_icon(Draw& d, int icon, Rect r) {
        d.image({art::voyage_icons},{icon%3*32,icon/3*32,32,32},r,true);
    }
    void port_scene(Draw& d, Rect area, int port) {
        if (port<0 || port>=PortCount || area.w<1 || area.h<1) return;
        constexpr int tw=188, th=105;
        const int width=std::min(tw,th*area.w/area.h);
        const int height=std::min(th,tw*area.h/area.w);
        d.image({art::ports},{Zone[port]*tw+(tw-width)/2,(th-height)/2,width,height},area);
    }
    void button(Draw& d, Rect r, Text text, int action, bool accent=false, bool inactive=false,
                Text price={}, int glyph=-1) {
        const bool chosen=pad_focus && button_count==focused && !inactive;
        const bool down=!inactive && pressed(r);
        plate(d,r,accent && !inactive ? ActionPlate : StandardPlate);
        if (inactive || down) d.darken(r.inset(3),inactive?6:3);
        if (chosen) focus_marker(d,r);
        const Color color=inactive ? quiet : accent ? gold : ivory;
        const Rect content=r.inset(2);
        const Rect previous_clip=d.clip;
        d.clip=d.clipped(content);
        if (!price.empty()) {
            d.center_lines(content,text,color,price,inactive ? quiet : mist);
        } else if (glyph>=0) {
            const Rect sprite{glyph%3*32,glyph/3*32,32,32};
            d.center_with_icon(content,text,color,{art::voyage_icons},sprite,std::clamp(r.h-7,12,17));
        } else {
            d.center(content,text,color);
        }
        d.clip=previous_clip;
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
            return x+20+d.measure(text);
        };
        int x=stat(safe.x+5,CoinsIcon,game.s.gold,gold);
        x=stat(x,HullIcon,game.s.hull,ivory);
        const auto date=format("第%d日",game.s.day+1);
        const int date_x=safe.right()-6-d.measure(date);
        const auto stores=format("%d/%d",game.s.stores,int(game.s.water));
        const auto compact=format("粮%d 水%d",game.s.stores,int(game.s.water));
        const bool fits=x+20+d.measure(stores)<=date_x;
        if (fits) {
            voyage_icon(d,RationIcon,{x,safe.y+4,14,14});
            d.label(x+16,safe.y+4,stores,ivory);
        }
        d.label(date_x,safe.y+4,date,mist);
        const int status_width=safe.w-(fits?80:d.measure(compact)+15);
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
        else d.label(safe.right()-6-d.measure(compact),safe.y+18,compact,mint);
    }
    void footer(Draw& d) {
        skin_box(d,{safe.x,safe.bottom()-28,safe.w,28},0,5);
        for (int i=0;i<5;++i) {
            const int left=safe.x+i*safe.w/5, right=safe.x+(i+1)*safe.w/5;
            const Rect hit{left,safe.bottom()-32,right-left,32};
            const Rect r{hit.x+1,hit.y,hit.w-2,hit.h};
            const bool focused_tab=pad_focus && button_count==focused;
            const bool active=screen==i;
            const bool down=pressed(hit);
            plate(d,r,active || down ? CurrentTabPlate : TabPlate);
            if (down) d.darken(r.inset(3),3);
            if (focused_tab) focus_marker(d,r);
            const Color label=active ? gold : ivory;
            const Image images{art::tabs};
            const Rect sprite{i*32,0,32,32};
            const Rect content=r.inset(2);
            if (r.w>=30+d.measure(tabs[i])) {
                d.center_with_icon(content,tabs[i],label,images,sprite,20);
            } else {
                const auto caption=d.fit(tabs[i],content.w,1);
                const int size=std::min(14,content.h-caption.ink.h-1);
                const int y=content.y+(content.h-size-1-caption.ink.h)/2;
                d.image(images,sprite,{r.x+(r.w-size)/2,y,size,size},true);
                d.center_run({content.x,y+size+1,content.w,caption.ink.h},caption,label);
            }
            buttons[button_count++]={hit,OpenChart+i};
        }
    }

    void start_screen(Draw& d) {
        const bool horizontal=wide_display() && safe.w>=345;
        d.cover({horizontal?art::harbor:art::title_compact},d.clip);
        const Image logo{art::wordmark};
        const int logo_w=horizontal?std::min(288,safe.w*51/100):std::min(218,safe.w-36);
        const int logo_h=logo.height()*logo_w/logo.width();
        const int logo_x=horizontal?safe.x+safe.w/20:safe.x+(safe.w-logo_w)/2;
        const int logo_y=safe.y+(horizontal?safe.h*29/100:std::max(27,safe.h/6));
        d.image(logo,{0,0,logo.width(),logo.height()},{logo_x,logo_y,logo_w,logo_h},true);
        const int width=std::min(148,safe.w-34);
        const int button_y=horizontal?std::min(safe.bottom()-44,logo_y+logo_h+19):safe.bottom()-42;
        button(d,{logo_x+(logo_w-width)/2,button_y,width,32},
               !loaded ? "读取航海日志" : had_save ? "继续航行" : "开始航程",Begin,true,!loaded,{},
               had_save?ResumeIcon:SailIcon);
        button(d,{safe.right()-58,safe.y+3,54,22},"设置",ShowAudio,false,false,{},SettingsIcon);
    }

    Point map_point(float x, float y) const {
        return {map.x+map.w/2+int(std::lround((x-chart_x)*factor)),
                map.y+map.h/2+int(std::lround((y-chart_y)*factor))};
    }
    Point map_point(Point p) const { return map_point(p.x,p.y); }
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
    void draw_merchants(Draw& d) {
        constexpr Color flags[]{rgb(0xb6d7d0),rgb(0xd78f68)};
        for (std::size_t i=0;i<game.s.merchants.size();++i) {
            const auto& ship=game.s.merchants[i];
            if (ship.destination==NoMerchantPort) continue;
            const auto location=game.merchant_position(i);
            if (!game.chart_cell(location.x/ChartCell,location.y/ChartCell)) continue;
            const auto p=map_point(location);
            if (!map.contains(p)) continue;
            d.diamond(p,5,ink);
            d.diamond(p,3,flags[i]);
            voyage_icon(d,SailIcon,{p.x-6,p.y-13,12,12});
        }
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
    void draw_drift_mark(Draw& d) {
        if (game.s.drift_stage!=DriftSearching) return;
        if (game.sees_drift()) {
            const auto mark=map_point(game.drift_position());
            d.diamond(mark,3,gold);
            voyage_icon(d,LetterIcon,{mark.x-8,mark.y-19,16,16});
            return;
        }
        const auto center=map_point(DriftGround);
        const int r=std::max(8,int(13*factor));
        const Point ring[]{{center.x,center.y-r},{center.x+r,center.y},{center.x,center.y+r},{center.x-r,center.y}};
        for (int side=0;side<4;++side) d.line(ring[side],ring[(side+1)%4],gold,true);
        d.label(center.x-3,center.y-7,"?",gold);
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
        float ship_x=ship.x, ship_y=ship.y;
        if (game.s.helm) {
            const auto blend=game.paused?1.f:std::clamp(float(now-last_day)/HelmPulseMs,0.f,1.f);
            ship_x=std::lerp(float(previous_helm.x),float(game.s.helm_x),blend)/16;
            ship_y=std::lerp(float(previous_helm.y),float(game.s.helm_y),blend)/16;
        }
        if (game.sea() && follow) { chart_x=ship_x; chart_y=ship_y; }
        atlas_background(d);
        const auto path=game.sea() ? game.route : passage(game.s.origin,selected);
        draw_routes(d,path,game.sea());
        draw_wake(d);
        draw_merchants(d);
        draw_rumor_marks(d);
        draw_drift_mark(d);
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
            Rect r{p.x,p.y,d.measure(text)+4,d.font.cell+2};
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
            if (left+d.measure(label)+4>map.right()) left=p.x-7-d.measure(label)-4;
            const int top=std::clamp(p.y-17,map.y+2,map.bottom()-17);
            if (p.y<map.y || p.y>=map.bottom()) continue;
            draw_label({left,top},label,i==selected ? gold : ivory,true,false);
        }
        for (auto& n:names) {
            const Point source{(n.lon+45)*7,(ChartNorth-n.lat)*8};
            if (!game.chart_cell(source.x/ChartCell,source.y/ChartCell)) continue;
            auto p=map_point(source);
            draw_label({p.x-d.measure(n.label)/2,p.y},n.label,n.land ? rgb(0x293e37) : mist,false,false);
        }
        if (zoom>=3) for (int i=0;i<PlaceCount;++i) {
            if (!game.charted(i) || i==selected || i==target() || i==game.s.origin) continue;
            const auto p=map_point(Places[i].map);
            if (map.contains(p)) draw_label({p.x+4,p.y-14},Places[i].name,ivory,true,false);
        }
        if (game.sea()) {
            auto p=map_point(ship_x,ship_y);
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
        const int pane=dense_layout()?std::clamp(body.w/3-22,104,148):std::clamp(body.w/3,95,170);
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
        const bool salvage=berth<0 && game.near_drift();
        const int landmark=berth>=0?berth:game.s.helm?game.visible_landmark():-1;
        draw_map(d,landmark,berth);

        const int icon_w=28, y=map.bottom()+3;
        button(d,{map.x,y,icon_w,24},"+",ZoomIn);
        button(d,{map.x+icon_w+3,y,icon_w,24},"-",ZoomOut);
        button(d,{map.x+2*(icon_w+3),y,36,24},landmark>=0 && zoom<5?"近岸":"定位",Center);
        button(d,{map.x+2*(icon_w+3)+39,y,map.w-2*(icon_w+3)-39,24},
               salvage?"捞木匣":game.s.helm?"靠泊":game.sea()?(game.s.stop_next?"取消停靠":"下站停靠"):"目的地",
               salvage?OpenDriftRecovery:game.s.helm?DockHelm:game.sea()?NextStop:RouteList,
               salvage || game.s.stop_next || berth>=0,game.s.helm && berth<0 && !salvage,{},
               salvage?LetterIcon:game.s.helm?MooringIcon:-1);
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
                    game.s.reefed ? "张帆" : "缩帆",Trim,false,false,{},game.s.reefed?SailIcon:ReefIcon);
                button(d,{info.x+4,info.bottom()-52,info.w-8,23},
                    game.paused ? "继续" : "停船",Pause,true,false,{},game.paused?ResumeIcon:PauseIcon);
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
                       route.count?Sail:visit_command,true,false,{},route.count || scouting?SailIcon:-1);
            }
        } else {
            if (game.sea()) {
                d.label(info.x+5,info.y+22,format("航段 %d/%d  %s",game.s.leg+1,
                    game.route.count,game.wind(game.s.leg)>5 ? "顺风" : game.wind(game.s.leg)<-5 ? "逆风" : "风平"),
                    mist,info.w-10);
                meter(d,{info.right()-80,info.y+22,73,12},miles_done(route),route.miles,true);
                const int w=(info.w-14)/3;
                button(d,{info.x+4,info.y+43,w,25},game.s.reefed ? "张帆" : "缩帆",Trim,
                       false,false,{},game.s.reefed?SailIcon:ReefIcon);
                button(d,{info.x+7+w,info.y+43,w,25},game.paused ? "继续" : "停船",Pause,
                       true,false,{},game.paused?ResumeIcon:PauseIcon);
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
            voyage_icon(d,PennantIcon,{info.x+5,info.y+40,14,14});
            d.label(info.x+22,info.y+41,breeze,mist,info.w-27);
            pair(d,{info.x+4,info.bottom()-78,info.w-8,21},"左舵",SteerLeft,"右舵",SteerRight);
            button(d,{info.x+4,info.bottom()-52,info.w-8,22},game.s.reefed?"张帆":"缩帆",Trim,
                   false,false,{},game.s.reefed?SailIcon:ReefIcon);
            button(d,{info.x+4,info.bottom()-26,info.w-8,22},game.paused?"继续":"停船",Pause,
                   true,false,{},game.paused?ResumeIcon:PauseIcon);
        } else {
            d.label(info.x+5,info.y+24,bearing,ivory,info.w-10);
            voyage_icon(d,PennantIcon,{info.x+5,info.y+42,14,14});
            d.label(info.x+22,info.y+43,breeze,mist,info.w-27);
            const Text names[]{"左舵","右舵",game.s.reefed?"张帆":"缩帆",game.paused?"继续":"停船"};
            const int actions[]{SteerLeft,SteerRight,Trim,Pause};
            const int glyphs[]{-1,-1,game.s.reefed?SailIcon:ReefIcon,game.paused?ResumeIcon:PauseIcon};
            const int step=(info.w-7)/4;
            for (int i=0;i<4;++i)
                button(d,{info.x+4+i*step,info.bottom()-25,step-3,21},names[i],actions[i],i==3,false,{},glyphs[i]);
        }
    }

    int market_pitch() const { return dense_layout()?17:19; }
    int market_rows() const { return std::clamp((body.h-72)/market_pitch(),2,8); }

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
        const bool regional=std::find(MerchantPorts.begin(),MerchantPorts.end(),s.origin)!=MerchantPorts.end();
        voyage_icon(d,CargoIcon,{x+5,y+2,13,13});
        d.label(x+21,y+2,format("船舱 %d/%d",game.load(),game.capacity()),gold,regional?w-174:w-21);
        const int right=x+w-4;
        if (regional) {
            button(d,{right-150,y+1,45,18},"船讯",ShowMerchants);
            if (w>=315) d.label(right-197,y+2,"买/卖",quiet);
        } else if (w>=235) d.label(right-145,y+2,"买/卖",quiet);
        button(d,{right-101,y+1,53,18},"议价",ShowBargain,false,false,{},SpeechIcon);
        button(d,{right-44,y+1,42,18},"比价",ShowPortPrices);
        for (int i=0; i<rows && page*rows+i<GoodsCount; ++i) {
            const int g=page*rows+i;
            const Rect item{x+3,y+19+i*market_pitch(),w-6,market_pitch()-1};
            const bool foc=pad_focus && button_count==focused;
            d.fill(item,foc ? rgb(0x3b5550) : g==good ? light : (i%2 ? rgb(0x1e3b3f) : panel));
            if (foc || g==good) nav_icon(d,CursorIcon,{item.x+1,item.y+4,10,10});
            goods_icon(d,g,{item.x+13,item.y,item.h,item.h});
            d.label(item.x+34,item.y+2,GoodsNames[g],g==good ? gold : ivory);
            d.label(item.x+83,item.y+2,format("%d",s.cargo[g]),mint);
            const auto price=format("%d / %d",game.quote(g,false),game.quote(g,true));
            d.label(right-d.measure(price)-5,item.y+2,price,ivory);
            buttons[button_count++]={item,GoodsRow+g};
        }
        const bool negotiated=s.bargain_good==good+1;
        const auto note=format(negotiated?"%s · 五件省%d%%":"选中%s  库存%d",
                               GoodsNames[good].data(),negotiated?s.bargain_rate:game.stock(good));
        d.label(x+4,body.bottom()-51,note,negotiated?mint:mist,w-(pages>1?72:10));
        if (pages>1) {
            button(d,{right-56,body.bottom()-54,25,23},"<",Previous,false,page==0);
            button(d,{right-27,body.bottom()-54,25,23},">",Next,false,page+1==pages);
        }
        const int step=w/4;
        for (int i=0;i<4;++i) {
            const bool sell=i>=2;
            const int batch=i%2==0 ? 1 : TradeLot;
            const auto offer=game.trade_quote(good,sell,batch);
            const auto label=format("%s%d",sell?"卖":"买",offer.units ? offer.units : batch);
            const auto price=format("%d%s",offer.gold,step>=48?"金":"");
            button(d,{x+i*step,body.bottom()-31,step-3,PricedButtonHeight},label,BuyOne+i,i==0 || offer.saved,!offer.units,
                   offer.units ? Text(price) : Text("—"));
        }
    }

    void merchant_board(Draw& d) {
        page_title(d,"商队消息",SailIcon);
        const int h=std::min(52,(body.h-34)/2);
        for (std::size_t i=0;i<game.s.merchants.size();++i) {
            const auto& ship=game.s.merchants[i];
            const bool underway=ship.destination!=NoMerchantPort;
            const int pier=MerchantPorts[underway?ship.destination:ship.port];
            const Rect card{body.x+5,body.y+28+int(i)*(h+4),body.w-10,h};
            const bool focused_card=pad_focus && button_count==focused;
            plate(d,card,focused_card?ActionPlate:StandardPlate);
            if (focused_card) focus_marker(d,card);
            const int x=card.x+6,w=card.w-12;
            const Text status=ship.held?"等风":underway?"在海上":"已靠港";
            d.label(x,card.y+4,MerchantNames[i],gold,w-65);
            d.label(card.right()-6-d.measure(status),card.y+4,status,ship.held?gold:mint);
            if (underway) {
                d.label(x,card.y+19,format("%s → %s",Places[MerchantPorts[ship.port]].name.data(),
                                          Places[pier].name.data()),ivory,w);
            } else d.label(x,card.y+19,Places[pier].name,ivory,w);
            if (ship.units)
                d.label(x,card.y+34,format(underway?"买了%s ×%d":"已卖出%s ×%d",
                                          GoodsNames[ship.good].data(),int(ship.units)),mist,w);
            else d.label(x,card.y+34,underway?"空船去找货":"正在码头找货",mist,w);
            if (game.charted(pier)) buttons[button_count++]={card,MarkMerchantFirst+int(i)};
        }
    }

    void bargain_page(Draw& d) {
        page_title(d,"码头议价",SpeechIcon);
        const auto& s=game.s;
        const int x=body.x+7, w=body.w-14, y=body.y;
        const int rate=game.negotiation_rate();
        const bool pending=s.bargain_good>0 && s.bargain_good<=GoodsCount;
        const int commodity=pending?s.bargain_good-1:good;
        goods_icon(d,commodity,{x,y+27,20,20});
        d.label(x+24,y+30,format("%s · 五件整批",GoodsNames[commodity].data()),gold,w-24);
        const auto fixed=format("这批货已谈好，买足五件省%d%%。离港后报价失效。",s.bargain_rate);
        const auto offer=format("花一日商议，首批买足五件省%d%%。每次停泊可谈一种货。",rate);
        Text detail=offer;
        if (pending) detail=fixed;
        else if (s.bargain_good==BargainUsed) detail="本次停泊的折扣已用完。";
        else if (!rate) detail="先学会本地短语，或请随船副官、码头口译帮忙。";
        else if (!game.can_negotiate(good)) detail="库存和舱位各需至少五件。";
        d.wrap({x,y+50,w,body.h-82},detail,ivory,15);
        if (!rate && !s.bargain_good)
            button(d,{x,body.bottom()-28,w,24},"请口译或学话",OpenLanguages,true,false,{},PhrasebookIcon);
        else if (game.can_negotiate(good))
            button(d,{x,body.bottom()-28,w,24},"谈五件整批 · 一日",Negotiate,true);
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
        button(d,{body.right()-71,body.y+2,67,20},"副官",ShowOfficers,s.first_mate!=NoOfficer,false,{},CrewIcon);
        d.label(body.x+5,body.y+22,format("船员 %d/%d  士气 %d  火炮 %d",s.crew,
            game.crew_limit(),s.spirits,s.guns),mist,body.w-10);
        int height=std::clamp((body.h-36)/4-3,20,34);
        const auto fill=format("补给 %d金",game.ration_cost());
        const auto repair=format("维修 %d金",game.repair_cost());
        const auto guns=format("加炮 %d金",145+s.guns*120);
        const auto upgrade=format("换船 %d金",s.ship==0?800:1850);
        const Text titles[]{fill,repair,"休整 24金","雇水手",s.ship<2?Text(upgrade):"已是商船",
                            s.guns<3?Text(guns):"火炮已满","应急信贷","食物淡水"};
        constexpr int glyphs[]{RationIcon,RepairIcon,-1,CrewIcon,SailIcon,-1,-1,FlaskIcon};
        for (int i=0;i<8;++i) {
            int x=body.x+(i%2)*(body.w/2+2);
            button(d,{x,body.y+37+(i/2)*(height+3),body.w/2-3,height},titles[i],
                   i==7?OpenProvisions:DockService+i,i==0,(i==4 && s.ship==2)||(i==5 && s.guns==3),{},glyphs[i]);
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
            const int contract_y=y+(d.measure(offer)<=body.w-16?16:31);
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
        d.label(body.x+5,body.y+3,format("声望%d 信任%d 测绘%d/%d",s.renown,s.trust,
                s.charts,PlaceCount-PortCount),gold,body.w-67);
        button(d,{body.right()-57,body.y+2,53,20},"设置",ShowAudio,false,false,{},SettingsIcon);
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
            const int badge=d.measure(state);
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
        d.label(x,y+17,format("%s / %s",LanguageNames[person.languages[0]].data(),
                LanguageNames[person.languages[1]].data()),on_duty?mint:mist,w);
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
            const int intent=game.naval_intent();
            const Text tell=intent==EnemyClose && s.needs_reload && s.range<4 ?
                "敌船趁你装填，正靠过来" : EnemyOrders[intent];
            const auto threat=format("%s  %s",tell.data(),
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
                "渔会的潮表就在手边。现在就能认出哪盏灯有假，也不用耗一天水粮。" : scene.message;
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

    void page_title(Draw& d, Text name, int glyph=-1) {
        skin_box(d,body,0,6);
        const int inset=glyph<0?6:24;
        if (glyph>=0) voyage_icon(d,glyph,{body.x+6,body.y+4,16,16});
        d.label(body.x+inset,body.y+5,name,gold,body.w-inset-55);
        button(d,{body.right()-49,body.y+3,45,23},"返回",Close);
    }
    void set_modal(Overlay layer) {
        modal=layer;
        focused=layer==StoryPage || layer==EndingPage || layer==Routes || layer==TalePage || layer==EventReport ? 1 : 0;
        dirty=true;
    }
    int row_pitch() const { return dense_layout()?19:23; }
    int list_rows() const { return std::clamp((body.h-55)/row_pitch(),2,10); }
    void row(Draw& d, int index, Text name, Text meta, int action, bool highlight=false, int top=28, int glyph=-1) {
        const Rect r{body.x+4,body.y+top+index*row_pitch(),body.w-8,row_pitch()-2};
        const bool marked=pad_focus && button_count==focused;
        d.fill(r,marked?rgb(0x5b6047):(index%2?light:ink));
        if (marked) nav_icon(d,CursorIcon,{r.x+2,r.y+(r.h-12)/2,12,12});
        const auto mw=d.measure(meta);
        const int start=marked?17:5;
        if (glyph>=0) voyage_icon(d,glyph,{r.x+start,r.y+(r.h-14)/2,14,14});
        const int left=start+(glyph<0?0:17);
        const int y=r.y+(r.h-d.font.cell+1)/2;
        d.label(r.x+left,y,name,highlight ? gold : ivory,r.w-mw-left-8);
        if (mw) d.label(r.right()-mw-4,y,meta,mist);
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
    Text port_contact(int port) const {
        const auto language=PortSpeech[port];
        if (game.fluency(language)==Fluent || game.crew_interpreter(language)!=NoOfficer) return "可沟通";
        return game.fluency(language)==Phrases?"会短语":"需口译";
    }
    void preflight(Draw& d) {
        page_title(d,"航前计划",SeaChartIcon);
        const auto path=passage(game.s.origin,selected);
        const auto forecast=game.forecast(path);
        const int x=body.x+7, w=body.w-14;
        if (selected<PortCount) {
            const auto status=port_contact(selected);
            const int start=x+w-d.measure(status)-16;
            voyage_icon(d,SpeechIcon,{start,body.y+29,14,14});
            d.label(start+16,body.y+30,status,mist);
            d.label(x,body.y+30,Places[selected].name,gold,start-x-4);
        } else d.label(x,body.y+30,Places[selected].name,gold,w);
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
        button(d,{x,body.bottom()-54,third,23},"停靠点",ShowStops,false,false,{},MooringIcon);
        button(d,{x+third+4,body.bottom()-54,third,23},"地理",ShowIntel,false,false,{},AstrolabeIcon);
        button(d,{x+(third+4)*2,body.bottom()-54,third,23},"掌舵",StartHelm,false,!ready,{},HelmIcon);
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
               surplus<0 ? "仍要启航" : "启航",ConfirmSail,true,!ready,{},SailIcon);
    }
    void route_stops(Draw& d) {
        page_title(d,"航段与停靠点",MooringIcon);
        const auto path=passage(game.s.origin,selected);
        const auto forecast=game.forecast(path);
        const int visible=list_rows(), pages=std::max(1,(path.count+visible-1)/visible);
        page=std::clamp(page,0,pages-1);
        for (int j=0;j<visible && page*visible+j<path.count;++j) {
            const int i=page*visible+j;
            const auto& leg=Legs[path.edge[i]];
            const int port=path.reverse[i]?leg.from:leg.to;
            const bool known=game.charted(port);
            const Text status=!known?"待查":port<PortCount?port_contact(port):"测绘点";
            const auto eta=format("%d日 · %s",forecast.arrival_days[i],status.data());
            row(d,j,known?Places[port].name:Text("未标记水域"),eta,known?Destination+port:0,port==selected,
                28,known?(port<PortCount?SpeechIcon:AstrolabeIcon):-1);
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
        const int rows=std::clamp((body.h-74)/row_pitch(),1,9);
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
        voyage_icon(d,RationIcon,{x,body.y+45,15,15});
        voyage_icon(d,FlaskIcon,{x,body.y+63,15,15});
        d.label(x+18,body.y+46,format("食物 %d/%d",food,food_cap),ivory,w-18);
        d.label(x+18,body.y+64,format("淡水 %d/%d",water,water_cap),ivory,w-18);
        if (w>=208) {
            const int bar=w/3;
            meter(d,{x+w-bar,body.y+50,bar,8},food,food_cap,false);
            meter(d,{x+w-bar,body.y+68,bar,8},water,water_cap,true);
        }
        button(d,{x,body.bottom()-54,half,20},"卸食物",UnloadFood,false,!food);
        button(d,{x+half+5,body.bottom()-54,w-half-5,20},"卸淡水",UnloadWater,false,!water);
        const auto food_cost=game.resupply_cost(food+10,water);
        const auto water_cost=game.resupply_cost(food,water+10);
        button(d,{x,body.bottom()-32,half,PricedButtonHeight},format("食物 +%d",std::min(10,food_cap-food)),
               BuyFood,true,!food_cost || game.s.gold<food_cost,format("%d金",food_cost));
        button(d,{x+half+5,body.bottom()-32,w-half-5,PricedButtonHeight},format("淡水 +%d",std::min(10,water_cap-water)),
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
        if (game.s.origin==madeira ||
            (game.s.origin==cadiz && game.s.drift_stage>=DriftRecovered)) {
            pair(d,{body.x+4,body.y+74,body.w-8,21},
                 game.s.origin==madeira?"银铃护航":"码头决斗",
                 game.s.origin==madeira?ShowEscort:ShowDuel,"红漆木匣",ShowDriftStory);
        } else if (game.s.origin==lisbon)
            row(d,2,"银铃护航","航海故事",ShowEscort,true);
        else if (game.s.origin==cadiz)
            row(d,2,"加的斯的旧誓",game.s.cadiz_victory?"已完成":"码头决斗",ShowDuel,true);
        else if (game.s.origin==tunis)
            row(d,2,"优素福","招募领航",Recruit,true);
        pair(d,{body.x+6,body.bottom()-28,body.w-12,24},"打听传闻",ShowRumors,"口译与学话",OpenLanguages);
    }
    void drift_story(Draw& d) {
        page_title(d,"红漆木匣",LetterIcon);
        const int x=body.x+6,y=body.y,w=body.w-12;
        const Rect action{x,body.bottom()-28,w,24};
        const auto stage=game.s.drift_stage;
        const bool at_start=game.port() && game.s.origin==madeira;
        const bool at_finish=game.port() && game.s.origin==cadiz;
        Text dialogue;
        if (stage==DriftUnheard) {
            dialogue="艾达说，昨晚送工资名册的小船翻了。红匣子绑了软木，沉不了，风把它吹到了丰沙尔西南。";
            button(d,action,at_start?"答应找木匣":"标记丰沙尔",at_start?int(TakeDriftLead):Destination+int(madeira),
                   true,game.sea() || !game.charted(madeira));
        } else if (stage==DriftSearching) {
            dialogue="从丰沙尔向西南掌舵，到圈出的海面慢慢找。木匣要靠近才看得见。想用钩子捞，就带一件木料。";
            if (!game.sea())
                button(d,action,at_start?"圈出搜索海域":"标记丰沙尔",
                       at_start?int(MarkDriftGround):Destination+int(madeira),true);
        } else if (stage==DriftRecovered && !at_finish) {
            dialogue="匣里是份工资名册，背面写的是卡斯蒂利亚语。拿去加的斯，请当地船工看看还有谁没领到工钱。";
            if (!game.sea()) button(d,action,"标记加的斯",Destination+int(cadiz),true);
        } else if (stage==DriftRecovered && !game.understands(Castilian)) {
            dialogue="名册右栏有几个名字被墨涂了。船工愿意帮你核对，可你还听不懂他们的话。找个口译来吧。";
            button(d,action,format("请口译 %d金",Game::InterpreterFee),HireInterpreter,true,
                   game.s.gold<Game::InterpreterFee);
        } else if (stage==DriftRecovered) {
            dialogue="右栏是拖欠的工钱。名册交给船工，以后在加的斯修船打七五折。交给港务馆，可领一百八十金。";
            pair(d,action,"交给船工",ReturnDriftRoll,"入档领赏",FileDriftRoll,true);
        } else {
            dialogue=stage==DriftReturned?
                "最后一箱货卸完，船工排起队来核对名单。此后你来加的斯修船，他们总会留几个人帮你钉船板。" :
                "港务馆晾干名册，逐笔查清了商号欠的钱。头一批工钱总算发了下去。";
        }
        d.wrap({x,y+34,w,body.h-66},dialogue,ivory,15);
    }
    void drift_recovery(Draw& d) {
        page_title(d,"打捞红漆木匣",LetterIcon);
        const int x=body.x+6,y=body.y,w=body.w-12,bottom=body.bottom();
        const int art_height=std::clamp(body.h-91,28,105);
        const Rect art_box{x,y+27,w,art_height};
        const int source_height=std::min(180,320*art_height/w);
        d.image({art::driftwood},{0,(180-source_height)*3/4,320,source_height},art_box);
        d.frame(art_box,border);
        const auto boat=game.drift_block(LaunchBoat);
        const auto hook=game.drift_block(RigHook);
        const Text hint=!game.near_drift()?"木匣已经漂远，请重新靠近":
            !boat.empty()?boat:"浪头不高，可以放艇";
        d.label(x,art_box.bottom()+5,hint,boat.empty()?mint:mist,w);
        const int third=(w-8)/3;
        button(d,{x,bottom-27,third,24},"放艇",BoatForDrift,true,!boat.empty());
        button(d,{x+third+4,bottom-27,third,24},"架钩×1木",HookForDrift,false,!hook.empty());
        button(d,{x+(third+4)*2,bottom-27,w-2*(third+4),24},"等一日",WaitForDrift,
               false,!game.near_drift());
    }
    void show_languages(Language preferred) {
        if (!game.port()) return;
        language_back=modal;
        lesson_language=game.can_study(preferred)?preferred:PortSpeech[game.s.origin];
        set_modal(LanguageDesk);
    }
    void language_desk(Draw& d) {
        const int port=game.s.origin;
        const Language primary=PortSpeech[port], second=port_second_language(port);
        page_title(d,"口译与学话",PhrasebookIcon);
        constexpr Text levels[]{"未学","会短语","通晓"};
        row(d,0,LanguageNames[primary],levels[game.fluency(primary)],SelectPortLanguage,lesson_language==primary);
        if (second!=LanguageCount)
            row(d,1,LanguageNames[second],levels[game.fluency(second)],SelectSecondLanguage,lesson_language==second);
        const int x=body.x+6,w=body.w-12,bottom=body.bottom();
        const int crew=game.crew_interpreter(lesson_language);
        const bool local=game.local_interpreter(lesson_language);
        const auto crew_help=format("%s可随船代译",Officers[crew].name.data());
        if (game.tide_ledger_here())
            button(d,{x,bottom-56,w,24},"两岸潮簿",OpenLedger,true,false,{},LetterIcon);
        else
            d.label(x,bottom-53,crew!=NoOfficer?Text(crew_help):
                    local?"口译跟到离港":"码头口译仅供本次停泊",mist,w);
        const auto hire=format("口译 %d金",Game::InterpreterFee);
        const auto rank=game.fluency(lesson_language);
        const auto learn=format("%s%d金·%d日",rank==Unfamiliar?"短语":"进修",
                                game.lesson_fee(lesson_language),game.lesson_days(lesson_language));
        const int half=(w-5)/2;
        button(d,{x,bottom-28,half,24},local?Text("已约口译"):Text(hire),HireInterpreter,false,
               local || game.s.gold<Game::InterpreterFee);
        button(d,{x+half+5,bottom-28,w-half-5,24},rank==Fluent?Text("已通晓"):Text(learn),TakeLesson,true,
               rank==Fluent || game.s.gold<game.lesson_fee(lesson_language));
    }
    void tide_ledger_page(Draw& d) {
        page_title(d,"两岸潮簿",LetterIcon);
        const int x=body.x+6,y=body.y,w=body.w-12,bottom=body.bottom();
        const Rect action{x,bottom-28,w,24};
        const auto stage=game.s.tide_ledger;
        const bool south=game.s.origin==tangier;
        d.label(x,y+26,south?"丹吉尔 · 阿拉伯语":"加的斯 · 卡斯蒂利亚语",mist,w);
        Text dialogue;
        if (stage>=LedgerPublished) {
            dialogue=stage==LedgerPublished?
                "两岸渡船都抄走了一份。孩子替父亲念起对岸的船名，读到最后一个时，恰好有船进湾。" :
                "船主付了谢金，收回两本簿子。你留下潮图，渡口再听到咒骂时，还是只能隔水喊话。";
        } else if (south && stage==LedgerCopied) {
            dialogue="南岸记的是水，北岸却说船应向西。到加的斯找渡船家，把两本簿子并排摊开。";
            button(d,action,"标记加的斯",MarkLedger,true);
        } else {
            const auto language=south?Arabic:Castilian;
            if (!game.understands(language)) {
                dialogue=south?"修桨人摊开一本湿潮簿。你认得页角的船形，却看不懂旁边的记号。" :
                    "渡船家指着旧簿上的一行字。他语速太快，船名和风向混成一串。";
                button(d,action,format("请口译 %d金",Game::InterpreterFee),HireInterpreter,true,
                       game.s.gold<Game::InterpreterFee);
            } else if (south) {
                dialogue="修桨人说，弟弟出海后一直没回。他记下木屑向东漂，想请你把这一页带给北岸渡船家。";
                button(d,action,"抄下潮簿",CopyLedger,true);
            } else {
                dialogue="北岸记的是风，南岸记的是水。东风往西吹，表层海流仍向东。渡船家问你愿不愿意公开抄本。";
                pair(d,action,"公开双语抄本",PublishLedger,"交还 · 100金",ReturnLedger);
            }
        }
        d.wrap({x,y+45,w,bottom-32-(y+45)},dialogue,ivory,15);
    }
    void escort_page(Draw& d) {
        page_title(d,"银铃护航");
        const int x=body.x+7, w=body.w-14;
        const auto stage=game.s.escort;
        const int origin=game.s.origin;
        if (stage>=EscortPublished) {
            const Text copy=stage==EscortPublished?
                "船契贴出去，修船的人一眼就认出了假印。此后银铃号进丰沙尔，总要先鸣铃，再等最后一条渔船靠岸。" :
                "商号付了三百四十金。银铃号又能做生意了，只是码头上的人还在打听，谁用过那枚假印。";
            d.wrap({x,body.y+34,w,body.h-39},copy,ivory,16);
            return;
        }
        if (stage==EscortOpen) {
            d.wrap({x,body.y+33,w,body.h-66},
                "伊内丝想请你护送银铃号去丰沙尔。船上有木匠和修船料，有人拿假船契要扣船。",
                ivory,15);
        } else if (stage==EscortSailing) {
            d.wrap({x,body.y+33,w,body.h-66},
                "从里斯本开往丰沙尔，银铃号会在这段航路上与你会合。要是走散了，回里斯本重新出发。",
                ivory,15);
        } else {
            const auto head=stage==EscortWonDuel?
                "断铃号船长交出了船契。"
                : "炮战后，水手从漂箱里捞出了假船契。";
            const auto copy=format("%s去丰沙尔公开船契，木匠就能作证。商号也愿出三百四十金私下收走。",head);
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
            "费罗把旧印交给裁判，又教你怎么侧身避剑。今后在敌船上决斗，你的体力会多一些。" :
            "费罗想用旧印证明父亲的清白。他要先试试你能不能护住证人。见他突刺就挡，见他守就佯攻，见他虚晃便抢先刺过去。";
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
        const bool side_by_side=body.w>=340;
        const int portrait=side_by_side?std::clamp(body.h-98,34,74):20;
        const Rect face{body.x+6,body.y+27,portrait,portrait};
        d.image({art::portraits},{c.portrait*86,0,86,86},face);
        d.frame(face,border);
        d.label(face.right()+6,face.y+3,c.speaker,gold);
        const Text copy=game.s.chapter==1 && game.s.tales[Lamps]==Shared ?
            "渔会有了你的潮表，知道哪盏灯有假。找渔船一天就能回来。我手里有你哥哥留下的货单，等你回来就给你。" :
            c.message;
        const bool more=game.s.chapter+1<Chapters;
        const int left=side_by_side?face.right()+6:face.x;
        const int top=side_by_side?face.y+21:face.bottom()+3;
        const int end=body.bottom()-(more?47:31);
        d.wrap({left,top,body.right()-left-6,end-top},copy,ivory,14);
        if (more) {
            const auto& next=Story[game.s.chapter+1];
            d.label(face.x,body.bottom()-45,format("下一站  %s",Places[next.place].name.data()),mint,body.w-12);
        }
        pair(d,{body.x+4,body.bottom()-28,body.w-8,25},
             game.s.chapter==1 && game.s.tales[Lamps]==Shared ? "找渔船 1天" : c.first,
             ChooseFirst,c.second,ChooseSecond,true);
    }
    void tale_list(Draw& d) {
        page_title(d,"港口故事");
        const int visible=list_rows();
        const int pages=(TaleCount+3+visible-1)/visible;
        page=std::clamp(page,0,pages-1);
        for (int j=0;j<visible && page*visible+j<TaleCount+3;++j) {
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
            if (i==TaleCount+2) {
                const auto stage=game.s.drift_stage;
                const Text status=stage>=DriftReturned?"已完成":stage==DriftRecovered?"加的斯":
                    stage==DriftSearching?"搜索海面":"丰沙尔";
                row(d,j,"红漆木匣",status,ShowDriftStory,stage==DriftSearching,28,LetterIcon);
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
            d.label(x,body.y+32,format("到%s找%s",Places[port].name.data(),
                    (beginning?tale.starter:tale.listener).data()),gold,w);
            if (!beginning) d.wrap({x,body.y+52,w,body.h-83},tale.opening,ivory);
            if (game.charted(port))
                button(d,{x,body.bottom()-28,w,25},"在海图上标记",TaleMark,true,game.sea());
            return;
        }
        const int portrait=beginning ? tale.start_portrait : tale.finish_portrait;
        d.image({art::portraits},{portrait*86,0,86,86},{x,body.y+28,19,19});
        d.label(x+24,body.y+29,beginning ? tale.starter : tale.listener,gold,w-24);
        const auto requirement=beginning ? Text{} : game.tale_requirement(tale_item);
        if (!requirement.empty()) d.label(x+w-56,body.y+29,tale_item==Chartmakers ? "缺木料" : "缺水粮",mist,56);
        const Text copy=tale_item==Lamps && !beginning && game.s.beacon!=BeaconCharted ?
            "莱昂说，问过回港的渔船，一天就能查清灯位。你可以把潮表留给渔会，也可以卖给船行，换二百二十金。" :
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
        int note_y=body.y+53;
        if (p<PortCount) {
            const Language language=PortSpeech[p];
            const bool can_speak=game.fluency(language)==Fluent || game.crew_interpreter(language)!=NoOfficer;
            d.label(body.x+6,body.y+47,format("%s · %s",LanguageNames[language].data(),
                    can_speak?"随船可译":"可请本港口译"),mist,body.w-12);
            note_y=body.y+67;
        }
        d.wrap({body.x+6,note_y,body.w-12,body.bottom()-note_y-4},l.note,ivory);
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
        page_title(d,"航海传闻",SpeechIcon);
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
        const auto language=PortSpeech[rumor.port];
        const bool understood=heard || game.understands(language);
        page_title(d,heard||found?Places[rumor.site].name:rumor.speaker);
        const int x=body.x+6, w=body.w-12;
        const auto needed=format("需%s口译",LanguageNames[language].data());
        d.label(x,body.y+29,found?"已记入海图":heard?"只知道大致海域":
                understood?"可抄下航路传闻":Text(needed),found?mint:mist,w);
        const Text clue=understood?rumor.clue:found?"泊地已标在海图上。":
            game.fluency(language)==Phrases?RumorFragments[rumor_item]:"对方指向海面。找人代译，或先在港口学几句。";
        d.wrap({x,body.y+48,w,body.h-78},clue,ivory,15);
        if (!understood && !found)
            button(d,{x,body.bottom()-28,w,24},"找口译或学话",OpenLanguages,true);
        else if (!heard && !found)
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
            const bool can_copy=game.understands(BookLanguage[expedition_item]);
            const auto need=format("手记用%s写成。找口译或学会这门语言后，便可抄录。",LanguageNames[BookLanguage[expedition_item]].data());
            d.wrap({x,y+49,w,body.h-81},at_archive && !can_copy?Text(need):
                "抄录手记，便能筹措考察经费。其他船也在打听这份手记，赶先完成观测可获首报赏金。",ivory);
            const auto mark_archive=format("标记%s",Places[study.archive].name.data());
            button(d,actions,at_archive?(can_copy?"抄录手记":"找口译或学话"):
                game.charted(study.archive)?Text(mark_archive):"尚无航路",
                at_archive?(can_copy?CopyClue:OpenLanguages):MarkExpedition,true,
                !at_archive && (game.sea() || !game.charted(study.archive)));
            return;
        }
        const int remaining=record.rival_day-game.s.day;
        d.label(x,y+27,remaining>0?Text(format("首报余 %d 日",remaining)):Text("对手已经先行"),
                remaining>0?mint:vermilion,w);
        if (witness || at_archive)
            button(d,{x+w-58,y+24,58,21},show_witness?"手记":
                   !witness && !game.understands(WitnessLanguage[expedition_item])?"找口译":"口述",
                   witness?ReadNotes:AskWitness);
        const bool deciding=record.stage==ReadClue && at_archive;
        const bool mark=record.stage==ReadClue || witness;
        const int notes_y=y+44;
        d.wrap({x,notes_y,w,bottom-(deciding?46:mark?31:5)-notes_y},
               show_witness && witness?study.witness:study.book,ivory,15);
        if (deciding) {
            d.label(x,bottom-46,"预支 / 首报交稿金",quiet,w);
            button(d,{x,bottom-31,half,PricedButtonHeight},"领资助",SponsorVoyage,true,false,
                   format("%d / %d",study.advance,study.purse-study.advance*3/2));
            button(d,{x+half+5,bottom-31,w-half-5,PricedButtonHeight},"自费出海",FundVoyage,false,false,
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
    void audio_settings(Draw& d) {
        page_title(d,"设置",SettingsIcon);
        constexpr Text labels[]{"背景音乐","航海音效","船员语音"};
        constexpr unsigned choices[]{GameAudio::Music,GameAudio::Effects,GameAudio::Voices};
        for (int i=0;i<3;++i) {
            const bool enabled=audio.enabled(choices[i]);
            row(d,i,labels[i],enabled?"开启":"关闭",audio.loaded?ToggleMusic+i:0,enabled);
        }
        const Text text=font_choice==AutomaticFont?(small_text()?"自动 · 紧凑":"自动 · 标准"):
            font_choice==SmallFont?"紧凑":"标准";
        row(d,3,"界面字号",text,loaded?CycleFont:0,true);
    }
    void render(Draw& d) {
        layout(d);
        d.fill(d.clip,ink);
        button_count=0;
        if (title) {
            if (modal==AudioSettings) audio_settings(d);
            else start_screen(d);
            focused=std::clamp(focused,0,std::max(0,button_count-1));
            return;
        }
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
        case AudioSettings: audio_settings(d); break;
        case LanguageDesk: language_desk(d); break;
        case TideLedger: tide_ledger_page(d); break;
        case BargainPage: bargain_page(d); break;
        case DriftStory: drift_story(d); break;
        case DriftRecovery: drift_recovery(d); break;
        case MerchantBoard: merchant_board(d); break;
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
        if (action>=OpenLanguages) {
            switch (action) {
            case OpenLanguages:
                show_languages(modal==RumorPage?PortSpeech[Rumors[rumor_item].port]:
                               modal==ExpeditionPage?BookLanguage[expedition_item]:LanguageCount);
                break;
            case SelectPortLanguage: lesson_language=PortSpeech[game.s.origin]; break;
            case SelectSecondLanguage: lesson_language=port_second_language(game.s.origin); break;
            case HireInterpreter: if (game.hire_interpreter()) changed(); break;
            case TakeLesson: if (game.take_lesson(lesson_language)) changed(); break;
            case OpenLedger: if (game.tide_ledger_here()) set_modal(TideLedger); break;
            case CopyLedger:
                if (game.copy_tide_ledger()) { audio.cue(Sound::Page); changed(); }
                break;
            case MarkLedger: select(cadiz); break;
            case PublishLedger: case ReturnLedger:
                if (game.decide_tide_ledger(action==PublishLedger)) { audio.cue(Sound::Page); changed(); }
                break;
            case ShowBargain: if (game.port()) set_modal(BargainPage); break;
            case Negotiate:
                if (game.negotiate(good)) { set_modal(None); audio.cue(Sound::Page); changed(); }
                break;
            case ShowDriftStory: activity_back=modal; set_modal(DriftStory); break;
            case TakeDriftLead: if (game.take_drift_lead()) changed(); break;
            case MarkDriftGround:
                scout_course=DriftGround;
                pinned_rumor=-1;
                selected=game.s.origin;
                screen=Chart;
                set_modal(None);
                follow=false;
                center_at(DriftGround);
                break;
            case OpenDriftRecovery:
                if (game.near_drift()) { game.paused=true; set_modal(DriftRecovery); }
                break;
            case BoatForDrift: case HookForDrift:
                if (game.recover_drift(action==BoatForDrift?LaunchBoat:RigHook)) {
                    audio.cue(Sound::Page);
                    set_modal(None);
                    changed();
                } else notify();
                break;
            case WaitForDrift:
                if (game.wait_for_drift()) {
                    if (!game.near_drift()) set_modal(None);
                    changed();
                } else notify();
                break;
            case ReturnDriftRoll: case FileDriftRoll:
                if (game.finish_drift(action==ReturnDriftRoll)) { audio.cue(Sound::Page); changed(); }
                break;
            case CycleFont:
                if (loaded) {
                    font_choice=FontChoice((unsigned(font_choice)+1)%unsigned(FontChoices));
                    font_pending=true;
                    font_retry_at=0;
                }
                break;
            case ShowMerchants: if (game.port()) set_modal(MerchantBoard); break;
            case MarkMerchantFirst: case MarkMerchantSecond: {
                const auto& ship=game.s.merchants[action-MarkMerchantFirst];
                if (game.port()) select(MerchantPorts[ship.destination==NoMerchantPort?ship.port:ship.destination]);
                break;
            }
            }
            return;
        }
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
        case ShowAudio: set_modal(AudioSettings); break;
        case ToggleMusic: case ToggleEffects: case ToggleVoices:
            audio.toggle(1U<<(action-ToggleMusic));
            break;
        case ShowRumors: rumor_back=modal; page=0; set_modal(RumorList); break;
        case RecordRumor:
            if (game.hear_rumor(rumor_item)) { audio.cue(Sound::Page); changed(); }
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
                previous_helm={game.s.helm_x,game.s.helm_y};
                audio.cue(Sound::CastOff);
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
                audio.cue(Sound::CastOff);
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
                previous_helm={game.s.helm_x,game.s.helm_y};
                audio.cue(Sound::CastOff);
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
            if (game.dock_helm(chart_water)) {
                audio.cue(Sound::Anchor);
                select(game.s.origin);
                changed();
            }
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
            previous_helm={game.s.helm_x,game.s.helm_y};
            last_day=now;
            break;
        case Trim: game.s.reefed=1-game.s.reefed; audio.cue(Sound::Trim); save_needed=true; break;
        case Retreat:
            game.turn_back(); selected=game.sea()?game.s.destination:game.s.origin;
            last_day=now;
            follow=true;
            changed();
            break;
        case BuyOne: case BuyFive: case SellOne: case SellFive: {
            const bool sell=action>=SellOne;
            const auto trade=game.trade(good,sell,action==BuyFive || action==SellFive?TradeLot:1);
            if (trade.units) {
                if (trade.saved)
                    std::snprintf(trade_receipt.data(),trade_receipt.size(),"买入%d件 %d金 · 省%d金",
                                  trade.units,trade.gold,trade.saved);
                else
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
            if (!game.understands(WitnessLanguage[expedition_item])) {
                show_languages(WitnessLanguage[expedition_item]);
                break;
            }
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
            if (s.weather==Privateer && action==FightSecond && !s.needs_reload) {
                flash_until=now+570;
                audio.cue(Sound::Broadside);
                audio.cue(Sound::FireCall);
            }
            if (game.duel() && action!=FightFourth) {
                duel_animation=action-FightFirst;
                duel_flash_until=now+419;
                audio.cue(Sound::Steel);
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
            previous_helm={game.s.helm_x,game.s.helm_y};
            last_day=now;
            set_modal(None);
            break;
        }
    }

    bool back() {
        if (title && modal==AudioSettings) { set_modal(None); return true; }
        if (title) return false;
        if (modal!=None) {
            if (modal==Atlas && atlas_item) { atlas_item=0; dirty=true; }
            else if (modal==TalePage && screen==Journal) set_modal(TaleList);
            else if (modal==SeaLog && sea_item>=0) { sea_item=-1; focused=0; dirty=true; }
            else if (modal==RouteStops || (modal==Intel && intel_from_preflight)) set_modal(Preflight);
            else if (modal==EscortPage || modal==DuelInvitation || modal==DriftStory) set_modal(activity_back);
            else if (modal==ExpeditionPage && expedition_from_book) set_modal(ExpeditionList);
            else if (modal==OfficerPage) set_modal(Wardroom);
            else if (modal==RumorPage) set_modal(RumorList);
            else if (modal==RumorList) { page=0; set_modal(rumor_back); }
            else if (modal==LanguageDesk) set_modal(language_back);
            else if (modal==TideLedger) set_modal(LanguageDesk);
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
                follow=true;
                dirty=true;
                const auto heading=static_cast<std::uint8_t>(direction_to({x-here.x,y-here.y}));
                if (heading!=game.s.heading) {
                    game.s.heading=heading;
                    game.notice="已调整航向";
                    changed();
                }
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
            } else if (e.action()==PointerPhase::move) {
                last_x=p.x; last_y=p.y;
                dirty=true;
            }
            if (e.ended()) {
                pointer_capture.release(e);
                dirty=true;
                if (e.action()==PointerPhase::up && !panning &&
                    std::abs(p.x-press_x)+std::abs(p.y-press_y)<14) tap(p);
            }
        }
    }
};

} // namespace
} // namespace tide

PXA_GAME(tide::VoyageApp)
