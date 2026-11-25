#include "draw.hpp"
#include "chart_style.hpp"
#include "pilot_draw.hpp"
#include "game_audio.hpp"
#include "game_save.hpp"
#include "land_draw.hpp"
#include "sea_art.hpp"
#include "town.hpp"
#include <pxa/app.hpp>

#include <array>
#include <cstdio>
#include <optional>
#include <span>

namespace tide {
namespace {

constexpr Color ink = rgb(0x102429), panel = rgb(0x19343a), light = rgb(0x25464a);
constexpr Color border = rgb(0x497172), gold = rgb(0xf2cd87), ivory = rgb(0xf5ebd3);
constexpr Color mist = rgb(0xb9cebe), quiet = rgb(0x86a7a3), vermilion = rgb(0xdf825e);
constexpr Color mint = rgb(0x97d5b2), ocean = rgb(0x173d46);
constexpr Text tabs[] = {"海图","市场","船坞","港务","日志"};
constexpr int PricedButtonHeight=33;

enum Screen { Chart, Market, Shipyard, Office, Journal };
enum class MapView { None, Chart, Helm, Pilot };
enum Plate { StandardPlate, ActionPlate };
enum HarborPanel { HarborPlaque, HarborWood, HarborGoldWood, HarborPaper, HarborTrim };
constexpr int WidePlateWidth=128, WidePlateHeight=56, WidePlateEdge=10, WideBarHeight=20;
constexpr int ShipPortraitWidth=112, ShipPortraitHeight=80;
enum FontChoice : std::uint8_t { AutomaticFont, SmallFont, LargeFont, FontChoices };
enum Overlay {
    None, Routes, StoryPage, Intel, Help, Atlas, EndingPage, TaleList, TalePage, SeaLog,
    Preflight, RouteStops, PortPrices, TradePassagePage, EventReport, TowRequest, PortDesk, EscortPage, DuelInvitation, ProvisionsPage,
    ExpeditionList, ExpeditionPage, Wardroom, OfficerPage, RumorList, RumorPage, AudioSettings,
    LanguageDesk, TideLedger, BargainPage, DriftStory, DriftRecovery, MerchantBoard, HarborTown, PortStoryPage,
    DispatchBoard, ForecastPage, RationsPage, MarketNews, TimberTrailPage, LandPage, TradeQuantityPage, HaggleOutcomePage,
    NatureWatch, NatureAlbum, NaturePage, CharterDesk, CharterRegistry, ReserveOrder, ReserveSale, PortCasePage, FlagRack, FlagPage, ShorePage, ShoreJournal, FishBook, EnclaveRegistry, EnclavePage, MerchantRaidOffer, PilotBook, PilotStory, PilotParcelPage, LandBriefing, SpecialtyInfo, WorldCatalog, WorldStudy, WorldTrail, HarborBoatPage
};
enum NavIcon { MapIcon, ScaleIcon, ShipIcon, LedgerIcon, JournalIcon, CoinsIcon, HullIcon,
               FoodIcon, CompassIcon, CursorIcon, AnchorIcon, GlassIcon };
enum VoyageIcon { SpeechIcon, PhrasebookIcon, LetterIcon, AstrolabeIcon, PennantIcon,
                  FlaskIcon, RationIcon, MooringIcon, SeaChartIcon, SettingsIcon, CrewIcon,
                  RepairIcon, ReefIcon, ResumeIcon, PauseIcon, SailIcon, HelmIcon, CargoIcon };
enum HelmGlyph {
    HelmTurnLeft=100, HelmTurnRight, HelmGunLeft, HelmAnchor, HelmRope,
    HelmBoard, HelmNet, HelmCanvas, HelmGunRight
};
enum ChartActionIcon {
    ZoomInIcon=120, ZoomOutIcon, ReturnShipIcon, ShipWheelIcon,
    DockAnchorIcon, DestinationChartIcon, CompareScaleIcon, HarborLogIcon
};
enum JournalIcon {
    PortStoryIcon=140, SeaRecordIcon, ShoreNewsIcon, DiscoveryIcon,
    NatureBookIcon, HandbookIcon, ExpeditionBookIcon, SettlementIcon
};
constexpr int ActionIconSize=32;
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
    ShowAudio, ToggleMusic, ToggleEffects, ToggleVoices, ShowWeather, ResumeWeather,
    ShowRations, ToggleFoodRation, ToggleWaterRation, ShowMarketNews, CycleSpeed, WorldChart, ToggleHelmView,
    BeginSkirmish, FirePort, FireStarboard, DropRope, BoardSkirmish,
    BeginFishery, WorkFishery, CancelFishery, ShowFishBook, SellWhaleOil, TradePreparePurchase, TradeStartPreparation,
    DockService=120, GoodsRow=200, Destination=300, AtlasEntry=400, TaleEntry=500, SeaEntry=600,
    ExpeditionEntry=700, OfficerEntry=800, RumorEntry=900,
    OpenLanguages=1000, SelectPortLanguage, SelectSecondLanguage, HireInterpreter, TakeLesson,
    OpenLedger, CopyLedger, MarkLedger, PublishLedger, ReturnLedger, ShowBargain,
    HaggleBuy, HaggleSell, HaggleLess, HaggleMore, HaggleMakeOffer, HaggleAccept,
    ShowDriftStory, TakeDriftLead, MarkDriftGround, OpenDriftRecovery, BoatForDrift,
    HookForDrift, WaitForDrift, ReturnDriftRoll, FileDriftRoll, CycleFont,
    ShowMerchants, MarkMerchantFirst, MarkMerchantSecond, OpenTown, ReturnTown,
    WalkToTownFacility, ReadPortStory=WalkToTownFacility+int(TownFacilityCount),
    FindPortStory=ReadPortStory+int(PortStoryCount), PortStoryChooseFirst, PortStoryChooseSecond, PortStoryGuide,
    WalkToCityGate, WalkToOutsideGate, WalkToRiver, WalkToShed, WalkToPostRoad, ReturnTimber, TakeTimberReward,
    OpenDispatch, ViewDispatchReceipt, AcceptDispatch, FileDispatch, MarkDispatch, AskCancelDispatch,
    CancelDispatch, OpenLandReport, WalkLandStream, WalkLandPass, WalkLandCopy, WalkLandLookout,
    WalkLandBack, FileLandReport, OpenTradeQuantity, QuantityBuy, QuantitySell,
    QuantityBackspace, QuantityMaximum, QuantityConfirm, WalkRegionalTrail, WaitLandTide, QuantityDigit,
    DispatchEntry=QuantityDigit+10,
    OpenNatureWatch=DispatchEntry+DispatchCount, OpenNatureAlbum, WaitForNature, SketchNature, NatureSelect,
    OpenCharterDesk=NatureSelect+int(NatureCount), OpenCharterRegistry, SignBerth, OpenReserveOrder,
    ChangeFlagship, OpenReserveSale, ConfirmReserveSale, LocateBerth, OpenPortCase, SettlePortCase, CharterEntry,
    PurchaseReserve=CharterEntry+int(HarborCharters.size()),
    ToggleCompany=PurchaseReserve+int(std::size(HullNames)), OpenFlags, RaiseColors, SignRoyalPass,
    OpenShoreTask, AdvanceShoreTask, OpenShoreJournal, FlagEntry, ShoreEntry=FlagEntry+int(ColorsCount),
    OpenEnclaves=ShoreEntry+int(ShoreTaskCount), DockEnclave, ShowLocalEnclave, MarkEnclave, LeaseEnclave,
    EnclaveWait, HireVillagers, HireCarpenter, CarrySettlers, LodgeSettlers, SellEnclaveGoods,
    StormEnclave, ReleaseGuards, RansomGuards, EnclaveTab, EnclaveEntry=EnclaveTab+4,
    EnclaveConstruction=EnclaveEntry+EnclaveCount, EnclaveSupply=EnclaveConstruction+EnclaveBuildingCount, PrepareMerchantRaid=EnclaveSupply+4, ConfirmMerchantRaid, RenderWhaleOil,
    EnterPilot, LeavePilot, PilotZoomIn, PilotZoomOut, PilotCenter, PilotMoor, PilotUnmoor,
    PilotWaterBuy, PilotFoodBuy, PilotBookOpen, PilotStoryOpen, PilotGuide, PilotAim, PilotOpenParcel,
    PilotHandleParcel, PilotSpeed, PilotCenterShip, PilotCoastHint,
    ToggleSpecialties, ShowSpecialty, OpenLandBriefing, StartRoadEscort, DefendEnclave,
    LandBrace, LandEndRound, LandRetreat, OpenFieldSurvey, WaitLandWeather, OpenWorldCatalog, OpenWorldStudy,
    LandWorldTrail, FieldObserve, FieldStalk, FieldShoot, ShoreFood, ShoreWater,
    ShowHarborBoat, InspectDispatchStop, SealDispatchStop, RetryStoryDispatch,
    RouteFilter, FieldPlate=RouteFilter+5, WorldStudyEntry=FieldPlate+3, LandCellAction=WorldStudyEntry+FieldStudyCount,
    PilotSiteEntry=LandCellAction+LandCells
};
enum DispatchPage { DispatchList=-1, DispatchReceipt=-2, DispatchConfirmCancel=-3 };
static_assert(TradeStartPreparation<DockService);

float scale_at_zoom(std::span<const float> scales, float level) {
    const int lower=std::min(int(level),int(scales.size())-2);
    return std::lerp(scales[lower],scales[lower+1],level-lower);
}

float zoom_at_scale(std::span<const float> scales, float scale) {
    if (scale<=scales.front()) return 0.f;
    for (std::size_t i=1;i<scales.size();++i)
        if (scale<=scales[i]) return float(i-1)+(scale-scales[i-1])/(scales[i]-scales[i-1]);
    return float(scales.size()-1);
}

struct FormattedText {
    char contents[160]{};
    operator Text() const { return contents; }
};

template<class... Args>
FormattedText format(const char* message, Args... args) {
    FormattedText out;
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
        .simulation_step_us=50000, .frame_period_ms=33, .maximum_updates=1};
    static constexpr unsigned SailingDayMs=1200, HelmWatchMs=SailingDayMs/SailingWatches;
    static constexpr unsigned HelmPulseMs=HelmWatchMs/HelmPulsesPerWatch;

    Game game;
    GameAudio audio;
    SeaPicture sea_picture;
    PilotAtlas pilot_atlas;
    ChartTerrainCache chart_terrain;
    struct SailingChrome {
        Rect bounds{};
        MapView mode=MapView::None;
        std::array<int,18> header{};
        std::array<int,19> controls{};
        Rect chart_area{};
        std::array<int,25> chart_info{};
        std::array<int,3> chart_footer{};
        std::array<char,160> notice{};
        std::size_t notice_size=0;
        bool header_ready=false, controls_ready=false;
        bool chart_info_ready=false, chart_footer_ready=false;
    };
    std::array<SailingChrome,2> sailing_chrome{};
    Rect land_area{};
    LandFight land_before;
    unsigned land_frame_start=0;
    int land_selected=0, land_striker=-1;
    bool land_enemy_turn=false;
    bool land_retreat=false;
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
    pxa::input::PointerCapture second_pointer;
    std::array<Button,48> buttons{};
    int button_count=0, focused=0;
    Rect safe{}, body{}, map{};
    Rect town_area{};
    TownWalk town;
    int town_id=-1;
    int port_story_id=0;
    Overlay port_story_back=TaleList;
    Overlay dispatch_back=None;
    int dispatch_page=DispatchList;
    unsigned town_frame=0, town_hint_until=0, town_boat_frame=0;
    bool inside_town_facility=false;
    bool outside=false, at_timber_river=false, inland_archive=false;
    Overlay land_return=None;
    Overlay town_facility=None;
    int scale=1, raster_scale=1, surface_x=0, surface_y=0;
    int selected=madeira, good=Wine, page=0, atlas_item=0, tale_item=0, sea_item=-1;
    bool bargain_sell=false;
    int bargain_rate=8, bargain_counter=0;
    TradeQuote settled_bargain;
    HaggleReply bargain_reply=InvalidOffer;
    int trade_port=-1;
    int trade_quantity=0;
    bool quantity_sell=false;
    Overlay quantity_back=None;
    int charter_item=-1;
    int flag_item=MerchantColors, shore_item=GoldWash;
    Overlay shore_back=None;
    Overlay port_case_back=None;
    int field_item=0, field_plate=0;
    bool field_local=false, field_result=false;
    FieldWalk field_walk;
    Rect field_area{}, harbor_vessel_hitbox{};
    unsigned field_tick=0, field_painted=0;
    int nature_item=IslandLaurel;
    Overlay nature_page_back=NatureAlbum;
    int expedition_item=0;
    int officer_item=Pilot;
    int rumor_item=0, pinned_rumor=-1, route_filter=0;
    Language lesson_language=Portuguese;
    HarborApproach approach;
    Point approach_position{-1,-1};
    int approach_port=-1;
    std::optional<Point> scout_course;
    bool officer_talk=false;
    float chart_x=0, chart_y=0, factor=1, zoom=3;
    Point previous_helm{};
    int press_x=0, press_y=0, last_x=0, last_y=0;
    bool panning=false, follow=true, pad_focus=false, intel_from_preflight=false;
    MapView drawn_map=MapView::None, pointer_map=MapView::None;
    struct MapPinch {
        Point second;
        float span, scale, anchor_x, anchor_y;
    };
    std::optional<MapPinch> pinch;
    bool open_helm_view=false;
    int enclave_item=0, enclave_tab=0, raid_company=-1;
    unsigned skirmish_tick=0;
    int pilot_pending=-1, pilot_site=-1, pilot_guide=-1;
    float pilot_zoom=-1;
    Overlay pilot_story_back=None;
    bool pilot_follow=true;
    unsigned pilot_rate=1;
    unsigned pilot_painted=0;
    Point pilot_visual_from{-1,-1};
    float pilot_x=0, pilot_y=0, pilot_scale_x=1.f, pilot_scale_y=1.f, pilot_pixels_per_mile=1.f;
    struct SeaFlash { Point position{}; unsigned started=0; int sprite=0; };
    std::array<SeaFlash,4> sea_flashes{};
    unsigned flash_slot=0;
    bool title=true, loaded=false, had_save=false, opening=false, reconfigure=true, landscape_requested=false;
    bool foreground=false, dirty=true, save_needed=false, save_active=false, save_failed=false;
    FontChoice font_choice=AutomaticFont;
    bool font_pending=false, font_writing=false;
    unsigned font_retry_at=0;
    unsigned now=0, last_day=0, last_step=0, notice_until=0, flash_until=0, saved_until=0;
    unsigned surface_retry=0, duel_flash_until=0;
    unsigned voyage_rate=1, presented_vessel_frame=0, presented_weather_frame=0;
    int duel_animation=0;
    unsigned long long next_frame=1;
    Screen screen=Chart;
    Overlay modal=None, activity_back=None, rumor_back=None, language_back=None;
    bool expedition_from_book=false, show_witness=false;
    Overlay rations_back=None;
    bool resume_after_voyage_info=false;

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

    void on_foreground(pxa::Context&) {
        foreground=true;
        audio.foreground(true);
        last_day=skirmish_tick=0;
        dirty=true;
    }
    void on_background(pxa::Context&) {
        foreground=false;
        audio.foreground(false);
        reset_pointers();
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
        reset_pointers();
        dirty=true;
        return true;
    }
    void on_window_changed(const pxa::WindowMetrics& value) {
        if (value.pixel_width!=metrics.pixel_width || value.pixel_height!=metrics.pixel_height) {
            reconfigure=true;
            reset_pointers();
        }
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
        const auto area = std::uint32_t(width)*height;
        const int layout_scale = area > 220000 ? 2 : 1;
        const bool native = layout_scale==2 && area<=420000 && !(width&1) && !(height&1);
        int density=native?2:1;
        const auto w = static_cast<std::uint16_t>(width/layout_scale);
        const auto h = static_cast<std::uint16_t>(height/layout_scale);
        chart_terrain.reset();
        sailing_chrome={};
        surface.reset();
        auto created = co_await context->surface().create_mapped({
            .width=std::uint16_t(w*density), .height=std::uint16_t(h*density),
            .buffers=2, .max_buffer_bytes=native?1792u*1024:1024u*1024});
        if (!created && native) {
            density=1;
            created=co_await context->surface().create_mapped({
                .width=w, .height=h, .buffers=2, .max_buffer_bytes=1024*1024});
            if (created) (void)context->log().write(pxa::LogLevel::info,"Tide Pact using scaled display");
        }
        if (!created) {
            surface_retry=now+1800;
            (void)context->log().write(pxa::LogLevel::error,"Tide Pact surface allocation failed");
            opening=false;
            co_return std::unexpected(created.error());
        }
        auto config = co_await created->configure({.width=std::uint16_t(w*density),.height=std::uint16_t(h*density)});
        if (config) {
            scale=layout_scale;
            raster_scale=density;
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
    void set_zoom(float n) {
        zoom=std::clamp(n,0.f,7.f);
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
        return Image{art::chart}.index(p.x,p.y)<2;
    }

    bool pilot_view() const {
        return !title && screen==Chart && modal==None && game.s.pilot.region;
    }

    PilotWater pilot_water() const {
        const int region=game.s.pilot.region;
        if (region==WorldPilotIndex+1) return pilot_atlas.water(game.s.pilot.patch);
        return region?pilot_detail_water(region-1):PilotWater{};
    }

    PilotWater pilot_detail_water(int index) const {
        return {sea_picture.pixels(PilotPictureBase+index),false,PilotRegions[index].roadstead};
    }

    int visible_pilot_detail() const {
        if (pilot_pending>=0 && pilot_pending<WorldPilotIndex) return pilot_pending;
        if (game.s.pilot.region && game.s.pilot.region<=WorldPilotIndex) return game.s.pilot.region-1;
        return game.s.pilot.region?pilot_detail_at(game.pilot_atlas_position(),640):-1;
    }

    void prepare_pilot_chart() {
        if (title || (!game.s.pilot.region && pilot_pending<0)) return;
        const auto ship=game.pilot_atlas_position();
        const Point camera=pilot_follow?ship:Point{int(pilot_x),int(pilot_y)};
        const auto crossing=game.pilot_exit_position();
        int priority=-1;
        if (game.pilot_exiting()) priority=pilot_patch_at({crossing.x/8,crossing.y/8});
        else if (pilot_pending==WorldPilotIndex) priority=game.starting_pilot_patch();
        else if (game.s.pilot.region==WorldPilotIndex+1) priority=game.s.pilot.patch;
        Point northwest{-1,-1},southeast{-1,-1};
        if (drawn_map==MapView::Pilot && pilot_zoom>=0.f && pilot_zoom<2.f &&
            pilot_scale_x>0.f && pilot_scale_y>0.f) {
            const int x=int(std::ceil(map.w*.5f/pilot_scale_x));
            const int y=int(std::ceil(map.h*.5f/pilot_scale_y));
            const auto column=[](int u) { return std::clamp(u/PilotPatchWidth,0,PilotTilesX-1); };
            const auto row=[](int v) { return std::clamp(v/PilotPatchHeight,0,PilotTilesY-1); };
            northwest={column(camera.x-x),row(camera.y-y)};
            southeast={column(camera.x+x),row(camera.y+y)};
        }
        const bool dragging=drawing_pilot_drag();
        pilot_atlas.prepare(*context,ship,camera,priority,dirty,northwest,southeast,dragging && pilot_zoom<2.f);
        if (game.pilot_exiting() && !game.paused) {
            const auto water=pilot_atlas.water(priority);
            if (water.bytes) {
                if (!game.connect_pilotage(water)) notify();
                save_needed=true;
                dirty=true;
            } else if (pilot_atlas.failed(priority)) {
                game.set_pilot_heading(game.s.pilot.heading);
                game.paused=true;
                game.notice="海图读取失败，停船后可重试";
                notify();
            }
        } else if (game.s.pilot.region==WorldPilotIndex+1) {
            const int detail=pilot_detail_at(ship);
            if (detail>=0 && (!game.paused || PilotRegions[detail].roadstead) &&
                game.refine_pilotage(detail,pilot_detail_water(detail))) {
                if (game.paused && pilot_follow) pilot_zoom=5;
                save_needed=true;
                dirty=true;
            }
        }
    }

    unsigned voyage_interval() const {
        return game.s.pilot.region?PilotTickMilliseconds/pilot_rate:game.s.helm?HelmPulseMs:SailingDayMs/voyage_rate;
    }

    bool full_helm_chart() const {
        return !title && screen==Chart && modal==None && game.s.helm &&
            (game.s.pilot.region || game.s.skirmish.active || (open_helm_view && !game.encounter()));
    }

    MapView map_view() const {
        if (title || screen!=Chart || modal!=None || game.encounter() || game.s.skirmish.active ||
            game.s.land_fight.kind) return MapView::None;
        if (pilot_view()) return MapView::Pilot;
        return full_helm_chart()?MapView::Helm:MapView::Chart;
    }

    void cycle_voyage_rate() {
        if (!game.sea() || game.encounter() || game.s.pilot.region || pilot_pending>=0) return;
        const unsigned interval=voyage_interval();
        const unsigned limit=game.s.helm?2:4;
        voyage_rate=voyage_rate==limit?1:voyage_rate*2;
        if (last_day && !game.paused)
            last_day=now-std::min(now-last_day,interval)*voyage_interval()/interval;
        else last_day=now;
    }

    void cycle_pilot_rate() {
        const auto water=pilot_water();
        if (!pilot_view() || !water.bytes || game.pilot_slow_water(water)) return;
        const unsigned interval=voyage_interval();
        pilot_rate=pilot_rate==4?1:pilot_rate*2;
        last_day=game.paused?now:now-std::min(now-last_day,interval)*voyage_interval()/interval;
    }

    void on_frame(pxa::Context&, pxa::game::FrameTick tick) {
        if (!foreground) return;
        now=unsigned(tick.timestamp_us / 1000);
        const bool battle=!title && (game.s.weather==Privateer || game.duel() || game.s.land_fight.kind);
        audio.tick(tick.timestamp_us,battle?Score::Battle:!title && game.sea()?Score::Voyage:Score::Harbor);
        if ((reconfigure || !surface) && !opening && now>=surface_retry) {
            opening=true;
            if (!context->tasks().start(open_surface())) opening=false;
        }
        if (!surface || opening) return;
        prepare_pilot_chart();

        if (modal==HarborTown && town.moving() && now-town_frame>=30) {
            town_frame=now;
            const int door=town.update(now);
            if (door>=0) arrive_in_town(door);
            dirty=true;
        }
        if (modal==HarborTown && !outside && local_vessel(game.s.origin)>=0 && now-town_boat_frame>=90) {
            town_boat_frame=now;
            dirty=true;
        }
        if (town_hint_until && now>=town_hint_until) {
            town_hint_until=0;
            dirty|=modal==HarborTown;
        }

        if (!title && game.s.skirmish.active) update_skirmish();
        if (!title && game.s.land_fight.kind && land_frame_start) {
            dirty=true;
            if (now-land_frame_start>=220) land_frame_start=0;
        }
        if (!title && game.sea() && !game.paused && !game.encounter()) {
            if (!last_day) last_day=now;
            // Recover a few missed pilot ticks after a slow frame. The cap
            // prevents a long I/O stall from jumping the vessel across the chart.
            const unsigned tick_budget=game.s.pilot.region?3:1;
            for (unsigned tick_index=0;tick_index<tick_budget;++tick_index) {
                const unsigned interval=voyage_interval();
                if (now-last_day<interval || !game.sea() || game.paused ||
                    game.encounter() || game.pilot_exiting()) break;
                const int old_day=game.s.day;
                const int old_hull=game.s.hull;
                const int old_food=game.s.stores, old_water=game.s.water;
                const auto old_marks=game.s.chart_marks;
                const bool manual=game.s.helm;
                const bool accelerated=game.s.pilot.region?pilot_rate>1:voyage_rate>1;
                const int moored=game.s.pilot.moored;
                const auto fish_stage=game.s.fishery.stage, fish_target=game.s.fishery.target;
                if (manual) {
                    previous_helm={game.s.helm_x,game.s.helm_y};
                    if (game.s.pilot.region) {
                        pilot_visual_from=game.pilot_atlas_position();
                        save_needed|=game.pilot_tick(pilot_water());
                    }
                    else game.helm_pulses(chart_water,voyage_rate);
                }
                else game.advance();
                last_day+=interval;
                if (now-last_day>=interval && tick_index+1==tick_budget)
                    last_day=now-(now-last_day)%interval;
                if (accelerated && game.sea() && !game.encounter() && !game.paused &&
                    ((old_food && !game.s.stores) || (old_water && !game.s.water))) {
                    game.paused=true;
                    pilot_rate=1;
                    game.notice=!game.s.stores && !game.s.water ? "水粮耗尽" :
                                !game.s.stores ? "食物耗尽" : "淡水耗尽";
                }
                if (pilot_rate>1 && game.s.pilot.region && game.pilot_slow_water(pilot_water())) {
                    pilot_rate=1;
                    if (!game.paused) {
                        game.notice="近岸减速，已切回一倍";
                        notify();
                    }
                }
                if (!game.sea() || game.encounter() || game.paused) voyage_rate=1;
                if ((game.s.day!=old_day && game.s.day%4==0) ||
                    !game.sea() || game.encounter() || game.paused || game.s.chart_marks!=old_marks)
                    save_needed=true;
                if (game.s.fishery.stage!=fish_stage || game.s.fishery.target!=fish_target) changed();
                if (modal==ExpeditionPage && game.s.day!=old_day) dirty=true;
                if (game.paused || game.s.hull<old_hull) notify();
                if (!moored && game.s.pilot.moored) {
                    audio.cue(Sound::Anchor);
                    pilot_site=game.s.pilot.moored-1;
                    pilot_story_back=None;
                    set_modal(PilotStory);
                }
                if (manual && game.s.helm && game.paused) {
                    if (game.s.hull<old_hull) audio.cue(Sound::Shoal);
                    else if (game.nearby_anchorage(chart_water)>=0) audio.cue(Sound::AnchorCall);
                    else if (game.s.chart_marks!=old_marks) {
                        audio.cue(Sound::Sighting);
                        audio.cue(Sound::LandCall);
                    }
                }
                if (!game.sea()) {
                    audio.cue(Sound::Anchor);
                    modal=None; screen=Chart; selected=game.s.origin;
                    open_helm_view=false;
                    notify();
                } else if (game.encounter()) {
                    modal=None; screen=Chart; focused=0; notify();
                }
            }
            if (screen==Chart && modal==None && (!game.s.pilot.region || now-pilot_painted>=30)) {
                pilot_painted=now;
                dirty=true;
            }
        } else last_day=now;
        if (notice_until && now >= notice_until) { notice_until=0; dirty=true; }
        if (saved_until && now >= saved_until) { saved_until=0; dirty=true; }
        if (!title && screen==Chart && modal==None && game.sea() && !game.s.pilot.region) {
            const unsigned frame=now/VesselFrameMs;
            if (frame!=presented_vessel_frame) {
                presented_vessel_frame=frame;
                dirty=true;
            }
        }
        if (!title && weather_animation()>=0 && !panning && !pinch) {
            const unsigned frame=now/180;
            if (frame!=presented_weather_frame) {
                presented_weather_frame=frame;
                dirty=true;
            }
        }
        if (modal==WorldTrail && !title) tick_field();
        sea_picture.show(*context,visible_sea_picture(),dirty);
        if (pilot_pending>=0 && !title) {
            const bool world=pilot_pending==WorldPilotIndex;
            const int asset=PilotPictureBase+pilot_pending;
            const int patch=world?game.starting_pilot_patch():-1;
            const auto water=world?pilot_atlas.water(patch):pilot_detail_water(pilot_pending);
            if (water.bytes) {
                if (game.begin_pilotage(pilot_pending,water)) {
                    pilot_zoom=-1;
                    pilot_site=-1;
                    pilot_guide=-1;
                    pilot_follow=true;
                    pilot_visual_from=game.pilot_atlas_position();
                    pilot_rate=1;
                    voyage_rate=1;
                    audio.cue(Sound::CastOff);
                    changed();
                } else notify();
                pilot_pending=-1;
            } else if (world?pilot_atlas.failed(patch):sea_picture.failed(asset)) {
                pilot_pending=-1;
                game.notice="领航图读取失败，可再试一次";
                notify();
            }
        }
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
        Draw draw(*pixels,frame->pixels(),frame->stride_bytes(),small_text()?CompactFace:StandardFace,raster_scale);
        render(draw,sailing_chrome[frame->buffer_index()]);
        if (frame->present(next_frame)) { ++next_frame; dirty=false; }
    }

    void layout(Draw& d) {
        const auto& inset=metrics.safe_insets;
        int sx=std::max(8,(int(inset.left)-surface_x+scale-1)/scale+3);
        int sy=std::max(8,(int(inset.top)-surface_y+scale-1)/scale+3);
        int rx=std::max(8,(int(inset.right)-surface_x+scale-1)/scale+3);
        int by=std::max(8,(int(inset.bottom)-surface_y+scale-1)/scale+3);
        safe={sx,sy,d.clip.w-sx-rx,d.clip.h-sy-by};
        body={safe.x,safe.y+39,safe.w,safe.h-39-(full_helm_chart()?5:36)};
    }

    void skin_box(Draw& d, Rect r, int tile, int edge) {
        if (d.pixel_scale>1) {
            wide_plate(d,r,tile,WidePlateEdge,edge);
            return;
        }
        d.nine_slice({art::skin},{tile*72,0,72,72},r,8,edge);
    }
    void plate(Draw& d, Rect r, Plate style, int edge=4) {
        if (d.pixel_scale>1) {
            wide_plate(d,r,2+style,8,edge);
            return;
        }
        d.nine_slice({art::controls},{style*96,0,96,32},r,edge,edge,true);
    }
    void harbor_panel(Draw& d, Rect r, HarborPanel style, int edge=6) {
        if (d.pixel_scale>1) {
            wide_plate(d,r,4+style,WidePlateEdge,edge,style==HarborTrim);
            return;
        }
        d.nine_slice({art::port_ui},{style*64,0,64,40},r,6,edge,true);
    }
    void wide_plate(Draw& d,Rect r,int tile,int source_edge,int edge,bool trim=false) {
        d.nine_slice({art::ui_wide},{tile*WidePlateWidth,0,WidePlateWidth,WidePlateHeight},
                     r,source_edge,edge,true,trim?Draw::SliceCenter::Transparent:Draw::SliceCenter::Tile);
    }
    bool pressed(Rect r) const {
        return pointer_capture.active() && pointer_map==MapView::None && r.contains({press_x,press_y}) &&
               r.contains({last_x,last_y}) && std::abs(last_x-press_x)+std::abs(last_y-press_y)<14;
    }
    void focus_marker(Draw& d, Rect r) {
        const Point cue{r.x+r.w/2,r.y+1};
        d.diamond(cue,3,ink);
        d.diamond(cue,2,ivory);
    }
    void meter(Draw& d, Rect r, int value, int maximum, bool voyage) {
        if (d.pixel_scale>1) {
            auto native=d.native_pixels();
            r=d.pixel_rect(r);
            const Image atlas{art::ui_wide};
            native.nine_slice(atlas,{0,WidePlateHeight,WidePlateWidth,WideBarHeight},r,4,4);
            const Rect track=r.inset(4);
            if (track.w<1 || track.h<1) return;
            maximum=std::max(1,maximum);
            const int width=int(std::int64_t(track.w)*std::clamp(value,0,maximum)/maximum);
            if (!width) return;
            native.clip=native.clipped({track.x,track.y,width,track.h});
            native.blit(atlas,{WidePlateWidth*(voyage?2:1),WidePlateHeight,WidePlateWidth,WideBarHeight},
                        track,false,false);
            native.fill({track.x+width-1,track.y,1,track.h},voyage?gold:mint);
            return;
        }
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
        if (icon<GoodsCount) d.image({art::goods},{icon%4*48,icon/4*48,48,48},r,true);
        else d.image({art::specialty_icons},{(icon-GoodsCount)*24,0,24,24},r,true);
    }
    void voyage_icon(Draw& d, int icon, Rect r) {
        d.image({art::voyage_icons},{icon%3*32,icon/3*32,32,32},r,true);
    }
    int weather_animation() const {
        if (modal==LandPage && !inland_archive) {
            switch (game.land_weather()) {
            case DryHeat: return 2;
            case WetTrail: case FloodedTrail: return 0;
            case SnowTrail: case Whiteout: return 1;
            default: return -1;
            }
        }
        if (modal!=None || screen!=Chart || !pilot_view()) return -1;
        const auto weather=game.conditions().weather;
        return weather==Snow?1:weather==Rain || weather==Squall?0:-1;
    }

    void weather_marks(Draw& d,Rect area,int effect,int columns,int rows) {
        if (effect<0 || area.w<28 || area.h<24) return;
        const auto clip=d.clip;
        d.clip=d.clipped(area);
        columns=std::min(columns,std::max(1,area.w/33));
        rows=std::min(rows,std::max(1,area.h/29));
        const int pitch_x=area.w/columns, pitch_y=area.h/rows;
        for (int y=0;y<rows;++y) {
            for (int x=0;x<columns;++x) {
                const int phase=(presented_weather_frame+x+2*y)%4;
                const int dx=(x*17+y*13)%11-5;
                const int dy=(x*7+y*11)%9-4;
                const Rect mark{area.x+x*pitch_x+(pitch_x-28)/2+dx,
                                area.y+y*pitch_y+(pitch_y-24)/2+dy,28,24};
                d.image({art::weather_motion},{phase*32,effect*28,32,28},mark,true);
            }
        }
        d.clip=clip;
    }

    void weather_icon(Draw& d, SeaWeather weather, Rect r) {
        if (weather==Snow) {
            d.image({art::weather_motion},{0,28,32,28},r,true);
            return;
        }
        d.image({art::weather_icons},{int(weather)*32,0,32,32},r,true);
    }
    void port_scene(Draw& d, Rect area, int port) {
        if (!is_port(port) || area.w<1 || area.h<1) return;
        if (const auto* view=sea_picture.pixels(harbor_picture(port))) {
            const Image picture{view};
            const int width=std::min(picture.width(),picture.height()*area.w/area.h);
            const int height=std::min(picture.height(),picture.width()*area.h/area.w);
            d.image(picture,{(picture.width()-width)/2,(picture.height()-height)*2/5,width,height},area);
        } else d.fill(area,ocean);
    }
    void button(Draw& d, Rect r, Text text, int action, bool accent=false, bool inactive=false,
                Text price={}, int glyph=-1) {
        const bool chosen=pad_focus && button_count==focused && !inactive;
        if (!inactive && button_count<int(buttons.size())) buttons[button_count++]={r,action};
        // Cached map panels keep their buttons even when their pixels are reused.
        if (!d.clip.w || !d.clip.h) return;
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
        } else if (glyph>=ZoomInIcon) {
            const bool journal_icon=glyph>=PortStoryIcon;
            const Image atlas{journal_icon?art::journal_icons:art::chart_actions};
            const int base=journal_icon?int(PortStoryIcon):int(ZoomInIcon);
            const int item=glyph-base, cell=ActionIconSize;
            const Rect sprite{item%4*cell,item/4*cell,cell,cell};
            const int size=std::clamp(r.h-6,12,18);
            if (text.empty()) {
                const Rect icon{content.x+(content.w-size)/2,content.y+(content.h-size)/2,size,size};
                d.image(atlas,sprite,icon,true);
                if (inactive) d.darken(icon,6);
            } else d.center_with_icon(content,text,color,atlas,sprite,size);
        } else if (glyph>=HelmTurnLeft) {
            const bool reflected= glyph==HelmGunRight;
            const int item=(reflected?HelmGunLeft:glyph)-HelmTurnLeft;
            const Rect sprite{item%4*HelmIconSize,item/4*HelmIconSize,HelmIconSize,HelmIconSize};
            d.center_with_icon(content,text,color,{art::helm_icons},sprite,std::clamp(r.h-7,12,17),reflected);
        } else if (glyph>=0) {
            const Rect sprite{glyph%3*32,glyph/3*32,32,32};
            d.center_with_icon(content,text,color,{art::voyage_icons},sprite,std::clamp(r.h-7,12,17));
        } else {
            d.center(content,text,color);
        }
        d.clip=previous_clip;
    }
    void pair(Draw& d, Rect r, Text a, int aa, Text b, int bb, bool accent=false) {
        const int half=(r.w-5)/2;
        button(d,{r.x,r.y,half,r.h},a,aa,accent);
        button(d,{r.x+half+5,r.y,r.w-half-5,r.h},b,bb);
    }
    void harbor_button(Draw& d, Rect r, Text text, int action, bool preferred=false, bool inactive=false,
                       Text detail={}, int glyph=-1) {
        const bool focus=pad_focus && focused==button_count && !inactive;
        harbor_panel(d,r,(preferred || focus) && !inactive ? HarborGoldWood : HarborWood);
        if (inactive || pressed(r)) d.darken(r.inset(4),inactive?8:4);
        auto content=r.inset(4);
        const auto clip=d.clip;
        d.clip=d.clipped(content);
        const Color label=inactive?rgb(0xb1a38e):ivory;
        if (detail.empty() && glyph>=0) {
            d.center_with_icon(content,text,label,{art::voyage_icons},{glyph%3*32,glyph/3*32,32,32},13);
        } else if (!detail.empty()) {
            const int text_width=std::max(d.measure(text),d.measure(detail));
            if (glyph>=0 && text_width+27<=content.w) {
                const int x=content.x+(content.w-text_width-23)/2;
                voyage_icon(d,glyph,{x,content.y+(content.h-18)/2,18,18});
                content.x=x+23;
                content.w=text_width;
            }
            d.center_lines(content,text,label,detail,inactive?label:rgb(0xe5c28f));
        } else d.center(content,text,label);
        d.clip=clip;
        if (focus) focus_marker(d,r);
        if (!inactive) buttons[button_count++]={r,action};
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
        const int local_hour=game.s.sail_watch*HoursPerWatch+game.s.helm_pulse;
        const auto hour=format(game.s.pilot.region?" %02d:%02d":" %02d时",
                               local_hour,int(game.s.pilot.minutes));
        const auto date=format("第%d日%s",game.s.day+1,game.s.helm || game.s.sail_watch || game.s.helm_pulse?hour.contents:"");
        const int date_x=safe.right()-6-d.measure(date);
        const auto stores=format("%d/%d",game.s.stores,int(game.s.water));
        const auto compact=format("粮%d 水%d",game.s.stores,int(game.s.water));
        const bool fits=x+20+d.measure(stores)<=date_x;
        if (fits) {
            voyage_icon(d,RationIcon,{x,safe.y+4,14,14});
            d.label(x+16,safe.y+4,stores,ivory);
        }
        d.label(date_x,safe.y+4,date,mist);
        const bool show_weather=screen==Chart && modal==None && game.sea() && !game.encounter() && !game.s.pilot.region;
        const int reserve=fits?0:d.measure(compact)+8;
        const int weather_x=safe.right()-58-reserve;
        const int rations_x=weather_x-52;
        const int speed_x=rations_x-46;
        const int status_width=show_weather?speed_x-safe.x-10:safe.w-(fits?80:d.measure(compact)+15);
        if (save_failed && !save_active)
            d.label(safe.x+6,safe.y+18,"存档失败，正在重试",vermilion,status_width);
        else if (modal!=EventReport && modal!=HaggleOutcomePage && notice_until && !game.notice.empty())
            d.label(safe.x+6,safe.y+18,game.notice,mint,status_width);
        else {
            const auto& p=Places[game.s.origin];
            d.label(safe.x+6,safe.y+18,game.sea()?"航行中":p.name,quiet,status_width);
            if (saved_until && fits && !show_weather) d.label(safe.right()-108,safe.y+18,"已存档",quiet);
        }
        if (show_weather) {
            button(d,{speed_x,safe.y+16,42,18},format("%u倍速",voyage_rate),CycleSpeed,voyage_rate>1);
            button(d,{rations_x,safe.y+16,48,18},"配给",ShowRations,game.s.ration_flags!=0);
            button(d,{weather_x,safe.y+16,54,18},"风况",ShowWeather);
        }
        else if (fits) meter(d,{safe.right()-60,safe.y+20,55,9},game.s.hull,100,false);
        if (!fits) d.label(safe.right()-6-d.measure(compact),safe.y+18,compact,mint);
    }
    void sailing_header(Draw& d,SailingChrome& chrome) {
        const auto& s=game.s;
        const bool actions=chrome.mode!=MapView::Pilot && game.sea();
        const std::array<int,18> key{s.gold,s.hull,s.stores,s.water,s.day,s.sail_watch,s.helm_pulse,
            s.pilot.minutes,s.origin,d.font.cell,save_failed && !save_active,bool(saved_until),bool(notice_until),
            int(voyage_rate),s.ration_flags,pad_focus?focused:-1,actions,s.helm};
        const Text message=notice_until?game.notice:Text{};
        const bool touching=actions && pointer_capture.active() && pointer_map==MapView::None;
        if (!touching && chrome.header_ready && chrome.header==key &&
            message==Text(chrome.notice.data(),chrome.notice_size)) {
            if (actions) {
                // The pixel buffer still owns the last header, but the
                // current frame needs its three interactive buttons.
                const auto clip=d.clip;
                d.clip={};
                header(d);
                d.clip=clip;
            }
            return;
        }
        chrome.header=key;
        chrome.notice_size=std::min(message.size(),chrome.notice.size());
        if (chrome.notice_size) std::copy_n(message.data(),chrome.notice_size,chrome.notice.data());
        chrome.header_ready=!touching && message.size()<=chrome.notice.size();
        d.fill({0,0,d.clip.w,body.y},ink);
        header(d);
    }
    void footer(Draw& d) {
        constexpr int tab_count=std::size(tabs), tab_gap=4, padding=2;
        const int tabs_width=safe.w-tab_gap*(tab_count-1);
        // All labels have two glyphs. Size the icons for the narrowest tab.
        const int inline_icon=std::min(20,tabs_width/tab_count-2*(padding+d.font.cell)-4);
        for (int i=0;i<tab_count;++i) {
            const int left=safe.x+i*safe.w/tab_count, right=safe.x+(i+1)*safe.w/tab_count;
            const Rect hit{left,safe.bottom()-32,right-left,32};
            const int start=i*tabs_width/tab_count, end=(i+1)*tabs_width/tab_count;
            const Rect r{safe.x+start+i*tab_gap,hit.y,end-start,hit.h};
            const bool focused_tab=pad_focus && button_count==focused;
            const bool active=screen==i;
            const bool town_return=inside_town_facility && i==Office;
            buttons[button_count++]={hit,town_return?ReturnTown:OpenChart+i};
            if (!d.clip.w || !d.clip.h) continue;
            const Text title=town_return?"回城":tabs[i];
            const bool down=pressed(hit);
            harbor_panel(d,r,active || down ? HarborGoldWood : HarborPlaque,5);
            if (down) d.darken(r.inset(4),3);
            if (focused_tab) focus_marker(d,r);
            const Color label=active ? gold : ivory;
            const Image images{art::tabs};
            const Rect sprite{i*32,0,32,32};
            const Rect content=r.inset(padding);
            if (inline_icon>=14) {
                d.center_with_icon(content,title,label,images,sprite,inline_icon);
            } else {
                const auto caption=d.fit(title,content.w,1);
                const int size=std::min(14,content.h-caption.ink.h-1);
                const int y=content.y+(content.h-size-1-caption.ink.h)/2;
                d.image(images,sprite,{r.x+(r.w-size)/2,y,size,size},true);
                d.center_run({content.x,y+size+1,content.w,caption.ink.h},caption,label);
            }
        }
    }

    template<std::size_t N>
    void chart_section(Draw& d,Rect area,const std::array<int,N>& key,std::array<int,N>& previous,bool& ready) {
        const bool touching=pointer_capture.active() && pointer_map==MapView::None;
        if (touching || !ready || previous!=key) d.fill(area,ink);
        // Keep rebuilding the hitboxes even when this buffer already holds the pixels.
        else d.clip={};
        previous=key;
        ready=!touching;
    }

    void chart_footer(Draw& d,SailingChrome& chrome) {
        const auto clip=d.clip;
        const std::array<int,3> key{d.font.cell,inside_town_facility,pad_focus?focused-button_count:-100};
        chart_section(d,{safe.x,safe.bottom()-34,safe.w,34},key,chrome.chart_footer,chrome.chart_footer_ready);
        footer(d);
        d.clip=clip;
    }

    bool wide_title() const {
        return wide_display() && (safe.w?safe.w:int(metrics.pixel_width)/scale-16)>=345;
    }

    void start_screen(Draw& d) {
        const bool horizontal=wide_title();
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

    float chart_focus_scale(Point target) const {
        const float camera_w=std::max(12.f,std::abs(game.s.helm_x-target.x)/16.f+3.5f);
        const float camera_h=std::max(7.8f,std::abs(game.s.helm_y-target.y)/16.f+4.4f);
        return std::min({map.w/camera_w,map.h/camera_h,27.f});
    }

    std::array<float,8> chart_scales() const {
        std::array<float,8> scales{};
        scales[0]=std::min(float(map.w)/ChartWidth,float(map.h)/ChartHeight);
        scales[1]=std::min(float(map.w)/LegacyChartWidth,float(map.h)/LegacyChartHeight)*1.52f;
        for (std::size_t i=2;i<scales.size();++i) scales[i]=scales[i-1]*1.52f;
        const auto& fish=game.s.fishery;
        if (fish.target && full_helm_chart()) {
            const auto target=fish.stage==TrackingFish?fish.animal:fish.gear;
            scales[4]=std::max(scales[4],chart_focus_scale(target));
            for (std::size_t i=5;i<scales.size();++i) scales[i]=std::max(scales[i],scales[i-1]*1.25f);
        }
        return scales;
    }

    void atlas_background(Draw& d) {
        factor=game.s.skirmish.active?chart_focus_scale(game.s.skirmish.rival):scale_at_zoom(chart_scales(),zoom);
        chart_x=std::clamp(chart_x,0.f,float(ChartWidth));
        chart_y=std::clamp(chart_y,0.f,float(ChartHeight));
        auto native=d.native_pixels();
        const auto view=d.pixel_rect(map);
        const float density_scale=factor*d.pixel_scale;
        const Rect world{view.x+view.w/2-int(std::lround(chart_x*density_scale)),
                         view.y+view.h/2-int(std::lround(chart_y*density_scale)),
                         int(std::lround(ChartWidth*density_scale)),int(std::lround(ChartHeight*density_scale))};
        const bool helm=full_helm_chart();
        chart_terrain.draw_chart(native,view,world,game,d.pixel_scale,helm,!zoom && !helm,
                                 !zoom && helm && !game.s.skirmish.active);
    }
    Rect chart_scale_box() const { return {map.right()-78,map.bottom()-23,74,20}; }

    void draw_chart_scale(Draw& d,Rect box) {
        if (map.w<115 || factor<=0.f) return;
        constexpr int distances[]{1,2,5,10,20,50,100,200,500,1000,2000,5000,10000};
        const float latitude=ChartNorth-chart_y/ChartLatitudeScale;
        const float pixels_per_mile=factor*ChartLongitudeScale/
            (60.f*std::cos(latitude*0.0174532925f));
        int miles=distances[0];
        for (int candidate : distances) {
            if (candidate*pixels_per_mile>box.w-7) break;
            miles=candidate;
        }
        const int length=std::clamp(int(miles*pixels_per_mile+.5f),1,box.w-7);
        const int end=box.right()-3, begin=end-length, y=box.bottom()-4;
        const auto caption=format("%d海里",miles);
        const int label_x=end-d.measure(caption);
        d.label(label_x+1,box.y+1,caption,ink);
        d.label(label_x,box.y,caption,ivory);
        d.line({begin,y+1},{end,y+1},ink);
        d.line({begin,y},{end,y},gold);
        d.line({begin,y-3},{begin,y},gold);
        d.line({end,y-3},{end,y},gold);
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
            if (full_helm_chart() && zoom>=4) {
                const bool west=Anchor[MerchantPorts[ship.destination]].x<location.x;
                vessel_sprite(d,i==0?0:2,{p.x-8,p.y-9,17,17},west);
                d.diamond({p.x,p.y+3},2,flags[i]);
            } else {
                d.diamond(p,5,ink);
                d.diamond(p,3,flags[i]);
                voyage_icon(d,SailIcon,{p.x-6,p.y-13,12,12});
            }
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
    int vessel_frame() const { return (now/VesselFrameMs)%VesselFrames; }

    void small_ship(Draw& d,int hull,int heading,bool reefed,Rect box) {
        auto native=d.native_pixels();
        box=d.pixel_rect(box);
        const auto visible=native.clipped(box);
        const auto* atlas=art::pilot_ships;
        for (int y=visible.y;y<visible.bottom();++y) {
            const int row=(hull*2+int(reefed))*SmallShipSize+(y-box.y)*SmallShipSize/box.h;
            auto* pixels=native.row(y);
            for (int x=visible.x;x<visible.right();++x) {
                const int column=(heading&7)*SmallShipSize+(x-box.x)*SmallShipSize/box.w;
                const int index=row*SmallShipStride+column;
                const int color=(atlas[32+index/2]>>(4*(index&1)))&15;
                if (color!=15) pixels[x]=u16(atlas+color*2);
            }
        }
    }

    void vessel_sprite(Draw& d,int hull,Rect viewport,bool west) {
        small_ship(d,hull,west?6:2,false,viewport);
    }

    void map_vessel(Draw& d,Point at,int hull,int heading,bool following) {
        const int size=following?16:full_helm_chart() && zoom<3?19:23;
        const auto vector=Directions[heading];
        if (!game.paused && zoom) {
            const Point stern{at.x-vector.x/2,at.y-vector.y/2};
            const int wake=10+vessel_frame()/2;
            for (int side : {-1,1}) {
                const Point end{stern.x-vector.x*wake/16-vector.y*side/6,
                                stern.y-vector.y*wake/16+vector.x*side/6};
                d.line(stern,end,rgb(0x779e99),true);
            }
        }
        small_ship(d,hull,heading,game.s.reefed,{at.x-size/2,at.y-size/2,size,size});
        if (!following && zoom>=4 && game.s.colors!=FurledColors)
            flag_art(d,game.s.colors,{at.x+5,at.y-9,9,6});
    }

    void draw_map(Draw& d, int landmark, int berth) {
        const auto before=d.clip;
        d.clip=map;
        float fraction=game.sea() && !game.paused && !game.encounter() ? float(now-last_day)/voyage_interval() : 0.f;
        const auto ship=game.position(std::clamp(fraction,0.f,1.f));
        float ship_x=ship.x, ship_y=ship.y;
        if (game.s.helm) {
            const auto blend=game.paused?1.f:std::clamp(float(now-last_day)/voyage_interval(),0.f,1.f);
            ship_x=std::lerp(float(previous_helm.x),float(game.s.helm_x),blend)/16;
            ship_y=std::lerp(float(previous_helm.y),float(game.s.helm_y),blend)/16;
        }
        if (game.sea() && follow && zoom) {
            chart_x=ship_x;
            chart_y=ship_y;
            const auto& fish=game.s.fishery;
            if (full_helm_chart() && fish.target && zoom>=4) {
                const auto target=fish.stage==TrackingFish?fish.animal:fish.gear;
                chart_x=(chart_x+target.x/16.f)*.5f;
                chart_y=(chart_y+target.y/16.f)*.5f;
            }
        }
        atlas_background(d);
        if (full_helm_chart()) helm_rain(d);
        auto path=game.sea() ? game.route : passage(game.s.origin,selected);
        const bool voyage=game.sea() && !game.s.helm;
        const int marked=voyage?game.sailing_target():selected;
        if (voyage && game.s.stop_next) path.count=game.s.leg+1;
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

        struct Geo { float lon,lat; Text label; bool land; int associated_port=-1; };
        constexpr Geo names[] = {
            {-25,79,"北冰洋",false}, {-89,43,"北美洲",true}, {-57,-16,"南美洲",true},
            {101,48,"亚洲",true}, {134,-25,"澳大利亚",true}, {38,-76,"南极洲",true},
            {177,8,"太平洋",false}, {113,16,"南海",false}, {-61,-60,"德雷克海峡",false},
            {17,34,"地中海",false}, {-26,15,"佛得角",false,ribeira}, {64,-7,"印度洋",false},
            {-43,35,"大西洋",false}, {-47,71,"格陵兰",true}, {20,51,"欧洲",true},
            {16,-10,"非洲",true}, {77,21,"印度半岛",true}, {-74,13,"加勒比海",false},
            {45,16,"阿拉伯半岛",true}, {64,16,"阿拉伯海",false}, {51,27,"波斯湾",false},
            {39,20,"红海",false}, {35,75,"巴伦支海",false}, {-19,65,"冰岛",true},
            {18,63,"斯堪的纳维亚",true}, {-5,24,"撒哈拉",true}, {3,-2,"几内亚湾",false},
            {42,-21,"莫桑比克海峡",false}, {-23,0,"赤道",false}, {89,14,"孟加拉湾",false}
        };
        std::array<Rect,64> placed{};
        int labels=0;
        const Rect compass{map.right()-32,map.y+4,26,26};
        placed[labels++]=compass;
        const auto ship_mark=map_point(ship_x,ship_y);
        if (full_helm_chart()) {
            const int w=30, h=26;
            placed[labels++]={ship_mark.x-w/2-2,ship_mark.y-h, w+4,h+8};
            placed[labels++]={map.x+3,map.y+3,92,22};
            if (map.w>=115) placed[labels++]=chart_scale_box();
        }
        auto draw_label=[&](Point p, Text text, Color color, bool coast, bool force) {
            Rect r{p.x,p.y,d.measure(text)+4,d.font.cell+2};
            if (r.x<map.x+1 || r.y<map.y+1 || r.right()>map.right()-1 || r.bottom()>map.bottom()-1) return false;
            if (!force) {
                for (int i=0;i<labels;++i)
                    if (r.x<placed[i].right() && r.right()>placed[i].x &&
                        r.y<placed[i].bottom() && r.bottom()>placed[i].y) return false;
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
            return true;
        };
        auto port_label=[&](Point p,Text label,Color color,int gap) {
            const int width=d.measure(label)+4;
            int left=p.x+gap;
            if (left+width>map.right()) left=p.x-gap-width;
            const int top=std::clamp(p.y-17,map.y+2,map.bottom()-17);
            if (draw_label({left,top},label,color,true,false) || !full_helm_chart()) return;
            if (draw_label({p.x+gap,p.y+5},label,color,true,false) ||
                draw_label({p.x-gap-width,p.y+5},label,color,true,false)) return;
            const auto ship_box=placed[1];
            if (!ship_box.contains(p)) return;
            const Point offset{ship_box.right()+5,top};
            if (draw_label(offset,label,color,true,false))
                d.line({ship_box.right(),p.y},{offset.x-1,top+d.font.cell/2},quiet);
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
            const bool landed=bool(game.s.visited&place_bit(i));
            auto c=i==target()?gold:!is_port(i)?mint:landed?ivory:quiet;
            d.diamond(p,i==marked ? 6 : i==target() ? 4 : 3,ink);
            d.diamond(p,i==marked ? 4 : 2,c);
        }
        for (const int i : {marked,target(),game.s.origin}) {
            if (!game.charted(i)) continue;
            const auto p=map_point(Places[i].map);
            if (p.y<map.y || p.y>=map.bottom()) continue;
            port_label(p,Places[i].name,i==marked?gold:ivory,7);
        }
        for (auto& n:names) {
            const Point source=chart_point(n.lon,n.lat);
            if ((zoom || full_helm_chart()) && !game.chart_cell(source.x/ChartCell,source.y/ChartCell) &&
                (n.associated_port<0 || !game.charted(n.associated_port))) continue;
            auto p=map_point(source);
            draw_label({p.x-d.measure(n.label)/2,p.y},n.label,zoom && n.land?rgb(0x293e37):ivory,
                       !zoom || !n.land,false);
        }
        if (zoom>=3) for (int i=0;i<PlaceCount;++i) {
            if (!game.charted(i) || i==marked || i==target() || i==game.s.origin) continue;
            const auto p=map_point(Places[i].map);
            if (!map.contains(p)) continue;
            if (full_helm_chart()) port_label(p,Places[i].name,ivory,4);
            else draw_label({p.x+4,p.y-14},Places[i].name,ivory,true,false);
        }
        if (zoom>=4) {
            for (const auto& site : EnclaveSites) {
                if (!game.charted(site.home)) continue;
                const auto p=map_point(site.position);
                if (!map.contains(p)) continue;
                d.diamond(p,4,ink);
                d.diamond(p,2,vermilion);
                draw_label({p.x+4,p.y+5},site.name,gold,true,false);
            }
        }
        if (game.sea()) {
            auto p=ship_mark;
            const auto bow=game.position(.94f), stern=game.position(.08f);
            const int heading=game.s.helm?game.s.heading:direction_to({bow.x-stern.x,bow.y-stern.y});
            if (game.s.in_company)
                map_vessel(d,{p.x-Directions[heading].x,p.y-Directions[heading].y+5},
                           game.s.berth.vessel-1,heading,true);
            map_vessel(d,p,game.s.ship,heading,false);
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
        if (full_helm_chart()) draw_fishery(d);
        if (landmark>=0) {
            const auto mark=map_point(Anchor[landmark]);
            d.diamond(mark,5,ink);
            d.diamond(mark,3,berth>=0?mint:gold);
            d.dot(mark.x,mark.y,ink);
        }
        nav_icon(d,CompassIcon,compass);
        if (full_helm_chart()) draw_chart_scale(d,chart_scale_box());
        d.clip=before;
        d.frame(map,border);
    }

    void helm_rain(Draw& d) {
        const auto state=game.conditions();
        if (map.w<=0 || map.h<=0 || (state.weather!=Rain && state.weather!=Squall)) return;
        const bool strong=state.weather==Squall;
        const int count=std::clamp(map.w*map.h/(strong?710:1350),10,strong?94:54);
        const int phase=int((now/36)%4096);
        const int lean=Directions[state.direction].x>=0?1:-1;
        for (int i=0;i<count;++i) {
            const unsigned seed=std::uint32_t(i+1)*2654435761U;
            const int x=map.x+(int(seed>>16)+phase*2)%map.w;
            const int y=map.y+(int(seed&65535)+phase*4)%map.h;
            const int length=3+int((seed>>3)&3);
            d.line({x,y},{x+lean*length/2,y+length},
                   strong?rgb(0x658e90):rgb(0x527c80));
        }
    }

    void fish_sprite(Draw& d,int frame,Point at,int size) {
        d.image({art::fishery},{frame%4*26,frame/4*26,26,26},
                {at.x-size/2,at.y-size/2,size,size},true);
    }

    void draw_fishery(Draw& d) {
        const auto& job=game.s.fishery;
        if (!job.target) return;
        const auto at=map_point(job.animal.x/16.f,job.animal.y/16.f);
        if (job.target==SardineFishery) {
            if (job.stage!=GearReady) fish_sprite(d,job.age/3%2,at,27);
        } else if (job.stage!=GearReady) {
            fish_sprite(d,game.whale_at_surface()?4:5,at,33);
            if (job.age%28<5) fish_sprite(d,6,{at.x+5,at.y-10},23);
        }
        if (job.stage!=TrackingFish) {
            const auto gear=map_point(job.gear.x/16.f,job.gear.y/16.f);
            fish_sprite(d,job.target==WhaleFishery?7:job.work>=3?3:2,gear,31);
            if (job.stage==GearReady) d.diamond({gear.x,gear.y-15},3,gold);
        }
    }

    void chart_full_helm(Draw& d,SailingChrome* chrome=nullptr) {
        const auto& fish=game.s.fishery;
        map={body.x,body.y,body.w,body.h-(fish.target?69:47)};
        const int berth=game.nearby_anchorage(chart_water);
        const int landmark=berth>=0?berth:game.visible_landmark();
        const bool salvage=berth<0 && game.near_drift();
        draw_map(d,landmark,berth);
        const auto state=game.conditions();
        const int speed=game.speed_in_tenths_of_a_knot();
        const auto course=format("%s %d.%d节",Bearings[game.s.heading].data(),speed/10,speed%10);
        const Rect badge{map.x+4,map.y+4,d.measure(course)+31,19};
        skin_box(d,badge,0,4);
        weather_icon(d,state.weather,{badge.x+4,badge.y+2,15,15});
        d.label(badge.x+22,badge.y+3,course,ivory,badge.w-24);
        if (game.pilot_region()>=0 || pilot_pending>=0)
            button(d,{map.right()-51,map.y+32,46,19},pilot_pending>=0?"读图":"精航",EnterPilot,true,pilot_pending>=0);
        if (game.raid_target(chart_water)>=0)
            button(d,{badge.x,badge.bottom()+4,65,19},"截停商船",PrepareMerchantRaid);
        const auto bar=[&](int index,int count,int row) {
            const int left=body.x+index*(body.w+3)/count;
            return Rect{left,map.bottom()+2+row*22,body.x+(index+1)*(body.w+3)/count-3-left,21};
        };
        constexpr Text labels[]{{},{},"回船","海图"};
        constexpr int actions[]{ZoomIn,ZoomOut,Center,ToggleHelmView};
        constexpr int icons[]{ZoomInIcon,ZoomOutIcon,ReturnShipIcon,DestinationChartIcon};
        const bool lookout=game.nature_site()!=NoNatureSite && !salvage;
        const int fishing=fish.target?NoFishery:game.fishery_available();
        const bool village=berth>=0 && game.enclave_nearby(chart_water)>=0;
        const int count=5+int(lookout)+int(fishing!=NoFishery)+int(village);
        const auto clip=d.clip;
        if (chrome) {
            const std::array<int,19> key{lookout,fishing,village,salvage,berth>=0,game.s.reefed,game.paused,
                zoom==0,zoom>=7,d.font.cell,pad_focus?focused-button_count:-100};
            const bool touching=pointer_capture.active() && pointer_map==MapView::None;
            const bool refresh=touching || fish.target || !chrome->controls_ready || chrome->controls!=key;
            if (refresh) d.fill({0,map.bottom(),d.clip.w,d.clip.h-map.bottom()},ink);
            else d.clip={};
            chrome->controls=key;
            chrome->controls_ready=!touching && !fish.target;
        }
        for (int i=0;i<4;++i)
            button(d,bar(i,count,0),labels[i],actions[i],false,(i==0 && zoom>=7) || (i==1 && zoom==0),{},icons[i]);
        int next=4;
        if (lookout) button(d,bar(next++,count,0),"观察",OpenNatureWatch,true);
        if (fishing)
            button(d,bar(next++,count,0),fishing==WhaleFishery?"寻鲸":"寻鱼",BeginFishery,true);
        if (village) button(d,bar(next++,count,0),"村镇",DockEnclave);
        button(d,bar(next,count,0),salvage?"捞木匣":"靠泊",salvage?OpenDriftRecovery:DockHelm,
               berth>=0 || salvage,berth<0 && !salvage,{},salvage?int(LetterIcon):int(DockAnchorIcon));
        button(d,bar(0,4,1),"左舵",SteerLeft,false,false,{},HelmTurnLeft);
        button(d,bar(1,4,1),"右舵",SteerRight,false,false,{},HelmTurnRight);
        button(d,bar(2,4,1),game.s.reefed?"张帆":"缩帆",Trim,false,false,{},HelmCanvas);
        button(d,bar(3,4,1),game.paused?"继续":"停船",Pause,true,false,{},game.paused?ResumeIcon:PauseIcon);
        if (fish.target) {
            const auto reason=game.fishery_action_requirement();
            const auto width=body.w/2;
            const Text action=fish.target==WhaleFishery?(fish.stage==GearReady?"接回小艇":"放小艇"):
                fish.stage==GearReady?"收网":"撒网";
            const Rect label{body.x,map.bottom()+46,width-2,21};
            skin_box(d,label,0,3);
            d.center(label.inset(2),reason.empty()?(fish.target==WhaleFishery?"北大西洋露脊鲸":"欧洲沙丁鱼"):reason,
                     reason.empty()?mint:mist);
            button(d,{body.x+width+1,label.y,body.w/4-2,21},action,WorkFishery,true,!reason.empty(),
                   {},fish.target==SardineFishery?HelmNet:-1);
            button(d,{body.x+width+body.w/4+1,label.y,body.w-width-body.w/4-1,21},
                   "召回",CancelFishery);
        }
        d.clip=clip;
    }

    Point pilot_point(float x,float y) const {
        return {map.x+map.w/2+int(std::lround((x-pilot_x)*pilot_scale_x)),
                map.y+map.h/2+int(std::lround((y-pilot_y)*pilot_scale_y))};
    }
    Point pilot_point(Point atlas) const { return pilot_point(atlas.x,atlas.y); }

    std::array<float,7> pilot_scales() const {
        return {map.w/200.f,map.w/100.f,map.w/60.f,map.w/36.f,map.w/22.f,map.w/12.f,map.w/6.f};
    }

    static float pilot_longitude_scale(float camera_y, float pixels_per_mile) {
        const float latitude=ChartNorth-camera_y/(ChartLatitudeScale*float(PilotAtlasUnit));
        return pixels_per_mile*60.f*std::cos(latitude*.0174532925f)/(ChartLongitudeScale*PilotAtlasUnit);
    }
    bool drawing_pilot_drag() const {
        return panning && pointer_map==MapView::Pilot && !pinch && pilot_scale_x>0.f;
    }

    struct PilotPosition { float x,y; };
    PilotPosition pilot_visual_ship() const {
        const auto ship=game.pilot_atlas_position();
        if (game.paused || pilot_visual_from.x<0 || game.pilot_exiting())
            return {float(ship.x),float(ship.y)};
        const auto period=voyage_interval();
        const float fraction=float(std::min(now-last_day,period))/period;
        return {std::lerp(float(pilot_visual_from.x),float(ship.x),fraction),
                std::lerp(float(pilot_visual_from.y),float(ship.y),fraction)};
    }

    template<class F>
    void pilot_visible_sites(F visit) const {
        for (int id=0;id<int(std::size(PilotSites));++id)
            if (game.pilot_site_visible(id)) visit(id);
        for (int id=WorldPilotSite;id<PilotSiteEnd;++id) {
            const bool duplicate=id<CoastalPilotSite && std::any_of(std::begin(PilotRegions),std::end(PilotRegions),
                [&](const PilotRegion& region) { return region.port==id-WorldPilotSite; });
            if (!duplicate && game.pilot_site_visible(id)) visit(id);
        }
    }

    void pilot_mark(Draw& d,int glyph,Point center,int size) {
        d.image({art::pilot_marks},{glyph%3*PilotMarkSize,glyph/3*PilotMarkSize,PilotMarkSize,PilotMarkSize},
                {center.x-size/2,center.y-size/2,size,size},true);
    }

    void pilot_distance(Draw& d) {
        const float pixels_per_tenth=pilot_pixels_per_mile*.1f;
        const int limit=std::clamp(map.w/4,16,56);
        int tenths=1;
        for (const int candidate : {1,2,5,10,20,50,100,200,500,1000}) {
            if (candidate*pixels_per_tenth>limit) break;
            tenths=candidate;
        }
        const int end=map.right()-7, y=map.bottom()-8;
        const int start=end-std::max(1,int(tenths*pixels_per_tenth+.5f));
        const auto text=tenths<10?format("0.%d海里",tenths):format("%d海里",tenths/10);
        const int x=end-d.measure(text);
        d.label(x+1,y-14,text,ink);
        d.label(x,y-15,text,ivory);
        d.line({start,y+1},{end,y+1},ink);
        d.line({start,y},{end,y},gold);
        d.line({start,y-3},{start,y},gold);
        d.line({end,y-3},{end,y},gold);
        if (end-start>=16) d.dot((start+end)/2,y-1,gold);
    }

    int pilot_target() const {
        if (game.pilot_docking()>=0) return game.pilot_docking();
        if (pilot_guide>=0 && game.pilot_site_visible(pilot_guide)) return pilot_guide;
        return game.pilot_parcel_destination();
    }

    Rect pilot_viewport() const { return {body.x,body.y,body.w,body.h-46}; }

    void pilot_chart(Draw& d,SailingChrome* chrome=nullptr) {
        const auto& state=game.s.pilot;
        const auto water=pilot_water();
        map=pilot_viewport();
        if (pilot_zoom<0) pilot_zoom=water.world?2:game.pilot_area().roadstead?5:4;
        const auto saved_clip=d.clip;
        d.clip=d.clipped(map);
        const auto tracked=pilot_visual_ship();
        if (pilot_follow) {
            pilot_x=tracked.x;
            pilot_y=tracked.y;
        }
        pilot_x=std::clamp(pilot_x,0.f,float(PilotAtlasWidth-1));
        pilot_y=std::clamp(pilot_y,0.f,float(PilotAtlasHeight-1));
        pilot_pixels_per_mile=scale_at_zoom(pilot_scales(),pilot_zoom);
        pilot_scale_y=pilot_pixels_per_mile*60.f/(ChartLatitudeScale*PilotAtlasUnit);
        const auto viewport=d.pixel_rect(map);
        const int density=d.pixel_scale;
        const float sy=pilot_scale_y*density;
        // Keep scale fixed under the finger. Reprojecting at each latitude
        // during a drag discards every cached pixel even when zoom is unchanged.
        const float sx=drawing_pilot_drag()?pilot_scale_x*density:chart_terrain.stable_longitude_scale(
            pilot_longitude_scale(pilot_y,pilot_pixels_per_mile)*density,sy,viewport.w);
        pilot_scale_x=sx/density;
        const Point camera{int(std::lround(pilot_x*sx)),int(std::lround(pilot_y*sy))};
        const int detail_index=visible_pilot_detail();
        const Text area_name=game.s.pilot.region<=WorldPilotIndex?game.pilot_area().name:"精航海图";
        Rect detail_view{};
        PilotWater detail_water{};
        const float detail_start=detail_index>=0 && PilotRegions[detail_index].roadstead?2.5f:1.5f;
        const int detail_opacity=int(std::lround(15.f*std::clamp((pilot_zoom-detail_start)*2.f,0.f,1.f)));
        if (detail_index>=0 && detail_opacity) {
            const auto& area=PilotRegions[detail_index];
            const auto project=[&](Point at) {
                return Point{viewport.x+viewport.w/2+int(std::lround(at.x*sx))-camera.x,
                             viewport.y+viewport.h/2+int(std::lround(at.y*sy))-camera.y};
            };
            const auto tl=project(pilot_to_atlas(area,{0,0}));
            const auto br=project(pilot_to_atlas(area,{PilotWidth*PilotSubcell,PilotHeight*PilotSubcell}));
            detail_view={tl.x,tl.y,br.x-tl.x,br.y-tl.y};
            detail_water=pilot_detail_water(detail_index);
        }
        auto native=d.native_pixels();
        chart_terrain.draw(native,viewport,camera,sx,sy,pilot_atlas,game,
                           detail_index,detail_water,detail_view,detail_opacity,density);
        if (!panning && !pinch) weather_marks(d,map,weather_animation(),6,3);
        const int guide=pilot_target();
        const int berth=game.pilot_berth(water,guide);
        const bool docking=game.pilot_docking()>=0;
        const auto ship=pilot_point(tracked.x,tracked.y);
        const auto approach=game.pilot_approach(guide,water);
        const auto caption=[&]() {
            if (guide<0 && berth<0 && state.moored==0) return format("%s","点海面转舵");
            const auto site=game.pilot_site(guide>=0?guide:berth);
            if (docking) return format("%s 进港中",site.name.data());
            if (berth>=0 && !state.moored) {
                const auto landing=game.pilot_site(berth);
                const Text action=landing.transfer?"接驳":landing.kind==PilotPoint?"测绘点":"可泊";
                return format("%s %s",action.data(),landing.name.data());
            }
            if (guide<0 || berth==guide) return format("%s",site.name.data());
            if (site.kind!=PilotPoint && approach.known && !approach.clear)
                return format("%s 绕行",site.name.data());
            return format("%s  %d.%d海里",site.name.data(),approach.tenths/10,approach.tenths%10);
        }();
        const Rect detail{map.x+5,map.bottom()-23,std::min(map.w-75,d.measure(caption)+13),19};
        const Rect title_box{map.x+5,map.y+5,d.measure(area_name)+25,20};
        const Rect compass{map.right()-29,map.y+4,24,24};
        const auto current=game.pilot_current();
        int current_speed=game.paused?0:game.pilot_speed(water,berth);
        if (current_speed && current.tenths && !docking) {
            const auto way=PilotDirections[state.heading], flow=PilotDirections[current.bearing];
            const int east=way.x*current_speed+flow.x*current.tenths;
            const int south=way.y*current_speed+flow.y*current.tenths;
            current_speed=int(std::lround(std::sqrt(float(east*east+south*south))/100));
        }
        const auto course=format("%s  %d.%d节",PilotBearings[state.heading].data(),
                                 current_speed/10,current_speed%10);
        const Rect course_box{compass.x-7-d.measure(course),map.y+5,d.measure(course)+5,20};
        const auto tide_status=current.tenths?
            format("%s %s %d.%d节",current.flood?"涨潮":"退潮",PilotBearings[current.bearing].data(),
                   current.tenths/10,current.tenths%10):
            format("%s","潮流缓");
        const int tide_width=d.measure(tide_status)+12;
        const Rect tide_box{map.right()-5-tide_width,map.y+28,tide_width,18};
        struct Mark { Rect bounds; Point at; Text name; int priority; };
        std::array<Mark,PilotSiteEnd> marks;
        int mark_count=0;
        std::array<Rect,16> labels{};
        int used=0;
        labels[used++]=title_box;
        labels[used++]=compass;
        labels[used++]=course_box;
        if (current.in_channel) labels[used++]=tide_box;
        labels[used++]={map.right()-65,map.bottom()-26,65,24};
        labels[used++]=detail;
        labels[used++]={ship.x-12,ship.y-12,25,25};
        const auto overlaps=[](Rect a,Rect b) {
            return a.x<b.right() && a.right()>b.x && a.y<b.bottom() && a.bottom()>b.y;
        };
        auto place_label=[&](const Mark& mark) {
            if (used>=int(labels.size())) return;
            const int w=d.measure(mark.name)+3;
            const Point anchor=mark.at;
            const int gap=mark.bounds.w/2+3;
            Point candidates[]{{anchor.x+gap,anchor.y-d.font.cell/2},{anchor.x-w-gap,anchor.y-d.font.cell/2},
                               {anchor.x-w/2,anchor.y+gap},{anchor.x-w/2,anchor.y-gap-d.font.cell}};
            for (const auto at : candidates) {
                const Rect place{at.x,at.y,w,d.font.cell+1};
                if (place.x<map.x+2 || place.right()>map.right()-2 ||
                    place.y<map.y+2 || place.bottom()>map.bottom()-2) continue;
                if (std::any_of(labels.begin(),labels.begin()+used,[&](Rect other){ return overlaps(place,other); }) ||
                    std::any_of(marks.begin(),marks.begin()+mark_count,[&](const Mark& other){ return overlaps(place,other.bounds); }))
                    continue;
                labels[used++]=place;
                d.label(at.x-1,at.y+1,mark.name,ink);
                d.label(at.x+1,at.y+1,mark.name,ink);
                d.label(at.x,at.y,mark.name,mark.priority<=1?gold:ivory);
                return;
            }
        };
        {
            if (guide>=0 && approach.clear)
                d.line(ship,pilot_point(game.pilot_atlas_site(guide,true)),mint,true);
            const int zoom_growth=int(std::lround(std::clamp((pilot_zoom-2.f)*.5f,0.f,1.f)*4.f));
            auto draw_site=[&](int i) {
                const auto site=game.pilot_site(i);
                const bool marked=i==guide;
                const auto mark=pilot_point(game.pilot_atlas_site(i,false));
                const auto anchorage=game.pilot_atlas_site(i,true);
                if (anchorage.x>=0) {
                    const auto dock=pilot_point(anchorage);
                    if (map.contains(dock) && site.kind!=PilotPoint && (marked || pilot_zoom>=4)) {
                        const int distance=std::abs(mark.x-dock.x)+std::abs(mark.y-dock.y);
                        if ((marked || i==berth) && map.contains(mark) && distance>15 && distance<95)
                            d.line(dock,mark,quiet,true);
                        pilot_mark(d,2,dock,11);
                    }
                    d.diamond(dock,i==berth || marked?3:2,i==berth || marked?gold:mint);
                }
                if (!map.contains(mark)) return;
                if (pilot_zoom<=2 && site.kind!=PilotCity && !marked && i!=berth) {
                    d.diamond(mark,2,quiet);
                    marks[mark_count++]={{mark.x-2,mark.y-2,5,5},mark,{},4};
                    return;
                }
                const int size=(site.kind==PilotCity?14:10)+zoom_growth;
                pilot_mark(d,site.kind==PilotPoint?3:site.kind,mark,size);
                marks[mark_count++]={{mark.x-size/2,mark.y-size/2,size,size},mark,site.name,
                                     marked?0:i==berth?1:site.kind==PilotCity?2:3};
            };
            if (guide>=0) draw_site(guide);
            pilot_visible_sites([&](int i) { if (i!=guide) draw_site(i); });
            for (int priority=0;priority<=3;++priority)
                for (const auto& mark:std::span(marks).first(mark_count))
                    if (mark.priority==priority) place_label(mark);
            const int size=pilot_zoom>=4?22:20;
            const auto motion=PilotDirections[state.heading];
            if (map.inset(15).contains(ship)) {
                if (!game.paused) {
                    const auto turn=vessel_frame()/3;
                    for (int beam : {-1,1})
                        d.line({ship.x-motion.x/12,ship.y-motion.y/12},
                               {ship.x-motion.x*(15+turn)/100-motion.y*beam/19,
                                ship.y-motion.y*(15+turn)/100+motion.x*beam/19},rgb(0x85c4bf),true);
                }
                for (int i=1;i<16;i+=4) {
                    const auto a=PilotDirections[i], b=PilotDirections[(i+1)&15];
                    d.line({ship.x+a.x*14/100,ship.y+a.y*14/100},
                           {ship.x+b.x*14/100,ship.y+b.y*14/100},ink);
                    d.line({ship.x+a.x*12/100,ship.y+a.y*12/100},
                           {ship.x+b.x*12/100,ship.y+b.y*12/100},gold);
                }
                small_ship(d,game.s.ship,((state.heading+1)/2)&7,game.s.reefed || docking,
                           {ship.x-size/2,ship.y-size/2,size,size});
                d.diamond({ship.x+motion.x*16/100,ship.y+motion.y*16/100},2,mint);
            } else {
                const Point edge{std::clamp(ship.x,map.x+23,map.right()-23),
                                 std::clamp(ship.y,map.y+31,map.bottom()-28)};
                const auto direction=PilotDirections[pilot_bearing({ship.x-edge.x,ship.y-edge.y})];
                d.diamond(edge,10,ink);
                const Point tip{edge.x+direction.x/12,edge.y+direction.y/12};
                d.line({edge.x-direction.x/20,edge.y-direction.y/20},tip,gold);
                d.line(tip,{edge.x-direction.y/22,edge.y+direction.x/22},gold);
                d.line(tip,{edge.x+direction.y/22,edge.y-direction.x/22},gold);
                buttons[button_count++]={{edge.x-13,edge.y-13,27,27},PilotCenterShip};
            }
            pilot_distance(d);
        }
        skin_box(d,title_box,0,4);
        weather_icon(d,game.conditions().weather,{title_box.x+3,title_box.y+3,13,13});
        d.label(title_box.x+18,title_box.y+3,area_name,gold,title_box.w-20);
        nav_icon(d,CompassIcon,compass);
        skin_box(d,course_box,0,4);
        d.center(course_box,course,ivory);
        if (current.in_channel) {
            skin_box(d,tide_box,0,4);
            d.center(tide_box,tide_status,mint);
        }
        skin_box(d,detail,0,4);
        d.center(detail,caption,guide>=0 || berth>=0?gold:mist);
        d.clip=saved_clip;
        d.frame(map,border);
        const auto landing_site=berth>=0?game.pilot_site(berth):PilotSite{};
        const bool surveying=berth>=0 && landing_site.kind==PilotPoint;
        const bool slow=!water.bytes || game.pilot_slow_water(water);
        const auto water_offer=state.moored?game.pilot_supply_offer(true):decltype(game.pilot_supply_offer(true)){};
        const auto food_offer=state.moored?game.pilot_supply_offer(false):decltype(game.pilot_supply_offer(false)){};
        if (chrome) {
            const std::array<int,19> key{pilot_zoom>=6,!pilot_zoom,pilot_follow,int(pilot_rate),slow,
                state.moored,landing_site.transfer,surveying,berth>=0,docking,game.s.reefed,game.paused,
                water.bytes!=nullptr,d.font.cell,pad_focus?focused-button_count:-100,
                water_offer.amount,food_offer.amount,water_offer.cost,food_offer.cost};
            const bool touching=pointer_capture.active() && pointer_map==MapView::None;
            const bool refresh=touching || state.moored || !chrome->controls_ready || chrome->controls!=key;
            if (refresh) d.fill({0,map.bottom(),d.clip.w,d.clip.h-map.bottom()},ink);
            else d.clip={};
            chrome->controls=key;
            chrome->controls_ready=!touching && !state.moored;
        }
        const auto bar=[&](int index,int count,int row) {
            const int left=body.x+index*(body.w+3)/count;
            return Rect{left,map.bottom()+2+row*22,body.x+(index+1)*(body.w+3)/count-3-left,21};
        };
        button(d,bar(0,7,0),{},PilotZoomIn,false,pilot_zoom>=6,{},ZoomInIcon);
        button(d,bar(1,7,0),{},PilotZoomOut,false,!pilot_zoom,{},ZoomOutIcon);
        button(d,bar(2,7,0),"回船",PilotCenter,!pilot_follow,false,{},ReturnShipIcon);
        button(d,bar(3,7,0),"泊地",PilotBookOpen,false,false,{},HarborLogIcon);
        button(d,bar(4,7,0),format("%u倍",pilot_rate),PilotSpeed,pilot_rate>1,slow);
        button(d,bar(5,7,0),"远航",LeavePilot,false,state.moored);
        const Text landing=landing_site.transfer?"接驳":surveying?"抵达":"靠泊";
        button(d,bar(6,7,0),state.moored?"上岸":docking?"进港":landing,state.moored?PilotStoryOpen:PilotMoor,
               true,berth<0 || docking,{},surveying?int(AstrolabeIcon):int(DockAnchorIcon));
        if (state.moored) {
            button(d,bar(0,3,1),water_offer.amount?Text(format("淡水+%d %d金",water_offer.amount,water_offer.cost)):"淡水已满",
                   PilotWaterBuy,false,!water_offer.amount || game.s.gold<water_offer.cost);
            button(d,bar(1,3,1),food_offer.amount?Text(format("口粮+%d %d金",food_offer.amount,food_offer.cost)):"口粮已满",
                   PilotFoodBuy,false,!food_offer.amount || game.s.gold<food_offer.cost);
            button(d,bar(2,3,1),"解缆",PilotUnmoor,true);
        } else {
            button(d,bar(0,4,1),"左舵",SteerLeft,false,false,{},HelmTurnLeft);
            button(d,bar(1,4,1),"右舵",SteerRight,false,false,{},HelmTurnRight);
            button(d,bar(2,4,1),game.s.reefed?"张帆":"缩帆",Trim,false,false,{},HelmCanvas);
            button(d,bar(3,4,1),game.paused?"开船":"停船",Pause,true,!water.bytes,{},game.paused?ResumeIcon:PauseIcon);
        }
        d.clip=saved_clip;
    }

    void pilot_book(Draw& d) {
        if (!game.s.pilot.region) return;
        const int region=game.s.pilot.region-1;
        const auto area=game.pilot_area();
        const bool world=region==WorldPilotIndex;
        const auto stage=region>=int(std::size(PilotParcels))?PilotParcelStage::Delivered:game.s.pilot_parcels[region];
        struct Nearby { int id, distance; };
        std::array<Nearby,PilotSiteEnd> sites{};
        int visible=0;
        pilot_visible_sites([&](int id) {
            const auto distance=game.pilot_approach(id,{}).tenths;
            if (distance<=850) sites[visible++]={id,distance};
        });
        std::sort(sites.begin(),sites.begin()+visible,[](Nearby a,Nearby b) {
            return a.distance!=b.distance?a.distance<b.distance:a.id<b.id;
        });
        page_title(d,world?"附近泊地":area.name,MooringIcon);
        if (!visible) {
            d.center(body,"附近没有已知泊地",mist);
            return;
        }
        const int count=std::max(2,list_rows()), pages=(visible+count-1)/count;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<count && i+page*count<visible;++i) {
            const int id=sites[i+page*count].id;
            const auto mark=game.pilot_site(id);
            const bool visited=game.pilot_site_visited(id);
            const bool has_offer=stage==PilotParcelStage::Waiting && id-area.first==PilotParcels[region].sender;
            const bool destination=id==game.pilot_parcel_destination();
            const Text type=destination?"送货地":has_offer?"有差事":visited?"已靠泊":
                            mark.kind==PilotCity?"港城":mark.kind==PilotVillage?"村镇":"地标";
            row(d,i,mark.name,type,PilotSiteEntry+id,visited || destination,28,
                destination || has_offer?LetterIcon:-1);
        }
        if (pages>1) pager(d,pages);
    }

    void pilot_story(Draw& d) {
        if (pilot_site<0 || !game.s.pilot.region) return;
        const auto mark=game.pilot_site(pilot_site);
        page_title(d,mark.name,MooringIcon);
        const auto approach=game.pilot_approach(pilot_site,pilot_water());
        const bool moored=game.s.pilot.moored==pilot_site+1;
        const auto distance=format("%s · %d.%d海里",PilotBearings[approach.bearing].data(),
                                   approach.tenths/10,approach.tenths%10);
        const Text detail=moored?"船已靠泊":Text(distance);
        pilot_mark(d,mark.kind==PilotPoint?3:mark.kind,{body.x+27,body.y+49},36);
        d.label(body.x+50,body.y+27,detail,gold,body.w-55);
        d.wrap({body.x+50,body.y+44,body.w-55,body.h-77},mark.note,ivory,d.font.cell+1);
        const int y=body.bottom()-28, half=(body.w-12)/2;
        const bool job=game.pilot_parcel_at(pilot_site);
        const bool aim=!game.s.pilot.moored && approach.clear && approach.tenths>0;
        const bool hint=moored && game.pilot_coastal_hint()>=0;
        button(d,{body.x+5,y,job || aim || hint?half:body.w-10,24},"标在图上",PilotGuide,
               !job && !aim && !hint,false,{},SeaChartIcon);
        if (job) button(d,{body.x+7+half,y,body.w-12-half,24},"差事",PilotOpenParcel,true,false,{},LetterIcon);
        if (aim) button(d,{body.x+7+half,y,body.w-12-half,24},"转向泊地",PilotAim,true,false,{},HelmIcon);
        if (hint) button(d,{body.x+7+half,y,body.w-12-half,24},"问航路",PilotCoastHint,true,false,{},SpeechIcon);
    }

    void pilot_parcel_page(Draw& d) {
        if (!game.pilot_parcel_at(pilot_site)) return;
        const int region=game.s.pilot.region-1;
        const auto& parcel=PilotParcels[region];
        const auto stage=game.s.pilot_parcels[region];
        const bool delivered=stage==PilotParcelStage::Delivered;
        const auto& recipient=PilotSites[PilotRegions[region].first+parcel.recipient];
        page_title(d,parcel.title,LetterIcon);
        d.label(body.x+7,body.y+29,format("%s  %d金",
                delivered?"已收船钱":recipient.name.data(),parcel.reward),gold,body.w-14);
        d.wrap({body.x+7,body.y+48,body.w-14,body.h-80},
               delivered?parcel.delivery:stage==PilotParcelStage::Waiting?parcel.offer:
               "收件人来码头接货。把包裹交出去，就能领取船钱。",ivory,d.font.cell+2);
        button(d,{body.x+5,body.bottom()-28,body.w-10,24},
               delivered?"回到领航图":stage==PilotParcelStage::Waiting?"接下差事":"交货收钱",
               delivered?PilotGuide:PilotHandleParcel,true,false,{},delivered?SeaChartIcon:LetterIcon);
    }

    void pilot_tap(Point touch) {
        const auto& state=game.s.pilot;
        int selected_site=-1, shortest=11*11;
        pilot_visible_sites([&](int i) {
            for (const auto mark : {game.pilot_atlas_site(i,false),game.pilot_atlas_site(i,true)}) {
                if (mark.x<0 || mark.y<0) continue;
                const auto at=pilot_point(mark);
                const int dx=touch.x-at.x, dy=touch.y-at.y, squared=dx*dx+dy*dy;
                if (squared<shortest) { shortest=squared; selected_site=i; }
            }
        });
        if (selected_site>=0) {
            if (pilot_guide==selected_site) {
                pilot_site=selected_site;
                pilot_story_back=None;
                game.cancel_pilot_docking();
                game.paused=true;
                set_modal(PilotStory);
            } else pilot_guide=selected_site;
            return;
        }
        const auto tracked=pilot_visual_ship();
        const auto at=pilot_point(tracked.x,tracked.y);
        const Point delta{touch.x-at.x,touch.y-at.y};
        if (std::abs(delta.x)+std::abs(delta.y)>7 && !state.moored) {
            game.set_pilot_heading(pilot_bearing(delta));
            save_needed=true;
        }
    }

    void chart(Draw& d,SailingChrome* chrome=nullptr) {
        if (game.encounter()) { encounter_screen(d); return; }
        if (pilot_view()) { pilot_chart(d); return; }
        if (full_helm_chart()) { chart_full_helm(d); return; }
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
        if (chrome && chrome->chart_area!=map) {
            chrome->chart_area=map;
            chrome->controls_ready=false;
            chrome->chart_info_ready=false;
            d.fill(body,ink);
        }
        const int berth=game.s.helm?game.nearby_anchorage(chart_water):-1;
        const bool salvage=berth<0 && game.near_drift();
        const int landmark=berth>=0?berth:game.s.helm?game.visible_landmark():-1;
        draw_map(d,landmark,berth);

        const auto clip=d.clip;
        const int icon_w=28, y=map.bottom()+3;
        const bool zoom_to_shore=landmark>=0 && zoom<5;
        const int action_x=map.x+2*(icon_w+3)+39;
        const int action_w=map.right()-action_x;
        const bool helm_shortcut=game.port() && !salvage && action_w>=90;
        const int helm_w=helm_shortcut?
            std::min({(action_w-3)/2,83,action_w-d.measure("目的地")-7}):0;
        const auto site=game.nature_site();
        const bool lookout=site!=NoNatureSite && !salvage && action_w>=68;
        const int watch_w=lookout?std::clamp(action_w/3,33,73):0;
        const bool unrecorded=lookout && !game.nature_recorded(site==MadeiraWaters?CommonDolphin:Loggerhead);
        if (chrome) {
            const std::array<int,19> key{zoom_to_shore,lookout,unrecorded,salvage,game.s.helm,game.sea(),
                game.s.stop_next,berth>=0,helm_shortcut,zoom==0,zoom>=7,d.font.cell,
                pad_focus?focused-button_count:-100};
            chart_section(d,{map.x,y-2,map.w,26},key,chrome->controls,chrome->controls_ready);
        }
        button(d,{map.x,y,icon_w,24},{},ZoomIn,false,zoom>=7,{},ZoomInIcon);
        button(d,{map.x+icon_w+3,y,icon_w,24},{},ZoomOut,false,zoom==0,{},ZoomOutIcon);
        button(d,{map.x+2*(icon_w+3),y,36,24},zoom_to_shore?"近岸":"回船",Center);
        if (lookout)
            button(d,{action_x,y,watch_w,24},"观察",OpenNatureWatch,unrecorded);
        const int dock_offset=lookout?watch_w+3:0;
        const int main_action_w=action_w-dock_offset-(helm_shortcut?helm_w+3:0);
        const Text route_picker=main_action_w>=d.measure("选目的地")+4?"选目的地":"目的地";
        const int action_icon=salvage?int(LetterIcon):game.s.helm?int(DockAnchorIcon):
            game.sea()?-1:int(DestinationChartIcon);
        button(d,{action_x+dock_offset,y,main_action_w,24},
               salvage?"捞木匣":game.s.helm?"靠泊":game.sea()?(game.s.stop_next?"取消停靠":"下站停靠"):route_picker,
               salvage?OpenDriftRecovery:game.s.helm?DockHelm:game.sea()?NextStop:RouteList,
               salvage || game.s.stop_next || berth>=0,game.s.helm && berth<0 && !salvage,{},action_icon);
        if (helm_shortcut) {
            const bool roomy=helm_w>=d.measure("自由掌舵")+24;
            button(d,{map.right()-helm_w,y,helm_w,24},roomy?"自由掌舵":"掌舵",StartExplore,
                   false,false,{},ShipWheelIcon);
        }
        d.clip=clip;
        if (!game.s.helm && (game.sea() || game.port()))
            button(d,{map.right()-50,map.bottom()-23,47,20},"精航",EnterPilot,true);
        if (game.s.helm) {
            button(d,{map.right()-43,map.bottom()-23,40,20},"全幅",ToggleHelmView);
            if (game.pilot_region()>=0 || pilot_pending>=0)
                button(d,{map.right()-90,map.bottom()-23,43,20},pilot_pending>=0?"读图":"精航",
                       EnterPilot,true,pilot_pending>=0);
            const auto weather=game.conditions();
            const int speed=game.speed_in_tenths_of_a_knot();
            if (chrome) {
                const std::array<int,25> key{1,landmark,speed,game.s.heading,weather.weather,weather.direction,
                    weather.strength,game.s.reefed,game.paused,d.font.cell,pad_focus?focused-button_count:-100};
                chart_section(d,info,key,chrome->chart_info,chrome->chart_info_ready);
            }
            helm_panel(d,info,wide,landmark,weather,speed);
            d.clip=clip;
            return;
        }

        const int p=game.sea()?game.sailing_target():selected;
        const auto route=game.sea()?game.route:passage(game.s.origin,p);
        const int leg_count=game.sea() && game.s.stop_next?game.s.leg+1:route.count;
        const int traveled=game.sea()?miles_done(route):0;
        const int travel_end=leg_count<route.count?
            traveled-game.s.progress+Legs[route.edge[game.s.leg]].nautical_miles:route.miles;
        const auto forecast=game.sea()?PassageForecast{}:game.forecast(route);
        const bool shortage=forecast.provisions>game.s.stores || forecast.water>game.s.water;
        const bool local_study=game.expedition_here()>=0;
        const bool scouting=scout_course.has_value() && !game.sea();
        const int route_info=route.count ? ShowPreflight : scouting ? ShowRumors : ShowIntel;
        const Text route_label=route.count ? "航前计划" : scouting ? "传闻" : "地理";
        constexpr Text depart_label="按航线启航";
        const Text visit=scouting ? "循线出海" : story_here() ? "会面" : local_study ? "考察" :
                         !is_port(p) ? "测绘" : "掌舵出海";
        const int visit_action=story_here() ? ReadStory : local_study ? OpenObservation :
                               !is_port(p) ? Survey : StartExplore;
        const int visit_command=scouting?StartExplore:visit_action;
        const auto aim=scout_course.value_or(Anchor[p]);
        const int bearing=direction_to({aim.x-Anchor[p].x,aim.y-Anchor[p].y});
        const auto course=format("拟向%s",Bearings[bearing].data());
        const bool charted=game.charted(p);
        const Text destination_name=charted?Places[p].name:"未标记水域";
        const int wind=game.sea()?game.wind(game.s.leg):0;
        const int speed=game.sea()?game.speed_in_tenths_of_a_knot():0;
        if (chrome) {
            const std::array<int,25> key{0,game.sea(),p,route.count,route.miles,leg_count,game.s.leg,
                traveled,travel_end,forecast.days,shortage,scouting,pinned_rumor>=0,route_info,visit_command,
                story_here(),bearing,charted,wind,speed,game.s.reefed,game.paused,d.font.cell,
                pad_focus?focused-button_count:-100};
            chart_section(d,info,key,chrome->chart_info,chrome->chart_info_ready);
        }
        skin_box(d,info,0,5);
        d.label(info.x+5,info.y+4,scouting?(pinned_rumor>=0?"传闻海域":"自由出海"):destination_name,gold,info.w-10);
        if (wide) {
            if (!game.sea()) d.label(info.x+5,info.y+22,scouting?Text(course):Places[p].region,quiet,info.w-10);
            if (game.sea()) {
                const auto progress=format("%d/%d  %d.%d节",game.s.leg+1,leg_count,speed/10,speed%10);
                d.label(info.x+5,info.y+22,progress,mist,info.w-10);
                meter(d,{info.x+5,info.y+39,info.w-10,12},traveled,travel_end,true);
                const int controls=info.bottom()-80;
                if (info.y+53+d.font.cell<=controls)
                    d.label(info.x+5,info.y+53,wind>5?"顺风":wind<-5?"逆风":"风平",quiet,info.w-10);
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
                button(d,{info.x+4,info.bottom()-27,info.w-8,23},route.count?depart_label:visit,
                       route.count?Sail:visit_command,true,false,{},route.count || scouting?SailIcon:-1);
            }
        } else {
            if (game.sea()) {
                d.label(info.x+5,info.y+22,format("%d/%d  %d.%d节 %s",game.s.leg+1,leg_count,
                        speed/10,speed%10,wind>5?"顺风":wind<-5?"逆风":"风平"),
                        mist,info.w-89);
                meter(d,{info.right()-80,info.y+22,73,12},traveled,travel_end,true);
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
                pair(d,{info.x+4,info.y+43,info.w-8,25},route.count?depart_label:visit,
                     route.count?Sail:visit_command,route_label,route_info,true);
            }
        }
        d.clip=clip;
    }

    void helm_panel(Draw& d,Rect info,bool wide,int landmark,SeaConditions state,int speed) {
        skin_box(d,info,0,5);
        const auto bearing=format("%s %d.%d节",Bearings[game.s.heading].data(),speed/10,speed%10);
        constexpr Text compact_weather[]{"晴","热","雨","阵","雪"};
        const auto breeze=format("%s→%s%d级",
                                  (wide?compact_weather[state.weather]:SeaWeatherNames[state.weather]).data(),
                                  Bearings[state.direction].data(),state.strength);
        d.label(info.x+5,info.y+4,landmark>=0?Places[landmark].name:Text("自由掌舵"),gold,info.w-10);
        const int wind_y=info.y+(wide?41:43);
        const bool show_weather=d.measure(breeze)<=info.w-27;
        if (show_weather) weather_icon(d,state.weather,{info.x+5,wind_y-2,15,15});
        d.label(info.x+(show_weather?22:5),wind_y,breeze,mist,info.w-(show_weather?27:10));
        if (wide) {
            d.label(info.x+5,info.y+22,bearing,ivory,info.w-10);
            pair(d,{info.x+4,info.bottom()-78,info.w-8,21},"左舵",SteerLeft,"右舵",SteerRight);
            button(d,{info.x+4,info.bottom()-52,info.w-8,22},game.s.reefed?"张帆":"缩帆",Trim,
                   false,false,{},game.s.reefed?SailIcon:ReefIcon);
            button(d,{info.x+4,info.bottom()-26,info.w-8,22},game.paused?"继续":"停船",Pause,
                   true,false,{},game.paused?ResumeIcon:PauseIcon);
        } else {
            d.label(info.x+5,info.y+24,bearing,ivory,info.w-10);
            const Text names[]{"左舵","右舵",game.s.reefed?"张帆":"缩帆",game.paused?"继续":"停船"};
            const int actions[]{SteerLeft,SteerRight,Trim,Pause};
            const int glyphs[]{-1,-1,game.s.reefed?SailIcon:ReefIcon,game.paused?ResumeIcon:PauseIcon};
            const int step=(info.w-7)/4;
            for (int i=0;i<4;++i)
                button(d,{info.x+4+i*step,info.bottom()-25,step-3,21},names[i],actions[i],i==3,false,{},glyphs[i]);
        }
    }

    bool market_separate_headings() const { return body.w<298; }
    int market_pitch() const { return dense_layout()?17:market_separate_headings()?18:19; }
    int market_table_top() const { return market_separate_headings()?34:19; }
    int market_rows() const { return std::clamp((body.h-54-market_table_top())/market_pitch(),2,8); }
    int market_selection_page() const { return (good>=GoodsCount?good-GoodsCount:good)/market_rows(); }

    static FormattedText market_prices(int buy,int sell) {
        if (!buy && !sell) return format("%s","—");
        if (!buy) return format("— / %d",sell);
        return format("%d / %d",buy,sell);
    }

    void market(Draw& d) {
        if (!game.port()) {
            no_port(d,"靠港后才能交易");
            return;
        }
        if (game.market_held()) {
            skin_box(d,body,0,6);
            d.label(body.x+7,body.y+6,format("%s市场",Places[game.s.origin].name.data()),gold);
            d.wrap({body.x+7,body.y+31,body.w-14,body.h-64},
                   "港务已把此事记入港簿，商会暂时停止交易。",ivory,17);
            button(d,{body.x+7,body.bottom()-28,body.w-14,24},"查看港务留案",OpenPortCase,true);
            return;
        }
        const auto& s=game.s;
        const int rows=market_rows();
        const bool specials=good>=GoodsCount;
        const int first=specials?int(GoodsCount):0;
        const int count=specials?SpecialtyCount:int(GoodsCount);
        const int pages=(count+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        const int x=body.x, y=body.y, w=body.w;
        skin_box(d,body,0,6);
        const bool regional=std::find(MerchantPorts.begin(),MerchantPorts.end(),s.origin)!=MerchantPorts.end();
        const int right=body.right()-4;
        const int quote_w=std::max(31,d.measure("买价")+4);
        const int sell_end=right-5, buy_end=sell_end-quote_w-4, quote_start=buy_end-quote_w;
        const bool separate_headings=market_separate_headings();
        const int tools_end=separate_headings?right:quote_start-4;
        const int compare_w=w>=360?56:42;
        const int tools_extra=compare_w-42;
        const int hold_w=tools_end-tools_extra-(regional?150:101)-x-25;
        const auto hold=format("船舱 %d/%d",game.load(),game.capacity());
        voyage_icon(d,CargoIcon,{x+5,y+2,13,13});
        d.label(x+21,y+2,d.measure(hold)<=hold_w?hold:format("%d/%d",game.load(),game.capacity()),gold,hold_w);
        if (regional) button(d,{tools_end-tools_extra-150,y+1,45,18},"船讯",ShowMerchants);
        button(d,{tools_end-tools_extra-101,y+1,53,18},specials?"货源":"议价",specials?ShowSpecialty:ShowBargain,
               false,false,{},SpeechIcon);
        button(d,{tools_end-compare_w-2,y+1,compare_w,18},"比价",ShowPortPrices,false,false,{},CompareScaleIcon);
        const int heading_y=y+(separate_headings?19:2);
        d.label(buy_end-d.measure("买价"),heading_y,"买价",ivory);
        d.label(sell_end-d.measure("卖价"),heading_y,"卖价",mint);
        for (int i=0; i<rows && page*rows+i<count; ++i) {
            const int g=first+page*rows+i;
            const Rect item{x+3,y+market_table_top()+i*market_pitch(),w-6,market_pitch()-1};
            const bool foc=pad_focus && button_count==focused;
            d.fill(item,foc ? rgb(0x3b5550) : g==good ? light : (i%2 ? rgb(0x1e3b3f) : panel));
            if (g==good) d.fill({item.x,item.y+2,2,item.h-4},gold);
            if (foc) nav_icon(d,CursorIcon,{item.x+2,item.y+4,10,10});
            goods_icon(d,g,{item.x+13,item.y,item.h,item.h});
            const auto access=market_access(s.origin,g);
            const int cargo_x=specials?110:83;
            d.label(item.x+34,item.y+2,GoodsNames[g],g==good?gold:access==MarketAccess::Closed?mist:ivory,cargo_x-36);
            d.label(item.x+cargo_x,item.y+2,format("%d",s.cargo[g]),mint);
            const int buy=game.quote(g,false), sell=game.quote(g,true);
            const auto buy_text=buy?format("%d",buy):format("—");
            const auto sell_text=sell?format("%d",sell):format("—");
            d.label(buy_end-d.measure(buy_text),item.y+2,buy_text,buy?ivory:quiet);
            d.label(sell_end-d.measure(sell_text),item.y+2,sell_text,sell?mint:quiet);
            if (w>=220) {
                const int stock=game.stock(g);
                const Text supply=access==MarketAccess::Closed?"不经营":access==MarketAccess::Buyer?
                                  game.specialty_orders(s.origin,g)?"仅收购":"已收满":
                                  !stock?"售罄":access==MarketAccess::Producer?"本港出产":
                                  stock<=7?"紧缺":stock>=24?"充足":"平稳";
                const int meta=cargo_x+27, room=quote_start-4-item.x-meta;
                if (room>=d.measure(supply))
                    d.label(item.x+meta,item.y+2,supply,access==MarketAccess::Producer?gold:stock?quiet:vermilion);
            }
            buttons[button_count++]={item,GoodsRow+g};
        }
        const bool negotiated=!specials && s.bargain_good==good+1;
        const auto* news=market_notice(s.origin,good,s.day);
        const bool road_goods=s.origin==lisbon && good==Timber && game.road_timber_bonus()>0;
        const Text stock_note=news?(news->price_percent>0?"求购中":"到货中"):road_goods?"驿道来货":"";
        const auto access=market_access(s.origin,good);
        const auto note=access==MarketAccess::Closed?format("%s 不经营",GoodsNames[good].data()):
                        access==MarketAccess::Buyer?format("%s 还收%d件",GoodsNames[good].data(),
                                                           game.specialty_orders(s.origin,good)):
                        format("%s 库存%d %s",GoodsNames[good].data(),game.stock(good),stock_note.data());
        const auto bargain=format("%s · 五件省%d%%",GoodsNames[good].data(),s.bargain_rate);
        if (s.whale_oil || s.whale_blubber)
            button(d,{x+4,body.bottom()-54,std::min(104,w-71),22},
                   format("%s ×%d",s.whale_blubber?"鲸脂":"鲸油",int(s.whale_blubber?s.whale_blubber:s.whale_oil)),
                   ShowFishBook,true);
        else d.label(x+4,body.bottom()-51,negotiated?Text(bargain):Text(note),
                     negotiated?mint:news || road_goods?gold:mist,w-(pages>1?115:57));
        button(d,{right-(pages>1?106:47),body.bottom()-54,44,23},specials?"常货":"特产",ToggleSpecialties,specials);
        if (pages>1) {
            button(d,{right-56,body.bottom()-54,25,23},"<",Previous,false,page==0);
            button(d,{right-27,body.bottom()-54,25,23},">",Next,false,page+1==pages);
        }
        for (int column=0;column<5;++column) {
            const int left=x+w*column/5, width=x+w*(column+1)/5-left-3;
            const Rect r{left,body.bottom()-31,width,PricedButtonHeight};
            if (column==2) {
                if (width>=d.measure("自定数量")+4) button(d,r,"自定数量",OpenTradeQuantity,true);
                else button(d,r,"自定",OpenTradeQuantity,true,false,"数量");
                continue;
            }
            const int i=column<2?column:column-1;
            const bool sell=i>=2;
            const int batch=i%2==0 ? 1 : TradeLot;
            const auto offer=game.trade_quote(good,sell,batch);
            const auto label=format("%s%d",sell?"卖":"买",offer.units ? offer.units : batch);
            const auto price=format("%d%s",offer.gold,width>=48?"金":"");
            button(d,r,label,BuyOne+i,i==0 || offer.saved,!offer.units,offer.units?Text(price):Text("—"));
        }
    }

    void specialty_info(Draw& d) {
        page_title(d,GoodsNames[good],CargoIcon);
        goods_icon(d,good,{body.x+8,body.y+35,34,34});
        d.wrap({body.x+49,body.y+32,body.w-58,body.h-73},specialty_note(good),ivory,16);
        button(d,{body.x+5,body.bottom()-29,body.w-10,25},"查看各港比价与航程",ShowPortPrices,true);
    }

    void trade_quantity_page(Draw& d) {
        page_title(d,GoodsNames[good]);
        const int x=body.x+7, y=body.y, right=body.right()-7;
        const int pad_width=std::min(body.w/2,165);
        const int pad_x=right-pad_width, width=pad_x-x-7;
        const int maximum=game.trade_limit(good,quantity_sell);
        const bool over=trade_quantity>maximum;
        const auto quote=over?TradeQuote{}:game.trade_quote(good,quantity_sell,trade_quantity);
        const bool ready=trade_quantity>0 && quote.units==trade_quantity;
        const int half=(width-4)/2;
        button(d,{x,y+28,half,22},"买入",QuantityBuy,!quantity_sell);
        button(d,{x+half+4,y+28,width-half-4,22},"卖出",QuantitySell,quantity_sell);
        const Rect confirm{x,body.bottom()-PricedButtonHeight-4,width,PricedButtonHeight};
        const Rect receipt{x,y+54,width,confirm.y-y-59};
        harbor_panel(d,receipt,HarborPlaque,4);
        const int icon=width>=116?std::min({38,width/4,receipt.h-8}):0;
        if (icon>0)
            goods_icon(d,good,{receipt.x+6,receipt.y+(receipt.h-icon)/2,icon,icon});
        const int inset=icon>0?icon+12:6;
        const int summary_y=receipt.y+(receipt.h-2*d.font.cell-3)/2;
        d.label(receipt.x+inset,summary_y,format("数量 %d",trade_quantity),
                over?vermilion:ivory,receipt.w-inset-5);
        d.label(receipt.x+inset,summary_y+d.font.cell+3,format("最多 %d 件",maximum),
                mist,receipt.w-inset-5);
        const auto total=format("%s %d金",quantity_sell?"应收":"应付",quote.gold);
        const Text detail=ready?Text(total):over?"超出上限":maximum?"先选数量":"暂无可成交量";
        button(d,confirm,quantity_sell?"确认卖出":"确认买入",QuantityConfirm,true,!ready,detail);

        constexpr Text keys[]{"1","2","3","4","5","6","7","8","9","退格","0","最大"};
        constexpr int gap=3;
        const int keys_w=pad_width-2*gap, keys_h=confirm.bottom()-y-28-3*gap;
        for (int i=0;i<int(std::size(keys));++i) {
            const int column=i%3, row=i/3;
            const int left=column*keys_w/3, top=row*keys_h/4;
            const Rect key{pad_x+left+column*gap,y+28+top+row*gap,
                           (column+1)*keys_w/3-left,(row+1)*keys_h/4-top};
            const int action=i<9?QuantityDigit+i+1:i==9?QuantityBackspace:i==10?QuantityDigit:QuantityMaximum;
            button(d,key,keys[i],action,i==11,i==11?maximum==0:i==9 && trade_quantity==0);
        }
    }

    bool market_order(bool sell, int quantity) {
        const auto trade=game.trade(good,sell,quantity);
        if (!trade.units) { notify(); return false; }
        if (trade.saved)
            std::snprintf(trade_receipt.data(),trade_receipt.size(),"买入%d件 %d金 · 省%d金",
                          trade.units,trade.gold,trade.saved);
        else
            std::snprintf(trade_receipt.data(),trade_receipt.size(),"%s %d件  %s%d金",
                          sell?"卖出":"买入",trade.units,sell?"+":"-",trade.gold);
        game.notice=trade_receipt.data();
        changed();
        return true;
    }

    void merchant_board(Draw& d) {
        page_title(d,"商队消息",SailIcon);
        const int h=std::min(67,(body.h-34)/2);
        const bool receipt_row=h>=4*d.font.cell+6;
        const int pitch=std::min(15,(h-d.font.cell-6)/(receipt_row?3:2));
        for (std::size_t i=0;i<game.s.merchants.size();++i) {
            const auto& ship=game.s.merchants[i];
            const bool underway=ship.destination!=NoMerchantPort;
            const int pier=MerchantPorts[underway?ship.destination:ship.port];
            const Rect card{body.x+5,body.y+28+int(i)*(h+4),body.w-10,h};
            const bool focused_card=pad_focus && button_count==focused;
            plate(d,card,focused_card?ActionPlate:StandardPlate);
            if (focused_card) focus_marker(d,card);
            const int x=card.x+6,w=card.w-12;
            const Text status=ship.held?"等风":underway?"在海上":ship.units?"等买家":"已靠港";
            d.label(x,card.y+4,MerchantNames[i],gold,w-65);
            d.label(card.right()-6-d.measure(status),card.y+4,status,ship.held?gold:mint);
            if (underway) {
                d.label(x,card.y+4+pitch,format("%s → %s",Places[MerchantPorts[ship.port]].name.data(),
                                          Places[pier].name.data()),ivory,w);
            } else d.label(x,card.y+4+pitch,Places[pier].name,ivory,w);
            const int cargo_line=card.y+4+2*pitch;
            if (ship.units) {
                d.label(x,cargo_line,format("舱内 %s ×%d",GoodsNames[ship.good].data(),int(ship.units)),mist,w);
            } else if (receipt_row || !ship.sold_units) {
                d.label(x,cargo_line,underway?"空船去找货":"正在码头找货",mist,w);
            }
            if (ship.sold_units && (receipt_row || !ship.units)) {
                const auto port_name=Places[MerchantPorts[ship.sold_port]].name;
                const auto sale=format("%s卖出%s ×%d",port_name.data(),GoodsNames[ship.sold_good].data(),
                                       int(ship.sold_units));
                const auto age=ship.sold_day?format(game.s.day==ship.sold_day?"今日":"%d日前",game.s.day-ship.sold_day):
                                             format("%s","");
                const int y=cargo_line+(receipt_row?pitch:0), age_width=d.measure(age);
                const bool date_fits=d.measure(sale)+age_width+9<=w;
                d.label(x,y,sale,mint,w);
                if (date_fits) d.label(x+w-age_width,y,age,quiet);
            }
            if (game.charted(pier)) buttons[button_count++]={card,MarkMerchantFirst+int(i)};
        }
    }

    void bargain_page(Draw& d) {
        page_title(d,"码头议价",SpeechIcon);
        const auto& s=game.s;
        const int x=body.x+7, w=body.w-14, y=body.y;
        const bool pending=s.bargain_good>0 && s.bargain_good<=GoodsCount;
        const int commodity=pending?s.bargain_good-1:good;
        goods_icon(d,commodity,{x,y+27,20,20});
        d.label(x+24,y+30,format("%s ×5",GoodsNames[commodity].data()),gold,w-119);
        if (!s.bargain_good) {
            button(d,{x+w-90,y+26,43,21},"买入",HaggleBuy,!bargain_sell);
            button(d,{x+w-43,y+26,43,21},"卖出",HaggleSell,bargain_sell);
        }
        const auto blocked=game.bargain_block(good,bargain_sell);
        if (!blocked.empty()) {
            const auto fixed=format("这批货已谈好，回市场买足五件可省%d%%。",s.bargain_rate);
            d.wrap({x,y+53,w,body.h-83},pending?Text(fixed):blocked,ivory,15);
            if (!game.negotiation_rate() && !s.bargain_good)
                button(d,{x,body.bottom()-28,w,24},"口译与学话",OpenLanguages,true,false,{},PhrasebookIcon);
            return;
        }
        const auto usual=game.bargain_quote(good,bargain_sell,0);
        const auto offer=game.bargain_quote(good,bargain_sell,bargain_rate);
        const auto counter=game.bargain_quote(good,bargain_sell,bargain_counter);
        const int purse=bargain_sell?999999-s.gold:s.gold;
        d.label(x,y+51,format("市价 %d金  出价 %d金",usual.gold,offer.gold),ivory,w);
        const int patience=HagglePatience-s.bargain_warnings;
        const int supply=game.stock(good);
        const bool favor=s.origin==lisbon && good==Timber && !bargain_sell && s.timber_trail==TimberReturned;
        const auto mood=format("%s · 耐心%d",favor?"货主愿多让价":supply<=10?"货紧":supply>=24?"货足":"库存平稳",patience);
        const auto reply=format("商人还价%d金 · 耐心%d",counter.gold,patience);
        d.label(x,y+68,bargain_counter?Text(reply):Text(mood),bargain_counter?gold:mist,w);
        const int controls=body.bottom()-53;
        button(d,{x,controls,39,23},"−",HaggleLess,false,
               bargain_sell?bargain_rate<=2:bargain_rate>=MaxHaggleRate);
        button(d,{x+43,controls,39,23},"+",HaggleMore,false,
               bargain_sell?bargain_rate>=MaxHaggleRate:bargain_rate<=2);
        button(d,{x+86,controls,w-86,23},"出价",HaggleMakeOffer,true,
               !offer.saved || offer.gold>purse);
        const auto accept=format("接受还价 %d金",counter.gold);
        button(d,{x,body.bottom()-27,w,23},bargain_counter?Text(accept):Text("按市价成交"),
               HaggleAccept,false,counter.gold>purse);
    }

    void submit_bargain(int rate) {
        if (modal!=BargainPage) return;
        const auto quote=game.bargain_quote(good,bargain_sell,rate);
        const auto result=game.bargain_offer(good,bargain_sell,rate);
        if (result.reply==InvalidOffer) { notify(); return; }
        bargain_counter=result.counter;
        if (result.reply==DealAgreed || result.reply==TalksEnded) {
            settled_bargain=quote;
            bargain_reply=result.reply;
            set_modal(HaggleOutcomePage);
            if (result.reply==DealAgreed) audio.cue(Sound::Page);
        }
        changed();
    }

    void haggle_outcome_page(Draw& d) {
        page_title(d,"码头议价",SpeechIcon,false);
        const bool agreed=bargain_reply==DealAgreed;
        const int x=body.x+7, y=body.y, w=body.w-14;
        const auto& offer=settled_bargain;
        d.label(x,y+32,agreed?(offer.saved?"议价成功":"市价成交"):"议价失败",
                agreed?mint:vermilion,w);
        if (agreed) {
            goods_icon(d,good,{x,y+51,25,25});
            d.label(x+31,y+55,format("%s %s ×%d",bargain_sell?"卖出":"买入",GoodsNames[good].data(),offer.units),ivory,w-31);
            d.label(x+31,y+74,format("%s%d金",bargain_sell?"收到":"支付",offer.gold),gold,w-31);
            if (offer.saved)
                d.label(x+31,y+92,format("%s%d金",bargain_sell?"比市价多赚":"比市价少付",offer.saved),mint,w-31);
        } else {
            d.wrap({x,y+53,w,body.h-85},"商人收起账簿，本次停泊的议价结束。仍可回市场按市价买卖。",ivory,15);
        }
        button(d,{x,body.bottom()-28,w,24},"返回市场",Close,true);
    }

    void no_port(Draw& d, Text reason) {
        skin_box(d,body,0,6);
        d.center({body.x,body.y,body.w,body.h-27},reason,mist);
        button(d,{body.x+5,body.bottom()-30,body.w-10,25},"返回海图",OpenChart,true);
    }
    void ship_portrait(Draw& d, Rect area) {
        const int h=std::min(area.h,area.w*ShipPortraitHeight/ShipPortraitWidth);
        const int w=h*ShipPortraitWidth/ShipPortraitHeight;
        d.image({art::ship_portraits},{game.s.ship*ShipPortraitWidth,0,ShipPortraitWidth,ShipPortraitHeight},
                {area.x+(area.w-w)/2,area.y+(area.h-h)/2,w,h},true);
    }
    void shipyard(Draw& d) {
        const auto& s=game.s;
        if (!game.port()) {
            if (!game.sea() || game.encounter()) { no_port(d,"靠港后才能整备船只"); return; }
            skin_box(d,body,0,6);
            const int left=body.x+body.w/2;
            ship_portrait(d,{body.x+5,body.y+4,body.w/2-10,body.h-38});
            d.label(left,body.y+16,HullNames[s.ship],gold,body.right()-left-5);
            d.label(left,body.y+36,s.in_company?"两船随行":"单船航行",mist,body.right()-left-5);
            d.label(left,body.y+54,format("船体 %d%%",s.hull),s.hull<40?vermilion:ivory,body.right()-left-5);
            button(d,{body.x+7,body.bottom()-29,body.w-14,25},"查看旗号",OpenFlags,true,false,{},PennantIcon);
            return;
        }
        skin_box(d,body,0,6);
        Rect desk=body.inset(4);
        const bool split=body.w>=298 && body.h>=137;
        Rect toolbar{desk.right()-114,desk.y,114,20};
        const auto hull=format("船体 %d%%",s.hull);
        const auto crew=format("船员 %d/%d",s.crew,game.crew_limit());
        const auto morale=format("士气 %d  炮 %d",s.spirits,s.guns);
        if (split) {
            const Rect card{desk.x,desk.y,body.w*37/100,desk.h};
            desk.x=card.right()+5;
            desk.w=body.right()-4-desk.x;
            toolbar={desk.x,desk.y,desk.w,23};
            harbor_panel(d,card,HarborPlaque,5);
            d.center({card.x+5,card.y+5,card.w-10,17},HullNames[s.ship],gold);
            const int stats=card.bottom()-60;
            ship_portrait(d,{card.x+4,card.y+23,card.w-8,stats-card.y-26});
            d.label(card.x+7,stats,hull,s.hull<40?vermilion:ivory,card.w-14);
            meter(d,{card.x+7,stats+17,card.w-14,8},s.hull,100,s.hull<40);
            d.label(card.x+7,stats+29,crew,mist,card.w-14);
            d.label(card.x+7,stats+45,morale,mist,card.w-14);
        } else {
            ship_portrait(d,{desk.x,desk.y,33,32});
            const int name_x=desk.x+36;
            d.label(name_x,desk.y+3,HullNames[s.ship],gold,toolbar.x-name_x-4);
            d.label(name_x,desk.y+20,format("船体%d  船员%d  士气%d  炮%d",s.hull,s.crew,s.spirits,s.guns),
                    s.hull<40?vermilion:mist,desk.right()-name_x);
        }
        const int tool=(toolbar.w-6)/3;
        button(d,{toolbar.x,toolbar.y,tool,toolbar.h},"泊位",OpenCharterDesk,game.at_berth(),false,{},DockAnchorIcon);
        button(d,{toolbar.x+tool+3,toolbar.y,tool,toolbar.h},"副官",ShowOfficers,s.first_mate!=NoOfficer,false,{},CrewIcon);
        button(d,{toolbar.x+2*(tool+3),toolbar.y,toolbar.w-2*(tool+3),toolbar.h},"旗号",
               OpenFlags,false,false,{},PennantIcon);

        const int food_cost=game.ration_cost(), repair_cost=game.repair_cost(), hire_cost=game.sailor_hire_cost();
        const auto fill=format("补满水粮 %d金",food_cost);
        const auto refill=format("补满 %d金",food_cost);
        const auto repair=format("修船 %d金",repair_cost);
        const auto crew_cost=format("雇%d人 %d金",game.recruitable_sailors(),hire_cost);
        const auto guns=format("加炮 %d金",145+s.guns*120);
        const auto upgrade=format("换购 %d金",s.ship==0?800:1850);
        const int half=(desk.w-4)/2;
        const Text titles[]{food_cost?Text(d.measure(fill)+4<=half?fill:refill):"水粮已满",
                            repair_cost?Text(repair):"船体完好",s.spirits<100?"休整 24金":"士气已满",
                            hire_cost?Text(crew_cost):"水手满员",s.ship<2?Text(upgrade):"已是商船",
                            s.guns<3?Text(guns):"火炮已满","应急信贷","食物淡水"};
        const bool inactive[]{!food_cost,!repair_cost,s.spirits==100,!hire_cost,s.ship==2,s.guns==3,false,false};
        constexpr int glyphs[]{RationIcon,RepairIcon,-1,CrewIcon,SailIcon,HelmGunLeft,LetterIcon,FlaskIcon};
        const int y=desk.y+(split?29:35), rows_h=desk.bottom()-y-9;
        for (int i=0;i<8;++i) {
            const int row=i/2, top=row*rows_h/4;
            const Rect r{desk.x+(i%2)*(half+4),y+top+3*row,i%2?desk.w-half-4:half,
                         (row+1)*rows_h/4-top};
            button(d,r,titles[i],i==7?OpenProvisions:DockService+i,i==0,inactive[i],{},glyphs[i]);
        }
    }
    void charter_desk(Draw& d) {
        const int base=game.berth_port();
        const auto& berth=game.s.berth;
        const auto* charter=harbor_charter(game.s.origin);
        const int x=body.x+7, y=body.y+29, w=body.w-14, bottom=body.bottom();
        page_title(d,"商馆泊位",MooringIcon);
        if (charter) {
            const auto jurisdiction=format("%s · %s",Places[charter->port].name.data(),charter->authority.data());
            d.label(x,y,jurisdiction,gold,w);
        }
        if (game.at_berth()) {
            if (berth.vessel) {
                d.label(x,y+20,format("%s %s  船体%d  炮%d",game.s.in_company?"随行":"留泊",
                        HullNames[berth.vessel-1].data(),berth.hull,berth.guns),ivory,w);
                const Text swap=game.swap_requirement(), company=game.company_requirement();
                d.label(x,y+42,format("%s水手 %d",game.s.in_company?"随行船":"岸上候船",berth.sailors),mist,w-88);
                if (!game.s.in_company)
                    button(d,{x+w-82,y+38,82,22},"出售备船",OpenReserveSale);
                const Text hint=company.empty()?swap:company;
                if (!hint.empty() && (!game.s.in_company || !company.empty()))
                    d.label(x,y+62,hint,vermilion,w);
                const int count=game.s.in_company?2:3, col=(w-4*(count-1))/count;
                button(d,{x,bottom-29,col,25},"港口名录",OpenCharterRegistry);
                if (!game.s.in_company)
                    button(d,{x+col+4,bottom-29,col,25},"更换旗舰",ChangeFlagship,false,!swap.empty());
                button(d,{x+(count-1)*(col+4),bottom-29,col,25},game.s.in_company?"解编留港":"编入船队",
                       ToggleCompany,true,!company.empty());
            } else {
                d.wrap({x,y+21,w,bottom-y-55},"这里可留泊一艘备船。租下泊位后，船匠也会按商馆的价格维修。",ivory,17);
                pair(d,{x,bottom-29,w,25},"港口名录",OpenCharterRegistry,"购置备船",OpenReserveOrder);
            }
            return;
        }
        const Text blocked=game.lease_requirement();
        if (base>=0)
            d.label(x,y+22,format("%s%s",game.s.in_company?"解编须回":berth.vessel?"备船留在":"商馆泊位在",Places[base].name.data()),ivory,w);
        else
            d.label(x,y+22,"先选适合自己航路的港口。",ivory,w);
        d.wrap({x,y+41,w,bottom-y-73},blocked.empty()?"租契准许留船和接洽本地船匠。":blocked,mist,17);
        const int split=(w-4)/2;
        button(d,{x,bottom-29,split,25},"港口名录",OpenCharterRegistry);
        if (charter && !berth.vessel)
            button(d,{x+split+4,bottom-29,w-split-4,25},format("%s %d金",base>=0?"迁租":"租泊",charter->rent),
                   SignBerth,true,!blocked.empty());
        else if (base>=0)
            button(d,{x+split+4,bottom-29,w-split-4,25},"标出泊位",LocateBerth,true);
    }

    void charter_registry(Draw& d) {
        page_title(d,"港口名录",LetterIcon);
        if (charter_item<0) {
            const int x=body.x+5, y=body.y+30, w=body.w-10;
            const int half=(w-4)/2;
            const int height=std::clamp((body.h-33)/3-4,21,38);
            for (int i=0;i<int(HarborCharters.size());++i) {
                const auto& entry=HarborCharters[i];
                const bool known=game.charted(entry.port);
                button(d,{x+(i%2)*(half+4),y+(i/2)*(height+4),half,height},
                       known?Places[entry.port].name:Text("待查港口"),CharterEntry+i,
                       game.berth_port()==entry.port,!known);
            }
            return;
        }
        const auto& entry=HarborCharters[charter_item];
        const int x=body.x+7, y=body.y+30, w=body.w-14;
        d.label(x,y,format("%s · %s",Places[entry.port].name.data(),entry.authority.data()),gold,w);
        d.wrap({x,y+20,w,body.h-79},entry.advantage,ivory,17);
        d.label(x,body.bottom()-24,format("租泊 %d金  信任 %d  %s",entry.rent,entry.trust,
                LanguageNames[entry.language].data()),mist,w);
    }

    void reserve_order(Draw& d) {
        page_title(d,"购置留港备船",SailIcon);
        for (int i=0;i<int(std::size(HullNames));++i) {
            const auto price=format("%d金",BerthVesselPrices[i]);
            row(d,i,HullNames[i],price,PurchaseReserve+i);
            const int h=row_pitch()-3;
            vessel_sprite(d,i,{body.right()-d.measure(price)-32,body.y+28+i*row_pitch(),h,h},false);
        }
        d.label(body.x+7,body.bottom()-23,"买下后停在本港，靠泊时可换乘。",mist,body.w-14);
    }

    void reserve_sale(Draw& d) {
        page_title(d,"出售留港备船",LetterIcon);
        d.wrap({body.x+7,body.y+30,body.w-14,body.h-63},
               game.s.berth.sailors?"出售备船后，候船水手会在本港离船。租契仍然有效。":
                                   "卖出后仍可使用泊位，也可去别处另立租契。",ivory,17);
        pair(d,{body.x+6,body.bottom()-29,body.w-12,25},"留下备船",Close,
             format("出售 %d金",game.reserve_value()),ConfirmReserveSale);
    }

    void port_case(Draw& d) {
        page_title(d,"港务留案",LetterIcon);
        const int x=body.x+7, w=body.w-14, y=body.y+32;
        if (!game.market_held()) {
            d.label(x,y,"商会已重新开放交易。",mint,w);
            return;
        }
        d.label(x,y,format("还需等 %d 日",game.port_hold_until()-game.s.day),gold,w);
        const bool piracy=game.piracy_docket_until()>game.s.day;
        const auto settlement=game.port_settlement_cost();
        d.wrap({x,y+20,w,body.bottom()-y-48},piracy?
               "遇袭的商船已报案。该辖区的商会停止交易，巡逻船在近海查缉。缴清偿金可撤销通缉。":
               "港务在核查船只文书或港口纠纷。交偿金可提前结案，补给、修船和出海照常办理。",ivory,16);
        button(d,{x,body.bottom()-28,w,24},format("交偿金 %d金",settlement),SettlePortCase,
               true,game.s.gold<settlement);
    }

    void flag_art(Draw& d, int id, Rect area) {
        if (id<FurledColors) d.image({art::flags},{id*96,0,96,56},area,true);
    }

    void flag_rack(Draw& d) {
        page_title(d,"船上旗号",PennantIcon);
        const int x=body.x+6, y=body.y+30, w=body.w-12, row=std::clamp((body.h-44)/2,31,52);
        const int col=(w-5)/2;
        for (int id=0;id<ColorsCount;++id) {
            const Rect card{x+(id%2)*(col+5),y+(id/2)*(row+3),col,row-3};
            const bool selected=game.s.colors==id;
            const bool focus=pad_focus && button_count==focused;
            skin_box(d,card,(selected||focus)?1:0,4);
            if (id==FurledColors) d.center(card,FlagDetails[id].name,selected?gold:ivory);
            else {
                flag_art(d,id,{card.x+(card.w-41)/2,card.y+4,41,24});
                const Text name=id==CastilianColors?"卡斯蒂利亚王旗":FlagDetails[id].name;
                d.center({card.x+3,card.bottom()-19,card.w-6,16},name,selected?gold:ivory);
            }
            buttons[button_count++]={card,FlagEntry+id};
        }
    }

    void flag_page(Draw& d) {
        const auto& flag=FlagDetails[flag_item];
        const auto court=colors_court(flag_item);
        const int x=body.x+7, y=body.y+31, w=body.w-14, bottom=body.bottom();
        page_title(d,flag.name,PennantIcon);
        const int picture_h=std::max(0,bottom-y-55);
        const int picture_w=flag_item==FurledColors?0:std::min({132,w/3,picture_h*96/56});
        if (picture_w)
            flag_art(d,flag_item,{x,y,picture_w,picture_w*56/96});
        const int text_x=x+(picture_w?picture_w+7:0);
        d.wrap({text_x,y,x+w-text_x,bottom-y-59},flag.note,ivory,d.font.cell+2);
        const bool has_pass=game.has_royal_pass(court);
        const bool issuing=court!=NoCourt && game.port() && game.s.origin==issuing_port(court) && !has_pass;
        if (court!=NoCourt) {
            const Text status=has_pass?"本船持有盖印委状":issuing?game.royal_pass_requirement():"冒用王旗可能被港务留案";
            d.label(x,bottom-51,status.empty()?"可以申请差船委状":status,has_pass?mint:gold,w);
        }
        const int half=issuing?(w-5)/2:w;
        button(d,{x,bottom-29,half,24},flag_item==FurledColors?"收起旗帜":"升起此旗",RaiseColors,true);
        if (issuing)
            button(d,{x+half+5,bottom-29,w-half-5,24},format("领委状 %d金",Game::RoyalPassCost),
                   SignRoyalPass,false,!game.royal_pass_requirement().empty());
    }

    void shore_journal(Draw& d) {
        page_title(d,"岸上见闻",LetterIcon);
        for (int id=0;id<ShoreTaskCount;++id) {
            const int step=game.s.shore_tasks[id];
            row(d,id,step?ShoreTasks[id].title:Places[ShoreTasks[id].port].name,
                step>=3?"已记完":step?"待续":"有待探访",ShoreEntry+id,step>=3);
        }
    }

    void merchant_raid_offer(Draw& d) {
        if (raid_company<0 || raid_company>=int(MerchantNames.size())) return;
        page_title(d,MerchantNames[raid_company],PennantIcon);
        const int x=body.x+7, w=body.w-14, y=body.y+33;
        const auto jurisdiction=game.merchant_jurisdiction(raid_company);
        const auto& ship=game.s.merchants[raid_company];
        d.label(x,y,format("%s商船 · %s ×%d",jurisdiction.data(),GoodsNames[ship.good].data(),int(ship.units)),
                gold,w);
        d.wrap({x,y+19,w,body.bottom()-y-49},
               "船员看到对方挂起商旗。动手后，遇袭船只会向它的辖区报案。当地商会将停止与你交易。",
               ivory,15);
        pair(d,{x,body.bottom()-27,w,24},"放行",Close,"袭击商船",ConfirmMerchantRaid);
    }

    void enclave_registry(Draw& d) {
        page_title(d,"大西洋据点",MooringIcon);
        for (int i=0;i<EnclaveCount;++i) {
            const auto& site=EnclaveSites[i];
            const auto& holding=game.s.enclaves[i];
            const bool known=game.charted(site.home);
            const auto status=format("居民%d",int(holding.residents));
            row(d,i,known?site.name:"尚未定位",
                known?(holding.status?Text(status):Text("可察看地势")):Text("向船工打听"),EnclaveEntry+i,
                game.enclave_here()==i,30,MooringIcon);
        }
        if (game.s.migrant_site)
            d.label(body.x+6,body.bottom()-20,"船上已有三名拓居者，记得送他们抵达据点",gold,body.w-12);
    }

    void enclave_art(Draw& d,int tile,Rect target) {
        d.image({art::enclave},{tile%4*29,tile/4*29,29,29},target,true);
    }

    void enclave_page(Draw& d) {
        const auto& site=EnclaveSites[enclave_item];
        const auto& h=game.s.enclaves[enclave_item];
        const bool local=game.enclave_here()==enclave_item;
        page_title(d,site.name,MooringIcon);
        const int x=body.x+5, w=body.w-10, top=body.y+49, bottom=body.bottom();
        const int span=(w-4)/2;
        constexpr Text tabs[]{"村镇","工程","居民","争夺"};
        for (int i=0;i<4;++i) {
            const int left=x+w*i/4;
            button(d,{left,body.y+25,x+w*(i+1)/4-left-3,21},tabs[i],EnclaveTab+i,enclave_tab==i);
        }
        if (enclave_tab==0) {
            const int art_w=std::min(120,w*37/100);
            const int art_h=std::min(art_w*104/160,bottom-top-26);
            const Rect scene{x,top,art_w,art_h};
            sea_picture_panel(d,enclave_item==0?MachicoPicture:VilaFrancaPicture,scene);
            if (h.status) {
                constexpr Point lots[]{{17,34},{48,25},{73,15},{89,28},{45,12},{90,12}};
                const int size=std::clamp(art_w/5,13,21);
                for (int i=0;i<EnclaveBuildingCount;++i)
                    if (h.buildings&building_flag(i))
                        enclave_art(d,i,{scene.x+lots[i].x*scene.w/105-size/2,
                                          scene.y+lots[i].y*scene.h/49-size/2,size,size});
            }
            const int x_text=scene.right()+5, w_text=x+w-x_text;
            d.wrap({x_text,top,w_text,bottom-top-26},local?site.people:site.ground,ivory,14);
            if (local && !h.status) {
                const auto reason=game.enclave_lease_requirement(enclave_item);
                const auto price=format("签地契 %d金",site.fee);
                if (!reason.empty())
                    d.label(x,bottom-43,reason,vermilion,w);
                button(d,{x,bottom-25,w,23},price,LeaseEnclave,true,!reason.empty());
            } else if (local) {
                button(d,{x,bottom-25,span,23},"等一日",EnclaveWait);
                const int quote=game.enclave_export_quote(enclave_item);
                button(d,{x+span+4,bottom-25,w-span-4,23},
                       quote?Text(format("出货 %d金",quote)):Text("出货"),SellEnclaveGoods,true,!quote);
            } else {
                button(d,{x,bottom-25,span,23},"标在海图",MarkEnclave,true,game.sea());
                button(d,{x+span+4,bottom-25,w-span-4,23},"招募拓居者",CarrySettlers,false,
                       !h.status || !game.port() || game.s.origin!=lisbon || game.s.migrant_site);
            }
            return;
        }
        if (enclave_tab==1) {
            const auto stock=format("木料%d  粮食%d  工坊货%d",int(h.timber),int(h.food),int(h.exports));
            d.label(x,top+1,stock,ivory,w);
            const int item_y=top+17;
            const int pitch=std::clamp((bottom-item_y-21)/3,17,28);
            for (int i=0;i<EnclaveBuildingCount;++i) {
                const auto& order=BuildingOrders[i];
                const Rect r{x+(i%2)*(span+4),item_y+(i/2)*pitch,span,pitch-2};
                const bool built=h.buildings&building_flag(i);
                const auto cost=format("%s %d木 %d金",order.name.data(),int(order.timber),int(order.coins));
                button(d,r,built?Text(format("%s 已建",order.name.data())):Text(cost),
                       EnclaveConstruction+i,false,!local || !game.enclave_build_requirement(enclave_item,i).empty());
            }
            if (h.under_construction) {
                const auto building=BuildingOrders[h.under_construction-1].name;
                d.label(x,bottom-18,format("%s在建 · %d日后",building.data(),std::max(0,h.building_day-game.s.day)),
                        gold,w/2);
            }
            if (local && h.status)
                button(d,{x+span+4,bottom-20,w-span-4,18},"等一日",EnclaveWait,true);
            return;
        }
        if (enclave_tab==2) {
            d.label(x,top+1,format("居民%d  粮%d  木%d",int(h.residents),int(h.food),int(h.timber)),ivory,w);
            const int pitch=std::clamp((bottom-top-16)/3,17,27);
            const bool can=local && h.status;
            const bool wharf=h.buildings&building_flag(EnclaveWharf);
            const bool full=h.residents+2>((h.buildings&building_flag(EnclaveHouses))?18:6);
            const Text labels[]{"送粮","送木","取粮","取木","雇村民 41金","请木匠 76金"};
            const int orders[]{EnclaveSupply+int(DeliverEnclaveFood),EnclaveSupply+int(DeliverEnclaveTimber),
                               EnclaveSupply+int(CollectEnclaveFood),EnclaveSupply+int(CollectEnclaveTimber),
                               HireVillagers,HireCarpenter};
            for (int i=0;i<6;++i) {
                const bool closed=!can || (i==0 && !game.s.stores) || (i==1 && !game.s.cargo[Timber]) ||
                    (i==2 && (!wharf || h.food<=h.residents)) || (i==3 && (!wharf || !h.timber)) ||
                    (i>=4 && (full || h.food<5)) || (i==5 && h.carpenter);
                const Rect r{x+(i%2)*(span+4),top+15+i/2*pitch,span,pitch-2};
                if (i==4 && local && game.s.migrant_site==enclave_item+1)
                    button(d,r,"安置拓居者",LodgeSettlers,true);
                else button(d,r,labels[i],orders[i],i==0,closed);
            }
            return;
        }

        if (h.prisoners) {
            d.label(x,top+2,format("%d名守卫已缴械",int(h.prisoners)),gold,w);
            d.wrap({x,top+20,w,bottom-top-48},"家人到码头找人。放人能缩短港务留案，收赎金也可送他们回家。",ivory,14);
            button(d,{x,bottom-25,span,23},"释放守卫",ReleaseGuards,true,!local);
            button(d,{x+span+4,bottom-25,w-span-4,23},"交换赎金",RansomGuards,false,!local);
        } else if (h.status) {
            const auto blocked=game.land_battle_requirement(WharfDefense,enclave_item);
            d.label(x,top+2,"仓房守卫",gold,w);
            d.wrap({x,top+19,w,bottom-top-48},blocked.empty()?
                   Text("有人盯上新入仓的粮袋。带队守住仓房五回合，保护村里的存粮。"):blocked,ivory,14);
            button(d,{x,bottom-25,w,23},"布置防守",DefendEnclave,true,!local || !blocked.empty());
        } else {
            d.label(x,top+2,"码头的争夺",gold,w);
            const auto blocked=game.land_battle_requirement(WharfAssault,enclave_item);
            d.wrap({x,top+19,w,bottom-top-48},blocked.empty()?
                   Text("亲自带队控制旗位。争夺会耗去两日水粮，胜负都会留下港务留案。"):blocked,ivory,14);
            button(d,{x,bottom-25,w,23},"带队争夺",StormEnclave,false,!blocked.empty());
        }
    }

    void fish_book(Draw& d) {
        page_title(d,"海边的渔获",CargoIcon);
        const int x=body.x+6, y=body.y+28, width=body.w-12;
        const int art_height=std::max(30,(body.h-58)*3/5);
        sea_picture_panel(d,FisheryPicture,{x,y,width,art_height});
        const int quote=game.whale_oil_quote(), cost=game.whale_rendering_cost();
        const Rect button_at{x,body.bottom()-27,width,24};
        d.wrap({x,y+art_height+3,width,std::max(0,button_at.y-y-art_height-5)},
               game.s.whale_blubber?"渔队带回的鲸脂可交波尔多的岸上工匠炼油。":
               "葡萄牙近岸的银色鱼群能补口粮。比斯开湾入冬后，渔队会留意露脊鲸的双柱鲸息。",ivory);
        Rect processing=button_at, selling=button_at;
        if (cost && quote) {
            processing.w=width/2-2;
            selling.x=processing.right()+4;
            selling.w=button_at.right()-selling.x;
        }
        if (cost) button(d,processing,format("委托炼油 %d金 · 1日",cost),RenderWhaleOil,true,game.s.gold<cost);
        if (quote) button(d,selling,format("卖出%d桶鲸油 · 收%d金",int(game.s.whale_oil),quote),SellWhaleOil,true);
    }

    void shore_page(Draw& d) {
        const auto& task=ShoreTasks[shore_item];
        const int step=game.s.shore_tasks[shore_item], x=body.x+7, y=body.y+31, w=body.w-14;
        const int bottom=body.bottom();
        const bool local=shore_back!=ShoreJournal && game.shore_task_here()==shore_item;
        const bool has_step=local && step<3;
        const int picture_h=std::max(0,bottom-y-(has_step?55:19));
        const int picture_w=std::min({145,w/3,picture_h*160/104});
        page_title(d,task.title,AstrolabeIcon);
        if (picture_w) {
            const Rect picture{x,y,picture_w,picture_w*104/160};
            d.image({art::shore_tasks},{shore_item*160,0,160,104},picture);
            d.frame(picture,border);
        }
        const int text_x=x+(picture_w?picture_w+7:0), text_w=x+w-text_x;
        d.wrap({text_x,y,text_w,picture_h},task.story[step],ivory,d.font.cell+1);
        if (!has_step) return;
        const auto blocked=game.shore_requirement(shore_item);
        const bool route=shore_item==KilwaPorcelain && step==2 && game.s.origin!=calicut;
        if (!route) d.label(x,bottom-52,blocked,mist,w);
        button(d,{x,bottom-34,w,30},route?Text("在海图上标出卡利卡特"):task.action[step],
               route?Destination+int(calicut):AdvanceShoreTask,true,!route && !blocked.empty(),task.cost[step]);
    }

    void office(Draw& d) {
        if (game.sea()) { no_port(d,"到港或抵达测绘点后办理"); return; }
        const auto& p=Places[game.s.origin];
        const bool valid_contract=game.s.contract>=0 && game.s.day<=game.s.deadline;
        skin_box(d,body,0,6);
        const int archive=game.archive_here();
        const bool observation=game.expedition_here()>=0;
        const bool at_port=game.port();
        const bool side_by_side=at_port && body.w>=290 && body.h>=137;
        Rect desk=body;
        int details_y=body.y+42;
        if (side_by_side) {
            const int view_w=body.w*44/100;
            desk.x=body.x+view_w+10;
            desk.w=body.right()-desk.x-4;
            const Rect illustration{body.x+5,body.y+4,view_w,body.h-8};
            port_scene(d,illustration,game.s.origin);
            d.frame(illustration,border);
            d.darken({illustration.x+1,illustration.bottom()-32,illustration.w-2,31},12);
            d.label(illustration.x+6,illustration.bottom()-30,p.name,gold,illustration.w-12);
            d.label(illustration.x+6,illustration.bottom()-15,p.region,ivory,illustration.w-12);
            details_y=desk.y+34;
        } else {
            int banner=34;
            if (at_port) {
                banner=std::clamp(body.h-(archive>=0?119:100)-(valid_contract?7:0),36,190);
                const Rect illustration{body.x+4,body.y+4,body.w-8,banner-2};
                port_scene(d,illustration,game.s.origin);
                const int caption_w=std::min(illustration.w,
                    std::max(d.measure(p.name),d.measure(p.region))+12);
                d.darken({illustration.x,illustration.y,caption_w,std::min(33,illustration.h)},12);
            }
            d.label(body.x+9,body.y+6,p.name,gold,body.w-82);
            d.label(body.x+9,body.y+22,p.region,ivory,body.w-18);
            details_y=body.y+banner+8;
        }
        const bool has_town=at_port && town_index(game.s.origin)>=0;
        const bool village=game.enclave_here()>=0;
        const int survey=regional_survey_at(game.s.origin);
        const bool field=at_port && survey>=OriginalRegionalSurveys;
        const bool escort=at_port && game.s.origin==lisbon && !(game.s.shore_campaign&1);
        int next=desk.right()-5;
        if (has_town) {
            button(d,{next-58,desk.y+6,58,22},"进城",OpenTown,true);
            next-=62;
        }
        if (at_port && field_study_at(game.s.origin)>=0) {
            button(d,{next-70,desk.y+6,70,22},"岸上考察",OpenWorldStudy);
            next-=74;
        }
        if (field) {
            const bool report=game.land_stage()==LandNotes || game.land_stage()==LandFullNotes;
            button(d,{next-70,desk.y+6,70,22},report?"交考察图":"城外考察",OpenFieldSurvey,report);
            next-=74;
        }
        if (village) {
            button(d,{next-58,desk.y+6,58,22},"村镇",ShowLocalEnclave);
            next-=62;
        }
        if (escort) button(d,{next-58,desk.y+6,58,22},"陆战",OpenLandBriefing);
        if (!at_port && game.s.origin>=PreviousPlaceCount) {
            ocean_anchorage(d);
            return;
        }
        if (at_port) {
            const int dest=valid_contract ? game.s.contract : game.proposal();
            const int item=valid_contract ? game.s.contract_good : game.proposal_good();
            const auto offer=format("委托  %s ×3  →  %s",GoodsNames[item].data(),Places[dest].name.data());
            const int service_y=desk.bottom()-(archive>=0?75:51);
            const int offer_w=desk.w-16;
            const bool inline_offer=d.measure(offer)<=offer_w;
            if (inline_offer) d.label(desk.x+8,details_y,offer,ivory,offer_w);
            else {
                d.label(desk.x+8,details_y,format("委托 %s ×3",GoodsNames[item].data()),ivory,offer_w);
                if (details_y+16+d.font.cell<=service_y)
                    d.label(desk.x+8,details_y+16,format("送往 %s",Places[dest].name.data()),ivory,offer_w);
            }
            const int contract_y=details_y+(inline_offer?16:32);
            if (valid_contract && contract_y+14<=service_y)
                d.label(desk.x+8,contract_y,format("第 %d 日前  赏金 %d",game.s.deadline+1,game.reward(dest)),mist,offer_w);
            const int split=desk.w/2;
            const int local_tale=game.tale_here();
            if (archive>=0)
                button(d,{desk.x+3,desk.bottom()-75,desk.w-6,21},
                       game.s.expeditions[archive].stage==HasEvidence?"档案室 · 交考察记录":"档案室",
                       OpenArchive,true);
            button(d,{desk.x+3,desk.bottom()-51,split-6,21},"港口商会",OpenDesk);
            if (game.shore_task_here()>=0)
                button(d,{desk.x+split+2,desk.bottom()-51,desk.w-split-5,21},"岸上见闻",OpenShoreTask,true);
            else if (local_tale>=0)
                button(d,{desk.x+split+2,desk.bottom()-51,desk.w-split-5,21},
                       Tales[local_tale].title,PortTaleAction,true);
            button(d,{desk.x+3,desk.bottom()-26,split-6,24},
                valid_contract ? "交付委托" : "接行会委托",valid_contract ? Deliver : Accept,false,
                valid_contract && game.s.origin!=dest);
            button(d,{desk.x+split+2,desk.bottom()-26,desk.w-split-5,24},
                story_here() ? "会面" : "等候一日",story_here() ? ReadStory : WaitDay,story_here());
        } else {
            d.wrap({body.x+5,details_y,body.w-10,body.h-70},p.note,mist);
            pair(d,{body.x+4,body.bottom()-28,body.w-8,25},"测绘",Survey,
                 story_here() ? "读蜡封信" : game.tow_needed() ? "请求拖带" : observation ? "考察" : "海图",
                 story_here()?ReadStory:game.tow_needed()?ShowTow:observation?OpenObservation:OpenChart,true);
            if (story_here() && game.tow_needed())
                button(d,{body.x+5,body.bottom()-52,75,21},"请求拖带",ShowTow);
        }
    }
    void ocean_anchorage(Draw& d) {
        const int x=body.x+6, w=body.w-12, y=body.y+43, bottom=body.bottom();
        const int study=field_study_at(game.s.origin);
        const bool supplied=game.has_shore_supply();
        d.wrap({x,y,w,bottom-y-(supplied?58:32)},Places[game.s.origin].note,mist,15);
        if (supplied) {
            const auto provision=[&](Rect r, bool water) {
                const int quantity=game.shore_provision_amount(water);
                button(d,r,water?"兑淡水":"兑干粮",water?ShoreWater:ShoreFood,false,!quantity,
                       quantity?Text(format("%d份 · %d金",quantity,quantity*3)):Text("无法补充"));
            };
            const int half=(w-5)/2;
            provision({x,bottom-60,half,31},false);
            provision({x+half+5,bottom-60,w-half-5,31},true);
        }
        const Text right=study>=0?"登岸考察":"回到海图";
        const int half=(w-5)/2;
        button(d,{x,bottom-26,half,23},"测绘",Survey,false,bool(game.s.surveyed&place_bit(game.s.origin)));
        button(d,{x+half+5,bottom-26,w-half-5,23},right,study>=0?OpenWorldStudy:OpenChart,true);
    }

    void journal(Draw& d) {
        const auto& s=game.s;
        const int half=(body.w-13)/2;
        skin_box(d,body,0,6);
        const bool freight=s.dispatch.number || s.dispatch.last;
        d.label(body.x+5,body.y+3,format("声望%d 信任%d 测绘%d/%d",s.renown,s.trust,
                std::max(0,s.charts-(s.surveyed&
                    (place_bit(quanzhou)|place_bit(hakata)|place_bit(malacca)|place_bit(naha))).count()),
                PlaceCount-PortCount),gold,body.w-(freight?122:67));
        button(d,{body.right()-57,body.y+2,53,20},"设置",ShowAudio,false,false,{},SettingsIcon);
        if (freight)
            button(d,{body.right()-112,body.y+2,51,20},"运单",OpenDispatch,bool(s.dispatch.number),
                   false,{},LetterIcon);
        d.label(body.x+5,body.y+23,"主线",quiet);
        const Rect chapter_action{body.x+4,body.y+64,half,25};
        if (s.ending) {
            d.label(body.x+58,body.y+22,s.ending==1 ? "航图为众人所有" : "商号的独家合约",ivory,body.w-63);
            button(d,chapter_action,"重读结局",ShowEnding,true,false,{},HarborLogIcon);
        } else {
            d.label(body.x+58,body.y+23,Story[s.chapter].title,ivory,body.w-63);
            d.label(body.x+5,body.y+44,format("下一站  %s",Places[target()].name.data()),mist,body.w-68);
            const bool local=story_here();
            button(d,chapter_action,local?"会面":"在海图上标记",local?ReadStory:Destination+target(),
                   true,game.sea(),{},local?int(SpeechIcon):int(DestinationChartIcon));
        }
        button(d,{body.x+9+half,chapter_action.y,body.w-13-half,25},"考察簿",
               ShowExpeditions,false,false,{},ExpeditionBookIcon);
        button(d,{body.right()-64,body.y+40,60,22},"据点",OpenEnclaves,false,false,{},SettlementIcon);
        if (s.contract>=0 && body.h>=177)
            d.label(body.x+5,body.y+99,format("委托  %s / %s",
                GoodsNames[s.contract_good].data(),Places[s.contract].name.data()),mist,body.w-10);
        const int third=(body.w-16)/3, row=body.bottom()-27;
        constexpr Text labels[]{"港口故事","海上纪事","岸上见闻","远洋考察","自然图鉴","操作帮助"};
        constexpr int actions[]{ShowTales,ShowSeaLog,OpenShoreJournal,OpenWorldCatalog,OpenNatureAlbum,ShowHelp};
        constexpr int icons[]{PortStoryIcon,SeaRecordIcon,ShoreNewsIcon,DiscoveryIcon,NatureBookIcon,HandbookIcon};
        for (int i=0;i<int(std::size(labels));++i) {
            const int col=i%3;
            const int width=col<2?third:body.w-16-third*2;
            button(d,{body.x+4+col*(third+4),row-(1-i/3)*26,width,23},labels[i],
                   actions[i],false,false,{},icons[i]);
        }
    }

    void nature_art(Draw& d, int entry, Rect picture) {
        d.image({art::nature},{(entry%2)*144,(entry/2)*96,144,96},picture);
        d.frame(picture,border);
    }

    void world_catalog(Draw& d) {
        page_title(d,"远洋考察簿",ExpeditionBookIcon);
        button(d,{body.right()-107,body.y+3,54,23},"地理图鉴",ShowAtlas);
        const int rows=list_rows(), pages=(FieldStudyCount+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && i+page*rows<FieldStudyCount;++i) {
            const int id=i+page*rows;
            const auto& study=FieldStudies[id];
            const int count=int(bool(game.s.world_notebook&field_note_bit(id,0)))+
                            int(bool(game.s.world_notebook&field_note_bit(id,1)));
            row(d,i,game.charted(study.place)?Places[study.place].name:study.region,
                count?format("%d/2",count):game.charted(study.place)?Text("待考察"):Text("待探索"),
                WorldStudyEntry+id,count==2);
        }
        if (pages>1) pager(d,pages);
    }

    void world_study(Draw& d) {
        const auto& study=FieldStudies[field_item];
        const bool known=game.charted(study.place) || field_local;
        const int x=body.x+6, y=body.y+29, w=body.w-12, bottom=body.bottom();
        page_title(d,known?Places[study.place].name:study.region,ExpeditionBookIcon);
        const int art_w=std::min(w*(w<290?36:43)/100,std::max(1,bottom-y-41)*2);
        const int art_h=std::min(bottom-y-56,art_w*52/100);
        const Rect picture{x,y,art_w,art_h};
        const bool recorded=field_plate && game.s.world_notebook&field_note_bit(field_item,field_plate-1);
        if (known && (!field_plate || recorded))
            sea_picture_panel(d,WorldFieldPictureBase+field_item*3+field_plate,picture);
        const int text_x=known && (!field_plate || recorded)?picture.right()+7:x;
        const int text_w=x+w-text_x;
        Text copy=known?study.habitat:Text("这片海岸还没定位。从附近航区寻找线索，再沿岸测绘。");
        if (field_plate) {
            if (recorded) {
                d.label(text_x,y,field_plate==1?study.animal:study.plant,gold,text_w);
                copy=field_plate==1?study.animal_note:study.plant_note;
            } else copy="尚无记录。上岸走到观察点附近后可写生。";
        }
        const int copy_y=recorded?y+16:y;
        const auto blocked=field_local?game.field_requirement(field_item):Text{};
        d.wrap({text_x,copy_y,text_w,bottom-copy_y-(blocked.empty()?52:67)},
               copy,ivory,w<290?13:15);
        if (!known) return;
        const Text plates[]{"地貌",study.culture?"人物":"动物",study.culture?"工艺":"植物"};
        for (int i=0;i<3;++i) {
            const int left=x+i*w/3;
            button(d,{left,bottom-49,x+(i+1)*w/3-4-left,20},plates[i],FieldPlate+i,i==field_plate);
        }
        if (field_local) {
            if (!blocked.empty()) d.label(text_x,bottom-67,blocked,vermilion,text_w);
            button(d,{x,bottom-26,w,23},field_result?"继续考察":"上岸走走",LandWorldTrail,true,!blocked.empty());
        } else if (game.charted(study.place) && !game.sea())
            button(d,{x,bottom-26,w,23},"在海图上标出",Destination+study.place,true);
    }

    Point field_pointer(Point p) const {
        return {(p.x-field_area.x)*FieldColumns/field_area.w,(p.y-field_area.y)*FieldRows/field_area.h};
    }

    int field_subject() const {
        const auto& landmarks=FieldLandmarks[FieldStudies[field_item].ground];
        const int bird_distance=field_range(field_walk.player,landmarks.animal);
        const int plant_distance=field_range(field_walk.player,landmarks.plant);
        if (bird_distance>2 && plant_distance>2) return -1;
        return plant_distance<bird_distance?1:0;
    }

    int field_quarry() const {
        const int quarry=FieldStudies[field_item].hunt;
        return quarry>=0 && !field_walk.hunted && game.s.trail_hunt_day[quarry]!=game.s.day+1?quarry:-1;
    }

    void tick_field() {
        const auto ground=FieldStudies[field_item].ground;
        if (!sea_picture.pixels(WorldGroundPictureBase+ground)) return;
        const int quarry=field_quarry();
        if (now-field_tick>=105) {
            const auto direction=[&]() {
                using namespace pxa::input;
                const int x=int(controller.down(controller_right))-int(controller.down(controller_left));
                return Point{x,x?0:int(controller.down(controller_down))-int(controller.down(controller_up))};
            }();
            const auto sea=game.conditions();
            field_walk.advance(ground,now,direction,quarry,sea.direction,sea.weather==Rain);
            field_tick=now;
        }
        if ((quarry>=0 || now-field_walk.moved_at<165 || (field_walk.shot_at && now-field_walk.shot_at<390)) &&
            now-field_painted>=50) {
            field_painted=now;
            dirty=true;
        }
    }

    void scene_caption(Draw& d,Rect r,Text text,Color color) {
        d.darken(r,12);
        d.fill({r.x,r.bottom()-1,r.w,1},border);
        d.label(r.x+5,r.y+(r.h-12)/2,text,color,r.w-10);
    }

    void field_trail(Draw& d) {
        const auto& study=FieldStudies[field_item];
        const auto& landmarks=FieldLandmarks[study.ground];
        const int ground=WorldGroundPictureBase+study.ground;
        const auto* pixels=sea_picture.pixels(ground);
        const Rect view=d.clip;
        constexpr int button_h=23, gap=5;
        const auto draw_title=[&] {
            const Text place=Places[study.place].name;
            scene_caption(d,{safe.x,safe.y,std::min(d.measure(place)+10,safe.w-50),button_h},place,ivory);
            button(d,{safe.right()-45,safe.y,45,button_h},"返回",Close);
        };
        if (!pixels) {
            field_area={};
            d.fill(view,ink);
            d.center(view,sea_picture.failed(ground)?"地形暂时无法载入":"正在载入地形",ivory);
            draw_title();
            return;
        }

        const Image painting{pixels};
        const auto walk_point=[&](Point from,Point to,int w,int h,unsigned duration,unsigned started) {
            const int t=std::min(duration,now-started);
            const auto axis=[&](int a,int b,int extent,int count) {
                return ((2*a+1)*int(duration)+2*(b-a)*t)*extent/(2*count*int(duration));
            };
            return Point{axis(from.x,to.x,w,FieldColumns),axis(from.y,to.y,h,FieldRows)};
        };
        // Cover the screen without stretching the painting. Small or tall screens follow the traveler.
        const int w=std::max(view.w,(view.h*painting.width()+painting.height()-1)/painting.height());
        const int h=std::max(view.h,(view.w*painting.height()+painting.width()-1)/painting.width());
        const auto feet=walk_point(field_walk.from,field_walk.player,w,h,105,field_walk.moved_at);
        const int x=view.x-std::clamp(feet.x-view.w/2,0,w-view.w);
        const int y=view.y-std::clamp(feet.y-view.h/2,0,h-view.h);
        field_area={x,y,w,h};
        d.image(painting,{0,0,painting.width(),painting.height()},field_area);
        const auto center=[&](Point p) {
            return Point{x+(2*p.x+1)*w/(2*FieldColumns),y+(2*p.y+1)*h/(2*FieldRows)};
        };

        const auto spot=[&](Point at,int type) {
            const auto c=center(at);
            const bool marked=type>=0 && (game.s.world_notebook&field_note_bit(field_item,type));
            d.diamond({c.x,c.y+5},3,rgb(0x23372c));
            const Rect icon{c.x-8,c.y-10,16,16};
            if (study.culture && type>=0) voyage_icon(d,type==0?SpeechIcon:CargoIcon,icon);
            else d.image({art::field_marks},{(type+1)*24,0,24,24},icon,true);
            d.dot(c.x,c.y+5,marked?mint:gold);
        };
        spot(landmarks.camp,-1);
        spot(landmarks.animal,0);
        spot(landmarks.plant,1);
        if (field_range(field_walk.target,field_walk.player)) {
            const auto aim=center(field_walk.target);
            d.diamond(aim,3,ivory);
            d.diamond(aim,1,ink);
        }
        const int hunt=field_quarry();
        if (hunt>=0) {
            const auto at=walk_point(field_walk.previous_quarry,field_walk.quarry,w,h,190,field_walk.animal_at);
            const Point prey{x+at.x,y+at.y};
            const int phase=now/94%4;
            const bool flip=field_walk.previous_quarry.x>field_walk.quarry.x;
            d.image({art::field_animals},{phase*24,hunt*20,24,20},{prey.x-10,prey.y-13,20,17},true,flip);
        }
        if (field_walk.shot_at && now-field_walk.shot_at<320) {
            const auto a=center(field_walk.shot_from), b=center(field_walk.shot_to);
            const int t=std::min(240U,now-field_walk.shot_at);
            const Point arrow{a.x+(b.x-a.x)*t/240,a.y+(b.y-a.y)*t/240};
            d.line({arrow.x+(a.x-b.x)/7,arrow.y+(a.y-b.y)/7},arrow,ivory);
        }
        const Point player{x+feet.x,y+feet.y};
        const int row=field_walk.facing.x?2:field_walk.facing.y>0?0:1;
        const int phase=now-field_walk.moved_at<105?(field_walk.step%2)*2+int(now-field_walk.moved_at>=52):0;
        d.diamond({player.x,player.y+1},4,rgb(0x263831));
        d.image({art::field_traveler},{phase*24,row*32,24,32},
                {player.x-11,player.y-25,22,29},true,field_walk.facing.x<0);

        draw_title();
        const int subject=field_subject();
        const auto blocked=game.field_requirement(field_item);
        const bool seen=subject>=0 && (game.s.world_notebook&field_note_bit(field_item,subject));
        int right=safe.right(), wind_w=0;
        const int row_y=safe.bottom()-button_h;
        const auto action=[&](Text label,int id,bool accent,bool disabled=false) {
            const int width=std::max(51,d.measure(label)+14);
            right-=width;
            button(d,{right,row_y,width,button_h},label,id,accent,disabled);
            right-=gap;
        };
        if (hunt>=0) {
            action("放箭",FieldShoot,true,!blocked.empty() || !field_walk.can_shoot(study.ground));
            action(field_walk.stalking?"起身":"蹲走",FieldStalk,field_walk.stalking);
            const auto wind=game.conditions();
            const auto breeze=format("风往%s",Bearings[wind.direction].data());
            wind_w=d.measure(breeze)+10;
            scene_caption(d,{safe.right()-wind_w,row_y-21,wind_w,17},breeze,ivory);
        }
        if (subject>=0) action(seen?"看记录":study.culture?"记录":"写生",FieldObserve,true,!seen && !blocked.empty());

        const Text note=!blocked.empty()?blocked:
                        notice_until && !game.notice.empty()?game.notice:
                        subject>=0?(subject==0?study.animal:study.plant):
                        hunt>=0?QuarryNames[hunt]:
                        study.hunt>=0?Text("今日已猎"):Text(study.culture?"点地行走，靠近访查点":"点地行走，靠近观察点");
        int width=std::min(safe.w,d.measure(note)+10);
        const bool above_actions=width>right-safe.x;
        if (above_actions && wind_w) width=std::min(width,safe.w-wind_w-gap);
        scene_caption(d,{safe.x,above_actions?row_y-21:row_y,width,above_actions?17:button_h},
                      note,blocked.empty()?ivory:vermilion);
    }

    void nature_index(Draw& d, bool local) {
        const auto site=game.nature_site();
        int recorded=0;
        for (int id=0;id<NatureCount;++id) recorded+=game.nature_recorded(id);
        page_title(d,local?nature_site_name(site):Text("自然图鉴"),AstrolabeIcon);
        const int x=body.x+6, y=body.y+31, w=body.w-12;
        const int columns=local?1:2;
        const int scene_w=local?std::min(130,w/3):0;
        const int start_x=local?x+scene_w+6:x;
        const int card_w=(x+w-start_x-5*(columns-1))/columns;
        if (local) {
            const bool land=site==LaurelWoods || site==SaltMarsh;
            const Rect picture{x,y,scene_w,std::min(scene_w*104/160,body.bottom()-y-26)};
            if (land) {
                d.image({site==LaurelWoods?art::levada:art::marsh},{0,0,160,104},picture);
                d.frame(picture,border);
            } else {
                sea_picture_panel(d,Doldrums,picture);
            }
        }
        const int card_h=std::clamp((body.h-56)/3-3,24,38);
        int index=0;
        for (int id=0;id<NatureCount;++id) {
            const auto& specimen=NatureRecords[id];
            if (local && specimen.site!=site) continue;
            const bool known=game.nature_recorded(id);
            const Rect card{start_x+(index%columns)*(card_w+5),y+(index/columns)*(card_h+4),card_w,card_h};
            const bool focused_row=pad_focus && button_count==focused;
            skin_box(d,card,focused_row?1:0,4);
            const Text name=local && !known?specimen.trace:specimen.name;
            const int badge_width=d.measure("已记载")+8;
            const int badge_w=local && known && d.measure(name)+15+badge_width<=card.w?badge_width:0;
            const int thumb_w=(card.h-6)*3/2;
            const bool thumbnail=known && d.measure(name)+thumb_w+15+badge_w<=card.w;
            if (thumbnail) nature_art(d,id,{card.x+4,card.y+3,thumb_w,card.h-6});
            const int name_x=card.x+7+(thumbnail?thumb_w+3:0);
            const int text_y=card.y+(card.h-d.font.cell)/2;
            d.label(name_x,text_y,name,known?mint:local?ivory:quiet,card.right()-name_x-5-badge_w);
            if (badge_w) d.label(card.right()-badge_w+2,text_y,"已记载",mint);
            buttons[button_count++]={card,NatureSelect+id};
            ++index;
        }
        constexpr Text scenery[]{"","树影越过渠水，林里时有鸟声。","潮水从盐田的沟口进出。",
                                 "水手白天在船头轮流瞭望。","靠近群岛，透过浅水能看见沙底。"};
        const auto status=format("已记载 %d / %d",recorded,NatureCount);
        d.center({x,body.bottom()-21,w,18},local?scenery[site]:Text(status),mist);
    }

    void nature_page(Draw& d) {
        const auto& specimen=NatureRecords[nature_item];
        const bool known=game.nature_recorded(nature_item);
        const bool local=nature_page_back==NatureWatch;
        const int x=body.x+7, y=body.y+31, w=body.w-14;
        const int bottom=body.bottom();
        page_title(d,known || !local?specimen.name:specimen.trace,AstrolabeIcon);
        if (known) {
            const int art_w=std::min({168,w*46/100,(bottom-y-21)*3/2});
            const Rect picture{x,y,art_w,art_w*2/3};
            nature_art(d,nature_item,picture);
            const int text_x=picture.w?picture.right()+8:x, text_w=x+w-text_x;
            d.label(text_x,y,nature_site_name(specimen.site),gold,text_w);
            d.wrap({text_x,y+20,text_w,bottom-y-43},specimen.note,ivory,17);
            d.label(x,bottom-20,specimen.scientific,quiet,w);
            return;
        }
        d.label(x,y,nature_site_name(specimen.site),gold,w);
        d.wrap({x,y+21,w,bottom-y-(local?79:27)},specimen.hint,ivory,17);
        if (!local) {
            d.label(x,bottom-20,"尚未观察，找到线索后可绘制速写",mist,w);
            return;
        }
        const Text blocked=game.nature_requirement(nature_item);
        const Text wait=game.nature_wait();
        const bool this_wait=!wait.empty() && (nature_item==Trocaz || nature_item==Flamingo);
        d.label(x,bottom-53,blocked.empty()?"首次记载 · 声望 +1":blocked,blocked.empty()?mint:gold,w);
        const int wait_w=this_wait?w/3:0;
        if (this_wait) button(d,{x,bottom-35,wait_w-4,31},wait,WaitForNature,true);
        button(d,{x+wait_w,bottom-35,w-wait_w,31},specimen.sketch,SketchNature,true,
               !blocked.empty(),game.land_away()?"6小时 · 水1":"停船约6小时");
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
    int guild_illustration_width() const {
        const int room=body.w-12;
        const int width=std::min(room*42/100,room-170);
        return body.h>=134 && width>=70?width:0;
    }
    int visible_sea_picture() const {
        if (title) return Calm;
        if (modal==WorldStudy) return WorldFieldPictureBase+3*field_item+field_plate;
        if (modal==WorldTrail) return WorldGroundPictureBase+FieldStudies[field_item].ground;
        if (modal==HarborTown && !outside && town_id>=0) return harbor_street_picture(Towns[town_id].port);
        if (modal==PortStoryPage) {
            if (port_story_id>=WetPorcelain) return PortStoryPictureBase+port_story_id-WetPorcelain;
            return harbor_street_picture(PortStories[port_story_id].port);
        }
        if (modal==PortDesk && guild_illustration_width()) return guild_picture(game.s.origin);
        if (screen==Office && modal==None && game.port() && !game.s.land_fight.kind)
            return harbor_picture(game.s.origin);
        if (const int detail=visible_pilot_detail();detail>=0) return PilotPictureBase+detail;
        if (game.s.pilot.region || pilot_pending>=0) return Calm;
        if (modal==FishBook) return FisheryPicture;
        if (modal==EnclavePage && enclave_tab==0) return enclave_item==0?MachicoPicture:VilaFrancaPicture;
        if (modal==SeaLog && sea_item>=0) return read_sea(game.s.sea_log[sea_item]).kind;
        if (modal==NatureWatch && game.s.helm) return Doldrums;
        if (screen==Chart && modal==None) return game.s.weather;
        return Calm;
    }

    void sea_picture_panel(Draw& d, int kind, Rect illustration) {
        if (illustration.w<=0 || illustration.h<=0) return;
        if (const auto* pixels=sea_picture.pixels(kind)) {
            d.cover({pixels},illustration);
        } else {
            skin_box(d,illustration,0,4);
            voyage_icon(d,SeaChartIcon,{illustration.x+(illustration.w-24)/2,
                                       illustration.y+(illustration.h-24)/2,24,24});
        }
        d.frame(illustration,border);
    }

    void illustrated_encounter(Draw& d, int kind) {
        const auto& scene=Encounters[kind];
        skin_box(d,body,0,6);
        const int x=body.x+7, w=body.w-14, choices_y=body.bottom()-33;
        d.label(x,body.y+5,scene.title,gold,w);
        const int y=body.y+25, text_h=std::max(1,choices_y-y-6);
        const int picture_w=std::min(w/3,160), picture_h=std::min(text_h,picture_w*104/160);
        const Rect illustration{x,y+(text_h-picture_h)/2,picture_w,picture_h};
        sea_picture_panel(d,kind,illustration);
        const int text_x=illustration.right()+7;
        const Text blocked=kind==AdenMerchant?game.encounter_requirement(1):Text{};
        const Text copy=kind==FalseBeacon && game.s.tales[Lamps]==Shared?
            "渔会的潮表就在手边。照着潮表可直接辨出假灯，省下一天水粮。":scene.message;
        d.wrap({text_x,y,w+x-text_x,text_h},blocked.empty()?copy:blocked,ivory,d.font.cell+1);
        const auto price=format("买水%d金",game.water_price());
        for (int i=0;i<3;++i) {
            const int left=x+i*(w+3)/3, right=x+(i+1)*(w+3)/3-3;
            const Text choice=kind==ReliefShip && i==1?Text(price):scene.choices[i];
            button(d,{left,choices_y,right-left,27},choice,FightFirst+i,i==0,
                   !game.encounter_requirement(i).empty());
        }
    }
    void push_sea_flash(Point position,int sprite,int heading=-1) {
        if (heading>=0) {
            position.x+=Directions[heading].x/2;
            position.y+=Directions[heading].y/2;
        }
        sea_flashes[flash_slot++%sea_flashes.size()]={position,now,sprite};
        dirty=true;
    }

    void update_skirmish() {
        if (game.paused || !skirmish_tick) { skirmish_tick=now; return; }
        constexpr unsigned tick_ms=55;
        const unsigned elapsed=now-skirmish_tick;
        if (elapsed>tick_ms*3) skirmish_tick=now-tick_ms*3;
        while (now-skirmish_tick>=tick_ms && game.s.skirmish.active && !game.paused) {
            skirmish_tick+=tick_ms;
            step_skirmish();
        }
    }

    void step_skirmish() {
        const auto before=game.s.skirmish;
        const auto effects=game.tick_skirmish(chart_water);
        if (effects&SkirmishEnemyFire) {
            push_sea_flash(before.rival,0,game.s.skirmish.shots[1].heading);
            audio.cue(Sound::Broadside);
        }
        if (effects&SkirmishHit) push_sea_flash(before.rival,6);
        if (effects&SkirmishDamaged) push_sea_flash({game.s.helm_x,game.s.helm_y},6);
        if (effects&SkirmishSplash) {
            for (int i=0;i<2;++i) {
                const auto& shot=game.s.skirmish.shots[i];
                if (before.shots[i].steps && !shot.steps)
                    push_sea_flash(shot.position,2);
            }
        }
        if (effects&SkirmishEnded) {
            previous_helm={game.s.helm_x,game.s.helm_y};
            last_day=now;
            game.paused=game.sea();
            open_helm_view=game.s.helm;
            if (!game.sea()) selected=game.s.origin;
            changed();
        } else if (effects&(SkirmishHit|SkirmishDamaged|SkirmishRope)) changed();
        dirty=true;
    }

    Point skirmish_point(Point position) const { return map_point(position.x/16.f,position.y/16.f); }

    void skirmish_effect(Draw& d,int sprite,Point at,int width) {
        d.image({art::skirmish},{sprite%4*26,sprite/4*26,26,26},
                {at.x-width/2,at.y-width/2,width,width},true);
    }

    void tactical_ship(Draw& d,Point at,int hull,int heading,int colors,Color marker,bool moving) {
        const auto vector=Directions[heading];
        const Point side{-vector.y,vector.x};
        if (moving && !game.paused) {
            const int trail=22+vessel_frame()/2;
            for (int beam : {-1,1}) {
                const Point stern{at.x-vector.x*2/3+side.x*beam/5,at.y-vector.y*2/3+side.y*beam/5};
                const Point tip{at.x-vector.x*trail/16+side.x*beam/3,at.y-vector.y*trail/16+side.y*beam/3};
                d.line(stern,tip,rgb(0x75aaa5),true);
            }
        }
        const int size=map.w>=345?27:24;
        small_ship(d,hull,heading,marker==mint && game.s.reefed,
                   {at.x-size/2,at.y-size/2,size,size});
        if (colors!=FurledColors) flag_art(d,colors,{at.x+5,at.y-10,11,7});
        const Point tip{at.x+vector.x*19/16,at.y+vector.y*19/16};
        d.diamond(tip,3,ink);
        d.diamond(tip,2,marker);
    }

    void broadside_guide(Draw& d,int side) {
        const auto& s=game.s;
        const auto bearing=Directions[(s.heading+side*2+8)%8];
        for (int distance=24;distance<=SkirmishPlayerReach;distance+=12) {
            const Point point{s.helm_x+bearing.x*distance/16,s.helm_y+bearing.y*distance/16};
            if (point.x<0 || point.y<0 || !chart_water({point.x/16,point.y/16})) break;
            const auto position=skirmish_point(point);
            d.dot(position.x,position.y,mist);
        }
    }

    void skirmish_view(Draw& d) {
        const auto& battle=game.s.skirmish;
        const auto& s=game.s;
        map=d.clip;
        // The camera follows the middle of both ships so the helm and target remain visible.
        chart_x=(s.helm_x+battle.rival.x)/32.f;
        chart_y=(s.helm_y+battle.rival.y)/32.f;
        atlas_background(d);
        helm_rain(d);
        const auto player=skirmish_point({s.helm_x,s.helm_y}), enemy=skirmish_point(battle.rival);
        const Text target=battle.opponent>=FirstMerchantOpponent?"商船":
                          battle.opponent==PatrolOpponent?"巡逻船":"私掠船";
        const Text reaction=battle.snared?"中浮索":battle.opponent>=FirstMerchantOpponent || s.enemy<=15?
                            "撤离":battle.enemy_reload?"装填":"瞄准";
        const auto status=format("%s %d · %s",target.data(),s.enemy,reaction.data());
        const auto wind=game.conditions();
        const auto course=format("风向%s %d级",Bearings[wind.direction].data(),wind.strength);
        if (!game.paused && !battle.reload) {
            broadside_guide(d,-1);
            broadside_guide(d,1);
        }
        if (battle.rope_ticks) skirmish_effect(d,4,skirmish_point(battle.rope),23);
        const int rival_hull=battle.opponent>=FirstMerchantOpponent?2:1;
        const int rival_colors=battle.court==1?PortugueseColors:battle.court==2?CastilianColors:FurledColors;
        tactical_ship(d,enemy,rival_hull,battle.heading,rival_colors,vermilion,!battle.snared);
        tactical_ship(d,player,s.ship,s.heading,s.colors,mint,true);
        if (battle.snared) skirmish_effect(d,5,enemy,31);
        for (const auto& shot : battle.shots) {
            if (!shot.steps) continue;
            const auto at=skirmish_point(shot.position);
            const auto tail=Directions[shot.heading];
            d.line({at.x-tail.x/3,at.y-tail.y/3},at,gold);
            d.diamond(at,2,ivory);
        }
        for (const auto& flash : sea_flashes) {
            if (!flash.started || now-flash.started>=390) continue;
            skirmish_effect(d,flash.sprite+int(now-flash.started>=145),skirmish_point(flash.position),29);
        }
        constexpr int gap=4, height=23;
        const int info_w=std::min((safe.w-gap)/2,std::max(100,d.measure(status)+10));
        const Rect left{safe.x,safe.y,info_w,27};
        const Rect right{safe.right()-info_w,safe.y,info_w,27};
        scene_caption(d,left,status,ivory);
        meter(d,{left.x+5,left.bottom()-7,left.w-10,6},s.enemy,74,true);
        const auto hull=format("船体 %d",s.hull);
        scene_caption(d,right,hull,ivory);
        meter(d,{right.x+5,right.bottom()-7,right.w-10,6},s.hull,100,false);
        const int wind_w=std::min(info_w,d.measure(course)+10);
        scene_caption(d,{right.right()-wind_w,right.bottom()+gap,wind_w,18},course,ivory);
        if (battle.reload && !game.paused) {
            const int ticks=skirmish_reload_ticks(s.guns);
            scene_caption(d,{left.x,left.bottom()+gap,info_w,19},"装填",ivory);
            meter(d,{left.x+36,left.bottom()+gap+6,info_w-41,7},ticks-battle.reload,ticks,true);
        }
        if (game.paused)
            scene_caption(d,{left.x,left.bottom()+gap,info_w,19},"战斗暂停",gold);

        // Helm and gunnery occupy opposite corners, leaving the ships and their courses visible.
        const int controls_y=safe.bottom()-2*height-gap;
        draw_chart_scale(d,{safe.right()-74,controls_y-gap-20,74,20});
        if (notice_until && !game.notice.empty()) {
            const int width=std::min(safe.w-82,d.measure(game.notice)+10);
            scene_caption(d,{safe.x,controls_y-gap-18,width,18},game.notice,mint);
        }
        const int width=std::min(63,(safe.w-3*gap)/4);
        const auto control=[&](bool guns,int index) {
            const int group=guns?safe.right()-2*width-gap:safe.x;
            return Rect{group+index%2*(width+gap),safe.bottom()-(2-index/2)*height-(1-index/2)*gap,width,height};
        };
        button(d,control(false,0),"左舵",SteerLeft,false,false,{},HelmTurnLeft);
        button(d,control(false,1),"右舵",SteerRight,false,false,{},HelmTurnRight);
        button(d,control(false,2),s.reefed?"张帆":"缩帆",Trim,false,false,{},HelmCanvas);
        button(d,control(false,3),game.paused?"继续":"停船",Pause,true,false,{},game.paused?ResumeIcon:PauseIcon);
        const bool loading=game.paused || battle.reload;
        button(d,control(true,0),"左炮",FirePort,true,loading,{},HelmGunLeft);
        button(d,control(true,1),"右炮",FireStarboard,true,loading,{},HelmGunRight);
        button(d,control(true,2),"抛浮索",DropRope,false,game.paused || battle.rope_ticks || !s.cargo[Timber],{},HelmRope);
        button(d,control(true,3),"接舷",BoardSkirmish,false,!game.can_board_skirmish(),{},HelmBoard);
    }

    void encounter_screen(Draw& d) {
        const auto& s=game.s;
        const auto weather=s.weather;
        if (game.duel()) { duel_screen(d); return; }
        if (!sea_picture_path(weather).empty()) { illustrated_encounter(d,weather); return; }
        const auto arena=combat_backdrop(d,{art::storm},weather==Privateer);
        const int x=body.x+6, w=body.w-12;
        const auto& scene=Encounters[weather];
        d.label(x,body.y+5,s.skirmish.opponent==PatrolOpponent?"巡逻船要求停船":
                game.convoy_battle() && weather==Privateer?"银铃号护航 · 断铃号":scene.title,gold,w);
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
            const int ship_y=arena.y==body.y?body.y+53:arena.bottom()-36;
            vessel_sprite(d,s.ship,{x+8,ship_y,29,29},false);
            vessel_sprite(d,1,{x+43+s.range*(w-92)/4,ship_y,29,29},true);
            if (flash_until) {
                const int f=std::clamp(7-int((flash_until-now)/74),0,7);
                d.image({art::broadside},{f*32,0,32,32},{x+31,ship_y-4,36,36},true);
            }
            combat_buttons(d,SailOrders);
            if (s.helm)
                button(d,{body.x,body.bottom()+4,body.w,23},"在海图上交战",BeginSkirmish,true);
            return;
        } else {
            d.wrap({x,body.y+29,w,body.h-66},scene.message,ivory);
        }
        const int gap=3, step=w/3;
        for (int i=0;i<3;++i) {
            button(d,{x+i*step,body.bottom()-33,step-gap,28},scene.choices[i],FightFirst+i,i==0,
                   !game.encounter_requirement(i).empty());
        }
    }

    Point town_on_screen(Point p) const {
        return {town_area.x+p.x*town_area.w/TownExtent,town_area.y+p.y*town_area.h/TownExtent};
    }

    Rect harbor_view(Draw& d, int city) {
        const auto frame=d.clip.inset(2);
        const auto inside=frame.inset(2);
        const int h=std::min(inside.h,inside.w*5/8);
        const Rect view{inside.x,inside.y+(inside.h-h)/2,inside.w,h};
        constexpr int scene_w=288, scene_h=136;
        if (outside) d.image({art::outskirts},{0,0,scene_w,scene_h},view);
        else if (Towns[city].port==calicut) d.image({art::calicut},{0,0,scene_w,scene_h},view);
        else if (const auto* bytes=sea_picture.pixels(harbor_street_picture(Towns[city].port))) {
            const Image image{bytes};
            d.image(image,{0,0,image.width(),image.height()},view);
        } else d.fill(view,ocean);
        d.frame(view,rgb(0x342921));
        return view;
    }

    void harbor_boat(Draw& d,int id,Rect box,int phase,bool flip=false) {
        d.image({art::port_boats},{phase*PortBoatWidth,id*PortBoatHeight,PortBoatWidth,PortBoatHeight},
                box,true,flip);
    }

    void harbor_boat_page(Draw& d) {
        const int id=local_vessel(game.s.origin);
        if (id<0) return;
        const auto& boat=LocalVessels[id];
        page_title(d,boat.name,MooringIcon);
        const int w=std::min(120,body.w/3);
        const Rect art{body.x+5,body.y+33,w,w*3/4};
        harbor_boat(d,id,art,1);
        d.wrap({art.right()+5,art.y,body.right()-art.right()-12,body.bottom()-art.y-8},
               boat.note,ivory,d.font.cell+1);
    }

    void town_screen(Draw& d) {
        if (outside) { outskirts_screen(d); return; }
        const auto& layout=Towns[town_id];
        const int story=port_story_at(layout.port);
        const bool can_talk=story>=0 && (game.port_story_stage(story)<PortStoryFirst ||
                            game.dispatch_offer_here(PortStories[story].dispatch));
        const int talk_door=can_talk?int(PortStoryDoors[story]):-1;
        town_area=harbor_view(d,town_id);
        harbor_panel(d,d.clip.inset(2),HarborTrim);
        const int y=safe.y+1;
        const bool roomy=safe.w>=345;
        const Rect leave{safe.right()-(roomy?83:66),y,roomy?83:66,25};
        const Text name=Places[layout.port].name;
        const int talk_width=can_talk ? std::max(roomy?82:59,d.measure(PortStories[story].invitation)+12) : 0;
        const int talk_left=leave.x-(can_talk?talk_width+5:0);
        const int plaque_width=std::min(d.measure(name)+36,talk_left-safe.x-5);
        const Rect plaque{safe.x,y,plaque_width,25};
        harbor_panel(d,plaque,HarborPlaque);
        d.center_with_icon(plaque.inset(4),name,ivory,{art::navigation},{0,84,42,42},16);
        harbor_button(d,leave,"返回港务",Close,false,false,{},roomy?MooringIcon:-1);
        if (can_talk)
            harbor_button(d,{talk_left,y,talk_width,25},PortStories[story].invitation,FindPortStory,
                          true,false,{},roomy?LetterIcon:-1);

        harbor_vessel_hitbox={};
        if (const int id=local_vessel(layout.port); id>=0) {
            const int march=int(now/90%151);
            const int drift=march<76?march:151-march;
            const auto water=town_on_screen({105+3*drift,950});
            const int width=roomy?65:52, height=width*3/4;
            const Rect boat{water.x-width/2,water.y-height,width,height};
            constexpr int sequence[]{0,1,2,1};
            harbor_boat(d,id,boat,sequence[now/180%4],march>=76);
            harbor_vessel_hitbox=boat;
            harbor_button(d,{safe.right()-59,safe.bottom()-24,59,22},"识船",ShowHarborBoat,false);
        }

        draw_town_walker(d,roomy);

        for (int i=0;i<TownFacilityCount;++i) {
            if (i==TownArchive && game.archive_here()<0) continue;
            const auto point=town_on_screen(layout.doors[i]);
            const int width=std::max(46,d.measure(TownNames[i])+25);
            const int left=std::clamp(point.x-width/2,town_area.x+2,town_area.right()-width-2);
            const int offset=i==TownShipyard || i==TownDock?3:i==TownArchive?-39:-23;
            const Rect label{left,point.y+offset,width,21};
            const bool focus=pad_focus && focused==button_count;
            const bool target=town.moving() && (town.entrance==i ||
                              (town.entrance==TownStoryTarget && talk_door==i));
            if (i==TownArchive) d.line({point.x,label.bottom()},point,gold,true);
            d.diamond(point,2,rgb(0x342419));
            d.dot(point.x,point.y,gold);
            harbor_panel(d,label,focus || target?HarborGoldWood:HarborWood,4);
            if (focus) focus_marker(d,label);
            d.center_with_icon(label.inset(2),TownNames[i],target?gold:ivory,
                               {art::town_icons},{i%3*32,i/3*32,32,32},12);
            buttons[button_count++]={label,WalkToTownFacility+i};
        }
        if (layout.port==lisbon) {
            const auto gate=town_on_screen(LisbonGate);
            harbor_button(d,{town_area.right()-47,gate.y+4,45,21},"出城",WalkToCityGate,
                          town.entrance==CityGateTarget);
        } else if (regional_survey_at(layout.port)>=0) {
            const auto gate=town_on_screen(layout.field_gate);
            const Text field=layout.port==madeira?"去水渠":layout.port==cadiz?"去盐田":"去胡椒园";
            harbor_button(d,{town_area.right()-62,gate.y+4,60,21},field,WalkRegionalTrail,
                          town.entrance==RegionalTrailTarget);
        }

        if (town.moving() && town.entrance==RegionalTrailTarget) harbor_hint(d,"沿城边小路走");
        else if (town.moving() && town.entrance==CityGateTarget) harbor_hint(d,"前往城门");
        else if (town.moving() && town.entrance==TownStoryTarget && can_talk) {
            const auto caption=format("前往%s",TownNames[talk_door].data());
            harbor_hint(d,caption);
        } else if (town.moving() && town.entrance>=0 && town.entrance<TownFacilityCount) {
            const auto caption=format("前往%s",TownNames[town.entrance].data());
            harbor_hint(d,caption);
        } else if (pad_focus || town_hint_until) {
            harbor_hint(d,pad_focus?"按A前往":"点路面走动");
        }
    }

    void draw_town_walker(Draw& d, bool roomy) {
        const auto position=town_on_screen(town.position(now));
        constexpr int stride[]{0,1,2,1};
        const int frame=town.moving()?stride[(now/108)%4]:1;
        constexpr int sprite_w=24, sprite_h=32;
        const Rect sprite{frame*sprite_w,std::min(2,town.facing)*sprite_h,sprite_w,sprite_h};
        const int feet=roomy?31:25;
        d.image({art::town_walk},sprite,
                {position.x-feet*3/8,position.y-feet*7/8,feet*3/4,feet},true,town.facing==3);
    }

    void outskirts_screen(Draw& d) {
        town_area=harbor_view(d,town_id);
        harbor_panel(d,d.clip.inset(2),HarborTrim);
        const Rect plaque{safe.x,safe.y+1,d.measure("里斯本河堤")+20,25};
        harbor_panel(d,plaque,HarborPlaque);
        d.center(plaque.inset(3),"里斯本河堤",ivory);
        draw_town_walker(d,safe.w>=345);
        constexpr Text names[]{"回城","察看河堤","木料棚","旧驿道"};
        static_assert(std::size(names)==OutskirtsSites.size());
        for (int i=0;i<int(OutskirtsSites.size());++i) {
            const auto point=town_on_screen(OutskirtsSites[i]);
            const int w=std::max(48,d.measure(names[i])+16);
            const Rect sign{std::clamp(point.x-w/2,town_area.x+2,town_area.right()-w-2),
                            point.y+(i==1 || i==3?7:-23),w,21};
            d.dot(point.x,point.y,gold);
            harbor_button(d,sign,names[i],WalkToOutsideGate+i,town.entrance==OutsideGateTarget+i);
        }
        if (town.moving() && town.entrance>=OutsideGateTarget) harbor_hint(d,"沿河堤走动");
    }

    void timber_trail_page(Draw& d) {
        page_title(d,at_timber_river?"河堤的货签":"丢失的木料",at_timber_river?LetterIcon:CargoIcon);
        const auto stage=game.s.timber_trail;
        Text story;
        if (at_timber_river) story="车轮在泥里陷过。你从一道车辙里捡起货签，上面还留着货主的印。带给木料棚的亚丰索看看。";
        else if (stage==TrailUnknown) story="亚丰索翻遍账簿也没找到账上那批木料。运货的车沿河堤回来过，他请你去看看泥地上还有什么。";
        else if (stage==TrailFound) story="亚丰索认出货签。邻棚卸错了木料，沿印记就能找回来。他愿付你五十五金，也愿往后在里斯本买木料时多让一点。";
        else if (stage==TimberReturned) story="木料已经搬回原棚。亚丰索记下你的人情，里斯本买木料时可以多还一点价。";
        else story="找回的木料堆在棚里。亚丰索已经把五十五金谢礼付给你。";
        const int x=body.x+7,w=body.w-14;
        d.wrap({x,body.y+34,w,body.h-68},story,ivory,16);
        if (!at_timber_river && stage==TrailFound)
            pair(d,{x,body.bottom()-28,w,24},"请留个好价",ReturnTimber,"收55金",TakeTimberReward,true);
    }

    void land_page(Draw& d) {
        const int stage=game.land_stage(), region=regional_survey_at(game.s.origin);
        const auto* survey=region>=0?&RegionalSurveys[region]:nullptr;
        const int x=body.x+7, y=body.y+29, w=body.w-14;
        const int bottom=body.bottom(), row=bottom-37;
        const bool tidal=!inland_archive && stage==LandUnvisited && game.s.origin==cadiz;
        const bool low_tide=tidal && game.cadiz_low_tide();
        const auto condition=game.land_weather();
        const bool new_survey=region>=OriginalRegionalSurveys;
        const bool weather_closed=!inland_archive && new_survey && stage==LandUnvisited && shortcut_closed(condition);
        const bool narrow_warning=weather_closed && w<320;
        const int art_width=w*(w>=320?45:33)/100;
        const int available_height=bottom-y-(tidal || narrow_warning?65:51);
        const int image_height=std::clamp(available_height,0,std::min(192,art_width)*4/5);
        const int picture_width=std::min(art_width,image_height*7/4);
        const Rect picture{x,y,picture_width,image_height};
        const int text_x=picture.w?picture.right()+8:x, text_w=x+w-text_x;
        const bool filed=stage>=LandFiled;
        const bool full=stage==LandFullNotes || stage==LandFullFiled;
        const int reward=survey?(full?survey->full_reward:survey->sketch_reward):(full?215:150);
        page_title(d,survey?(inland_archive?survey->report:survey->site):
                   inland_archive?"里斯本 · 驿道图":"里斯本 · 旧驿道",AstrolabeIcon,!game.land_away());
        if (!inland_archive && game.nature_site()!=NoNatureSite)
            button(d,{body.right()-77,body.y+3,73,23},"自然观察",OpenNatureWatch,true);
        const unsigned char* scenes[]{art::levada,art::marsh,art::expedition_forest,
                                      art::expedition_wadi,art::expedition_vardo,art::expedition_malabar,
                                      art::expedition_sabaki};
        const auto season=game.land_season();
        const auto* landscape=survey?scenes[survey->environment]:inland_archive?art::inland:art::expedition_estuary;
        if (survey && survey->environment==BarentsCoast && season!=Winter) landscape=art::expedition_vardo_thaw;
        if (survey && survey->environment==MalabarHills && season!=SouthwestMonsoon) landscape=art::expedition_malabar_dry;
        const Image scene{landscape};
        if (picture.h && picture.w) {
            const int width=std::min(scene.width(),scene.height()*picture.w/picture.h);
            const int height=std::min(scene.height(),scene.width()*picture.h/picture.w);
            d.image(scene,{(scene.width()-width)/2,(scene.height()-height)/2,width,height},picture);
            weather_marks(d,picture,weather_animation(),3,2);
            if (!inland_archive) {
                const Rect status{picture.x+1,picture.bottom()-17,picture.w-2,16};
                d.darken(status,11);
                d.center(status,LandConditionNames[condition],shortcut_closed(condition)?gold:ivory);
            }
            d.frame(picture,border);
        }
        Text story;
        if (inland_archive) {
            if (survey) story=full?survey->archive_full:survey->archive_sketch;
            else if (filed)
                story=game.s.road_delivery?"丰沙尔的船匠收到了木料。往后商队还会沿这条路给里斯本供货。":
                      "抄图人把图抄给商队。山里的木料正运进市场，商会想找船送一批去丰沙尔。";
            else story=full?"你画出了望台背后的岔路。抄图人认出那正是商队常走错的地方。":
                     "抄图人对着路碑拓片核了地名，准备用你的图换下那张漏了路的旧图。";
        } else if (game.s.nature_hike) {
            story=game.s.origin==madeira?"回到山间水渠。沿着树影走，看能不能找到林中的住客。":
                                        "又到了湾边盐田。有些生灵要等潮水退去才看得清。";
        } else {
            switch (stage) {
            case LandUnvisited:
                story=survey?survey->approach:
                    game.knows_land_pass()?"河边好走。旧货道翻山，省一天，却要多带水。出发前得留足返程的水粮。":
                    "沿溪能走到老路碑。想抄近路，先去河堤找货签，或者让安娜担任大副。";
                break;
            case LandRidge: story=survey?survey->junction:"路碑的字还认得。拓碑就能交图。翻上望台，还能把背坡的岔路画进去。"; break;
            case LandInscription: story=survey?survey->sketch:"你拓下几处地名和里程。把拓片和草图带回里斯本吧。"; break;
            case LandLookout: story=survey?survey->full:"从望台上能看见背坡的两条路。你辨出通向村子的那条，添在图上。"; break;
            case LandNotes: case LandFullNotes:
                story=new_survey?"考察图已画好。回港务，交给当地管事。":"考察已经结束。进城去档案室交稿吧。";
                break;
            default: story="这处地形已经画进档案室的图册。"; break;
            }
        }
        if (!inland_archive) {
            const auto date=calendar_date(game.s.day);
            const auto season_line=format("%d月%d日  %s",date.month,date.day,ClimateSeasonNames[season].data());
            d.label(text_x,y,season_line,gold,text_w);
        }
        const int narrative_y=inland_archive?y:y+16;
        d.wrap({text_x,narrative_y,text_w,row-narrative_y-(tidal || weather_closed?23:5)},story,ivory,15);
        const auto leg_button=[&](Rect rect, LandAction step, Text label, int action, bool accent=false) {
            const auto cost=game.land_cost(step);
            const bool blocked=!game.land_requirement(step).empty();
            const auto price=format("%d日 粮%d 水%d",cost.days,cost.food,cost.water);
            button(d,rect,label,action,accent,blocked,price);
        };
        if (inland_archive && !filed) {
            button(d,{x,row,w,PricedButtonHeight},format("交图 · %d金",reward),FileLandReport,true);
        } else if (inland_archive && !survey && !game.s.road_delivery) {
            button(d,{x,row,w,PricedButtonHeight},"查看港际运单",OpenDispatch,true);
        } else if (game.s.nature_hike) {
            leg_button({x,row,w,PricedButtonHeight},ReturnToCity,"回城",WalkLandBack,true);
        } else if (!inland_archive && new_survey && (stage==LandNotes || stage==LandFullNotes)) {
            button(d,{x,row,w,PricedButtonHeight},"查看考察图",OpenFieldSurvey,true);
        } else if (!inland_archive && !new_survey && survey && stage>=LandNotes) {
            leg_button({x,row,w,PricedButtonHeight},FollowStream,
                       game.s.origin==madeira?"再访林地":"再访盐田",WalkLandStream,true);
        } else if (filed) {
            d.label(x,row+8,"已入档",mint,w);
        } else if (!inland_archive && stage==LandUnvisited) {
            const int half=(w-5)/2;
            const bool short_path=game.knows_land_pass();
            leg_button({x,row,half,PricedButtonHeight},FollowStream,survey?survey->routes[0]:"沿溪走",WalkLandStream,true);
            leg_button({x+half+5,row,w-half-5,PricedButtonHeight},CrossPass,
                       survey?survey->routes[1]:short_path?"翻山抄近路":"近路未解锁",WalkLandPass);
            if (tidal) {
                d.label(x,row-21,low_tide?"低潮，可走潮滩":"涨潮，潮滩不通",low_tide?mint:gold,w-61);
                if (!low_tide) button(d,{x+w-58,row-24,58,21},"候潮",WaitLandTide);
            } else if (weather_closed) {
                const int warning_x=narrow_warning?x:text_x;
                d.label(warning_x,row-20,game.land_requirement(CrossPass),gold,x+w-warning_x-73);
                button(d,{x+w-66,row-24,66,21},"等一日",WaitLandWeather);
            } else if (const Text blocked=game.land_requirement(FollowStream); !blocked.empty()) {
                d.center({x,row-17,w,15},blocked,vermilion);
            }
        } else if (!inland_archive && stage==LandRidge) {
            const int third=(w-8)/3;
            leg_button({x,row,third,PricedButtonHeight},CopyMilestone,survey?survey->observations[0]:"拓路碑",WalkLandCopy,true);
            leg_button({x+third+4,row,third,PricedButtonHeight},SurveyLookout,survey?survey->observations[1]:"登望台",WalkLandLookout);
            leg_button({x+2*third+8,row,w-2*third-8,PricedButtonHeight},ReturnToCity,survey?"回城":"回河堤",WalkLandBack);
        } else if (game.land_away()) {
            leg_button({x,row,w,PricedButtonHeight},ReturnToCity,survey?"回城":"回河堤",WalkLandBack,true);
        }
    }

    void change_outskirts(bool enter) {
        outside=enter;
        town={.at=enter?OutskirtsSites[0]:LisbonGate};
        set_modal(HarborTown);
    }

    void harbor_hint(Draw& d, Text caption) {
        const int w=d.measure(caption)+26;
        const Rect hint{safe.x+(safe.w-w)/2,safe.bottom()-21,w,19};
        d.darken(hint,12);
        d.diamond({hint.x+6,hint.y+9},2,gold);
        d.diamond({hint.right()-7,hint.y+9},2,gold);
        d.center(hint,caption,ivory);
    }

    void show_town() {
        if (!game.port()) return;
        const int city=town_index(game.s.origin);
        if (city<0) return;
        if (town_id!=city) {
            town={.at=Towns[city].doors[TownDock]};
            town_id=city;
        }
        if (outside) town={.at=Towns[city].doors[TownDock]};
        outside=false;
        inside_town_facility=false;
        screen=Office;
        town_hint_until=now+3300;
        set_modal(HarborTown);
    }

    void enter_town_facility(int id) {
        if (!game.port() || town_id<0 || game.s.origin!=Towns[town_id].port) return;
        switch (id) {
        case TownMarket: screen=Market; page=0; set_modal(None); break;
        case TownShipyard: screen=Shipyard; set_modal(None); break;
        case TownGuild: set_modal(PortDesk); break;
        case TownArchive:
            if (game.archive_here()<0) return;
            activate(OpenArchive);
            break;
        case TownTavern:
            rumor_back=HarborTown;
            page=0;
            set_modal(RumorList);
            break;
        case TownDock: select(game.s.origin); return;
        }
        inside_town_facility=true;
        town_facility=modal;
    }

    void arrive_in_town(int id) {
        if (id==CityGateTarget || id==OutsideGateTarget) { change_outskirts(id==CityGateTarget); return; }
        if (id==RiverTarget || id==TimberShedTarget) {
            at_timber_river=id==RiverTarget;
            if (at_timber_river && game.inspect_timber_trail()) changed();
            set_modal(TimberTrailPage);
            return;
        }
        if (id==PostRoadTarget || id==RegionalTrailTarget) {
            inland_archive=false;
            land_return=HarborTown;
            set_modal(LandPage);
            return;
        }
        if (id!=TownStoryTarget) {
            enter_town_facility(id);
            return;
        }
        const int story=port_story_at(game.s.origin);
        if (!game.port() || story<0) return;
        if (game.meet_port_story(story)) changed();
        port_story_id=story;
        port_story_back=HarborTown;
        set_modal(PortStoryPage);
    }

    void town_go(Point point, int facility=-1) {
        if (modal!=HarborTown) return;
        town.at=town.position(now);
        town.go(point,outside?OutskirtsQuay:Towns[town_id].quay,facility,now);
        if (const int entered=town.update(now); entered>=0) arrive_in_town(entered);
        dirty=true;
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

    void page_title(Draw& d, Text name, int glyph=-1, bool allow_back=true) {
        skin_box(d,body,0,6);
        const int inset=glyph<0?6:24;
        if (glyph>=0) voyage_icon(d,glyph,{body.x+6,body.y+4,16,16});
        d.label(body.x+inset,body.y+5,name,gold,body.w-inset-55);
        if (allow_back) button(d,{body.right()-49,body.y+3,45,23},"返回",Close);
    }
    void set_modal(Overlay layer) {
        if (modal==HarborTown && layer!=HarborTown) {
            town.at=town.position(now);
            town.cancel();
        }
        const bool reading=modal==ForecastPage || modal==RationsPage;
        const bool opens_book=layer==ForecastPage || layer==RationsPage;
        if (!reading && opens_book && game.sea() && !game.encounter()) {
            resume_after_voyage_info=!game.paused;
            game.paused=true;
        }
        if (reading && !opens_book) {
            if (resume_after_voyage_info && game.sea() && !game.encounter()) {
                game.paused=false;
                last_day=now;
                previous_helm={game.s.helm_x,game.s.helm_y};
            }
            resume_after_voyage_info=false;
        }
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
        button(d,{body.right()-169,body.y+3,36,23},"全图",WorldChart);
        button(d,{body.right()-129,body.y+3,37,23},"传闻",ShowRumors);
        button(d,{body.right()-89,body.y+3,37,23},"掌舵",StartExplore);
        constexpr Text names[]{"全部","邻近","港口","村镇","测绘点"};
        const int x=body.x+4, width=body.w-8, y=body.y+28;
        for (int i=0;i<int(std::size(names));++i) {
            const int left=x+width*i/5;
            button(d,{left,y,x+width*(i+1)/5-left-3,20},names[i],RouteFilter+i,i==route_filter);
        }
        std::array<int,PlaceCount> order{};
        int count=0;
        const auto add=[&](int p) {
            if (p<0 || !game.charted(p)) return;
            if (route_filter==1 && !std::ranges::any_of(Legs,[&](const Leg& leg) {
                return (leg.from==game.s.origin && leg.to==p) || (leg.to==game.s.origin && leg.from==p);
            })) return;
            if (route_filter==2 && !is_port(p)) return;
            if (route_filter==3 && !is_village(p)) return;
            if (route_filter==4 && (is_port(p) || is_village(p))) return;
            order[count++]=p;
        };
        if (target()!=game.s.origin) add(target());
        add(game.s.origin);
        for (int p=0;p<PlaceCount;++p)
            if (p!=game.s.origin && p!=target()) add(p);
        if (!count) {
            d.wrap({x+4,body.y+57,width-8,body.bottom()-body.y-62},
                   route_filter==1?"附近暂无已知泊地。可以掌舵探索，或到酒馆问路。":"海图上还没有这类地点。",
                   mist,15);
            return;
        }

        constexpr int top=52;
        const int rows=std::clamp((body.h-top-24)/row_pitch(),1,10);
        const int pages=(count+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int p=order[page*rows+i];
            const auto path=passage(game.s.origin,p);
            const auto miles=format("%d海里",path.miles);
            const bool here=p==game.s.origin;
            const int glyph=here?MooringIcon:p==target()?LetterIcon:is_port(p)?MooringIcon:AstrolabeIcon;
            row(d,i,Places[p].name,here?"所在处":p==target()?"主线":p==selected?"已选":Text(miles),
                Destination+p,p==selected,top,glyph);
        }
        pager(d,pages);
    }
    Text port_contact(int port) const {
        const auto language=port_language(port);
        if (game.fluency(language)==Fluent || game.crew_interpreter(language)!=NoOfficer) return "可沟通";
        return game.fluency(language)==Phrases?"会短语":"需口译";
    }
    void preflight(Draw& d) {
        page_title(d,"航前计划",SeaChartIcon);
        if (body.w>=189+d.measure("航前计划"))
            button(d,{body.right()-161,body.y+3,52,23},"配给",ShowRations,game.s.ration_flags!=0);
        button(d,{body.right()-105,body.y+3,52,23},"风况",ShowWeather);
        const auto path=passage(game.s.origin,selected);
        const auto forecast=game.forecast(path);
        const int x=body.x+7, y=body.y, w=body.w-14;
        const int controls_y=body.bottom()-54;
        const Text contact=is_port(selected)?port_contact(selected):Text{};
        Text timing;
        if (game.s.dispatch.number && game.dispatch_target()==selected) {
            if (selected!=DispatchOrders[game.dispatch_order()].to) timing="中途停靠";
            else timing=game.s.day+forecast.days>game.s.dispatch.due?"将逾期":"可按期";
        }
        const auto estimate=format("%d海里 · 预计%d日",path.miles,forecast.days);
        const int food=game.s.stores, water=game.s.water;
        const bool shortage=food<forecast.provisions || water<forecast.water;
        if (body.w>=300 && body.h>=146) {
            const Rect route_card{x,y+28,w*47/100,controls_y-y-32};
            harbor_panel(d,route_card,HarborPlaque,4);
            const int label_x=route_card.x+6, label_w=route_card.w-12;
            const auto route_name=format("%s → %s",Places[game.s.origin].name.data(),Places[selected].name.data());
            d.label(label_x,route_card.y+5,d.measure(route_name)<=label_w?Text(route_name):Places[selected].name,
                    gold,label_w);
            d.label(label_x,route_card.y+23,path.count?Text(estimate):"暂无可用航线",ivory,label_w);
            const auto status=format("%.*s%s%.*s",int(contact.size()),contact.empty()?"":contact.data(),
                                     !contact.empty() && !timing.empty()?" · ":"",
                                     int(timing.size()),timing.empty()?"":timing.data());
            d.label(label_x,route_card.y+40,status,timing=="将逾期"?vermilion:mist,label_w);
            if (route_card.h>=75 && game.s.first_mate!=NoOfficer)
                d.label(label_x,route_card.y+57,format("当值副官  %s",Officers[game.s.first_mate].name.data()),
                        quiet,label_w);

            const int supply_x=route_card.right()+5, supply_w=x+w-supply_x;
            const int card_y=y+31+d.font.cell, card_h=controls_y-card_y-4;
            d.center({supply_x,y+28,supply_w,d.font.cell+2},"现有 / 航程所需",mist);
            const int half=(supply_w-4)/2;
            const auto provision=[&](Rect card,Text label,int glyph,int available,int needed) {
                harbor_panel(d,card,HarborPlaque,4);
                const auto color=available<needed?vermilion:mint;
                const Rect sprite{glyph%3*32,glyph/3*32,32,32};
                d.center_with_icon({card.x+4,card.y+2,card.w-8,d.font.cell+1},label,ivory,
                                   {art::voyage_icons},sprite,13);
                d.center({card.x+4,card.y+d.font.cell+3,card.w-8,d.font.cell},format("%d / %d",available,needed),color);
                meter(d,{card.x+5,card.bottom()-9,card.w-10,7},available,needed,available<needed);
            };
            provision({supply_x,card_y,half,card_h},"口粮",RationIcon,food,forecast.provisions);
            provision({supply_x+half+4,card_y,supply_w-half-4,card_h},"淡水",FlaskIcon,water,forecast.water);
        } else {
            const int contact_w=contact.empty()?0:d.measure(contact)+20;
            d.label(x,y+30,Places[selected].name,gold,w-contact_w);
            if (contact_w) {
                const int start=x+w-contact_w;
                voyage_icon(d,SpeechIcon,{start,y+29,14,14});
                d.label(start+16,y+30,contact,mist);
            }
            const auto summary=format("%s%s%.*s",estimate.contents,timing.empty()?"":" · ",
                                      int(timing.size()),timing.empty()?"":timing.data());
            d.label(x,y+48,path.count?Text(summary):"暂无可用航线",ivory,w);
            const int half=w/2;
            d.label(x,y+65,format("粮有%d 需%d",food,forecast.provisions),food<forecast.provisions?vermilion:mint,half-3);
            d.label(x+half,y+65,format("水有%d 需%d",water,forecast.water),water<forecast.water?vermilion:mint,w-half);
            if (body.h>=160 && game.s.first_mate!=NoOfficer)
                d.label(x,y+84,format("当值副官  %s",Officers[game.s.first_mate].name.data()),quiet,w);
        }
        const bool ship_ok=game.s.hull>12;
        const bool ready=path.count && ship_ok && game.s.stores>=game.daily_rations() &&
                         game.s.water>=game.daily_water(Fair);
        const int third=(w-8)/3;
        button(d,{x,controls_y,third,23},"停靠点",ShowStops,false,false,{},DockAnchorIcon);
        button(d,{x+third+4,controls_y,third,23},"地理",ShowIntel,false,false,{},AstrolabeIcon);
        button(d,{x+(third+4)*2,controls_y,third,23},"掌舵",StartHelm,false,!ready,{},ShipWheelIcon);
        const int half=(w-5)/2;
        const int cost=game.resupply_cost(game.voyage_supply_target(forecast.provisions),
                                          game.voyage_supply_target(forecast.water));
        const Rect service{x,body.bottom()-27,half,23};
        if (game.tow_needed()) button(d,service,"请求拖带",ShowTow);
        else if (!game.port()) button(d,service,"返海图",Close);
        else if (!ship_ok && game.s.gold>=game.repair_cost())
            button(d,service,format("维修 %d金",game.repair_cost()),DockService+1);
        else if (!ship_ok || (cost>game.s.gold && !ready))
            button(d,service,"去船坞",OpenShipyard);
        else if (cost)
            button(d,service,format("补水粮 %d金",cost),TopUp,false,cost>game.s.gold,{},RationIcon);
        else if (path.count)
            d.center(service,shortage?"补给舱不足":"全程水粮齐备",shortage?vermilion:mint);
        button(d,{x+half+5,service.y,w-half-5,service.h},
               shortage?"仍要启航":"按航线启航",ConfirmSail,true,!ready,{},SailIcon);
    }
    void weather_forecast(Draw& d) {
        const bool manual=game.s.helm;
        const bool underway=game.sea();
        page_title(d,manual?"附近海况":"沿途风况",PennantIcon);
        button(d,{body.right()-105,body.y+3,52,23},"配给",ShowRations,game.s.ration_flags!=0);
        const int x=body.x+7, w=body.w-14, right=x+w;
        const auto forecast=manual?PassageForecast{}:underway?game.remaining_forecast():
                            game.forecast(passage(game.s.origin,selected));
        const Text sail=game.s.reefed?"已缩帆":"全帆";
        const int sail_x=right-d.measure(sail);
        d.label(sail_x,body.y+29,sail,quiet);
        const auto date=calendar_date(game.s.day);
        const auto season=local_season(game.position(),game.s.day);
        const auto season_line=format("%d月%d日  %s",date.month,date.day,ClimateSeasonNames[season].data());
        if (manual) {
            d.label(x,body.y+29,season_line,gold,sail_x-x-6);
            d.label(x,body.y+45,"掌舵驶离后，风况会变",mist,w);
        } else {
            const int destination=underway?game.sailing_target():selected;
            const auto name=game.charted(destination)?Places[destination].name:Text("未标记水域");
            const auto eta=format("%s  约%d日",name.data(),forecast.days);
            d.label(x,body.y+29,eta,gold,sail_x-x-6);
            const auto food=format("粮 %d/%d",forecast.provisions,game.s.stores);
            const auto water=format("水 %d/%d",forecast.water,int(game.s.water));
            d.label(x,body.y+45,food,forecast.provisions>game.s.stores?vermilion:mint);
            const int water_x=x+d.measure(food)+12;
            d.label(water_x,body.y+45,water,forecast.water>game.s.water?vermilion:mint);
            const int season_x=right-d.measure(season_line);
            if (season_x>=water_x+d.measure(water)+8)
                d.label(season_x,body.y+45,season_line,gold);
        }

        const auto outlook=manual?game.local_forecast():forecast.outlook;
        const int pitch=std::clamp((body.h-89)/ForecastDays,15,25);
        const int shown=manual?ForecastDays:std::min(ForecastDays,forecast.days);
        const int date_width=d.measure(format("第%d日",std::min(100000,game.s.day+ForecastDays+1)));
        for (int i=0;i<shown;++i) {
            const auto& entry=outlook[i];
            const Rect row{x,body.y+62+i*pitch,w,pitch-2};
            const int label_y=row.y+(row.h-d.font.cell)/2;
            d.fill(row,i%2?rgb(0x132e32):rgb(0x193b3f));
            d.label(x+4,label_y,format("第%d日",entry.day+1),ivory,date_width);
            const int icon=std::min(row.h,22);
            weather_icon(d,entry.sea.weather,{x+date_width+10,row.y+(row.h-icon)/2,icon,icon});
            const int details=x+date_width+13+icon;
            const auto wind=format("%s  向%s%d级",SeaWeatherNames[entry.sea.weather].data(),
                                   Bearings[entry.sea.direction].data(),entry.sea.strength);
            const auto water_use=format("耗水%d",entry.water);
            const auto rations_use=format("粮%d 水%d",entry.food,entry.water);
            const Text use=game.rationing(SaveFood)?Text(rations_use):Text(water_use);
            const auto pace=format("%d.%d节",entry.speed_tenths/10,entry.speed_tenths%10);
            const Color severity=entry.sea.weather==Squall?vermilion:entry.sea.weather==Heat?gold:mist;
            const int use_x=right-5-d.measure(use);
            const bool show_water=manual || details+d.measure(wind)+d.measure(pace)+16<=use_x;
            if (show_water) d.label(use_x,label_y,use,entry.sea.weather==Heat?gold:quiet);
            int wind_end=use_x;
            if (!manual) {
                wind_end=(show_water?use_x-10:right-5)-d.measure(pace);
                d.label(wind_end,label_y,pace,ivory);
            }
            d.label(details,label_y,wind,severity,wind_end-details-5);
        }
        const int half=(w-5)/2;
        const int button_y=body.bottom()-27;
        const bool stop=underway && !manual && game.s.leg+1<game.route.count;
        button(d,{x,button_y,half,23},stop?(game.s.stop_next?"取消停靠":"下站停靠"):
               game.s.reefed?"张帆":"缩帆",stop?NextStop:Trim,false,false,{},
               stop?MooringIcon:game.s.reefed?SailIcon:ReefIcon);
        button(d,{x+half+5,button_y,w-half-5,23},underway?"继续航行":"候风一日",
               underway?ResumeWeather:WaitDay,true,!underway && !game.port(),{},underway?ResumeIcon:-1);
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
            const Text status=!known?"待查":is_port(port)?port_contact(port):"测绘点";
            const auto eta=format("%d日 · %s",forecast.arrival_days[i],status.data());
            row(d,j,known?Places[port].name:Text("未标记水域"),eta,known?Destination+port:0,port==selected,
                28,known?(is_port(port)?SpeechIcon:AstrolabeIcon):-1);
        }
        pager(d,pages);
    }
    void port_prices(Draw& d) {
        const auto title=format("%s比价",GoodsNames[good].data());
        page_title(d,title);
        button(d,{body.right()-105,body.y+3,52,23},"市况",ShowMarketNews);
        d.label(body.x+6,body.y+28,"当日买价 / 卖价",quiet,body.w-12);
        std::array<int,PortCount> order{};
        int count=0;
        for (int i=0;i<PortCount;++i) {
            const int port=port_place(i);
            if (game.charted(port)) order[count++]=port;
        }
        const auto visited=[&](int port){return (game.s.visited&place_bit(port))!=0;};
        std::sort(order.begin(),order.begin()+count,[&](int a,int b) {
            if (a==game.s.origin || b==game.s.origin) return a==game.s.origin && b!=a;
            if (visited(a)!=visited(b)) return visited(a);
            const int first=visited(a)?game.price_at(a,good,true):0;
            const int second=visited(b)?game.price_at(b,good,true):0;
            return first==second?a<b:first>second;
        });
        const int rows=std::clamp((body.h-74)/row_pitch(),1,9);
        const int pages=std::max(1,(count+rows-1)/rows);
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int port=order[page*rows+i];
            const bool here=port==game.s.origin;
            if (visited(port)) {
                const auto prices=market_prices(game.price_at(port,good,false),game.price_at(port,good,true));
                row(d,i,Places[port].name,prices,here?0:Destination+port,here,47);
            } else row(d,i,Places[port].name,"未到访",Destination+port,false,47);
        }
        if (pages>1) pager(d,pages);
        else d.label(body.x+6,body.bottom()-22,"点港口查看航程与到港估价",quiet,body.w-12);
    }
    void trade_passage(Draw& d) {
        page_title(d,"贸易航程",CargoIcon);
        const auto plan=game.trade_plan(trade_port,good);
        const int x=body.x+7, y=body.y+28, w=body.w-14;
        if (!plan.days) {
            d.label(x,y,"这里暂无可用航线",quiet,w);
            return;
        }
        const int pitch=std::min(17,(body.h-62)/4);
        d.label(x,y,format("%s · %s",Places[trade_port].name.data(),GoodsNames[good].data()),gold,w);
        d.label(x,y+pitch,format("约%d日  需粮%d 水%d",plan.days,plan.food,plan.water),ivory,w);
        const auto cargo=plan.purchase.units?format("本港买%d件  %d金",plan.purchase.units,plan.purchase.gold):
                                            format("舱内%d件",plan.cargo);
        d.label(x,y+2*pitch,cargo,ivory,w);
        const auto sale=format("估卖%d件 %d金 · 余%d件",plan.sale.units,plan.sale.gold,plan.cargo-plan.sale.units);
        const Text receipt=plan.market==ArrivalMarket::Unknown?"未到访，抵港后问价":
            plan.market==ArrivalMarket::Closed?"该港不经营此货":
            plan.market==ArrivalMarket::Held?"抵港时仍暂停交易":
            !plan.cargo?"本港暂无可买或自带货物":
            plan.sale.units?Text(sale):"预计暂无可成交量";
        d.label(x,y+3*pitch,receipt,plan.sale.units?mint:mist,w);
        const bool affordable=plan.supply_gold+plan.purchase.gold<=game.s.gold;
        const auto supplies=format("补给%d金%s",plan.supply_gold,affordable?"":" · 金币不足");
        const Text supply_info=!plan.supplies_fit?"水粮舱装不下全程用量":
            plan.supply_gold?Text(supplies):"水粮已备";
        d.label(x,y+4*pitch,supply_info,plan.supplies_fit && affordable?quiet:vermilion,w);
        const int bottom=body.bottom()-27;
        if (plan.purchase.units) pair(d,{x,bottom,w,23},"去买货",TradePreparePurchase,
                                     "航前计划",TradeStartPreparation);
        else button(d,{x,bottom,w,23},"航前计划",TradeStartPreparation,true);
    }
    void market_news(Draw& d) {
        page_title(d,"各港市况",LetterIcon);
        d.label(body.x+6,body.y+28,"已知港口的收货与到货消息",mist,body.w-12);
        const int today=game.s.day;
        std::array<int,std::size(MarketNotices)> order{};
        int count=0;
        for (int i=0;i<int(std::size(MarketNotices));++i)
            if (game.charted(MarketNotices[i].port)) order[count++]=i;
        std::sort(order.begin(),order.begin()+count,[&](int a,int b) {
            return MarketNotices[a].until(today)<MarketNotices[b].until(today);
        });
        const int rows=std::clamp((body.h-74)/row_pitch(),1,9);
        const int pages=std::max(1,(count+rows-1)/rows);
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const auto& news=MarketNotices[order[page*rows+i]];
            const auto title=format("%s · %s",Places[news.port].name.data(),news.headline.data());
            const int days=news.until(today);
            const auto timing=days?format("%d日后",days):format("还剩%d日",news.remaining(today));
            row(d,i,title,timing,Destination+news.port,!days,47);
        }
        if (pages>1) pager(d,pages);
        else d.label(body.x+6,body.bottom()-22,"点选消息可在海图上标记港口",quiet,body.w-12);
    }
    void provisions_page(Draw& d) {
        page_title(d,"食物与淡水",RationIcon);
        button(d,{body.right()-105,body.y+3,52,23},"配给",ShowRations,game.s.ration_flags!=0);
        const int x=body.x+7, w=body.w-14, half=(w-5)/2;
        const int food=game.s.stores, water=game.s.water;
        const int food_cap=game.food_capacity(), water_cap=game.water_capacity();
        const int cap=2*game.max_stores();
        const auto shared=format("共用补给舱 %d/%d",food+water,cap);
        const int bar_x=x+d.measure(shared)+9;
        d.label(x,body.y+29,shared,ivory,w);
        if (x+w-bar_x>=32)
            meter(d,{bar_x,body.y+32,x+w-bar_x,8},food+water,cap,false);

        const int cards_y=body.y+47, cards_h=body.bottom()-60-cards_y;
        const auto supply=[&](int left,int width,Text name,int glyph,int amount,int limit,int cost,bool drink) {
            const Rect card{left,cards_y,width,cards_h};
            harbor_panel(d,card,HarborPlaque,4);
            const auto load=format("%s %d/%d",name.data(),amount,limit);
            const Rect sprite{glyph%3*32,glyph/3*32,32,32};
            d.center_with_icon({card.x+4,card.y+3,card.w-8,d.font.cell+3},load,ivory,
                               {art::voyage_icons},sprite,15);
            if (card.h>=d.font.cell+16)
                meter(d,{card.x+5,card.bottom()-10,card.w-10,7},amount,limit,drink);

            const auto unload=format("卸%s %d份",name.data(),std::min(10,amount));
            button(d,{left,body.bottom()-57,width,20},unload,drink?UnloadWater:UnloadFood,false,!amount);
            const int count=std::min(10,limit-amount);
            const auto title=count?format("添%s +%d",name.data(),count):format("%s已满",name.data());
            const auto price=format("%d金",cost);
            const Text detail=!cost?"":cost>game.s.gold?"金币不足":Text(price);
            button(d,{left,body.bottom()-PricedButtonHeight-2,width,PricedButtonHeight},title,
                   drink?BuyWater:BuyFood,true,!cost || game.s.gold<cost,detail);
        };
        supply(x,half,"口粮",RationIcon,food,food_cap,game.resupply_cost(food+10,water),false);
        supply(x+half+5,w-half-5,"淡水",FlaskIcon,water,water_cap,game.resupply_cost(food,water+10),true);
    }

    void rations_page(Draw& d) {
        page_title(d,"船员配给",RationIcon);
        const int x=body.x+7, w=body.w-14, right=x+w;
        const int morale=game.s.spirits;
        const int tomorrow=std::min(99999,game.s.day+1);
        const auto weather=conditions_at(game.position(),tomorrow).weather;
        const auto planned=game.sailing_ration(tomorrow,weather);
        const auto spirits=format("士气 %d/100",morale);
        d.label(x,body.y+28,spirits,morale<40?vermilion:ivory);
        const int meter_x=x+d.measure(spirits)+11;
        meter(d,{meter_x,body.y+31,std::min(139,right-meter_x),9},morale,100,false);

        for (int i=0;i<2;++i) {
            const bool food=i==0;
            const int y=body.y+47+i*32;
            const bool reduced=game.rationing(food?SaveFood:SaveWater);
            const int stores=food?game.s.stores:int(game.s.water);
            const int allowance=food?planned.food:planned.water;
            d.fill({x,y,w,29},i?rgb(0x132e32):rgb(0x193b3f));
            voyage_icon(d,food?RationIcon:FlaskIcon,{x+3,y+3,23,23});
            const auto name=format("%s · %s",food?"食物":"淡水",reduced?"节用":"照常");
            const auto amount=format("余%d份  下日耗%d份",stores,allowance);
            const int text_width=w-119;
            d.label(x+30,y+1,name,reduced?gold:ivory,text_width);
            d.label(x+30,y+14,amount,stores<allowance?vermilion:mist,text_width);
            button(d,{right-84,y+3,80,23},reduced?"恢复照常":"改为节用",
                   food?ToggleFoodRation:ToggleWaterRation,!reduced);
        }
        d.label(x,body.y+111,"偶数日少发一份，士气会下降",mist,w);
        d.label(x,body.y+127,"炎热天淡水照常发放",quiet,w);
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
        if (game.dispatch_board_here())
            button(d,{body.right()-124,body.y+3,70,23},"港际运单",OpenDispatch,true);
        Rect desk{body.x+6,body.y+28,body.w-12,body.h-34};
        const int picture=guild_picture(game.s.origin);
        const int art_width=picture==Calm?0:guild_illustration_width();
        if (art_width) {
            sea_picture_panel(d,picture,{desk.x,desk.y,art_width,desk.h});
            desk.x+=art_width+5;
            desk.w-=art_width+5;
        }
        const bool invested=game.guild_stake(game.s.origin);
        const auto cost=format("入股 %d金",game.guild_cost());
        const bool held=game.market_held();
        button(d,{desk.x,desk.y,desk.w,PricedButtonHeight},held?"港务留案":"本港商栈",
               held?OpenPortCase:InvestPort,!invested || held,false,held?"暂停交易":invested?"已入股":Text(cost));
        const int y=body.bottom()-28, hint_y=desk.y+PricedButtonHeight+5;
        const Rect story{desk.x,y-26,desk.w,22};
        if (hint_y+d.font.cell<=story.y)
            d.label(desk.x+3,hint_y,held?"结案后恢复交易":invested?"本港买货约省6%":"入股后买货约省6%",
                    held?quiet:mist,desk.w-6);
        if (game.s.origin==madeira ||
            (game.s.origin==cadiz && game.s.drift_stage>=DriftRecovered)) {
            pair(d,story,game.s.origin==madeira?"银铃护航":"码头决斗",
                 game.s.origin==madeira?ShowEscort:ShowDuel,"红漆木匣",ShowDriftStory);
        } else if (game.s.origin==lisbon)
            button(d,story,"银铃护航",ShowEscort,false,false,{},PortStoryIcon);
        else if (game.s.origin==cadiz)
            button(d,story,game.s.cadiz_victory?"加的斯的旧誓 · 已完成":"加的斯的旧誓",
                   ShowDuel,false,false,{},PortStoryIcon);
        else if (game.s.origin==tunis)
            button(d,story,"招募优素福",Recruit,false,false,{},CrewIcon);
        else if (const int tale=game.tale_here();game.shore_task_here()>=0 && tale>=0)
            button(d,story,Tales[tale].title,PortTaleAction,true,false,{},PortStoryIcon);
        const int half=(desk.w-5)/2;
        button(d,{desk.x,y,half,24},"打听传闻",ShowRumors,false,false,{},SpeechIcon);
        button(d,{desk.x+half+5,y,desk.w-half-5,24},"口译与学话",OpenLanguages,false,false,{},PhrasebookIcon);
    }
    void dispatch_board(Draw& d) {
        const auto& record=game.s.dispatch;
        const int active=game.dispatch_order();
        const int x=body.x+7, y=body.y, width=body.w-14;
        const Rect bottom{x,body.bottom()-29,width,25};
        if (dispatch_page==DispatchConfirmCancel && active>=0) {
            page_title(d,"退回运单",LetterIcon);
            const auto& rate=DispatchRates[DispatchOrders[active].kind];
            d.wrap({x,y+35,width,body.h-68},
                   format("货件或旅客将在本港下船，已付的%d金押金不会退回。",rate.bond),ivory);
            pair(d,bottom,"继续运送",Close,"确认退单",CancelDispatch,true);
            return;
        }
        if (dispatch_page==DispatchReceipt) {
            if (!record.last) { dispatch_page=DispatchList; }
            else {
                const auto& order=DispatchOrders[record.last-1];
                page_title(d,"最近运单",LetterIcon);
                d.label(x,y+34,order.title,gold,width);
                d.label(x,y+54,DispatchOutcomeLabels[record.outcome],ivory,width);
                const auto& rate=DispatchRates[order.kind];
                if (record.outcome==DispatchOnTime || record.outcome==DispatchLate ||
                    record.outcome==DispatchInspectedOnTime) {
                    const auto fee=game.dispatch_quote(record.last-1).fee;
                    const int bonus=record.outcome==DispatchInspectedOnTime?DispatchInspectionBonus:0;
                    d.label(x,y+74,format("入账%d金 · 已含退押金",
                            rate.bond+(record.outcome==DispatchLate?fee/2:fee)+bonus),mint,width);
                    d.wrap({x,y+94,width,bottom.y-y-98},order.delivery,ivory,14);
                } else d.label(x,y+74,format("扣除押金 %d金",rate.bond),vermilion,width);
                button(d,bottom,"返回运单簿",Close);
                return;
            }
        }
        const int id=active>=0?active:dispatch_page;
        if (id<0) {
            page_title(d,"港际运单",LetterIcon);
            int offer=0;
            for (int i=0;i<DispatchCount;++i) {
                if (!game.dispatch_offer_here(i)) continue;
                const auto& order=DispatchOrders[i];
                row(d,offer++,order.title,i==RoadFreight?"驿道货运":DispatchRates[order.kind].name,
                    DispatchEntry+i,true);
            }
            if (!offer)
                d.wrap({x,y+35,width,body.h-68},"本港暂无新运单。可在里斯本、丰沙尔等港的商会接单。",mist);
            if (record.last) button(d,bottom,"查看最近运单",ViewDispatchReceipt,false,false,{},LetterIcon);
            return;
        }

        const auto& order=DispatchOrders[id];
        const auto& rate=DispatchRates[order.kind];
        page_title(d,order.title,LetterIcon);
        const bool at_stop=game.dispatch_stop_pending();
        if (at_stop)
            button(d,{body.right()-103,y+3,49,23},"退单",AskCancelDispatch);
        if (active>=0 && record.last && (!at_stop || body.w>=255))
            button(d,{body.right()-(at_stop?157:103),y+3,49,23},"回执",ViewDispatchReceipt);
        const auto known=[&](int p) { return game.charted(p)?Places[p].name.data():"未定位"; };
        const bool overdue=active>=0 && game.s.day>record.due;
        if (at_stop) {
            d.label(x,y+30,format("%s 中转 · 送往%s",known(order.stop),known(order.to)),gold,width);
            const auto eta=game.forecast(passage(order.stop,order.to));
            d.label(x,y+48,format("余期%d日 · 后程约%d日 · 按期验货赏%d金",
                    std::max(0,record.due-game.s.day),eta.days,DispatchInspectionBonus),
                    overdue?vermilion:mint,width);
            d.wrap({x,y+68,width,bottom.y-y-76},order.waypoint,ivory,14);
            const int half=(width-5)/2;
            const Rect actions{bottom.x,bottom.y-4,width,bottom.h+4};
            button(d,{actions.x,actions.y,half,actions.h},"验货重封",InspectDispatchStop,true,
                   game.s.gold<DispatchInspectionCost || game.s.day==99999,
                   format("%d金 · 耗1日",DispatchInspectionCost),CargoIcon);
            button(d,{actions.x+half+5,actions.y,width-half-5,actions.h},"保留原封",SealDispatchStop,
                   false,false,"当天可走",LetterIcon);
            return;
        }
        d.image({art::dispatch_icons},{int(order.kind)*32,0,32,32},{x,y+27,27,27},true);
        d.label(x+32,y+30,format("%s · 送往%s",rate.name.data(),known(order.to)),gold,width-32);
        if (active<0) {
            const auto quote=game.dispatch_quote(id);
            d.label(x+32,y+47,format("占舱%d · 押金%d金",rate.space,rate.bond),mist,width-32);
            const auto timing=format("预计%d日 · 时限%d日 · 运费%d金",quote.days,quote.days+rate.slack,quote.fee);
            d.label(x,y+66,timing,ivory,width);
            const auto requirement=game.dispatch_requirement(id);
            if (!requirement.empty()) d.label(x,y+84,requirement,vermilion,width);
            else if (order.stop>=0) d.label(x,y+84,format("须在%s靠港",known(order.stop)),mint,width);
            else if (id==RoadFreight) d.label(x,y+84,"完单后木料进入丰沙尔市场",mint,width);
            else d.label(x,y+84,"迟到运费减半，交付退押金",mist,width);
            button(d,bottom,"交押金接单",AcceptDispatch,true,!requirement.empty());
            return;
        }
        d.label(x+32,y+47,format("限第%d日 · 占舱%d",record.due+1,rate.space),
                overdue?vermilion:mist,width-32);
        const int next=game.dispatch_target();
        const bool ready=game.port() && next==order.to && game.s.origin==order.to;
        const auto quote=game.dispatch_quote(id);
        if (next!=order.to)
            d.label(x,y+66,format("先在%s靠港",known(next)),gold,width);
        else if (record.stopped==DispatchInspected)
            d.label(x,y+66,"已验货重封",mint,width);
        else if (record.stopped==DispatchSealed)
            d.label(x,y+66,"已保留原封",mint,width);
        else if (order.stop>=0)
            d.label(x,y+66,format("已在%s停靠",known(order.stop)),mint,width);
        else if (overdue) d.label(x,y+66,format("已逾期%d日",game.s.day-record.due),vermilion,width);
        else d.label(x,y+66,format("余%d日",record.due-game.s.day),mint,width);
        d.label(x,y+84,format("%s%d金 · 退押金%d金",overdue?"逾期运费":"运费",
                overdue?quote.fee/2:quote.fee+(record.stopped==DispatchInspected?DispatchInspectionBonus:0),
                rate.bond),overdue?vermilion:ivory,width);
        if (game.port()) {
            const int half=(width-5)/2;
            button(d,{bottom.x,bottom.y,half,bottom.h},ready?"交付运单":"标记下一港",
                   ready?FileDispatch:MarkDispatch,true);
            button(d,{bottom.x+half+5,bottom.y,width-half-5,bottom.h},"退单",AskCancelDispatch);
        }
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
        lesson_language=game.can_study(preferred)?preferred:port_language(game.s.origin);
        set_modal(LanguageDesk);
    }
    void language_desk(Draw& d) {
        const int port=game.s.origin;
        const Language primary=port_language(port), second=port_second_language(port);
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
        std::array<int,PortStoryCount> seen{};
        int seen_count=0;
        for (int i=0;i<PortStoryCount;++i)
            if (game.port_story_stage(i)!=PortStoryUnknown) seen[seen_count++]=i;
        const int visible=list_rows();
        const int count=seen_count+TaleCount+3;
        const int pages=(count+visible-1)/visible;
        page=std::clamp(page,0,pages-1);
        for (int j=0;j<visible && page*visible+j<count;++j) {
            const int i=page*visible+j-seen_count;
            if (i<0) {
                const int id=seen[i+seen_count];
                const auto stage=game.port_story_stage(id);
                const auto& story=PortStories[id];
                const bool underway=story.dispatch>=0 && game.dispatch_order()==story.dispatch;
                const auto outcome=game.story_receipt(story.dispatch).outcome;
                const Text status=underway?"运送中":
                    outcome==DispatchInspectedOnTime?"按期交付":
                    outcome?DispatchOutcomeLabels[outcome]:
                    stage==PortStoryMet?Places[story.port].name:stage==PortStoryFirst && story.dispatch>=0?"已托付":"已完成";
                row(d,j,story.title,status,ReadPortStory+id,stage==PortStoryMet || underway,28,LetterIcon);
                continue;
            }
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
    void strait_story_page(Draw& d) {
        const auto& story=PortStories[port_story_id];
        const auto stage=game.port_story_stage(port_story_id);
        const bool finished=stage>=PortStoryFirst;
        const bool local=game.port() && game.s.origin==story.port;
        const bool active=stage==PortStoryFirst && game.dispatch_order()==story.dispatch;
        const auto receipt=active?StoryDispatchReceipt{}:game.story_receipt(story.dispatch);
        const bool retry=dispatch_failed(receipt.outcome);
        const auto retry_requirement=retry && local?game.dispatch_requirement(story.dispatch):Text{};
        const bool translation=local && (retry?retry_requirement=="需要口译":
                               !finished && game.port_story_requirement(port_story_id,0)=="需要口译");
        const bool actions=active || ((!finished || retry) && !game.sea());
        Text copy=finished?story.endings[stage-PortStoryFirst]:story.opening;
        if (translation) copy="货主指了指海图上的两处港口。请位口译来听清托付。";
        else if (retry)
            copy=receipt.outcome==DispatchCancelled?"货件退回了发货港。货主在等新的船班。":
                                                   "这趟航程中断了。货主备了替换的货件，可以回港找他。";
        else if (receipt.outcome) copy=DispatchOrders[story.dispatch].delivery;
        constexpr Color ink=rgb(0x312117), note=rgb(0x795022);
        const auto frame=d.clip.inset(2);
        if (const auto* pixels=sea_picture.pixels(PortStoryPictureBase+port_story_id-WetPorcelain))
            d.cover({pixels},frame);
        else d.fill(frame,ocean);
        harbor_panel(d,frame,HarborTrim);
        harbor_button(d,{safe.right()-51,safe.y+1,51,24},"返回",Close);
        const int inset=safe.w>=345?9:4;
        const int x=safe.x+inset, width=safe.w-2*inset;
        const Rect buttons{x,safe.bottom()-33,width,32};
        const int end=actions?buttons.y-4:safe.bottom()-2;
        const int text_w=width-20;
        const int text_h=d.wrap({0,0,text_w,512},copy,ink,15,false);
        const int y=std::max(safe.y+30,end-text_h-34);
        harbor_panel(d,{x,y,width,end-y},HarborPaper);
        voyage_icon(d,LetterIcon,{x+9,y+9,14,14});
        const int title_w=std::min(d.measure(story.title)+6,text_w-20);
        d.label(x+28,y+8,story.title,ink,title_w);
        const auto status=receipt.outcome==DispatchInspectedOnTime?Text("验货后按期交付"):
                          receipt.outcome?DispatchOutcomeLabels[receipt.outcome]:story.person;
        if (receipt.day>=0)
            d.label(x+33+title_w,y+8,format("第%d日 · %s",receipt.day+1,status.data()),note,text_w-title_w-26);
        else d.label(x+33+title_w,y+8,status,note,text_w-title_w-26);
        d.wrap({x+10,y+27,text_w,end-y-30},copy,ink,15);

        if (active) {
            harbor_button(d,buttons,"查看运单",OpenDispatch,true,false,{},LetterIcon);
        } else if ((!finished || retry) && !game.sea()) {
            if (!local) {
                harbor_button(d,buttons,format("标记%s",Places[story.port].name.data()),
                              Destination+story.port,true,false,{},SeaChartIcon);
            } else if (translation) {
                harbor_button(d,buttons,format("请口译 %d金",Game::InterpreterFee),HireInterpreter,true,
                              game.s.gold<Game::InterpreterFee,{},SpeechIcon);
            } else if (retry) {
                const auto bond=format("押金 %d金",DispatchRates[DispatchOrders[story.dispatch].kind].bond);
                harbor_button(d,buttons,"重新接单",RetryStoryDispatch,true,!retry_requirement.empty(),
                              retry_requirement.empty()?Text(bond):retry_requirement,LetterIcon);
            } else {
                const int half=(width-5)/2;
                for (int choice=0;choice<2;++choice) {
                    const auto requirement=game.port_story_requirement(port_story_id,choice);
                    const Rect area{x+choice*(half+5),buttons.y,choice?width-half-5:half,buttons.h};
                    harbor_button(d,area,story.choices[choice],PortStoryChooseFirst+choice,choice==0,
                                  !requirement.empty(),requirement.empty()?story.effects[choice]:requirement,
                                  choice==0?LetterIcon:CrewIcon);
                }
            }
        }
    }
    void port_story_page(Draw& d) {
        if (port_story_id>=WetPorcelain) { strait_story_page(d); return; }
        const auto& story=PortStories[port_story_id];
        const auto stage=game.port_story_stage(port_story_id);
        const bool finished=stage>=PortStoryFirst;
        const bool local=game.port() && game.s.origin==story.port;
        const bool can_guide=finished && port_story_id==TornChart && !game.sea();
        const bool translation=local && !finished && port_story_id==TwoLetters && !game.understands(Castilian);
        const bool has_actions=can_guide || (!finished && (local || !game.sea()));
        Text copy=finished?story.endings[stage-PortStoryFirst]:story.opening;
        if (translation) copy="两封信都用卡斯蒂利亚语写成。先请口译问清内容，再选一封签字吧。";

        constexpr Color paper_ink=rgb(0x312117), note_ink=rgb(0x795022);
        constexpr int spacing=17;
        const int inset=safe.w>=345?12:5;
        const int left=safe.x+inset, right=safe.right()-inset;
        const int portrait_size=std::clamp((right-left)/5,39,82);
        const int x=left+portrait_size+22, width=right-x-10;
        const int dialogue_h=d.wrap({0,0,width,int(copy.size())*spacing},copy,paper_ink,spacing,false);
        const int summary_h=finished?d.font.cell+4:0;
        const int action_h=translation || can_guide || !local ? 28 : 35;
        const int bottom=safe.bottom()-(has_actions?action_h-6:3);
        const int panel_h=std::max(portrait_size+19,49+dialogue_h+summary_h+12);
        const int top=std::max(safe.y+30,bottom-panel_h);
        const Rect paper{left,top,right-left,bottom-top};
        const Rect portrait{left+10,top+9,portrait_size,portrait_size};
        const Rect options{left+9,safe.bottom()-action_h,paper.w-18,action_h};

        d.darken(harbor_view(d,town_index(story.port)),5);
        harbor_panel(d,d.clip.inset(2),HarborTrim);
        harbor_button(d,{safe.right()-51,safe.y+1,51,24},"返回",Close);
        harbor_panel(d,paper,HarborPaper);
        harbor_panel(d,portrait.inset(-3),HarborTrim,4);
        d.image({art::port_people},{port_story_id*64,0,64,64},portrait);
        d.frame(portrait,rgb(0x594129));

        voyage_icon(d,LetterIcon,{x,top+8,15,15});
        d.label(x+19,top+8,story.title,paper_ink,width-19);
        const int name_w=std::min(width,d.measure(story.person)+15);
        const Rect name{x,top+25,name_w,20};
        harbor_panel(d,name,HarborPlaque,4);
        d.center(name.inset(3),story.person,ivory);
        if (name.right()+10<right-13) {
            const int y=name.y+10;
            d.line({name.right()+7,y},{right-13,y},note_ink);
            d.diamond({(name.right()+right-6)/2,y},2,note_ink);
        }
        const int text_top=top+49;
        const int text_bottom=(has_actions?options.y-2:bottom-6)-summary_h;
        const int written=d.wrap({x,text_top,width,text_bottom-text_top},copy,paper_ink,spacing);
        if (finished) d.label(x,written+2,story.effects[stage-PortStoryFirst],note_ink,width);
        if (can_guide) {
            harbor_button(d,options,game.charted(ribeira)?"定位里贝拉格兰德":"圈出大致海域",
                          PortStoryGuide,true,false,{},SeaChartIcon);
        } else if (!finished && !local) {
            if (!game.sea())
                harbor_button(d,options,format("标记%s",Places[story.port].name.data()),
                              Destination+story.port,true,false,{},SeaChartIcon);
        } else if (translation) {
            harbor_button(d,options,format("请口译 %d金",Game::InterpreterFee),HireInterpreter,true,
                          game.s.gold<Game::InterpreterFee,{},SpeechIcon);
        } else if (!finished) {
            constexpr int choice_icons[WetPorcelain][2]{{RepairIcon,CrewIcon},{CargoIcon,SpeechIcon},
                                                        {SeaChartIcon,AstrolabeIcon},{LetterIcon,PennantIcon}};
            const int half=(options.w-5)/2;
            for (int choice=0;choice<2;++choice) {
                const Text requirement=game.port_story_requirement(port_story_id,choice);
                const Rect area{options.x+choice*(half+5),options.y,half,options.h};
                harbor_button(d,area,story.choices[choice],PortStoryChooseFirst+choice,choice==0,
                              !requirement.empty(),requirement.empty()?story.effects[choice]:requirement,
                              choice_icons[port_story_id][choice]);
            }
        }
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
            const int y=body.y+31;
            int text_x=x;
            if (!sea_picture_path(entry.kind).empty()) {
                const int width=std::min(160,w/3);
                const Rect picture{x,y,width,std::min(width*104/160,body.bottom()-y-6)};
                sea_picture_panel(d,entry.kind,picture);
                text_x=picture.right()+8;
            }
            const int text_w=x+w-text_x;
            d.label(text_x,y,scene.title,gold,text_w);
            d.label(text_x,y+20,format("第 %d 日",entry.day+1),mist,text_w);
            d.wrap({text_x,y+41,text_w,body.bottom()-y-44},scene.results[entry.choice],ivory);
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
        if (is_port(p)) {
            const Language language=port_language(p);
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
        for (int p=0;p<PlaceCount;++p)
            if (!is_port(p) && game.charted(p)) entries[count++]=p;
        if (!count) {
            d.label(body.x+6,body.y+38,"出海后逐步收录测绘水域",mist,body.w-12);
            return;
        }
        const int rows=list_rows(), pages=(count+rows-1)/rows;
        page=std::clamp(page,0,pages-1);
        for (int i=0;i<rows && page*rows+i<count;++i) {
            const int p=entries[page*rows+i];
            const bool seen=bool(game.s.surveyed&place_bit(p));
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
            const bool heard=game.heard_rumor(id);
            row(d,i,heard?Places[rumor.site].name:rumor.speaker,
                heard?"已抄录":found?"本港见闻":"新线索",RumorEntry+id,heard);
        }
        pager(d,pages);
    }
    void rumor_page(Draw& d) {
        const auto& rumor=Rumors[rumor_item];
        const bool found=game.charted(rumor.site), heard=game.heard_rumor(rumor_item);
        const auto language=port_language(rumor.port);
        const bool understood=heard || game.understands(language);
        const bool local=game.port() && game.s.origin==rumor.port;
        page_title(d,heard?Places[rumor.site].name:rumor.speaker);
        const int x=body.x+6, w=body.w-12;
        const auto needed=format("需%s口译",LanguageNames[language].data());
        d.label(x,body.y+29,heard?(found?"已记入海图":"只知道大致海域"):
                understood?"可抄录这段见闻":Text(needed),heard?mint:mist,w);
        const Text clue=understood?rumor.clue:
            game.fluency(language)==Phrases?RumorFragments[rumor_item]:"听不清细节。可以在本港请口译。";
        d.wrap({x,body.y+48,w,body.h-78},clue,ivory,15);
        const int y=body.bottom()-28;
        if (local && !heard) {
            const int pick=found?(w-5)/2:w;
            button(d,{x,y,pick,24},understood?"抄录见闻":"找口译或学话",
                   understood?RecordRumor:OpenLanguages,true);
            if (found) button(d,{x+pick+5,y,w-pick-5,24},"定位海图",MarkRumor);
        } else if (found || heard)
            button(d,{x,y,w,24},found?"定位海图":"圈出大致海域",MarkRumor,true,game.sea());
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
        if (at_archive && game.land_stage()>=LandNotes) {
            const int land=regional_survey_at(game.s.origin);
            const Text report=land>=0?RegionalSurveys[land].report:Text("驿道图");
            button(d,{body.right()-107,body.y+3,54,23},report,OpenLandReport,game.land_stage()<LandFiled);
        }
        if (on_site || record.stage>=HasEvidence) {
            const bool large=body.h>=174 || w>=290;
            const int art_w=large?std::min(w/3,126):46;
            const int art_y=y+25, art_h=art_w/2;
            const Rect picture{x+w-art_w,art_y,art_w,art_h};
            if (on_site && expedition_item==BirdsAtLowWater)
                d.image({art::expedition_arguin},{0,0,320,160},picture);
            else d.image({art::discoveries},{expedition_item*160,0,160,80},picture);
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
            "在市场选货后点数量，可输入件数并预览总价。议价以五件为一批，出价后会看到还价、成交或失败。船坞可补给和修船。",
            "出发前看预计天数。船员越多耗粮越快。海上可缩帆、停船、掉头或在下站靠泊。遇到私掠船时，火炮齐射后需装填，机动可以贴近并瞄准。距一且有六名船员时可以接舷。",
            "海里是航程单位。一节等于每小时航行一海里。航线按整日估算。自由掌舵每次推进一小时，靠泊后时刻会保留。",
            "金色菱形标明下一处主线。到青色测绘点后记入图鉴。交易和航行中会自动保存，重开可续航。",
            "决斗时先看对方起手。格挡克突刺，佯攻破格挡，突刺截佯攻。",
            "在目的地页点掌舵可自由出海，用左右舵或点海面改航向。未知港口可从传闻圈出大致海域，驶近后会发现港口。",
            "航海时点顶部倍速。沿航线可用一、二或四倍，自由掌舵可用一或二倍。船速和水粮消耗按原航程计算。",
            "里斯本可从河堤走旧驿道，丰沙尔可调查灌渠，加的斯可去盐田。按水粮与潮位选择路，回城到档案室交图。",
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
    void land_briefing(Draw& d) {
        page_title(d,"里斯本 · 林道护送",CrewIcon);
        d.wrap({body.x+8,body.y+31,body.w-16,body.h-62},
               "三袋船粮等着从林道运回码头。货主说有人在路口拦车。带两名同伴送队长走到金色路标，护送成功可得90金。",
               ivory,16);
        const auto blocked=game.land_battle_requirement(RoadEscort);
        button(d,{body.x+5,body.bottom()-28,body.w-10,24},blocked.empty()?"带队出城":blocked,
               StartRoadEscort,true,!blocked.empty(),{},CrewIcon);
    }

    void show_land_battle() {
        land_selected=0;
        land_striker=-1;
        land_retreat=false;
        land_frame_start=0;
        land_enemy_turn=false;
        set_modal(None);
        changed();
    }

    Rect land_tile(int cell) const {
        const auto p=land_point(cell);
        const int w=land_area.w/LandColumns,h=land_area.h/LandRows;
        return {land_area.x+p.x*w,land_area.y+p.y*h,w,h};
    }

    Point land_unit_point(int cell) const {
        const auto tile=land_tile(cell);
        return {tile.x+tile.w/2,tile.bottom()-4};
    }

    void land_screen(Draw& d) {
        const auto& fight=game.s.land_fight;
        if (land_selected<0 || land_selected>=LandFriends || !fight.units[land_selected].hp) land_selected=0;
        skin_box(d,safe,0,6);
        constexpr Text names[]{"","林道护送","码头争夺","守住仓房"};
        const auto goal=fight.kind==RoadEscort?"让队长抵达金旗":
            fight.kind==WharfAssault?"占住金旗并撑过敌方回合":"守住仓房五回合";
        d.label(safe.x+7,safe.y+7,names[fight.kind],gold,safe.w-73);
        const auto round=format("回合 %d",int(fight.round));
        d.label(safe.right()-d.measure(round)-7,safe.y+7,round,ivory);
        d.label(safe.x+7,safe.y+23,goal,mist,safe.w-14);
        const int cell_h=std::max(13,(safe.h-88)/LandRows);
        const int cell_w=std::min((safe.w-12)/LandColumns,cell_h*2);
        land_area={safe.x+(safe.w-cell_w*LandColumns)/2,safe.y+40,
                   cell_w*LandColumns,cell_h*LandRows};
        const int focus=pad_focus?focused:-1;
        const auto steps=land_steps(fight,land_selected);
        const auto old_clip=d.clip;
        d.clip=d.clipped(land_area);
        for (int cell=0;cell<LandCells;++cell) {
            const auto tile=land_tile(cell);
            const int unit=land_occupant(fight,cell);
            const bool move=steps[cell]!=255 && steps[cell]>0;
            const bool target=unit>=LandFriends && land_can_strike(fight,land_selected,unit);
            draw_land_ground(d,tile,land_ground(fight,cell),cell,fight.kind!=RoadEscort);
            if (unit==land_selected || target)
                d.frame(tile.inset(1),target?vermilion:gold);
            if (move)
                d.diamond({tile.x+tile.w/2,tile.y+tile.h/2},2,mint);
            if (move || (unit>=0 && unit<LandFriends) || target) {
                if (button_count==focus) d.frame(tile.inset(2),mint);
                buttons[button_count++]={tile,LandCellAction+cell};
            }
        }
        const int elapsed=land_frame_start?std::min(220U,now-land_frame_start):220;
        for (int y=0;y<LandRows;++y) for (int i=0;i<LandUnits;++i) {
            const auto unit=fight.units[i];
            if (!unit.hp || land_point(unit.cell).y!=y) continue;
            Point at=land_unit_point(unit.cell);
            const auto before=land_before.units[i];
            const bool walking=elapsed<170 && before.hp && before.cell!=unit.cell;
            if (walking) {
                const auto old=land_unit_point(before.cell);
                at={old.x+(at.x-old.x)*elapsed/170,old.y+(at.y-old.y)*elapsed/170};
            }
            const bool attacked=land_striker==i || (land_enemy_turn && i>=LandFriends &&
                (unit.orders&(LandStruck|LandBraced))==LandStruck);
            const int pose=elapsed<165 && attacked?2:
                walking && (elapsed/45)%2?1:unit.orders&LandBraced?3:0;
            const int role=i==0?0:i<LandFriends?1:2;
            const int sprite_h=std::clamp(cell_h-1,13,24), sprite_w=sprite_h*24/27;
            const auto team=i<LandFriends?mint:vermilion;
            d.diamond({at.x,at.y+1},i==land_selected?7:5,ink);
            d.diamond({at.x,at.y},i==land_selected?6:4,team);
            d.image({art::shore_troops},{pose*24,role*27,24,27},
                    {at.x-sprite_w/2,at.y-sprite_h+2,sprite_w,sprite_h},true,i>=LandFriends);
            const int hp_y=at.y-sprite_h;
            d.fill({at.x-7,hp_y,14,3},ink);
            d.fill({at.x-6,hp_y+1,12*unit.hp/land_max_hp(i),1},team);
            if (elapsed<150 && before.hp>unit.hp) d.diamond({at.x+6,at.y-8},3,gold);
        }
        d.clip=old_clip;
        d.frame(land_area,border);
        const auto unit=fight.units[land_selected];
        constexpr Text crew[]{"队长","矛手","水手"};
        const Text movement=unit.orders&LandMoved?"已移动":"可移动";
        const Text attack=unit.orders&LandBraced?"已架盾":unit.orders&LandStruck?"已攻击":"可攻击";
        const auto status=format("%s %d/%d · %s · %s",crew[land_selected].data(),int(unit.hp),
                                 land_max_hp(land_selected),movement.data(),attack.data());
        d.label(safe.x+7,land_area.bottom()+4,status,ivory,safe.w-14);
        const int width=(safe.w-16)/3, bottom=safe.bottom()-26;
        button(d,{safe.x+5,bottom,width,23},"架盾",LandBrace,false,(unit.orders&LandStruck)!=0);
        button(d,{safe.x+8+width,bottom,width,23},"结束回合",LandEndRound,true);
        button(d,{safe.x+11+2*width,bottom,width,23},land_retreat?"确认撤退":"撤退",LandRetreat,land_retreat);
    }

    void land_result(LandFight before,int striker=-1,bool enemy_turn=false) {
        land_before=before;
        land_striker=striker;
        land_frame_start=now;
        land_enemy_turn=enemy_turn;
        land_retreat=false;
        if (!game.s.land_fight.kind) {
            screen=Office;
            if (before.kind==RoadEscort) set_modal(None);
            else {
                enclave_item=before.site;
                enclave_tab=3;
                set_modal(EnclavePage);
            }
        }
        changed();
    }

    void land_action(int action) {
        const auto before=game.s.land_fight;
        if (action==LandRetreat) {
            if (land_retreat) { game.withdraw_land_battle(); land_result(before); }
            else land_retreat=true;
            return;
        }
        land_retreat=false;
        if (action==LandBrace) {
            if (game.brace_land_unit(land_selected)) land_result(before);
        } else if (action==LandEndRound) {
            if (game.end_land_round()) land_result(before,-1,true);
        } else if (action>=LandCellAction && action<LandCellAction+LandCells) {
            const int cell=action-LandCellAction, unit=land_occupant(before,cell);
            if (unit>=0 && unit<LandFriends) land_selected=unit;
            else if (unit>=LandFriends) {
                if (game.strike_land_unit(land_selected,unit)) {
                    audio.cue(Sound::Steel);
                    land_result(before,land_selected);
                }
            } else if (game.move_land_unit(land_selected,cell)) land_result(before);
        }
    }

    void render(Draw& d,SailingChrome& chrome) {
        layout(d);
        drawn_map=map_view();
        button_count=0;
        if (drawn_map!=MapView::None && !game.land_away()) {
            if (chrome.bounds!=safe || chrome.mode!=drawn_map) {
                chrome={};
                chrome.bounds=safe;
                chrome.mode=drawn_map;
                d.fill(d.clip,ink);
            }
            sailing_header(d,chrome);
            if (drawn_map==MapView::Pilot) pilot_chart(d,&chrome);
            else if (drawn_map==MapView::Helm) chart_full_helm(d,&chrome);
            else {
                chart(d,&chrome);
                chart_footer(d,chrome);
            }
            focused=std::clamp(focused,0,std::max(0,button_count-1));
            return;
        }
        chrome={};
        if (!title && modal==WorldTrail) {
            field_trail(d);
            focused=std::clamp(focused,0,std::max(0,button_count-1));
            return;
        }
        if (!title && modal==None && screen==Chart && game.s.skirmish.active) {
            skirmish_view(d);
            focused=std::clamp(focused,0,std::max(0,button_count-1));
            return;
        }
        if (!title || modal==AudioSettings) d.fill(d.clip,ink);
        if (!title && game.s.land_fight.kind) {
            land_screen(d);
            focused=std::clamp(focused,0,std::max(0,button_count-1));
            return;
        }
        if (modal==HarborTown) {
            town_screen(d);
            focused=std::clamp(focused,0,std::max(0,button_count-1));
            return;
        }
        if (modal==PortStoryPage) {
            port_story_page(d);
            focused=std::clamp(focused,0,std::max(0,button_count-1));
            return;
        }
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
        case ForecastPage: weather_forecast(d); break;
        case RouteStops: route_stops(d); break;
        case PortPrices: port_prices(d); break;
        case TradePassagePage: trade_passage(d); break;
        case MarketNews: market_news(d); break;
        case EventReport: event_report(d); break;
        case TowRequest: tow_request(d); break;
        case PortDesk: port_desk(d); break;
        case DispatchBoard: dispatch_board(d); break;
        case EscortPage: escort_page(d); break;
        case DuelInvitation: duel_invitation(d); break;
        case ProvisionsPage: provisions_page(d); break;
        case RationsPage: rations_page(d); break;
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
        case TradeQuantityPage: trade_quantity_page(d); break;
        case HaggleOutcomePage: haggle_outcome_page(d); break;
        case TimberTrailPage: timber_trail_page(d); break;
        case LandPage: land_page(d); break;
        case WorldCatalog: world_catalog(d); break;
        case WorldStudy: world_study(d); break;
        case WorldTrail: break;
        case HarborBoatPage: harbor_boat_page(d); break;
        case NatureWatch: nature_index(d,true); break;
        case NatureAlbum: nature_index(d,false); break;
        case NaturePage: nature_page(d); break;
        case CharterDesk: charter_desk(d); break;
        case CharterRegistry: charter_registry(d); break;
        case ReserveOrder: reserve_order(d); break;
        case ReserveSale: reserve_sale(d); break;
        case PortCasePage: port_case(d); break;
        case FlagRack: flag_rack(d); break;
        case FlagPage: flag_page(d); break;
        case ShorePage: shore_page(d); break;
        case ShoreJournal: shore_journal(d); break;
        case FishBook: fish_book(d); break;
        case EnclaveRegistry: enclave_registry(d); break;
        case EnclavePage: enclave_page(d); break;
        case MerchantRaidOffer: merchant_raid_offer(d); break;
        case PilotBook: pilot_book(d); break;
        case PilotStory: pilot_story(d); break;
        case PilotParcelPage: pilot_parcel_page(d); break;
        case SpecialtyInfo: specialty_info(d); break;
        case LandBriefing: land_briefing(d); break;
        case DriftStory: drift_story(d); break;
        case DriftRecovery: drift_recovery(d); break;
        case MerchantBoard: merchant_board(d); break;
        case HarborTown: case PortStoryPage: break;
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
        if (game.land_away()) {
            const auto reserve=game.land_cost(ReturnToCity);
            d.center({safe.x,safe.bottom()-27,safe.w,22},
                     format("返程预留 粮%d 水%d",reserve.food,reserve.water),mist);
        }
        else if (!full_helm_chart()) {
            if (!game.encounter() || modal!=None) footer(d);
            else if (!game.s.helm || game.s.weather!=Privateer)
                d.center({safe.x,safe.bottom()-30,safe.w,25},"选择如何应对",mist);
        }
        focused=std::clamp(focused,0,std::max(0,button_count-1));
    }

    void select(int p) {
        if (!game.sea()) voyage_rate=1;
        if (!game.charted(p)) { game.notice="还不知道确切泊地"; notify(); return; }
        inside_town_facility=false;
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
        if (!title && game.s.land_fight.kind) {
            land_action(action);
            return;
        }
        if (action>=WorldStudyEntry && action<WorldStudyEntry+FieldStudyCount) {
            if (modal==WorldCatalog) {
                field_item=action-WorldStudyEntry;
                field_local=field_result=false;
                field_plate=0;
                set_modal(WorldStudy);
            }
            return;
        }
        if (action>=FieldPlate && action<FieldPlate+3) {
            if (modal==WorldStudy) field_plate=action-FieldPlate;
            return;
        }
        if (action>=QuantityDigit && action<QuantityDigit+10) {
            if (modal==TradeQuantityPage && trade_quantity<100)
                trade_quantity=trade_quantity*10+action-QuantityDigit;
            return;
        }
        if (action>=DispatchEntry && action<DispatchEntry+DispatchCount) {
            const int id=action-DispatchEntry;
            if (modal==DispatchBoard && !game.s.dispatch.number && game.dispatch_offer_here(id)) {
                dispatch_page=id;
                focused=0;
            }
            return;
        }
        if (action>=ReadPortStory && action<ReadPortStory+int(PortStoryCount)) {
            const int id=action-ReadPortStory;
            if (modal==TaleList && game.port_story_stage(id)!=PortStoryUnknown) {
                port_story_id=id;
                port_story_back=TaleList;
                set_modal(PortStoryPage);
            }
            return;
        }
        if (action>=RouteFilter && action<RouteFilter+5) {
            if (modal==Routes) {
                route_filter=action-RouteFilter;
                page=0;
                focused=4+route_filter;
                dirty=true;
            }
            return;
        }
        if (action>=WalkToTownFacility && action<WalkToTownFacility+int(TownFacilityCount)) {
            const int id=action-WalkToTownFacility;
            if (modal==HarborTown) town_go(Towns[town_id].doors[id],id);
            return;
        }
        if (action>=NatureSelect && action<NatureSelect+int(NatureCount)) {
            if (modal==NatureWatch || modal==NatureAlbum) {
                nature_item=action-NatureSelect;
                nature_page_back=modal;
                set_modal(NaturePage);
            }
            return;
        }
        if (action>=CharterEntry && action<PurchaseReserve) {
            if (modal==CharterRegistry && game.charted(HarborCharters[action-CharterEntry].port)) {
                charter_item=action-CharterEntry;
                focused=0;
            }
            return;
        }
        if (action>=PurchaseReserve && action<PurchaseReserve+int(std::size(HullNames))) {
            if (modal==ReserveOrder) {
                if (game.purchase_reserve(action-PurchaseReserve)) {
                    changed();
                    set_modal(CharterDesk);
                } else notify();
            }
            return;
        }
        if (action>=FlagEntry && action<ShoreEntry) {
            if (modal==FlagRack) { flag_item=action-FlagEntry; set_modal(FlagPage); }
            return;
        }
        if (action>=ShoreEntry && action<ShoreEntry+int(ShoreTaskCount)) {
            if (modal==ShoreJournal) {
                shore_item=action-ShoreEntry;
                shore_back=ShoreJournal;
                set_modal(ShorePage);
            }
            return;
        }
        if (action>=EnclaveTab && action<EnclaveEntry) {
            enclave_tab=action-EnclaveTab;
            return;
        }
        if (action>=EnclaveEntry && action<EnclaveConstruction) {
            const int site=action-EnclaveEntry;
            if (game.charted(EnclaveSites[site].home)) {
                enclave_item=site;
                enclave_tab=0;
                set_modal(EnclavePage);
            } else {
                game.notice="先打听这片海岸的泊地";
                notify();
            }
            return;
        }
        if (action>=EnclaveConstruction && action<EnclaveSupply) {
            if (game.build_enclave(enclave_item,action-EnclaveConstruction)) changed();
            else notify();
            return;
        }
        if (action>=EnclaveSupply && action<EnclaveSupply+4) {
            if (game.supply_enclave(enclave_item,action-EnclaveSupply)) changed();
            else notify();
            return;
        }
        if (modal==PilotBook && action>=PilotSiteEntry && action<PilotSiteEntry+PilotSiteEnd) {
            pilot_site=action-PilotSiteEntry;
            pilot_story_back=PilotBook;
            set_modal(PilotStory);
            return;
        }
        if (action>=OpenLanguages) {
            switch (action) {
            case ToggleSpecialties:
                good=good>=GoodsCount?int(Wine):int(MadeiraSugar);
                page=0;
                break;
            case ShowSpecialty:
                if (good>=GoodsCount) set_modal(SpecialtyInfo);
                break;
            case OpenLandBriefing:
                if (game.port() && game.s.origin==lisbon) set_modal(LandBriefing);
                break;
            case StartRoadEscort:
                if (modal==LandBriefing && game.begin_land_battle(RoadEscort)) show_land_battle();
                break;
            case DefendEnclave:
                if (modal==EnclavePage && game.begin_land_battle(WharfDefense,enclave_item)) show_land_battle();
                else notify();
                break;
            case EnterPilot:
                if (pilot_pending>=0 || game.s.pilot.region || game.encounter()) break;
                if (game.s.fishery.target) { game.notice="先收回渔具，再换领航图"; notify(); break; }
                if (!game.s.helm && !(game.sea()?game.take_over_helm():game.start_helm(-1))) { notify(); break; }
                pilot_pending=game.pilot_region();
                previous_helm={game.s.helm_x,game.s.helm_y};
                game.paused=true;
                screen=Chart;
                set_modal(None);
                game.notice="正在展开近岸领航图";
                notify();
                break;
            case LeavePilot:
                if (pilot_pending>=0) break;
                if (game.leave_pilotage(chart_water)) {
                    pilot_rate=1;
                    previous_helm={game.s.helm_x,game.s.helm_y};
                    open_helm_view=false;
                    follow=true;
                    center_at(game.position());
                    last_day=now;
                    changed();
                } else notify();
                break;
            case PilotZoomIn: case PilotZoomOut:
                pilot_zoom=std::clamp(pilot_zoom+(action==PilotZoomIn?1.f:-1.f),0.f,6.f);
                break;
            case PilotCenter: case PilotCenterShip:
                pilot_follow=true;
                break;
            case PilotSpeed: cycle_pilot_rate(); break;
            case PilotCoastHint:
                if (modal==PilotStory) {
                    const int nearby=game.pilot_coastal_hint();
                    if (game.record_pilot_coast(nearby)) {
                        pilot_guide=nearby;
                        pilot_site=nearby;
                        changed();
                    }
                }
                break;
            case PilotMoor:
                if (game.begin_pilot_docking(pilot_water(),pilot_target())) {
                    pilot_rate=1;
                    if (!game.sea()) {
                        audio.cue(Sound::Anchor);
                        select(game.s.origin);
                        open_helm_view=false;
                    } else if (game.s.pilot.moored) {
                        audio.cue(Sound::Anchor);
                        pilot_site=game.s.pilot.moored-1;
                        pilot_story_back=None;
                        set_modal(PilotStory);
                    } else {
                        last_day=now;
                        audio.cue(Sound::Trim);
                    }
                    changed();
                }
                break;
            case PilotUnmoor: game.cast_off_pilotage(); changed(); break;
            case PilotWaterBuy: case PilotFoodBuy:
                if (game.pilot_supplies(action==PilotWaterBuy)) changed();
                break;
            case PilotBookOpen:
                page=0;
                if (game.s.pilot.region) {
                    game.cancel_pilot_docking();
                    game.paused=true;
                    set_modal(PilotBook);
                }
                break;
            case PilotStoryOpen:
                if (game.s.pilot.moored) pilot_site=game.s.pilot.moored-1;
                if (pilot_site>=0 && game.s.pilot.region) {
                    pilot_story_back=None;
                    game.paused=true;
                    set_modal(PilotStory);
                }
                break;
            case PilotGuide: case PilotAim:
                if (pilot_site>=0 && game.s.pilot.region) {
                    pilot_guide=pilot_site;
                    if (action==PilotAim && !game.s.pilot.moored) {
                        const auto approach=game.pilot_approach(pilot_site,pilot_water());
                        if (approach.clear) {
                            game.set_pilot_heading(approach.bearing);
                            save_needed=true;
                        }
                    }
                    set_modal(None);
                }
                break;
            case PilotOpenParcel:
                if (game.pilot_parcel_at(pilot_site)) set_modal(PilotParcelPage);
                break;
            case PilotHandleParcel:
                if (modal==PilotParcelPage && game.handle_pilot_parcel()) {
                    pilot_guide=-1;
                    if (game.pilot_parcel_destination()>=0) set_modal(None);
                    changed();
                }
                break;
            case RenderWhaleOil:
                if (modal==FishBook && game.render_whale_oil()) changed();
                break;
            case PrepareMerchantRaid:
                if (const int company=game.raid_target(chart_water); company>=0) {
                    raid_company=company;
                    game.paused=true;
                    save_needed=true;
                    set_modal(MerchantRaidOffer);
                }
                break;
            case ConfirmMerchantRaid:
                if (modal==MerchantRaidOffer && game.begin_merchant_raid(raid_company,chart_water)) {
                    sea_flashes={};
                    skirmish_tick=now;
                    open_helm_view=true;
                    set_modal(None);
                    changed();
                } else notify();
                break;
            case OpenEnclaves: set_modal(EnclaveRegistry); break;
            case DockEnclave: case ShowLocalEnclave:
                if (action==DockEnclave && !game.dock_helm(chart_water)) { notify(); break; }
                if (game.enclave_here()>=0) {
                    if (action==DockEnclave) { select(game.s.origin); changed(); }
                    enclave_item=game.enclave_here();
                    enclave_tab=0;
                    set_modal(EnclavePage);
                }
                break;
            case MarkEnclave:
                select(EnclaveSites[enclave_item].home);
                break;
            case LeaseEnclave:
                if (game.lease_enclave(enclave_item)) changed();
                else notify();
                break;
            case EnclaveWait:
                if (game.enclave_here()==enclave_item) { game.wait_enclave_day(); changed(); }
                break;
            case CarrySettlers:
                if (game.take_enclave_migrants(enclave_item)) changed();
                else notify();
                break;
            case LodgeSettlers:
                if (game.land_enclave_migrants(enclave_item)) changed();
                else notify();
                break;
            case HireVillagers: case HireCarpenter:
                if (game.hire_enclave_people(enclave_item,action==HireCarpenter)) changed();
                else notify();
                break;
            case SellEnclaveGoods:
                if (game.sell_enclave_exports(enclave_item)) changed();
                break;
            case StormEnclave:
                if (game.siege_enclave(enclave_item)) show_land_battle();
                else notify();
                break;
            case ReleaseGuards: case RansomGuards:
                if (game.decide_enclave_prisoners(enclave_item,action==ReleaseGuards?ReleaseEnclavePrisoners:RansomEnclavePrisoners))
                    changed();
                break;
            case ToggleCompany:
                if (modal==CharterDesk && game.change_company()) changed();
                break;
            case OpenFlags:
                if (game.encounter() || game.land_away()) break;
                if (game.sea()) {
                    game.paused=true;
                    voyage_rate=1;
                }
                set_modal(FlagRack);
                break;
            case RaiseColors: if (modal==FlagPage && game.display_colors(flag_item)) changed(); break;
            case SignRoyalPass: if (modal==FlagPage && game.receive_royal_pass()) changed(); break;
            case OpenShoreTask:
                if (game.shore_task_here()>=0) {
                    shore_item=game.shore_task_here();
                    shore_back=modal;
                    set_modal(ShorePage);
                }
                break;
            case AdvanceShoreTask:
                if (modal==ShorePage && shore_back!=ShoreJournal && game.advance_shore_task(shore_item)) {
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case OpenShoreJournal: set_modal(ShoreJournal); break;
            case OpenPortCase:
                if (game.market_held()) {
                    port_case_back=modal;
                    set_modal(PortCasePage);
                }
                break;
            case SettlePortCase:
                if (modal==PortCasePage && game.settle_port_case()) changed();
                break;
            case OpenCharterDesk:
                if (game.port()) set_modal(CharterDesk);
                break;
            case OpenCharterRegistry:
                charter_item=-1;
                set_modal(CharterRegistry);
                break;
            case SignBerth:
                if (modal==CharterDesk && game.open_berth()) changed();
                break;
            case OpenReserveOrder:
                if (game.at_berth() && !game.s.berth.vessel) set_modal(ReserveOrder);
                break;
            case ChangeFlagship:
                if (modal==CharterDesk && game.swap_vessel()) changed();
                break;
            case OpenReserveSale:
                if (game.at_berth() && game.s.berth.vessel) set_modal(ReserveSale);
                break;
            case ConfirmReserveSale:
                if (modal==ReserveSale && game.sell_reserve()) { changed(); set_modal(CharterDesk); }
                break;
            case LocateBerth: if (game.berth_port()>=0) select(game.berth_port()); break;
            case OpenWorldCatalog:
                page=0;
                set_modal(WorldCatalog);
                break;
            case ShowHarborBoat:
                if (modal==HarborTown && !outside && local_vessel(game.s.origin)>=0) set_modal(HarborBoatPage);
                break;
            case OpenWorldStudy:
                if (modal==None && screen==Office && !game.sea() && !game.encounter()) {
                    const int id=field_study_at(game.s.origin);
                    if (id<0) break;
                    field_item=id;
                    field_local=true;
                    field_result=false;
                    field_plate=0;
                    set_modal(WorldStudy);
                }
                break;
            case LandWorldTrail:
                if (modal==WorldStudy && field_local && game.field_requirement(field_item).empty()) {
                    if (!field_result) field_walk=FieldWalk(FieldStudies[field_item].ground);
                    else field_walk.target=field_walk.player;
                    field_tick=now;
                    field_area={};
                    set_modal(WorldTrail);
                }
                break;
            case FieldObserve:
                if (modal==WorldTrail && field_area.w) {
                    const int type=field_subject();
                    if (type>=0 && ((game.s.world_notebook&field_note_bit(field_item,type)) ||
                        game.sketch_field(field_item,type))) {
                        field_plate=type+1;
                        field_result=true;
                        set_modal(WorldStudy);
                        changed();
                    }
                }
                break;
            case FieldStalk:
                if (modal==WorldTrail && field_quarry()>=0) field_walk.stalking=!field_walk.stalking;
                break;
            case FieldShoot:
                if (modal==WorldTrail && field_area.w && field_quarry()>=0 &&
                    field_walk.can_shoot(FieldStudies[field_item].ground) &&
                    game.hunt_field(field_item)) {
                    field_walk.hunted=true;
                    field_walk.shot_from=field_walk.player;
                    field_walk.shot_to=field_walk.quarry;
                    field_walk.shot_at=now;
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case ShoreFood: case ShoreWater:
                if (modal==None && screen==Office && game.shore_resupply(action==ShoreWater)) changed();
                break;
            case OpenNatureWatch:
                if (game.nature_site()==NoNatureSite) break;
                if (game.s.helm) {
                    game.paused=true;
                    voyage_rate=1;
                }
                set_modal(NatureWatch);
                break;
            case OpenNatureAlbum:
                set_modal(NatureAlbum);
                break;
            case WaitForNature:
                if (modal==NaturePage && nature_page_back==NatureWatch && game.wait_for_nature())
                    changed();
                break;
            case SketchNature:
                if (modal==NaturePage && nature_page_back==NatureWatch && game.record_nature(nature_item)) {
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case OpenLanguages:
                show_languages(modal==RumorPage?port_language(Rumors[rumor_item].port):
                               modal==ExpeditionPage?BookLanguage[expedition_item]:LanguageCount);
                break;
            case SelectPortLanguage: lesson_language=port_language(game.s.origin); break;
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
            case OpenTradeQuantity:
                if (game.port() && screen==Market && modal==None) {
                    trade_quantity=0;
                    quantity_sell=!game.trade_limit(good,false) && game.trade_limit(good,true)>0;
                    quantity_back=None;
                    set_modal(TradeQuantityPage);
                }
                break;
            case QuantityBuy: case QuantitySell:
                if (modal==TradeQuantityPage) quantity_sell=action==QuantitySell;
                break;
            case QuantityBackspace:
                if (modal==TradeQuantityPage) trade_quantity/=10;
                break;
            case QuantityMaximum:
                if (modal==TradeQuantityPage) trade_quantity=game.trade_limit(good,quantity_sell);
                break;
            case QuantityConfirm:
                if (modal==TradeQuantityPage && trade_quantity>0 &&
                    game.trade_quote(good,quantity_sell,trade_quantity).units==trade_quantity &&
                    market_order(quantity_sell,trade_quantity)) set_modal(quantity_back);
                break;
            case ShowBargain:
                if (game.port()) {
                    bargain_sell=false;
                    bargain_rate=8;
                    bargain_counter=0;
                    set_modal(BargainPage);
                }
                break;
            case HaggleBuy: case HaggleSell:
                if (bargain_sell!=(action==HaggleSell)) {
                    bargain_sell=action==HaggleSell;
                    bargain_counter=0;
                }
                break;
            case HaggleLess: case HaggleMore: {
                const int direction=action==HaggleMore?2:-2;
                bargain_rate=std::clamp(bargain_rate+(bargain_sell?direction:-direction),2,MaxHaggleRate);
                break;
            }
            case HaggleMakeOffer: submit_bargain(bargain_rate); break;
            case HaggleAccept: submit_bargain(bargain_counter); break;
            case ShowDriftStory: activity_back=modal; set_modal(DriftStory); break;
            case TakeDriftLead: if (game.take_drift_lead()) changed(); break;
            case MarkDriftGround:
                inside_town_facility=false;
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
            case OpenTown: case ReturnTown: show_town(); break;
            case WalkToCityGate:
                if (modal==HarborTown && !outside && game.s.origin==lisbon)
                    town_go(LisbonGate,CityGateTarget);
                break;
            case WalkToOutsideGate: case WalkToRiver: case WalkToShed: case WalkToPostRoad:
                if (modal==HarborTown && outside) {
                    const int id=action-WalkToOutsideGate;
                    town_go(OutskirtsSites[id],OutsideGateTarget+id);
                }
                break;
            case WalkRegionalTrail:
                if (modal==HarborTown && !outside && regional_survey_at(game.s.origin)>=0)
                    town_go(Towns[town_id].field_gate,RegionalTrailTarget);
                break;
            case OpenFieldSurvey:
                if (game.port() && regional_survey_at(game.s.origin)>=OriginalRegionalSurveys &&
                    (modal==LandPage || (modal==None && screen==Office))) {
                    if (modal!=LandPage) land_return=None;
                    inland_archive=game.land_stage()>=LandNotes;
                    set_modal(LandPage);
                }
                break;
            case WaitLandWeather:
                if (modal==LandPage && !inland_archive && game.wait_land_weather()) changed();
                break;
            case WaitLandTide:
                if (modal==LandPage && game.wait_land_tide()) changed();
                break;
            case OpenLandReport:
                if (modal==ExpeditionPage && game.archive_here()==expedition_item && game.land_stage()>=LandNotes) {
                    inland_archive=true;
                    land_return=ExpeditionPage;
                    set_modal(LandPage);
                }
                break;
            case WalkLandStream: case WalkLandPass: case WalkLandCopy: case WalkLandLookout: case WalkLandBack:
                if (modal==LandPage && !inland_archive) {
                    if (game.explore_land(static_cast<LandAction>(action-WalkLandStream))) {
                        if (action==WalkLandBack) {
                            if (regional_survey_at(game.s.origin)>=0) {
                                if (land_return==HarborTown && town_index(game.s.origin)>=0) {
                                    show_town();
                                    town.at=Towns[town_id].field_gate;
                                } else {
                                    screen=Office;
                                    set_modal(None);
                                }
                            } else if (!outside) {
                                show_town();
                                change_outskirts(true);
                                town.at=OutskirtsSites.back();
                            }
                            else set_modal(HarborTown);
                        }
                        focused=0;
                        audio.cue(Sound::Page);
                        changed();
                    } else notify();
                }
                break;
            case FileLandReport:
                if (modal==LandPage && inland_archive && game.file_land_report()) {
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case ReturnTimber: case TakeTimberReward:
                if (modal==TimberTrailPage && !at_timber_river && game.resolve_timber_trail(action==ReturnTimber)) {
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case FindPortStory: {
                const int id=port_story_at(game.s.origin);
                if (modal==HarborTown && id>=0 && (game.port_story_stage(id)<PortStoryFirst ||
                    game.dispatch_offer_here(PortStories[id].dispatch)))
                    town_go(Towns[town_id].doors[PortStoryDoors[id]],TownStoryTarget);
                break;
            }
            case PortStoryChooseFirst: case PortStoryChooseSecond:
                if (modal==PortStoryPage && game.resolve_port_story(port_story_id,action-PortStoryChooseFirst)) {
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case RetryStoryDispatch:
                if (modal==PortStoryPage && game.accept_dispatch(PortStories[port_story_id].dispatch)) {
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case PortStoryGuide:
                if (modal==PortStoryPage && port_story_id==TornChart &&
                    game.port_story_stage(port_story_id)>=PortStoryFirst) {
                    rumor_item=RibeiraRumor;
                    activate(MarkRumor);
                }
                break;
            case OpenDispatch:
                dispatch_back=modal;
                dispatch_page=DispatchList;
                set_modal(DispatchBoard);
                break;
            case InspectDispatchStop: case SealDispatchStop:
                if (modal==DispatchBoard && game.choose_dispatch_stop(action==InspectDispatchStop)) {
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case ViewDispatchReceipt:
                dispatch_page=DispatchReceipt;
                focused=0;
                break;
            case AcceptDispatch:
                if (modal==DispatchBoard && game.accept_dispatch(dispatch_page)) {
                    dispatch_page=DispatchList;
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case FileDispatch:
                if (modal==DispatchBoard && game.deliver_dispatch()) {
                    dispatch_page=DispatchReceipt;
                    audio.cue(Sound::Page);
                    changed();
                }
                break;
            case MarkDispatch:
                if (game.port() && game.s.dispatch.number) select(game.dispatch_target());
                break;
            case AskCancelDispatch:
                dispatch_page=DispatchConfirmCancel;
                focused=0;
                break;
            case CancelDispatch:
                if (dispatch_page==DispatchConfirmCancel && game.cancel_dispatch()) {
                    dispatch_page=DispatchReceipt;
                    focused=0;
                    changed();
                }
                break;
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
            const int port=action-Destination;
            if (modal==PortPrices) {
                if (game.port() && is_port(port) && port!=game.s.origin && game.charted(port)) {
                    trade_port=port;
                    set_modal(TradePassagePage);
                }
            } else if (!game.sea()) select(port);
            return;
        }
        if (action>=GoodsRow && action<GoodsRow+int(MarketGoodsCount)) { good=action-GoodsRow; return; }
        if (action>=DockService) { game.service(action-DockService); changed(); return; }
        if (action>=OpenChart && action<=OpenJournal) {
            inside_town_facility=false;
            screen=Screen(action-OpenChart);
            page=screen==Market?market_selection_page():0;
            set_modal(None);
            return;
        }
        switch (action) {
        case Begin:
            if (!loaded) break;
            title=false;
            if (!had_save) { selected=lisbon; set_modal(StoryPage); }
            else if (game.land_away()) {
                screen=Office;
                inland_archive=false;
                const int region=regional_survey_at(game.s.origin);
                land_return=region<OriginalRegionalSurveys?HarborTown:None;
                set_modal(LandPage);
            }
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
                inside_town_facility=false;
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
                inside_town_facility=false;
                if (scout_course) {
                    const auto here=game.position();
                    game.s.heading=direction_to({scout_course->x-here.x,scout_course->y-here.y});
                }
                scout_course.reset();
                screen=Chart; set_modal(None); follow=true; last_day=now;
                voyage_rate=1;
                previous_helm={game.s.helm_x,game.s.helm_y};
                audio.cue(Sound::CastOff);
                changed();
            } else notify();
            break;
        case Close: back(); break;
        case ZoomIn: set_zoom(zoom+1); break;
        case WorldChart:
            set_modal(None);
            follow=false;
            set_zoom(0);
            break;
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
                voyage_rate=1;
                set_modal(None);
                follow=true;
                last_day=now;
                audio.cue(Sound::CastOff);
                changed();
            } else notify();
            break;
        case ShowPreflight: set_modal(Preflight); break;
        case ShowWeather:
            if (modal==Preflight || (modal==None && game.sea() && !game.encounter()))
                set_modal(ForecastPage);
            break;
        case ResumeWeather:
            if (modal==ForecastPage && game.sea()) {
                resume_after_voyage_info=true;
                set_modal(None);
            }
            break;
        case ShowRations:
            if (modal==Preflight || modal==ForecastPage || modal==ProvisionsPage ||
                (modal==None && game.sea() && !game.encounter())) {
                rations_back=modal;
                set_modal(RationsPage);
            }
            break;
        case ToggleFoodRation: case ToggleWaterRation:
            if (modal==RationsPage) {
                game.toggle_ration(action==ToggleFoodRation?SaveFood:SaveWater);
                save_needed=true;
            }
            break;
        case ShowStops: page=0; set_modal(RouteStops); break;
        case TopUp: {
            const auto forecast=game.forecast(passage(game.s.origin,selected));
            if (game.resupply(game.voyage_supply_target(forecast.provisions),
                              game.voyage_supply_target(forecast.water))) changed();
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
                voyage_rate=1;
                previous_helm={game.s.helm_x,game.s.helm_y};
                audio.cue(Sound::CastOff);
                changed();
            } else notify();
            break;
        case SteerLeft: case SteerRight:
            if (game.s.helm) {
                const int delta=action==SteerLeft?-1:1;
                if (game.s.pilot.region) game.set_pilot_heading(game.s.pilot.heading+delta);
                else game.s.heading=(game.s.heading+delta+8)%8;
                changed();
            }
            break;
        case BeginFishery:
            if (game.begin_fishery(chart_water)) {
                follow=true;
                set_zoom(std::max(zoom,4.f));
                changed();
            } else notify();
            break;
        case WorkFishery:
            if (game.work_fishery()) changed();
            else notify();
            break;
        case CancelFishery:
            if (game.cancel_fishery()) changed();
            break;
        case ShowFishBook:
            if (game.port()) set_modal(FishBook);
            break;
        case SellWhaleOil:
            if (game.sell_whale_oil()) { set_modal(None); changed(); }
            break;
        case BeginSkirmish:
            if (game.begin_skirmish(chart_water)) {
                open_helm_view=true;
                sea_flashes={};
                skirmish_tick=now;
                focused=0;
                changed();
            } else notify();
            break;
        case FirePort: case FireStarboard:
            if (game.fire_skirmish(action==FirePort?-1:1)) {
                push_sea_flash({game.s.helm_x,game.s.helm_y},0,game.s.skirmish.shots[0].heading);
                audio.cue(Sound::Broadside);
                audio.cue(Sound::FireCall);
                save_needed=true;
            }
            break;
        case DropRope:
            if (game.drop_drift_rope()) changed();
            break;
        case BoardSkirmish:
            if (game.board_skirmish()) changed();
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
        case TradePreparePurchase:
            if (modal==TradePassagePage && game.port()) {
                trade_quantity=game.trade_plan(trade_port,good).purchase.units;
                quantity_sell=false;
                if (trade_quantity) {
                    quantity_back=TradePassagePage;
                    set_modal(TradeQuantityPage);
                }
            }
            break;
        case TradeStartPreparation:
            if (modal==TradePassagePage && game.trade_plan(trade_port,good).days) {
                select(trade_port);
                set_modal(Preflight);
            }
            break;
        case ShowMarketNews: page=0; set_modal(MarketNews); break;
        case NextStop:
            game.s.stop_next=1-game.s.stop_next;
            game.notice=game.s.stop_next?"将在下一处靠泊":"继续前往原目的地";
            changed();
            break;
        case CycleSpeed: cycle_voyage_rate(); break;
        case ToggleHelmView:
            if (game.s.helm && !game.encounter()) {
                open_helm_view=!open_helm_view;
                if (open_helm_view) {
                    follow=true;
                    set_zoom(std::max(zoom,4.f));
                    center_at(game.position());
                }
            }
            break;
        case Pause:
            if (game.s.pilot.moored || pilot_pending>=0 || (game.s.pilot.region && !pilot_water().bytes)) break;
            game.paused=!game.paused;
            if (game.paused) {
                game.cancel_pilot_docking();
                save_needed=true;
            }
            previous_helm={game.s.helm_x,game.s.helm_y};
            last_day=now;
            break;
        case Trim:
            game.s.reefed=1-game.s.reefed;
            audio.cue(Sound::Trim);
            save_needed=true;
            break;
        case Retreat:
            game.turn_back(); selected=game.sea()?game.s.destination:game.s.origin;
            last_day=now;
            follow=true;
            changed();
            break;
        case BuyOne: case BuyFive: case SellOne: case SellFive:
            market_order(action>=SellOne,action==BuyFive || action==SellFive?TradeLot:1);
            break;
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
        if (game.s.land_fight.kind) { land_retreat=!land_retreat; dirty=true; return true; }
        if (game.s.skirmish.active && !title) {
            if (!game.paused) activate(Pause);
            return true;
        }
        if (pilot_pending>=0) { pilot_pending=-1; dirty=true; return true; }
        if (pilot_view()) {
            activate(game.pilot_docking()>=0?Pause:LeavePilot);
            return true;
        }
        if (full_helm_chart()) { open_helm_view=false; dirty=true; return true; }
        if (modal==NaturePage) { set_modal(nature_page_back); return true; }
        if (modal==NatureWatch) { set_modal(game.land_away()?LandPage:None); return true; }
        if (game.land_away()) return true;
        if (inside_town_facility && modal==town_facility) {
            show_town();
            return true;
        }
        if (modal!=None) {
            if (modal==CharterRegistry && charter_item>=0) { charter_item=-1; focused=0; dirty=true; }
            else if (modal==CharterRegistry || modal==ReserveOrder || modal==ReserveSale) set_modal(CharterDesk);
            else if (modal==PortCasePage) set_modal(port_case_back);
            else if (modal==FlagPage) set_modal(FlagRack);
            else if (modal==ShorePage) set_modal(shore_back);
            else if (modal==PortStoryPage) set_modal(port_story_back);
            else if (modal==PilotStory) set_modal(pilot_story_back);
            else if (modal==PilotParcelPage) set_modal(PilotStory);
            else if (modal==TimberTrailPage) set_modal(HarborTown);
            else if (modal==LandPage) set_modal(land_return);
            else if (modal==HarborBoatPage) set_modal(HarborTown);
            else if (modal==WorldStudy) set_modal(field_local?None:WorldCatalog);
            else if (modal==WorldTrail) {
                field_result=false;
                field_plate=0;
                set_modal(WorldStudy);
            }
            else if (modal==HarborTown && outside) change_outskirts(false);
            else if (modal==DispatchBoard) {
                if (dispatch_page!=DispatchList) {
                    dispatch_page=DispatchList;
                    focused=0;
                    dirty=true;
                } else set_modal(dispatch_back);
            }
            else if (modal==Atlas && atlas_item) { atlas_item=0; dirty=true; }
            else if (modal==TalePage && screen==Journal) set_modal(TaleList);
            else if (modal==SeaLog && sea_item>=0) { sea_item=-1; focused=0; dirty=true; }
            else if (modal==RationsPage) set_modal(rations_back);
            else if (modal==ForecastPage) set_modal(game.sea()?None:Preflight);
            else if (modal==RouteStops || (modal==Intel && intel_from_preflight)) set_modal(Preflight);
            else if (modal==EscortPage || modal==DuelInvitation || modal==DriftStory) set_modal(activity_back);
            else if (modal==ExpeditionPage && expedition_from_book) set_modal(ExpeditionList);
            else if (modal==OfficerPage) set_modal(Wardroom);
            else if (modal==RumorPage) set_modal(RumorList);
            else if (modal==RumorList) { page=0; set_modal(rumor_back); }
            else if (modal==LanguageDesk) set_modal(language_back);
            else if (modal==TideLedger) set_modal(LanguageDesk);
            else if (modal==MarketNews) { page=0; set_modal(PortPrices); }
            else if (modal==TradePassagePage) set_modal(PortPrices);
            else if (modal==TradeQuantityPage) set_modal(quantity_back);
            else if (modal==PortPrices || modal==SpecialtyInfo) { page=market_selection_page(); set_modal(None); }
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
        if (modal==WorldTrail) {
            if (controller.pressed(controller_a)) activate(FieldObserve);
            if (controller.pressed(controller_x)) activate(FieldShoot);
            if (controller.pressed(controller_y)) activate(FieldStalk);
            if (controller.pressed(controller_b)) back();
            return;
        }
        if (controller.pressed(controller_up)) focus_move(0,-1);
        if (controller.pressed(controller_down)) focus_move(0,1);
        if (controller.pressed(controller_left)) focus_move(-1,0);
        if (controller.pressed(controller_right)) focus_move(1,0);
        if (controller.pressed(controller_a) && button_count) {
            pad_focus=true;
            activate(buttons[focused].action);
        }
        if (controller.pressed(controller_b)) back();
        if (game.s.helm && screen==Chart && modal==None &&
            (!game.encounter() || game.s.skirmish.active)) {
            if (controller.pressed(controller_x)) activate(SteerLeft);
            if (controller.pressed(controller_y)) activate(SteerRight);
        }
        if (controller.pressed(controller_start) && !title && modal==None &&
            screen==Chart && (!game.encounter() || game.s.skirmish.active))
            activate(game.sea() ? Pause : selected!=game.s.origin ? Sail : story_here() ? ReadStory :
                     game.expedition_here()>=0 ? OpenObservation : StartExplore);
    }
    void tap(Point p) {
        for (int i=0;i<button_count;++i) {
            if (buttons[i].rect.contains(p)) { activate(buttons[i].action); return; }
        }
        if (modal==WorldTrail && field_area.contains(p)) {
            const auto tile=field_pointer(p);
            if (field_walkable(FieldStudies[field_item].ground,tile)) field_walk.target=tile;
            dirty=true;
            return;
        }
        if (modal==HarborTown && town_area.contains(p)) {
            if (!outside) {
                if (harbor_vessel_hitbox.contains(p)) { activate(ShowHarborBoat); return; }
                const auto& layout=Towns[town_id];
                int best=12*12, door=-1;
                for (int i=0;i<TownFacilityCount;++i) {
                    if (i==TownArchive && game.archive_here()<0) continue;
                    const Point spot=town_on_screen(layout.doors[i]);
                    const int dx=p.x-spot.x, dy=p.y-spot.y;
                    const int distance=dx*dx+dy*dy;
                    if (distance<best) { best=distance; door=i; }
                }
                if (door>=0) { town_go(layout.doors[door],door); return; }
            }
            const Point world{(p.x-town_area.x)*TownExtent/town_area.w,
                              (p.y-town_area.y)*TownExtent/town_area.h};
            const auto at=outside?town_street(OutskirtsQuay,OutskirtsSites,world):
                                  town_street(Towns[town_id],world);
            if (at) town_go(*at);
            return;
        }
        if (title || modal!=None || screen!=Chart || !map.contains(p)) return;
        if (pilot_view()) {
            pilot_tap(p);
            return;
        }
        if (game.s.skirmish.active) {
            const auto at=skirmish_point({game.s.helm_x,game.s.helm_y});
            if (std::abs(p.x-at.x)+std::abs(p.y-at.y)>=10) {
                game.s.heading=static_cast<std::uint8_t>(direction_to({p.x-at.x,p.y-at.y}));
                save_needed=true;
            }
            return;
        }
        if (game.encounter()) return;
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
    void reset_pointers() {
        pointer_capture.reset();
        second_pointer.reset();
        pinch.reset();
        pointer_map=MapView::None;
    }

    void begin_pinch(Point second) {
        const float dx=(last_x+second.x)*.5f-(map.x+map.w/2);
        const float dy=(last_y+second.y)*.5f-(map.y+map.h/2);
        const float span=std::max(8.f,std::hypot(float(last_x-second.x),float(last_y-second.y)));
        if (pointer_map==MapView::Pilot)
            pinch=MapPinch{second,span,pilot_pixels_per_mile,pilot_x+dx/pilot_scale_x,pilot_y+dy/pilot_scale_y};
        else pinch=MapPinch{second,span,factor,chart_x+dx/factor,chart_y+dy/factor};
    }

    void move_pinch() {
        const auto& grip=*pinch;
        const float span=std::max(8.f,std::hypot(float(last_x-grip.second.x),float(last_y-grip.second.y)));
        const float target_scale=grip.scale*span/grip.span;
        const float dx=(last_x+grip.second.x)*.5f-(map.x+map.w/2);
        const float dy=(last_y+grip.second.y)*.5f-(map.y+map.h/2);
        if (pointer_map==MapView::Pilot) {
            const auto scales=pilot_scales();
            pilot_zoom=zoom_at_scale(scales,target_scale);
            const float new_scale=scale_at_zoom(scales,pilot_zoom);
            const float vertical=new_scale*60.f/(ChartLatitudeScale*PilotAtlasUnit);
            pilot_y=std::clamp(grip.anchor_y-dy/vertical,0.f,float(PilotAtlasHeight-1));
            pilot_x=std::clamp(grip.anchor_x-dx/pilot_longitude_scale(pilot_y,new_scale),0.f,float(PilotAtlasWidth-1));
            pilot_follow=false;
        } else {
            const auto scales=chart_scales();
            zoom=zoom_at_scale(scales,target_scale);
            const float new_scale=scale_at_zoom(scales,zoom);
            chart_x=std::clamp(grip.anchor_x-dx/new_scale,0.f,float(ChartWidth));
            chart_y=std::clamp(grip.anchor_y-dy/new_scale,0.f,float(ChartHeight));
            follow=false;
        }
        dirty=true;
    }

    void pointer(const pxa::ui::CanvasPointer& e) {
        using pxa::input::PointerPhase;
        const auto device_pixel=[&](int logical) {
            return pxa::ui::canvas_to_surface_coordinate(logical,input_display);
        };
        const Point p{(device_pixel(e.x)-surface_x)/scale,
                      (device_pixel(e.y)-surface_y)/scale};
        const bool same_map=pointer_map!=MapView::None && pointer_map==drawn_map && pointer_map==map_view();
        if (pinch) {
            const bool primary=pointer_capture.owns(e);
            auto& touch=primary?pointer_capture:second_pointer;
            if (!touch.owns(e)) return;
            // Both contacts belong to this gesture until both are lifted or cancelled.
            if (e.ended()) {
                touch.release(e);
                if (!pointer_capture.active() && !second_pointer.active()) reset_pointers();
            } else if (e.action()==PointerPhase::move && same_map &&
                       pointer_capture.active() && second_pointer.active()) {
                if (primary) { last_x=p.x; last_y=p.y; }
                else pinch->second=p;
                move_pinch();
            }
            return;
        }
        if (e.action()==PointerPhase::down) {
            if (pointer_capture.active() && same_map && !pointer_capture.owns(e) &&
                map.contains({last_x,last_y}) && map.contains(p) && second_pointer.try_capture(e)) {
                begin_pinch(p);
                return;
            }
            if (!pointer_capture.try_capture(e)) return;
            pad_focus=false;
            press_x=last_x=p.x; press_y=last_y=p.y;
            pointer_map=drawn_map==map_view() && map.contains(p)?drawn_map:MapView::None;
            panning=false;
            dirty=true;
        } else if (pointer_capture.owns(e)) {
            if (e.action()==PointerPhase::move && same_map) {
                const bool dragging=panning;
                panning |= std::abs(p.x-press_x)+std::abs(p.y-press_y)>5;
                if (panning) {
                    const int dx=p.x-(dragging?last_x:press_x), dy=p.y-(dragging?last_y:press_y);
                    if (pointer_map==MapView::Pilot) {
                        pilot_x-=dx/pilot_scale_x;
                        pilot_y-=dy/pilot_scale_y;
                        pilot_follow=false;
                    } else {
                        chart_x-=dx/factor;
                        chart_y-=dy/factor;
                        follow=false;
                    }
                    dirty=true;
                }
                last_x=p.x; last_y=p.y;
            } else if (e.action()==PointerPhase::move) {
                last_x=p.x; last_y=p.y;
                dirty=true;
            }
            if (e.ended()) {
                const bool can_tap=pointer_map==MapView::None || same_map;
                reset_pointers();
                dirty=true;
                if (e.action()==PointerPhase::up && can_tap && !panning &&
                    std::abs(p.x-press_x)+std::abs(p.y-press_y)<14) tap(p);
            }
        }
    }
};

} // namespace
} // namespace tide

PXA_GAME(tide::VoyageApp)
