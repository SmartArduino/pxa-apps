#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/game_upload.hpp>
#include <pxa/ui_display.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include "battle.hpp"
#include "save.hpp"
#include "art.hpp"
#include "sans_text.hpp"
#include "history.hpp"
#include "game_audio.hpp"

namespace han {

constexpr int sceneWidth = 384, sceneHeight = 264;
constexpr uint16_t rgb(int r, int g, int b) {
    return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
constexpr uint16_t ink = rgb(13,20,22), panel = rgb(25,36,35), edge = rgb(104,91,64);
constexpr uint16_t ivory = rgb(241,226,188), dim = rgb(175,170,146), gold = rgb(244,196,111);
constexpr uint16_t mint = rgb(128,222,180), coral = rgb(251,142,101);
constexpr uint16_t seaRoute = rgb(103,161,180), seaRouteActive = rgb(161,218,231);
constexpr uint16_t colors[] = {mint, rgb(151,180,243), coral, rgb(221,203,153),
    rgb(205,181,241), rgb(243,196,137), rgb(240,228,168), rgb(163,218,218)};

struct Rect {
    int x,y,w,h;
    int cx() const {return x + w/2;}
    int cy() const {return y + h/2;}
    Rect shiftY(int dy) const {return {x,y+dy,w,h};}
    Rect inset(int dx,int dy) const {return {x+dx,y+dy,w-2*dx,h-2*dy};}
    bool contains(int u,int v) const {return u>=x && v>=y && u<x+w && v<y+h;}
    bool overlaps(Rect r) const {return x<r.x+r.w && r.x<x+w && y<r.y+r.h && r.y<y+h;}
};
constexpr Rect standardMap{8,42,264,177};
constexpr Rect fullMap{0,0,384,264};
constexpr Rect atlasBack{8,6,80,26}, atlasCity{96,6,189,26}, atlasNames{293,6,83,26};
constexpr Rect atlasMinus{8,229,40,27}, atlasPlus{54,229,40,27};
constexpr Rect cityPanel{279,42,97,177};
constexpr Rect mapResources[]={{8,6,122,28},{138,6,72,28},{216,6,72,28},{295,6,81,28}};
constexpr Rect mapStatus{8,220,368,21};
constexpr Rect fieldCards[]={{8,5,140,55},{153,5,78,55},{236,5,140,55}};
constexpr Rect fieldMapPanel{8,64,368,27};
constexpr Rect fieldTrack{87,69,209,17};
constexpr Rect fieldStatus{8,210,307,25};
constexpr Rect fieldPause{320,210,56,25};
constexpr int hudEdge=3;

enum class Page {home, choose, map, deploy, battle, report, help, ending, replace, council, roster,
                 cityDesk, dispatch, forces, gear, captive, gazetteer, seasonCouncil, regions, search,
                 soundSettings, alliedOffensive, fiefs, chronicle};
enum class Command {resume, newGame, guide, back, side, seat, work, march, season, select,
                    engage, shift, skill, pause, retreat, accept, home, replace, zoom, council,
                    roster, officer, hire, chooseOfficer, treat, ally, market, militia, skipMarch, atlas,
                    chapter, difficulty, cityDesk, survey, forces, chooseSize, approach, send, recall,
                    rosterPage, gear, pickGear, equip, drill, orders, previewOrder, cancelOrder, maneuver, duel, duelMove,
                    captiveChoice, gazetteer, browseCity, choosePlan, advanceSeason, inspectThreat,
                    regions, chooseRegion, locateRegionCity, claimRegion,
                    openSearch, searchOfficer, welcomeTalent, audioMenu, audioToggle, origin,
                    battleRank, rankSupport, clearRank,
                    confirmRetreat, centerMap, mapNames, pickTroop, pickWeapon, moveGeneral, strike,
                    battleView,deployTab,deployLane,pickEngine,cycleEngineLane,fireEngine,rankAssist,
                    prisonCamp,prisonTab,prisonPage,launchOrders,anchorArmy,
                    gearCategory,buyGear,unequip,alliedOffensive,requestAllied,cancelAllied,
                    fiefs,fiefPage,fiefDesk,cityPolicy,chronicle,chronicleStep,chronicleMap,chronicleDeed,fieldView};
struct Control {
    Rect box;
    Command command;
    int data;
    bool enabled;
};
enum UiIcon {coinsIcon,grainIcon,orderIcon,recruitIcon,farmIcon,wallsIcon,marketIcon,
             pacifyIcon,surveyIcon,spearIcon,bowIcon,horseIcon,namesIcon,squareIcon,wedgeIcon,
             wingsIcon,bladeIcon,skillIcon,ordersIcon,pauseIcon,moraleIcon,duelIcon,flankIcon,withdrawIcon,
             shieldIcon,crossbowIcon,maceIcon,routedIcon,lossIcon,captiveIcon,meritIcon,sealIcon,
             musicIcon,drumIcon,voiceIcon,manualIcon,armorIcon,saddleIcon,fanIcon,gazetteerIcon};
static_assert(std::size(art::commandIcons)==gazetteerIcon+1);
static_assert(std::size(art::detailIcons)==gazetteerIcon-shieldIcon+1);
enum class ArmoryQuality {detail,compact,shared};
enum MapProp {cityGate,sinan,armyTent,mapScroll,coastalBoat};
enum BattlePlaque {bronzePlaque,jadePlaque,vermilionPlaque};
enum GaugeTone {jadeGauge=1,redGauge,goldGauge,blueMarchGauge,redMarchGauge};
constexpr UiIcon formationIcons[]={squareIcon,wedgeIcon,wingsIcon};
constexpr UiIcon weaponIcons[]={spearIcon,bladeIcon,bowIcon};
inline constexpr const char* seasons[]={"春","夏","秋","冬"};
inline constexpr const char* rankNames[]={"上路","中路","下路"};
constexpr UiIcon troopIcons[]={spearIcon,bowIcon,horseIcon,shieldIcon,crossbowIcon,armorIcon};
static_assert(std::size(troopIcons)==troopCount);
inline constexpr const char* cityPolicies[]={"手动","民生","守备"};
inline constexpr const char* difficultyNames[]={"入门","常规","群雄"};
inline constexpr const char* difficultyHints[]={
    "开局钱粮更多，前四季可安心备战",
    "前两季备战，敌军会补防和下令",
    "敌军更早出击，攻城粮耗更高",
};
enum class Notice {none, recruited, cultivated, fortified, moved, income, saved, saveError, treaty, supplies, levies,
                    civic, encamped, withdrawn, routeBlocked, allied, goal, prisonNews};

struct App {
    pxa::Context* context = nullptr;
    pxa::ui::DisplayMetrics display{};
    std::optional<pxa::game::Renderer> renderer;
    std::array<int,art::pageCount> slots{};
    bool loadingArt=false;
    Rect mapWindow=standardMap;
    int tallScene=0;
    int smallText=0;
    float systemFontScale=1.f;
    bool showMapNames=false, fieldView=false;
    Page chronicleFrom=Page::map;
    int chronicleCursor=-1, chronicleRead=-1;
    int mapFocusCity=-1;
    std::array<Rect,cityCount> mapLabels{};
    std::array<Rect,cityCount> mapMarkers{};
    std::array<std::pair<int,int>,cityCount> cityCenters{};
    Rect mapCompass{};
    struct CityLabelPlacement {
        Rect offset{};
        float minScale=0;
    };
    std::array<CityLabelPlacement,cityCount> cityLabelPlacements{};
    float labelFontScale=0;
    int labelYear=0;
    pxa::game::DrawBuffer<32768> commands;
    // Leave space for the HUD controls when all city markers are visible.
    std::array<Control,cityCount+20> controls{};
    int count = 0, focus = 0, selected = 2, destination = -1, side = 0;
    BattlePlan deployment;
    int deploymentLane=1;
    bool deploymentKit=false;
    int width = 0, height = 0;
    float scale = 1, ox = 0, oy = 0, tallOy = 0;
    float drawOy = 0;
    bool sansText = false, texelAlpha = false, mirrorQuad = false;
    SansText sans;
    Page page = Page::home, helpFrom = Page::home;
    Page audioFrom=Page::home;
    Page alliedFrom=Page::map;
    GameAudio audio;
    Notice notice = Notice::none;
    Campaign campaign;
    Battle battle;
    Order departure;
    int chosenChapter=0, dispatchLead=-1, defenseLeader=-1, arrivingColumn=-1, gearCursor=0;
    int alliedTarget=-1;
    int fiefsPage=0, deskMapOrigin=-1;
    static constexpr int fiefRows=4;
    GearSlot gearCategory=armsSlot;
    bool personalArt=true;
    Difficulty chosenDifficulty=Difficulty::standard;
    bool stageSiege=false, tacticsOpen=false, retreatPrompt=false;
    int tacticsRank=-1;
    std::optional<Maneuver> proposedOrder;
    static constexpr int fieldPanLimit=Battle::fieldLength-sceneWidth;
    float battleCamera=0;
    int fieldLand=0;
    bool followBattle=true,battleDragging=false;
    bool warmingArt=false, lookaheadDone=false;
    DispatchSize dispatchSize=legion;
    int searchingOfficer=-1;
    int gazetteerCity=0;
    int regionalChoice=0;
    SeasonPlan seasonPlan=SeasonPlan::regular;
    Discovery discovery{};
    Settlement aftermath{};
    PrisonNews prisonNews{};
    int prisonTab=0,prisonIndex=-1;
    Page prisonFrom=Page::map;
    const char* prisonMessage=nullptr;
    float mapCenterX=worldWidth/2.f, mapCenterY=worldHeight/2.f;
    float mapZoom=0;
    int officerSlot=0;
    Page rosterFrom=Page::council;
    std::array<Order,ownerCount> marching{};
    MarchRoute playerRoute{};
    struct MapRoutes {
        MarchRoute preview;
        std::array<std::optional<std::pair<float,float>>,columnCount> camps;
    };
    std::optional<MapRoutes> mapRoutes;
    bool seasonMarch=false, mapDragging=false, dragMoved=false;
    int marchingMs=0;
    float dragX=0, dragY=0;
    static constexpr int marchDuration=2500;
    bool isMarching() const {return seasonMarch || bool(playerRoute);}
    int travelDuration() const {
        return marchDuration+(playerRoute?std::min(3500,900*(playerRoute.legs()-1)):0);
    }
    std::pair<Order,float> playerMarchLeg() const {
        const float progress=std::clamp(float(marchingMs)/travelDuration(),0.f,1.f)*playerRoute.legs();
        const int leg=std::min(playerRoute.legs()-1,int(progress));
        return {playerRoute.leg(leg),progress-leg};
    }

    bool hasSave = false, saving = false, saveQueued = false, damagedSave = false;
    bool ready = false, dirty = true, resize = false, controllerMode = false;
    bool pointerDown = false;
    std::optional<Control> pressedControl;
    int pointerId = -1;
    float pressX=0, pressY=0;
    float pointerX=0, pointerY=0;
    struct MapPinch {
        int id;
        float x,y,distance,scale,worldX,worldY;
    };
    std::optional<MapPinch> pinch;
    struct MapMotion {
        float vx=0, vy=0;
        uint64_t dragAt=0, tickAt=0;
        unsigned remainingUs=0;
    };
    MapMotion mapMotion;
    float mapTapX=0,mapTapY=0;
    uint64_t mapTapTime=0;
    int reportWinner = 0, reportCity = 0, reportLoss = 0;
    bool reportDefense = false, reportRetreat = false, reportSiege = false;
    uint64_t previousTick = 0;
    unsigned clockPeriod = 33;
    unsigned elapsed = 0, drawErrors = 0;
    pxa::input::Controller gamepad;
    uint32_t seenPadShortcuts=0;
    uint64_t repeatPadAt=0;
    static constexpr uint32_t padDirections=pxa::input::controller_up|pxa::input::controller_down|
        pxa::input::controller_left|pxa::input::controller_right;
    static constexpr uint32_t padShortcuts=pxa::input::controller_x|pxa::input::controller_y|
        pxa::input::controller_l|pxa::input::controller_r;

    void info(std::string_view message) { (void)context->log().write(pxa::LogLevel::info,message); }
    void logState(const char* action) {
        char body[160];
        std::snprintf(body,sizeof body,"han-banners %s page=%d turn=%d side=%d cities=%d actions=%d chapter=%d difficulty=%d",
                      action,int(page),campaign.turn,campaign.player,campaign.holdings(campaign.player),
                      campaign.actions,campaign.chapter,int(campaign.difficulty));
        info(body);
    }
    void on_error(pxa::Context&,pxa::Error error) {
        char body[80];
        std::snprintf(body,sizeof body,"han-banners service error %d",int(error));
        info(body);
    }
    void immersive() {
        (void)context->window().request_orientation(pxa::OrientationPreference::landscape);
        (void)context->window().fullscreen(pxa::WindowBarMode::hidden,pxa::WindowBarMode::hidden);
    }
    pxa::Result<void> on_start(pxa::Context& app, std::span<const std::byte> config) {
        context = &app;
        auto metrics = pxa::ui::decode_start_display(config);
        if (!metrics) return std::unexpected(metrics.error());
        display = *metrics;
        immersive();
        return context->tasks().start(initialize());
    }
    pxa::Task<void> initialize() {
        FifteenthSave data{};
        for (const auto& slot:saveSlots) {
            auto result=co_await context->storage().get(slot.key,std::span(data).first(slot.size));
            if (!result && result.error()==pxa::Error::not_found) continue;
            hasSave=result && *result==slot.size && restoreSave(data,slot.size,campaign);
            damagedSave=!hasSave;
            break;
        }
        if (hasSave) logState("restore");
        co_return co_await openRenderer();
    }
    bool nativeWidePanel() const {
        return std::min(display.width,display.height)==480 &&
               std::max(display.width,display.height)==854;
    }
    bool squareLayout() const {
        return width>=360 && width*5>=height*4 && height*4>=width*3;
    }
    void layout() {
        const float fx = float(width)/display.width;
        const float fy = float(height)/display.height;
        float left = fx * std::max(display.safe.left,display.system_bars.left);
        float right = fx * std::max(display.safe.right,display.system_bars.right);
        float top = fy * std::max(display.safe.top,display.system_bars.top);
        float bottom = fy * std::max(display.safe.bottom,display.system_bars.bottom);
        // The ESPP4-A launcher keeps a gesture target even while its bar is hidden.
        if (nativeWidePanel()) bottom=std::max(bottom,24.f*fy);
        float cw = std::max(1.f,width-left-right), ch = std::max(1.f,height-top-bottom);
        scale = std::min(cw/sceneWidth,ch/sceneHeight);
        tallScene=squareLayout()?std::clamp(int(ch/scale)-sceneHeight,0,120):0;
        if (tallScene<24) tallScene=0;
        const float density=float(display.density_q16)/65536.f;
        systemFontScale=std::clamp(float(display.font_scale_q16)/65536.f,.85f,1.5f);
        const bool compactDisplay=density>=1.5f &&
            std::min(cw/fx,ch/fy)/density<=320.f;
        smallText=compactDisplay?int(std::round(9.f*density*fy)):0;
        setMapWindow(fullAtlas()?fullMap:standardMap);
        clampCamera();
        const bool scalableText=scale/fx>=1.45f || compactDisplay || systemFontScale>1.f;
        const bool useSans=scalableText && fontSlots()>=10;
        if (sansText!=useSans || sansText) sans.reset();
        sansText=useSans && sans.init();
        labelFontScale=0;
        ox = std::round(left+(cw-sceneWidth*scale)/2);
        oy = std::round(top+(ch-sceneHeight*scale)/2);
        tallOy = std::round(top+(ch-(sceneHeight+tallScene)*scale)/2);
    }
    pxa::Task<void> openRenderer() {
        ready = false;
        for (;;) {
            resize = false;
            renderer.reset();
            pxa::game::RenderOptions opt;
            const bool large=std::max(display.width,display.height)>480;
            const auto pixels=unsigned(display.width)*display.height;
            const bool largeFrame=pixels>=800u*450u;
            const int divisor=pixels>854u*480u?2:1;
            width=opt.width=std::max(1,int(display.width)/divisor);
            height=opt.height=std::max(1,int(display.height)/divisor);
            opt.buffers = largeFrame?3:2;
            opt.direct_scanout = true;
            // On the 854x480 panel, rasterize while the display composes the previous frame.
            opt.borrow_commands=nativeWidePanel();
            opt.max_draw_bytes = commands.capacity();
            auto surface=co_await context->game().create(opt);
            // Orientation can change while the Host is creating the surface.
            if (resize) continue;
            if (surface) {
                renderer.emplace(std::move(*surface));
            } else {
                auto error=surface.error();
                if (opt.buffers==3 && (error==pxa::Error::resource_limit || error==pxa::Error::unsupported ||
                                       (opt.borrow_commands && error==pxa::Error::invalid_argument))) {
                    opt.buffers=2;
                    opt.borrow_commands=false;
                    auto fallback=co_await context->game().create(opt);
                    if (resize) continue;
                    if (fallback) renderer.emplace(std::move(*fallback));
                    else error=fallback.error();
                }
                if (!renderer) {
                    if (!large || divisor!=1 || error!=pxa::Error::resource_limit) co_return std::unexpected(error);
                    width=opt.width=std::max(1,int(display.width)/2);
                    height=opt.height=std::max(1,int(display.height)/2);
                    auto smaller=co_await context->game().create(opt);
                    if (resize) continue;
                    if (!smaller) co_return std::unexpected(smaller.error());
                    renderer.emplace(std::move(*smaller));
                }
            }
            clockPeriod=largeFrame || squareLayout()?16:33;
            break;
        }
        texelAlpha=renderer->supports(pxa::game::RenderCapability::sprite_texel_alpha);
        mirrorQuad=renderer->supports(pxa::game::RenderCapability::textured_quad) &&
                   renderer->supports(pxa::game::RenderCapability::affine_uv) &&
                   renderer->supports(pxa::game::RenderCapability::painter_polygon);
        auto palette = pxa::game::Upload(*renderer,commands.bytes()).palette(art::palette);
        if (!palette) co_return palette;
        slots.fill(-1);
        detailedArt=width>=640 && height>=360 && renderer->info().max_texture_dimension>=256;
        nativeBattlePictures=true;
        sharpBattlePictures=true;
        nativeMapPictures=true;
        wideRoomPictures=true;
        detailedMenus=true;
        detailedBattleActors=true;
        detailedChoiceHall=true;
        menuIconDetail=true;
        armoryQuality=ArmoryQuality::detail;
        layout();
        char renderSize[112];
        const unsigned dpi=unsigned((uint64_t(display.density_q16)*160+32768)/65536);
        std::snprintf(renderSize,sizeof renderSize,
                      "han-banners render %dx%d on %ux%u dpi=%u sans=%d native-field=%d",
                      width,height,unsigned(display.width),unsigned(display.height),dpi,sansText,nativeFieldArt());
        info(renderSize);
        ready = true;
        dirty = true;
        if (!audio.bank.context) audio.start(*context);
        (void)context->clock().set_period(clockPeriod);
        logState("ready");
        draw();
        co_return pxa::Result<void>{};
    }
    art::Tile titleBackground() const {
        return width*2>=height*3 ? art::coverWide : art::coverSquare;
    }

    bool detailedArt=false, nativeBattlePictures=true, nativeMapPictures=true;
    bool sharpBattlePictures=true,wideRoomPictures=true;
    bool detailedMenus=true, detailedBattleActors=true, detailedChoiceHall=true;
    bool fieldMotionArt=true,zhaoFieldArt=true;
    bool menuIconDetail=true;
    ArmoryQuality armoryQuality=ArmoryQuality::detail;
    static constexpr int portraitSignature(int portrait) {
        constexpr int portraits[]={0,6,7,8,3,1,2,5,9,4,10,17};
        static_assert(std::size(portraits)==std::size(art::signatureLeaders));
        for (int i=0;i<int(std::size(portraits));++i)
            if (portrait==portraits[i]) return i;
        return -1;
    }
    bool zhaoAnimation(int id) const {
        return zhaoFieldArt && id==portraitSignature(officers[0][0].portrait);
    }
    int signatureFor(const Officer* actor) const {
        return personalArt && actor?portraitSignature(actor->portrait):-1;
    }
    bool detailedSharedLeaders() const {
        return sansText && !nativeFieldArt() && !(page==Page::battle && fieldMotionArt);
    }
    const art::Tile* battleGauges() const {return detailedSharedLeaders()?art::hudBars:art::compactHudBars;}
    bool detailedMenuArt() const {return detailedArt && sansText && detailedMenus;}
    art::Tile officerPortrait(int actor) const {
        return detailedMenuArt()?art::detailedPortraits[actor]:art::portraits[actor];
    }
    art::Tile rulerPortrait(int actor) const {
        return detailedArt && sansText?art::rulerPortraits[actor]:art::portraits[actor];
    }
    int detailedTroop(const Lineup& lineup) const {
        if (!detailedArt || !sansText || !detailedBattleActors) return -1;
        int active=-1;
        for (auto troop:lineup) {
            if (troop>=shields) continue;
            if (active>=0 && active!=troop) return -1;
            active=troop;
        }
        return active;
    }
    art::Tile generalFrame(int id,int side,int pose) const {
        if (zhaoAnimation(id)) return art::zhaoYunFrames[side][pose];
        return detailedArt && sansText && detailedBattleActors?art::largeLeaders[id][side][pose]:
            art::signatureLeaders[id][side][pose];
    }
    bool nativeBackdropSize() const {
        return height==480 && (width==800 || width==854);
    }
    bool nativeFieldArt() const {
        return nativeBattlePictures && ((detailedArt && sansText && nativeBackdropSize()) ||
                                        (tallScene && width>=450 && renderer->info().max_texture_dimension>=256));
    }
    bool nativeMapHall() const {
        return detailedArt && sansText && nativeMapPictures && nativeBackdropSize();
    }
    bool useSharpField() const {
        // Reserve room for the opening drum and command voice when both
        // armies also bind a full set of troop animation frames.
        return detailedArt && sansText && sharpBattlePictures && page==Page::battle &&
            (!usesClassicTroops(battle.plan.lineup) || !usesClassicTroops(battle.enemyLineup));
    }
    art::Tile armoryBackdrop() const {
        return armoryQuality==ArmoryQuality::shared?art::warHall:art::armory;
    }
    art::Backdrop backdropTiles(art::Tile image,bool field=false) const {
        if (field && nativeFieldArt()) {
            const auto& fields=useSharpField()?art::sharpFields:art::nativeFields;
            for (const auto& backdrop:fields)
                if (backdrop.compact==image.page) return backdrop;
        }
        if (detailedArt && sansText) {
            if (image.page==art::armory.page && armoryQuality==ArmoryQuality::detail) return art::armoryDetail;
            if (page==Page::map && nativeMapHall() && image.page==art::warHall.page) return art::mapHall;
            if (image.page==art::warHall.page &&
                (page==Page::choose?detailedChoiceHall:
                 page!=Page::map && page!=Page::roster && page!=Page::deploy &&
                 nativeBackdropSize() && wideRoomPictures))
                return art::choiceHall;
            for (const auto& backdrop:art::detailedBackdrops)
                if (backdrop.compact==image.page) return backdrop;
        }
        return {image.page,image.page,1,1,image.w,image.h};
    }

    static constexpr int textureSlotCap=18;
    int textureSlots() const {
        const int supported=renderer->supports(pxa::game::RenderCapability::texture_slots_48)?textureSlotCap:16;
        return std::min(supported,int(renderer->info().max_textures));
    }
    int fontSlots() const {return textureSlots()-SansText::banks;}

    using Pages=std::array<bool,art::pageCount>;
    static bool usesClassicTroops(const Lineup& lineup) {
        return std::any_of(lineup.begin(),lineup.end(),[](Troop troop){return troop<shields;});
    }
    art::Tile battlefield() const {
        return battle.walls>=3?art::siege:art::battlefields[sites[battle.to].terrain];
    }
    static void atlasPath(char (&name)[48],unsigned page) {
        std::snprintf(name,sizeof name,"assets/%x.pxr",page);
    }
    pxa::Task<void> warmArt(Page source,std::array<art::Tile,4> tiles) {
        for (auto tile:tiles) {
            if (page!=source || (source==Page::battle && (!tacticsOpen || tacticsRank>=0 || proposedOrder))) break;
            if (!tile.w || slots[tile.page]>=0) continue;
            char name[48];
            atlasPath(name,tile.page);
            (void)co_await context->assets().prefetch(pxa::AssetKind::texture,name);
        }
        warmingArt=false;
        co_return pxa::Result<void>{};
    }
    void warmNextScene() {
        if (warmingArt || lookaheadDone || !count) return;
        std::array<art::Tile,4> tiles{};
        if (page==Page::deploy) {
            const auto& city=campaign.cities[departure.to];
            const bool ownLegacy=usesClassicTroops(deployment.lineup);
            const int ownDetail=detailedTroop(deployment.lineup);
            tiles={city.fort>=3?art::siege:art::battlefields[sites[departure.to].terrain],
                   sansText?art::detailTroops[1][0][0]:art::troops[1][0][0],
                   ownDetail>=0?art::largeTroops[0][ownDetail][0]:
                       ownLegacy?(sansText?art::detailTroops[0][0][0]:art::troops[0][0][0]):art::Tile{},
                   battleGauges()[0]};
        } else if (page==Page::battle && sansText && tacticsOpen && tacticsRank<0 && !proposedOrder && battle.mayDuel()) {
            tiles={art::siege,art::duelFrames[0][0],art::duelFrames[1][0]};
        } else return;
        const auto backdrop=backdropTiles(tiles[0],page==Page::deploy);
        tiles[0]={backdrop.first,0,0,backdrop.w,backdrop.h};
        lookaheadDone=true;
        warmingArt=true;
        if (!context->tasks().start(warmArt(page,tiles))) warmingArt=false;
    }
    Pages pagesNeeded() const {
        Pages set{};
        if (!sansText)
            for (int i=art::firstFontPage;i<art::firstDetailedPage;++i) set[i]=true;
        auto require=[&](art::Tile image){set[image.page]=true;};
        auto backdrop=[&](art::Tile image,bool field=false) {
            const auto tiles=backdropTiles(image,field);
            for (int i=0;i<tiles.columns*tiles.rows;++i) set[tiles.first+i]=true;
        };
        require(art::plaques[0]);
        if (detailedMenuArt() && menuIconDetail &&
            (page==Page::gear || page==Page::soundSettings || page==Page::cityDesk || page==Page::report))
            require(art::detailIcons[0]);
        auto room=[&](art::Tile background=art::warHall) {
            backdrop(background);
            require(art::windows[0]);
        };
        auto actor=[&](int slot) {
            if (slot>=0) require(officerPortrait(campaign.leader(slot).portrait));
        };
        switch (page) {
            case Page::home:
                backdrop(titleBackground());
                if (detailedMenuArt()) for (auto tile:art::logoTiles) require(tile);
                else require(art::logo);
                break;
            case Page::replace:backdrop(titleBackground());require(art::windows[0]);break;
            case Page::ending:
                backdrop(titleBackground());require(art::windows[0]);
                require(art::outcomes[campaign.won>0?0:1]);break;
            case Page::choose:room();require(rulerPortrait(0));break;
            case Page::roster:
                room();
                for (int i=rosterStart();i<std::min(rosterCount(),rosterStart()+rosterSize);++i)
                    actor(rosterSlot(i));
                break;
            case Page::map:
                if (fullAtlas()) require(art::battlePanels[0]);
                else room();
                if (!fullAtlas() && nativeMapHall()) for (const auto strip:art::mapHallSides) require(strip);
                // Keep the tiles resident while panning across quadrant seams.
                for (int n=0;n<4;++n) require(mapTile(n));
                if (isMarching()) require(art::marchers[0][0][0]);
                break;
            case Page::chronicle:
            case Page::gazetteer:
            case Page::seasonCouncil:
            case Page::alliedOffensive:
            case Page::fiefs:
            case Page::soundSettings:
            case Page::regions:room();break;
            case Page::dispatch:room();actor(dispatchLead);break;
            case Page::cityDesk:room(art::cityCourtyard);actor(campaign.governor(selected));break;
            case Page::search:
                room(art::cityCourtyard);
                actor(searchingOfficer);
                break;
            case Page::deploy: {
                room();
                actor(plannedLeader());
                const int enemy=campaign.cities[battle.defending?departure.from:departure.to].owner;
                const int foe=enemy<3?campaign.enemyOfficer(enemy,departure.to):-1;
                if (foe>=0) require(officerPortrait(officers[enemy][foe].portrait));
                require(art::troops[0][0][0]);
                break;
            }
            case Page::battle:
                if (battle.commander>=0) require(art::battlePortraits[battle.officer().portrait]);
                if (auto* foe=battle.opponent()) require(art::battlePortraits[foe->portrait]);
                require(art::windows[0]);require(battleGauges()[0]);
                for (int side=0;side<2;++side) {
                    const auto* actor=side?battle.opponent():battle.commander>=0?&battle.officer():nullptr;
                    if (const int id=signatureFor(actor);id>=0) require(generalFrame(id,side,0));
                }
                if (battle.duel.visible()) {
                    backdrop(art::siege);
                    require(art::duelFrames[0][0]);require(art::duelFrames[1][0]);
                } else {
                    backdrop(battlefield(),true);
                    if (fieldMotionArt) require(art::battleEffects[0][0]);
                    for (int side=0;side<2;++side) {
                        const auto& lineup=side?battle.enemyLineup:battle.plan.lineup;
                        if (const int troop=detailedTroop(lineup);troop>=0)
                            require(art::largeTroops[side][troop][0]);
                        else if (usesClassicTroops(lineup))
                            require(detailedArt && sansText?art::detailTroops[side][0][0]:art::troops[side][0][0]);
                    }
                }
                break;
            case Page::report:
                require(art::windows[0]);require(art::outcomes[reportWinner>0?0:1]);
                backdrop(reportSiege?art::siege:art::battlefields[sites[reportCity].terrain]);break;
            case Page::gear:
                room(armoryBackdrop());require(art::equipment[0]);require(art::equipment[legacyGearCount]);break;
            case Page::forces:room();require(art::equipment[0]);break;
            case Page::captive:
                room();
                if (const auto* person=prisonOfficer()) require(officerPortrait(person->portrait));
                break;
            case Page::help:
            case Page::council:room();break;
        }
        return set;
    }
    pxa::Task<void> syncArt(Pages needed) {
        std::array<bool,textureSlotCap> occupied{};
        std::array<std::byte,1> blank{};
        pxa::Result<void> status{};
        const auto max=size_t(sansText?fontSlots():textureSlots());
        for (unsigned i=0;i<slots.size();++i) {
            if (slots[i]<0) continue;
            if (needed[i] && size_t(slots[i])<max) {
                occupied[slots[i]]=true;
            } else {
                status=pxa::game::Upload(*renderer,commands.bytes()).texture({uint8_t(slots[i])},1,1,blank);
                if (!status) break;
                slots[i]=-1;
            }
        }
        for (unsigned i=0;status && i<slots.size();++i) {
            if (!needed[i] || slots[i]>=0) continue;
            const auto end=occupied.begin()+max;
            const auto empty=std::find(occupied.begin(),end,false);
            char name[48];
            atlasPath(name,i);
            if (empty==end) status=std::unexpected(pxa::Error::resource_limit);
            else {
                auto asset=co_await context->assets().load(pxa::AssetKind::texture,name);
                const int slot=empty-occupied.begin();
                status=asset?renderer->bind_asset(*asset,{uint8_t(slot)})
                            :std::unexpected(asset.error());
                if (status) {slots[i]=slot;*empty=true;}
            }
            if (!status) {
                const bool budget=status.error()==pxa::Error::quota_exceeded || status.error()==pxa::Error::resource_limit;
                const bool fieldAnimation=i==art::battleEffects[0][0].page;
                const bool zhaoFrames=i==art::zhaoYunFrames[0][0].page;
                if (budget && (fieldAnimation || zhaoFrames)) {
                    if (useSharpField()) {
                        sharpBattlePictures=false;
                        info("han-banners using standard battlefield panorama");
                    } else if (nativeFieldArt()) {
                        nativeBattlePictures=false;
                        info("han-banners using a lighter panorama for field animations");
                    } else if (zhaoFrames) {
                        zhaoFieldArt=false;
                        info("han-banners using standard Zhao Yun animation");
                    } else {
                        fieldMotionArt=false;
                        info("han-banners using compact field sprites and effects");
                    }
                    status={};
                } else if (budget && i>=art::sharpFields[0].first) {
                    sharpBattlePictures=false;
                    status={};
                    info("han-banners using standard battlefield panorama");
                } else if (budget && i==art::detailIcons[0].page) {
                    menuIconDetail=false;
                    status={};
                    info("han-banners using compact menu icons");
                } else if (budget && i>=art::armory.page) {
                    armoryQuality=i==art::armory.page?ArmoryQuality::shared:ArmoryQuality::compact;
                    status={};
                    info("han-banners using smaller armory backdrop");
                } else if (budget && i>=art::mapHall.first && nativeMapPictures) {
                    nativeMapPictures=false;
                    status={};
                    info("han-banners using scaled map backdrop");
                } else if (budget && i>=art::firstDetailActorPage && i<art::mapHall.first && detailedBattleActors) {
                    detailedBattleActors=false;
                    status={};
                    info("han-banners using compact field animations");
                } else if (budget && i>=art::firstDetailMenuPage && i<art::firstDetailActorPage && detailedMenus) {
                    detailedMenus=false;
                    status={};
                    info("han-banners using compact menu art");
                } else if (budget && i>=art::nativeFields[0].first && i<art::firstDetailMenuPage && nativeBattlePictures) {
                    nativeBattlePictures=false;
                    status={};
                    info("han-banners using scaled battlefield art");
                } else if (budget && i>=art::choiceHall.first && i<art::nativeFields[0].first) {
                    if (page==Page::choose) detailedChoiceHall=false;
                    else wideRoomPictures=false;
                    status={};
                    info("han-banners using standard hall backdrop");
                } else if (budget && ((i>=art::firstDetailedPage && i<art::pastDetailedPage) ||
                                    i==art::rulerPortraits[0].page)) {
                    detailedArt=false;
                    status={};
                    info("han-banners using compact backgrounds");
                } else if (budget && personalArt && i>=art::firstSignaturePage &&
                           i<art::firstSignaturePage+std::size(art::signatureLeaders)) {
                    personalArt=false;
                    status={};
                    info("han-banners using shared commander frames");
                } else info(name);
                break;
            }
        }
        loadingArt=false;
        dirty=bool(status);
        previousTick=0;
        co_return status;
    }
    pxa::Task<void> saveLoop() {
        while (saveQueued) {
            saveQueued = false;
            auto data = encode(campaign);
            // The campaign payload is larger than the default storage request buffer.
            std::array<std::byte,8+std::string_view(saveKey).size()+Save{}.size()> packet;
            auto written = co_await context->storage().set(saveKey,data,packet);
            if (!written) {
                notice = Notice::saveError;
                dirty = true;
                char message[64];
                std::snprintf(message,sizeof message,"han-banners save failed error=%d",int(written.error()));
                info(message);
            } else {
                damagedSave = false;
                if (notice==Notice::saveError) {
                    notice=Notice::none;
                    dirty=true;
                }
                info("han-banners checkpoint");
            }
        }
        saving = false;
        co_return pxa::Result<void>{};
    }
    void checkpoint() {
        hasSave = true;
        saveQueued = true;
        if (saving) return;
        saving = true;
        if (!context->tasks().start(saveLoop())) {
            saving = false;
            notice = Notice::saveError;
        }
    }

    int pageExtra() const {
        return tallScene && (page==Page::map || (page==Page::battle && !battle.duel.visible()))?tallScene:0;
    }
    float originY() const {return pageExtra()?tallOy:oy;}
    bool wrapSquareControls() const {return smallText && tallScene>=72;}
    int mapExtra() const {return tallScene-(wrapSquareControls()?36:0);}
    int battleExtra() const {return tallScene-(wrapSquareControls()?32:0);}
    bool fullAtlas() const {return mapWindow.w==fullMap.w;}
    void setMapWindow(Rect base) {
        mapWindow=base;
        mapWindow.h+=fullAtlas()?tallScene:mapExtra();
        labelFontScale=0;
    }
    Rect cityBounds() const {return {cityPanel.x,cityPanel.y,cityPanel.w,cityPanel.h+mapExtra()};}
    int sx(float x) const {return int(std::floor(ox+x*scale+.5f));}
    int sy(float y) const {return int(std::floor(drawOy+y*scale+.5f));}
    Rect toPixels(Rect r) const {
        const int x=sx(r.x),y=sy(r.y);
        return {x,y,sx(r.x+r.w)-x,sy(r.y+r.h)-y};
    }
    void blockPixels(pxa::game::Frame& f,Rect r,uint16_t color) const {
        if (r.w<=0 || r.h<=0) return;
        const int16_t l=int16_t(r.x*16),t=int16_t(r.y*16);
        const int16_t e=int16_t((r.x+r.w)*16),b=int16_t((r.y+r.h)*16);
        f.quad({l,t,e,t,e,b,l,b},{color});
    }
    void block(pxa::game::Frame& f,Rect r,uint16_t color) const {
        blockPixels(f,toPixels(r),color);
    }
    void line(pxa::game::Frame& f,int x,int y,int u,int v,uint16_t color,int thickness=1) const {
        int16_t l = int16_t(sx(x)*16), t = int16_t(sy(y)*16);
        int16_t e = int16_t(sx(u)*16), b = int16_t(sy(v)*16);
        if (l==e && t==b) return;
        const float vx=e-l,vy=b-t;
        const float length=std::sqrt(vx*vx+vy*vy);
        int16_t dx = int16_t(-(b-t)*8*thickness*scale/length);
        int16_t dy = int16_t((e-l)*8*thickness*scale/length);
        f.quad({int16_t(l+dx),int16_t(t+dy),int16_t(e+dx),int16_t(b+dy),
                int16_t(e-dx),int16_t(b-dy),int16_t(l-dx),int16_t(t-dy)},{color});
    }
    void frame(pxa::game::Frame& f,Rect r,uint16_t border=edge,uint16_t fill=panel) const {
        const auto p=toPixels(r);
        blockPixels(f,p,border);
        blockPixels(f,{p.x+1,p.y+1,p.w-2,p.h-2},fill);
    }
    void tilePixels(pxa::game::Frame& f,art::Tile tile,Rect pixels,
                    bool alpha=false,uint16_t tint=0) const {
        if (pixels.w<=0 || pixels.h<=0) return;
        pxa::game::Sprite sprite{int16_t(pixels.x),int16_t(pixels.y),
            uint16_t(pixels.w),uint16_t(pixels.h),tile.x,tile.y,tile.w,tile.h};
        f.sprites({uint8_t(slots[tile.page])},{&sprite,1},
                  {.transparent_index0=alpha,.solid_color=bool(tint),.color={tint}});
    }
    void tile(pxa::game::Frame& f,art::Tile tile,Rect dst,bool alpha=false) const {
        const int x=sx(dst.x),y=sy(dst.y);
        tilePixels(f,tile,{x,y,sx(dst.x+dst.w)-x,sy(dst.y+dst.h)-y},alpha);
    }
    void facingTilePixels(pxa::game::Frame& f,art::Tile image,Rect p,bool flipped) const {
        if (!flipped) {
            tilePixels(f,image,p,true);
            return;
        }
        if (p.w<=0 || p.h<=0) return;
        const pxa::game::AtlasBinding atlas{uint8_t(slots[image.page])};
        if (mirrorQuad) {
            using pxa::game::Vertex;
            const int16_t l=int16_t(p.x*16), r=int16_t((p.x+p.w)*16);
            const int16_t t=int16_t(p.y*16), b=int16_t((p.y+p.h)*16);
            const int16_t u=int16_t(image.x*16), v=int16_t(image.y*16);
            const int16_t endU=int16_t((image.x+image.w)*16), endV=int16_t((image.y+image.h)*16);
            f.textured_quad(atlas,{Vertex{l,t,endU,v,0},Vertex{r,t,u,v,0},
                                   Vertex{r,b,u,endV,0},Vertex{l,b,endU,endV,0}},
                            {.affine_uv=true,.painter=true,.transparent_index0=true});
            return;
        }
        // Older renderers still support a bounded batch of reversed strips.
        std::array<pxa::game::Sprite,32> strips;
        const int columns=std::min({int(strips.size()),p.w,int(image.w)});
        for (int i=0;i<columns;++i) {
            const int x=i*p.w/columns, end=(i+1)*p.w/columns;
            const int u=image.x+image.w-1-(2*i+1)*image.w/(2*columns);
            strips[i]={int16_t(p.x+x),int16_t(p.y),uint16_t(end-x),uint16_t(p.h),uint16_t(u),image.y,1,image.h};
        }
        f.sprites(atlas,std::span(strips).first(columns));
    }
    void groundedTile(pxa::game::Frame& f,art::Tile image,int x,int foot,int w,int h,
                      bool flipped=false) const {
        auto p=toPixels({x-w/2,foot-h+1,w,h});
        // Nearest-neighbor sampling can enlarge the transparent rows below
        // the boots. Offset those rows, keeping the last visible row at foot.
        p.y+=image.footInset*p.h/image.h;
        facingTilePixels(f,image,p,flipped);
    }
    struct BackdropCrop {
        art::Backdrop tiles;
        Rect source;
    };
    BackdropCrop backdropCrop(art::Tile image,float anchor,float zoom=1.f,bool field=false) const {
        const auto tiles=backdropTiles(image,field);
        const int sourceWidth=tiles.w*tiles.columns,sourceHeight=tiles.h*tiles.rows;
        const float aspect=float(width)/height;
        const bool fieldPainting=tiles.compact>=art::battlefields[0].page && tiles.compact<=art::siege.page;
        const bool native=(field || fieldPainting || tiles.first==art::armoryDetail.first ||
                           tiles.compact==art::cityCourtyard.page) &&
                          sourceWidth>=width;
        const bool fittedHall=tiles.compact==art::warHall.page && tiles.columns==3 && tiles.rows==1;
        const int w=native?width:fittedHall?sourceWidth:
            std::max(1,int(std::round(std::min(float(sourceWidth),sourceHeight*aspect)/zoom)));
        const int h=native||fittedHall?sourceHeight:
            std::max(1,int(std::round(std::min(float(sourceHeight),sourceWidth/aspect)/zoom)));
        return {tiles,{int(std::round((sourceWidth-w)*anchor)),(sourceHeight-h)/2,w,h}};
    }
    void backdropRegion(pxa::game::Frame& f,const BackdropCrop& crop,Rect region) const {
        if (region.w<=0 || region.h<=0) return;
        const auto& [tiles,source]=crop;
        const auto [cx,cy,w,h]=source;
        for (int row=0;row<tiles.rows;++row) for (int col=0;col<tiles.columns;++col) {
            const int x=col*tiles.w,y=row*tiles.h;
            const int l=std::max(cx,x),t=std::max(cy,y);
            const int r=std::min(cx+w,x+tiles.w),b=std::min(cy+h,y+tiles.h);
            if (l>=r || t>=b) continue;
            const int dl=(l-cx)*width/w,dt=(t-cy)*height/h;
            const int dr=(r-cx)*width/w,db=(b-cy)*height/h;
            const int left=std::max(dl,region.x),top=std::max(dt,region.y);
            const int right=std::min(dr,region.x+region.w),bottom=std::min(db,region.y+region.h);
            if (left>=right || top>=bottom) continue;
            const int sl=l+(left-dl)*(r-l)/(dr-dl),st=t+(top-dt)*(b-t)/(db-dt);
            const int sr=l+((right-dl)*(r-l)+dr-dl-1)/(dr-dl);
            const int sb=t+((bottom-dt)*(b-t)+db-dt-1)/(db-dt);
            const auto page=uint8_t(tiles.first+row*tiles.columns+col);
            tilePixels(f,{page,uint16_t(sl-x),uint16_t(st-y),uint16_t(sr-sl),uint16_t(sb-st)},
                       {left,top,right-left,bottom-top});
        }
    }
    void fillBackground(pxa::game::Frame& f,art::Tile image,float anchor=.5f) const {
        backdropRegion(f,backdropCrop(image,anchor),{0,0,width,height});
    }
    template<class Paint>
    static void around(Rect outer,Rect hole,Paint paint) {
        paint(Rect{outer.x,outer.y,outer.w,hole.y-outer.y});
        paint(Rect{outer.x,hole.y+hole.h,outer.w,outer.y+outer.h-hole.y-hole.h});
        paint(Rect{outer.x,hole.y,hole.x-outer.x,hole.h});
        paint(Rect{hole.x+hole.w,hole.y,outer.x+outer.w-hole.x-hole.w,hole.h});
    }
    template<class Paint>
    void betweenPanels(std::span<const Rect> panels,Paint paint,int edge=0) const {
        const int start=panels.front().y,end=start+panels.front().h;
        const int top=sy(start+edge),bottom=sy(end-edge),h=bottom-top;
        if (edge) {
            paint(Rect{0,sy(start),width,top-sy(start)});
            paint(Rect{0,bottom,width,sy(end)-bottom});
        }
        int left=0;
        for (const auto& panel:panels) {
            paint(Rect{left,top,sx(panel.x)-left,h});
            left=sx(panel.x+panel.w);
        }
        paint(Rect{left,top,width-left,h});
    }
    void mapBackdrop(pxa::game::Frame& f) const {
        if (fullAtlas()) {
            const auto fill=[&](Rect r) {blockPixels(f,r,rgb(25,44,43));};
            around({0,0,width,height},toPixels(mapWindow),fill);
            return;
        }
        constexpr float anchor=.42f;
        const auto crop=backdropCrop(art::warHall,anchor);
        int left=0,right=width;
        if (nativeMapHall()) {
            left=sx(mapResources[0].x);
            right=sx(mapStatus.x+mapStatus.w);
            const int crop=int(std::round((art::mapHallWidth-width)*anchor));
            constexpr int span=art::mapHallSideSpan,texels=art::mapHallSideTexels;
            const auto margin=[&](int x,int w,int source,int start) {
                const int from=std::clamp(source*texels/span,0,texels-1);
                const int to=std::clamp(((source+w)*texels+span-1)/span,from+1,texels);
                int y=0;
                for (auto strip:art::mapHallSides) {
                    const Rect target{x,y,w,strip.h};
                    strip.x=uint16_t(start+from);
                    strip.w=uint16_t(to-from);
                    tilePixels(f,strip,target);
                    y+=strip.h;
                }
            };
            margin(0,left,crop,0);
            margin(right,width-right,crop+right-(art::mapHallWidth-span),texels);
        }
        const auto paint=[&](Rect r) {
            const int end=std::min(right,r.x+r.w);
            r.x=std::max(left,r.x);
            r.w=end-r.x;
            backdropRegion(f,crop,r);
        };
        const auto band=[&](int top,int bottom) {paint({0,sy(top),width,sy(bottom)-sy(top)});};
        const Rect panels[]={mapWindow,cityBounds()};
        const auto status=mapStatus.shiftY(mapExtra());
        paint({0,0,width,sy(mapResources[0].y)});
        betweenPanels(mapResources,paint);
        band(mapResources[0].y+mapResources[0].h,mapWindow.y);
        betweenPanels(panels,paint);
        band(mapWindow.y+mapWindow.h,status.y);
        betweenPanels({&status,1},paint,hudEdge);
        const int bottom=sy(status.y+status.h);
        paint({0,bottom,width,height-bottom});
    }
    void fieldBackdrop(pxa::game::Frame& f) {
        const float anchor=battleCamera/fieldPanLimit;
        const auto crop=backdropCrop(battlefield(),anchor,1.22f,true);
        const int land=crop.tiles.h*crop.tiles.rows/2;
        const int landPixels=(land-crop.source.y)*height/crop.source.h;
        fieldLand=int(std::ceil((landPixels-drawOy)/scale))+5;
        const auto paint=[&](Rect r) {backdropRegion(f,crop,r);};
        const auto band=[&](int top,int bottom) {paint({0,sy(top),width,sy(bottom)-sy(top)});};
        if (fieldView && !battle.paused) {
            paint({0,0,width,height});
            return;
        }
        // Leave the backdrop behind transparent HUD corners and the pause plaque.
        paint({0,0,width,sy(fieldCards[0].y)});
        betweenPanels(fieldCards,paint,hudEdge);
        band(fieldCards[0].y+fieldCards[0].h,fieldMapPanel.y);
        betweenPanels({&fieldMapPanel,1},paint,hudEdge);
        const int fieldTop=fieldMapPanel.y+fieldMapPanel.h;
        int footer=sy(fieldTop);
        const auto status=fieldStatus.shiftY(battleExtra());
        if (!battle.paused && !tacticsOpen && !battle.awaitingOrders) {
            band(fieldTop,status.y);
            betweenPanels({&status,1},paint,hudEdge);
            footer=sy(status.y+status.h);
        }
        paint({0,footer,width,height-footer});
    }
    void contain(pxa::game::Frame& f,art::Tile image,Rect area,bool alpha=false) const {
        const auto dest=toPixels(area);
        const float ratio=std::min(float(dest.w)/image.w,float(dest.h)/image.h);
        const int w=int(std::round(image.w*ratio)),h=int(std::round(image.h*ratio));
        tilePixels(f,image,{dest.x+(dest.w-w)/2,dest.y+(dest.h-h)/2,w,h},alpha);
    }
    static pxa::game::Sprite slice(art::Tile tile,Rect dst,int x,int y,int w,int h) {
        return {int16_t(dst.x),int16_t(dst.y),uint16_t(dst.w),uint16_t(dst.h),
                uint16_t(tile.x+x),uint16_t(tile.y+y),uint16_t(w),uint16_t(h)};
    }
    void nineSlice(pxa::game::Frame& f,art::Tile tile,Rect p,Rect caps,bool center) const {
        const int dx=std::clamp(caps.w,0,p.w/2),dy=std::clamp(caps.h,0,p.h/2);
        const int srcX[]={0,caps.x,tile.w-caps.x,tile.w};
        const int srcY[]={0,caps.y,tile.h-caps.y,tile.h};
        const int x[]={0,dx,p.w-dx,p.w},y[]={0,dy,p.h-dy,p.h};
        std::array<pxa::game::Sprite,9> sprites;
        int n=0;
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) {
            if ((!center && r==1 && c==1) || x[c]==x[c+1] || y[r]==y[r+1]) continue;
            sprites[n++]=slice(tile,{p.x+x[c],p.y+y[r],x[c+1]-x[c],y[r+1]-y[r]},
                               srcX[c],srcY[r],srcX[c+1]-srcX[c],srcY[r+1]-srcY[r]);
        }
        if (n) f.sprites({uint8_t(slots[tile.page])},{sprites.data(),size_t(n)});
    }
    void skinFrame(pxa::game::Frame& f,Rect area,bool selected=false) const {
        const auto p=toPixels(area);
        if (p.w<=0 || p.h<=0) return;
        const int border=std::max(2,int(12*scale+.5f));
        nineSlice(f,art::windows[selected],p,{14,12,border,border},true);
    }
    void battlePanel(pxa::game::Frame& f,Rect area,BattlePlaque tone=bronzePlaque) const {
        const auto p=toPixels(area);
        if (p.w<=0 || p.h<=0) return;
        nineSlice(f,art::battlePanels[tone],p,
                  {12,8,std::max(2,int(7*scale)),std::max(2,int(5*scale))},true);
    }
    void plaque(pxa::game::Frame& f,Rect area,bool enabled=true,bool lit=false) const {
        const auto pixels=toPixels(area);
        if (pixels.w<=0 || pixels.h<=0) return;
        const auto plate=art::plaques[enabled?(lit?1:0):2];
        nineSlice(f,plate,pixels,{18,8,std::max(2,int(10*scale)),std::max(2,int(6*scale))},true);
    }
    void drawStrip(pxa::game::Frame& f,art::Tile tile,Rect area) const {
        if (area.w<=0 || area.h<=0) return;
        const int end=std::min(area.w/2,std::max(1,area.h/2));
        constexpr int cap=8;
        const int src[]={0,cap,tile.w-cap,tile.w};
        const int dst[]={0,end,area.w-end,area.w};
        std::array<pxa::game::Sprite,3> sprites;
        int n=0;
        for (int i=0;i<3;++i) if (dst[i+1]>dst[i])
            sprites[n++]=slice(tile,{area.x+dst[i],area.y,dst[i+1]-dst[i],area.h},
                               src[i],0,src[i+1]-src[i],tile.h);
        f.sprites({uint8_t(slots[tile.page])},{sprites.data(),size_t(n)});
    }
    void gauge(pxa::game::Frame& f,Rect area,int value,int maximum,GaugeTone tone,bool reverse=false) const {
        const Rect p=toPixels(area);
        const bool marching=tone>=blueMarchGauge;
        drawStrip(f,marching?art::marchBars[0]:battleGauges()[0],p);
        const int border=std::max(1,p.h/6);
        const int room=p.w-2*border;
        if (value<=0 || maximum<=0 || room<=0 || p.h<=2*border) return;
        const int amount=std::max(1,room*std::min(value,maximum)/maximum);
        const int x=reverse?p.x+p.w-border-amount:p.x+border;
        const auto fill=marching?art::marchBars[1+tone-blueMarchGauge]:battleGauges()[tone];
        drawStrip(f,fill,{x,p.y+border,amount,p.h-2*border});
    }
    static uint32_t next(std::string_view& s) {
        uint8_t c = uint8_t(s.front());
        unsigned bytes = c<128 ? 1 : (c<224 ? 2 : 3);
        uint32_t code = c & (bytes==1 ? 127 : bytes==2 ? 31 : 15);
        for (unsigned i=1;i<bytes && i<s.size();++i) code=(code<<6)|(uint8_t(s[i])&63);
        s.remove_prefix(std::min<size_t>(bytes,s.size()));
        return code;
    }
    struct BitmapGlyphLookup {
        uint32_t code=UINT32_MAX;
        uint16_t index=0;
    };
    mutable std::array<BitmapGlyphLookup,128> bitmapGlyphs{};
    const art::Glyph& glyph(uint32_t code) const {
        const auto& face=art::glyphs;
        auto& cached=bitmapGlyphs[(code^(code>>7))&(bitmapGlyphs.size()-1)];
        if (cached.code==code) return face[cached.index];
        const auto it=std::lower_bound(face.begin(),face.end(),code,
            [](const art::Glyph& item,uint32_t key){return item.code<key;});
        cached={code,uint16_t(it!=face.end() && it->code==code?it-face.begin():art::fallbackGlyph)};
        return face[cached.index];
    }
    int fontScale(int requested) const {
        const float native=scale*requested/art::fontPixels;
        return std::max(1,int(requested>=28?std::round(native):std::floor(native)));
    }
    int fontHeight(int size) const {
        if (!sansText) return art::fontPixels*fontScale(size);
        const float native=std::max(float(smallText),scale*std::max(11.f,size*.85f));
        return std::clamp(int(std::round(native*systemFontScale)),9,42);
    }
    int glyphAdvance(uint32_t code,int height) const {
        return sansText?sans.advance(code,height):glyph(code).advance*(height/art::fontPixels);
    }
    int textAdvance(std::string_view s,int height) const {
        int width=0;
        while (!s.empty()) width+=glyphAdvance(next(s),height);
        return width;
    }
    int textWidth(std::string_view s,int size) const {
        return textAdvance(s,fontHeight(size));
    }
    size_t linePrefix(std::string_view s,int height,int available) const {
        const size_t length=s.size();
        int width=0;
        while (!s.empty()) {
            const size_t used=length-s.size();
            width+=glyphAdvance(next(s),height);
            if (width>available) return used;
        }
        return length;
    }
    struct TextStamp {
        art::Tile tile{};
        Rect rect{};
        uint8_t slot=0;
    };

    TextStamp stamp(uint32_t code,int height,int x,int y) {
        if (sansText) {
            const auto* letter=sans.glyph(code,height,texelAlpha);
            if (!letter) return {};
            const auto w=uint16_t(letter->width),h=uint16_t(letter->height);
            return {{0,uint16_t(letter->x),uint16_t(letter->y),w,h},
                    {x+letter->left,y+(17*height+10)/20+letter->top,w,h},
                    uint8_t(fontSlots()+letter->bank)};
        }
        const auto tile=glyph(code).tile;
        return {tile,{x,y,tile.w*(height/art::fontPixels),height},uint8_t(slots[tile.page])};
    }

    void textPixels(pxa::game::Frame& f,std::string_view s,int u,int v,int h,
                    uint16_t color,uint16_t outline=0,const Rect* clip=nullptr) {
        const bool alpha=sansText && texelAlpha;
        auto drawRun=[&](uint16_t tint,int stroke) {
            std::array<pxa::game::Sprite,32> sprites;
            int n=0,slot=-1,left=u;
            auto flush=[&] {
                if (!n) return;
                f.sprites({uint8_t(slot)},{sprites.data(),size_t(n)},
                          {.transparent_index0=true,.solid_color=true,.texel_alpha=alpha,.color={tint}});
                n=0;
            };
            auto line=s;
            while (!line.empty()) {
                const auto code=next(line);
                if (code!=' ') {
                    const auto [tile,r,bank]=stamp(code,h,left,v);
                    if (tile.w) {
                        if (slot!=bank || n+(stroke?4:1)>int(sprites.size())) flush();
                        slot=bank;
                        if (clip && (!clip->contains(r.x,r.y) ||
                                     !clip->contains(r.x+r.w-1,r.y+r.h-1))) {
                            const int unit=sansText?1:h/art::fontPixels;
                            const int l=std::max(0,(clip->x-r.x+unit-1)/unit);
                            const int t=std::max(0,(clip->y-r.y+unit-1)/unit);
                            const int rgt=std::min(int(tile.w),(clip->x+clip->w-r.x)/unit);
                            const int btm=std::min(int(tile.h),(clip->y+clip->h-r.y)/unit);
                            if (l<rgt && t<btm)
                                sprites[n++]=slice(tile,{r.x+l*unit,r.y+t*unit,(rgt-l)*unit,(btm-t)*unit},
                                                   l,t,rgt-l,btm-t);
                        } else if (stroke) {
                            sprites[n++]=slice(tile,{r.x-stroke,r.y,r.w,r.h},0,0,tile.w,tile.h);
                            sprites[n++]=slice(tile,{r.x+stroke,r.y,r.w,r.h},0,0,tile.w,tile.h);
                            sprites[n++]=slice(tile,{r.x,r.y-stroke,r.w,r.h},0,0,tile.w,tile.h);
                            sprites[n++]=slice(tile,{r.x,r.y+stroke,r.w,r.h},0,0,tile.w,tile.h);
                        } else sprites[n++]=slice(tile,r,0,0,tile.w,tile.h);
                    }
                }
                left+=glyphAdvance(code,h);
            }
            flush();
        };
        if (outline) drawRun(outline,std::max(1,h/12));
        drawRun(color,0);
    }
    void text(pxa::game::Frame& f,std::string_view s,int x,int y,int size=14,
              uint16_t color=ivory,int align=0,uint16_t outline=0) {
        const int h=fontHeight(size);
        const int dx=align?textAdvance(s,h)/(align==1?2:1):0;
        textPixels(f,s,sx(x)-dx,(sy(y)+sy(y+size)-h)/2,h,color,outline);
    }
    void boxText(pxa::game::Frame& f,std::string_view s,Rect box,int size=14,
                 uint16_t color=ivory,int align=0) {
        const Rect p=toPixels(box);
        if (p.w<=0 || p.h<=0 || s.empty()) return;
        const int step=sansText?1:art::fontPixels;
        const int nominal=fontHeight(size);
        const int minimum=sansText?std::min({p.h,nominal,std::max(9,int(scale*9))}):art::fontPixels;
        int h=std::clamp(p.h/step*step,minimum,nominal);
        int w=textAdvance(s,h);
        while (w>p.w && h>minimum) {
            h-=step;
            w=textAdvance(s,h);
        }
        char shortened[128];
        if (w>p.w) {
            constexpr std::string_view suffix="...";
            const int suffixWidth=textAdvance(suffix,h);
            if (suffixWidth>p.w) return;
            auto count=linePrefix(s,h,p.w-suffixWidth);
            if (count>sizeof shortened-suffix.size()) {
                count=sizeof shortened-suffix.size();
                while (count && (uint8_t(s[count])&0xc0)==0x80) --count;
            }
            std::memcpy(shortened,s.data(),count);
            std::memcpy(shortened+count,suffix.data(),suffix.size());
            s={shortened,count+suffix.size()};
            w=textAdvance(s,h);
        }
        const int dx=align?(p.w-w)/(align==1?2:1):0;
        textPixels(f,s,p.x+dx,p.y+(p.h-h)/2,h,color,0,&p);
    }
    void number(pxa::game::Frame& f,int n,int x,int y,int size,uint16_t color=ivory,int align=0) {
        char str[24];
        std::snprintf(str,sizeof str,"%d",n);
        text(f,str,x,y,size,color,align);
    }
    bool registerControl(Rect box,Command action,int data,bool enabled=true) {
        if (count==int(controls.size())) return false;
        int index=count++;
        const int inspected=destination<0?selected:destination;
        const bool preferred=mapFocusCity>=0?action==Command::select && data==mapFocusCity:
            action!=Command::select || data==inspected;
        if (enabled && focus<0 && preferred) focus=index;
        controls[index]={box,action,data,enabled};
        return enabled && ((controllerMode && focus==index) ||
               (pressedControl && !dragMoved && pressedControl->command==action && pressedControl->data==data));
    }
    uint32_t padKeyFor(Command command,int data) const {
        using namespace pxa::input;
        switch (command) {
            case Command::back:
            case Command::accept:
            case Command::pause:
            case Command::cancelOrder: return controller_b;
            case Command::skill: return controller_x;
            case Command::orders: return tacticsOpen?controller_b:controller_y;
            case Command::zoom:
            case Command::rosterPage:
            case Command::fiefPage:
            case Command::prisonPage:
            case Command::browseCity: return data<0?controller_l:controller_r;
            default:return 0;
        }
    }
    const char* padLabel(Command command,int data,bool focused) const {
        using namespace pxa::input;
        const auto key=padKeyFor(command,data);
        if (key==controller_b) return "B";
        if (key & seenPadShortcuts) {
            if (key==controller_x) return "X";
            if (key==controller_y) return "Y";
            if (key==controller_l) return "L";
            if (key==controller_r) return "R";
        }
        return focused?"A":nullptr;
    }
    art::Tile iconTile(int icon) const {
        if (detailedArt && sansText && icon>=shieldIcon) {
            const auto detail=art::detailIcons[icon-shieldIcon];
            if (slots[detail.page]>=0) return detail;
        }
        return art::commandIcons[icon];
    }
    void button(pxa::game::Frame& f,Rect r,const char* label,Command action,int data=0,
                bool enabled=true,bool main=false,int icon=-1) {
        const bool focused=registerControl(r,action,data,enabled);
        plaque(f,r,enabled,main||focused);
        if (focused && r.w>60) block(f,{r.x+4,r.y+5,2,r.h-10},gold);
        auto labelBox=r.inset(r.w<=60?3:6,r.h<21?1:r.h<24?2:3);
        const int iconSize=r.h<=21?12:16;
        const bool iconOnly=icon>=0 && !*label;
        if (controllerMode && enabled) {
            const char* key=padLabel(action,data,focused);
            const bool narrow=r.w<=32;
            const int hintWidth=narrow?11:13,hintGap=narrow?1:2;
            Rect remaining=narrow?r.inset(2,1):labelBox;
            const Rect hint{remaining.x+remaining.w-hintWidth,r.cy()-7,hintWidth,14};
            remaining.w-=hintWidth+hintGap;
            if (key && remaining.w>0 && (!iconOnly || remaining.w>=iconSize+2) &&
                textWidth(label,12)<=toPixels(remaining).w) {
                frame(f,hint,focused?gold:edge,ink);
                boxText(f,key,hint.inset(1,1),11,focused?gold:ivory,1);
                labelBox=remaining;
            }
        }
        if (iconOnly) {
            const auto image=iconTile(icon);
            const Rect box{labelBox.cx()-iconSize/2,r.cy()-iconSize/2,iconSize,iconSize};
            if (enabled) contain(f,image,box,true);
            else tilePixels(f,image,toPixels(box),true,dim);
            return;
        }
        const int iconGap=iconSize+4;
        if (icon>=0 && textWidth(label,14)<=toPixels(labelBox).w-(sx(iconGap)-sx(0))) {
            contain(f,iconTile(icon),{labelBox.x+1,r.cy()-iconSize/2,iconSize,iconSize},true);
            labelBox.x+=iconGap;
            labelBox.w-=iconGap;
        }
        boxText(f,label,labelBox,14,enabled?ivory:dim,1);
    }
    void iconText(pxa::game::Frame& f,UiIcon icon,std::string_view label,Rect area,int size,uint16_t color) {
        const int side=std::min(area.h,18);
        contain(f,iconTile(icon),{area.x,area.cy()-side/2,side,side},true);
        boxText(f,label,{area.x+side+4,area.y,area.w-side-4,area.h},size,color);
    }
    void header(pxa::game::Frame& f,const char* label,const char* sub=nullptr) {
        plaque(f,{7,3,370,31});
        block(f,{16,10,2,16},gold);
        const int titleEnd=sub?std::min(176,32+int(std::ceil(textWidth(label,19)/scale))):365;
        boxText(f,label,{25,6,titleEnd-25,24},19,gold);
        if (sub) boxText(f,sub,{titleEnd+8,6,357-titleEnd,24},13,ivory,2);
    }
    void bookText(pxa::game::Frame& f,std::string_view line,Rect box,uint16_t color=ivory) {
        constexpr int size=14;
        const auto pixels=toPixels(box);
        const int h=fontHeight(size);
        const int step=h+std::max(2,int(2*scale));
        int y=pixels.y;
        while (!line.empty() && y+h<=pixels.y+pixels.h) {
            const auto count=linePrefix(line,h,pixels.w);
            if (!count) break;
            textPixels(f,line.substr(0,count),pixels.x,y,h,color,0,&pixels);
            line.remove_prefix(count);
            y+=step;
        }
    }
    void resourcePlate(pxa::game::Frame& f,Rect area,UiIcon icon,
                       std::string_view value,uint16_t color=ivory) {
        plaque(f,area);
        contain(f,art::commandIcons[icon],{area.x+6,area.cy()-8,16,16},true);
        boxText(f,value,{area.x+25,area.y+2,area.w-32,area.h-4},14,color,1);
    }
    void portrait(pxa::game::Frame& f,art::Tile face,Rect r) {
        frame(f,{r.x-1,r.y-1,r.w+2,r.h+2},edge,ink);
        contain(f,face,r);
    }
    void portrait(pxa::game::Frame& f,int actor,Rect r) {
        portrait(f,officerPortrait(actor),r);
    }
    void drawLogo(pxa::game::Frame& f,Rect area) const {
        if (!detailedMenuArt()) {
            contain(f,art::logo,area,true);
            return;
        }
        constexpr auto& tiles=art::logoTiles;
        constexpr int sourceWidth=tiles[0].w+tiles[1].w;
        const auto viewport=toPixels(area);
        const int w=std::min(viewport.w,viewport.h*sourceWidth/tiles[0].h);
        const int h=w*tiles[0].h/sourceWidth;
        const int x=viewport.x+(viewport.w-w)/2,y=viewport.y+(viewport.h-h)/2;
        const int left=w*tiles[0].w/sourceWidth;
        tilePixels(f,tiles[0],{x,y,left,h},true);
        tilePixels(f,tiles[1],{x+left,y,w-left,h},true);
    }
    void drawHome(pxa::game::Frame& f) {
        fillBackground(f,titleBackground(),.57f);
        block(f,{14,14,26,2},gold);
        block(f,{14,14,2,15},gold);
        block(f,{344,248,26,2},gold);
        block(f,{368,235,2,15},gold);
        drawLogo(f,{25,59,213,57});
        block(f,{28,119,104,1},edge);
        if (hasSave) {
            button(f,{28,140,136,29},"继续征途",Command::resume,0,true,true);
            button(f,{28,175,136,28},"新建战役",Command::newGame);
            button(f,{28,210,136,27},"玩法说明",Command::guide,0,true,false,manualIcon);
        } else {
            button(f,{28,158,136,31},"新建战役",Command::newGame,0,true,true);
            button(f,{28,197,136,29},"玩法说明",Command::guide,0,true,false,manualIcon);
            if (damagedSave) text(f,"存档无法读取",28,232,13,coral);
        }
        button(f,{274,211,87,27},"音频",Command::audioMenu,0,true,false,musicIcon);
    }
    void drawChoose(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const auto& chapter=chapters[chosenChapter];
        header(f,"择主起兵",chapter.moment);
        std::array<int,ownerCount> holdings{};
        for (int city=0;city<cityCount;++city) {
            const auto* change=opening(chosenChapter,city);
            ++holdings[change?change->owner:sites[city].owner];
        }
        constexpr const char* rulers[]={"刘备","曹操","孙权"};
        for (int i=0;i<3;++i) {
            Rect card{10+122*i,69,118,106};
            bool emphasis=registerControl(card,Command::side,i);
            const bool selectedRuler=side==i;
            skinFrame(f,card,selectedRuler||emphasis);
            portrait(f,rulerPortrait(i),{card.cx()-29,card.y+5,58,58});
            block(f,{card.x+8,card.y+67,102,2},colors[i]);
            boxText(f,rulers[i],{card.x+7,card.y+70,104,17},16,selectedRuler?gold:ivory,1);
            char base[48];
            std::snprintf(base,sizeof base,"%s · %d城",cityLabel(chapter.base[i],chapter.year),holdings[i]);
            boxText(f,base,{card.x+8,card.y+87,card.w-16,16},12,colors[i],1);
        }
        for (int i=0;i<chapterCount;++i)
            button(f,{10+122*i,41,118,24},chapters[i].name,Command::chapter,i,true,i==chosenChapter);
        boxText(f,chapter.description,{19,175,346,18},12,ivory,1);
        for (int i=0;i<3;++i)
            button(f,{10+122*i,194,118,21},difficultyNames[i],Command::difficulty,i,true,i==int(chosenDifficulty));
        boxText(f,difficultyHints[int(chosenDifficulty)],{18,215,348,16},12,mint,1);
        button(f,{10,231,85,25},"返回",Command::back);
        button(f,{239,231,135,25},"起兵",Command::seat,0,true,true,orderIcon);
    }
    const char* ownerName(int o) const {return factions[o].title;}
    const char* cityName(int i) const {return cityLabel(i,campaign.year(),campaign.season());}
    const char* districtName(int i) const {return cityDistrict(i,campaign.year(),campaign.season());}
    static constexpr float detailScale=.74f,maxMapScale=1.55f;
    float overviewScale() const {
        return std::min(float(mapWindow.w-16)/worldWidth,float(mapWindow.h-16)/worldHeight);
    }
    float mapScale() const {
        if (mapZoom<=1) return std::lerp(overviewScale(),detailScale,mapZoom);
        return std::lerp(detailScale,maxMapScale,mapZoom-1);
    }
    float zoomLevel(float worldScale) const {
        const float fit=overviewScale();
        if (worldScale<=fit*1.005f) return 0;
        worldScale=std::min(worldScale,maxMapScale);
        return worldScale<=detailScale?(worldScale-fit)/(detailScale-fit):
            1+(worldScale-detailScale)/(maxMapScale-detailScale);
    }
    float zoomStep(int direction) const {
        return direction>0?std::min(2.f,std::floor(mapZoom+.001f)+1):
            std::max(0.f,std::ceil(mapZoom-.001f)-1);
    }
    void clampCamera() {
        float halfW=std::min(worldWidth/2.f,mapWindow.w/(mapScale()*2));
        float halfH=std::min(worldHeight/2.f,mapWindow.h/(mapScale()*2));
        mapCenterX=std::clamp(mapCenterX,halfW,worldWidth-halfW);
        mapCenterY=std::clamp(mapCenterY,halfH,worldHeight-halfH);
    }
    void focusCamera(int city) {
        auto [x,y]=cityMapPoint(city,campaign.year());
        mapCenterX=x;
        mapCenterY=y;
        clampCamera();
    }
    void zoomAt(float x,float y) {
        if (mapZoom>=2) return;
        const float offsetX=x-mapWindow.cx(),offsetY=y-mapWindow.cy();
        const float oldScale=mapScale();
        mapZoom=zoomStep(1);
        mapCenterX+=offsetX/oldScale-offsetX/mapScale();
        mapCenterY+=offsetY/oldScale-offsetY/mapScale();
        clampCamera();
        focus=-1;
        dirty=true;
    }
    bool moveMapCamera(float dx,float dy) {
        const float previousX=mapCenterX,previousY=mapCenterY;
        mapCenterX+=dx;
        mapCenterY+=dy;
        clampCamera();
        const bool moved=mapCenterX!=previousX || mapCenterY!=previousY;
        dirty|=moved;
        return moved;
    }
    void panMap(float x,float y,uint64_t stamp,bool sample=true) {
        const float zoom=mapScale();
        const float previousX=mapCenterX,previousY=mapCenterY;
        const bool moved=moveMapCamera((dragX-x)/zoom,(dragY-y)/zoom);
        dragX=x;dragY=y;
        if (!sample || !moved || stamp<=mapMotion.dragAt) return;
        const auto elapsed=stamp-mapMotion.dragAt;
        if (elapsed>=1000 && elapsed<=100'000) {
            const float perSecond=1'000'000.f/elapsed;
            const float cap=650.f/zoom;
            mapMotion.vx=std::lerp(mapMotion.vx,std::clamp((mapCenterX-previousX)*perSecond,-cap,cap),.6f);
            mapMotion.vy=std::lerp(mapMotion.vy,std::clamp((mapCenterY-previousY)*perSecond,-cap,cap),.6f);
        } else mapMotion.vx=mapMotion.vy=0;
        mapMotion.dragAt=stamp;
    }
    void releaseMap(uint64_t stamp) {
        if (stamp<mapMotion.dragAt || stamp-mapMotion.dragAt>90'000) return;
        const float speed=std::hypot(mapMotion.vx,mapMotion.vy)*mapScale();
        if (speed<150.f) return;
        // Use pointer times for samples and clock ticks for animation.
        mapMotion.tickAt=0;
        mapMotion.remainingUs=260'000;
    }
    void glideMap(uint64_t now) {
        if (!mapMotion.remainingUs || now<=mapMotion.tickAt) return;
        const auto elapsed=mapMotion.tickAt?now-mapMotion.tickAt:clockPeriod*1000u;
        const auto step=unsigned(std::min<uint64_t>(elapsed,mapMotion.remainingUs));
        mapMotion.remainingUs-=step;
        const float dt=float(std::min(step,50'000u))/1'000'000;
        mapMotion.tickAt=now;
        const float x=mapCenterX,y=mapCenterY;
        if (!moveMapCamera(mapMotion.vx*dt,mapMotion.vy*dt)) {mapMotion={};return;}
        const float decay=std::max(0.f,1.f-11.f*dt);
        mapMotion.vx=mapCenterX==x?0.f:mapMotion.vx*decay;
        mapMotion.vy=mapCenterY==y?0.f:mapMotion.vy*decay;
        if (!mapMotion.remainingUs || std::hypot(mapMotion.vx,mapMotion.vy)*mapScale()<12.f) mapMotion={};
    }
    float mapLeft() const {return mapWindow.cx()-mapCenterX*mapScale();}
    float mapTop() const {return mapWindow.cy()-mapCenterY*mapScale();}
    int mapX(float x) const {return int(std::round(mapLeft()+x*mapScale()));}
    int mapY(float y) const {return int(std::round(mapTop()+y*mapScale()));}
    const Road* findRoad(Order order) const {
        for (const auto& r:roads) if (r.joins(order.from,order.to)) return &r;
        return nullptr;
    }
    void drawSegment(pxa::game::Frame& f,int x,int y,int u,int v,uint16_t color,int stroke) {
        const Rect clip{mapWindow.x+1,mapWindow.y+1,mapWindow.w-2,mapWindow.h-2};
        if (clip.contains(x,y) && clip.contains(u,v)) {
            line(f,x,y,u,v,color,stroke);
            return;
        }
        if (std::max(x,u)<clip.x || std::min(x,u)>=clip.x+clip.w ||
            std::max(y,v)<clip.y || std::min(y,v)>=clip.y+clip.h) return;
        const int dx=u-x, dy=v-y;
        float start=0,end=1;
        auto cut=[&](int p,int q) {
            if (!p) return q>=0;
            const float t=float(q)/p;
            if (p<0) start=std::max(t,start);
            else end=std::min(t,end);
            return start<=end;
        };
        if (!cut(-dx,x-clip.x) || !cut(dx,clip.x+clip.w-1-x) ||
            !cut(-dy,y-clip.y) || !cut(dy,clip.y+clip.h-1-y)) return;
        int a=x+int(std::round(start*dx)), b=y+int(std::round(start*dy));
        int c=x+int(std::round(end*dx)), d=y+int(std::round(end*dy));
        if (a!=c || b!=d) line(f,a,b,c,d,color,stroke);
    }
    using MapPoint=std::pair<float,float>;
    struct RouteLine {
        std::array<MapPoint,roadBends+2> points{};
        int count=0;
        Rect bounds{};
    };
    std::array<RouteLine,std::size(roads)> mapRoads{};
    int mapRoadYear=0;
    RouteLine routeLine(const Road& road) const {
        RouteLine line;
        line.points[line.count++]=cityMapPoint(road.from,campaign.year());
        for (const auto& bend:road.bends) {
            if (!bend.lon) break;
            line.points[line.count++]={worldX(bend.lon),worldY(bend.lat)};
        }
        line.points[line.count++]=cityMapPoint(road.to,campaign.year());
        return line;
    }
    void prepareMapRoads() {
        const int year=campaign.year();
        if (year==mapRoadYear) return;
        mapRoadYear=year;
        for (size_t i=0;i<std::size(roads);++i) {
            auto& line=mapRoads[i];
            line=routeLine(roads[i]);
            auto [left,top]=line.points[0];
            float right=left,bottom=top;
            for (const auto& [x,y]:std::span(line.points).first(line.count)) {
                left=std::min(left,x);right=std::max(right,x);
                top=std::min(top,y);bottom=std::max(bottom,y);
            }
            const int x=int(std::floor(left)),y=int(std::floor(top));
            line.bounds={x,y,int(std::ceil(right))-x+1,int(std::ceil(bottom))-y+1};
        }
    }
    struct MapProjection {
        float zoom,left,top;
        Rect visible;
    };
    MapProjection mapProjection() const {
        const float zoom=mapScale(),left=mapLeft(),top=mapTop();
        const int x=int(std::floor((mapWindow.x-2-left)/zoom));
        const int y=int(std::floor((mapWindow.y-2-top)/zoom));
        const int right=int(std::ceil((mapWindow.x+mapWindow.w+2-left)/zoom));
        const int bottom=int(std::ceil((mapWindow.y+mapWindow.h+2-top)/zoom));
        return {zoom,left,top,{x,y,right-x,bottom-y}};
    }
    void path(pxa::game::Frame& f,const Road& road,const MapProjection& projection,uint16_t color,int stroke) {
        const auto& line=mapRoads[&road-roads];
        if (!line.bounds.overlaps(projection.visible)) return;
        for (int i=1;i<line.count;++i) {
            auto [x,y]=line.points[i-1];
            auto [u,v]=line.points[i];
            drawSegment(f,int(std::round(projection.left+x*projection.zoom)),int(std::round(projection.top+y*projection.zoom)),
                          int(std::round(projection.left+u*projection.zoom)),int(std::round(projection.top+v*projection.zoom)),color,stroke);
        }
    }
    art::Tile mapTile(int n) const {
        return sansText?art::mapTiles[n]:art::compactMapTiles[n];
    }
    Rect mapTileCrop(int n) const {
        constexpr int w=worldWidth/2,h=worldHeight/2;
        const int wx=(n%2)*w,wy=(n/2)*h;
        const auto m=mapScale();
        const int l=std::clamp(int(std::ceil((mapWindow.x+1-mapLeft())/m))-wx,0,w);
        const int r=std::clamp(int(std::floor((mapWindow.x+mapWindow.w-1-mapLeft())/m))-wx,0,w);
        const int t=std::clamp(int(std::ceil((mapWindow.y+1-mapTop())/m))-wy,0,h);
        const int b=std::clamp(int(std::floor((mapWindow.y+mapWindow.h-1-mapTop())/m))-wy,0,h);
        return {l,t,r-l,b-t};
    }
    void mapArt(pxa::game::Frame& f) {
        const Rect p=toPixels(mapWindow);
        const Rect interior{p.x+1,p.y+1,p.w-2,p.h-2};
        around(p,interior,[&](Rect r) {blockPixels(f,r,edge);});
        int left=p.x+p.w,top=p.y+p.h,right=p.x,bottom=p.y;
        constexpr int w=worldWidth/2,h=worldHeight/2;
        const float zoom=mapScale(),originX=mapLeft(),originY=mapTop();
        for (int n=0;n<4;++n) {
            const Rect crop=mapTileCrop(n);
            if (crop.w<=0 || crop.h<=0) continue;
            const auto base=mapTile(n);
            const int l=(crop.x*base.w+w/2)/w,r=((crop.x+crop.w)*base.w+w/2)/w;
            const int t=(crop.y*base.h+h/2)/h,b=((crop.y+crop.h)*base.h+h/2)/h;
            if (l>=r || t>=b) continue;
            const int wx=(n%2)*w+crop.x,wy=(n/2)*h+crop.y;
            const int x=sx(originX+wx*zoom),y=sy(originY+wy*zoom);
            const int endX=sx(originX+(wx+crop.w)*zoom),endY=sy(originY+(wy+crop.h)*zoom);
            if (x>=endX || y>=endY) continue;
            const Rect dst{x,y,endX-x,endY-y};
            tilePixels(f,{base.page,uint16_t(l),uint16_t(t),uint16_t(r-l),uint16_t(b-t)},dst);
            left=std::min(left,dst.x); top=std::min(top,dst.y);
            right=std::max(right,dst.x+dst.w); bottom=std::max(bottom,dst.y+dst.h);
        }
        const auto fill=[&](Rect r) {blockPixels(f,r,rgb(25,44,43));};
        if (left>=right || top>=bottom) fill(interior);
        else {
            // The four tiles share destination edges, so only the margins need a fill.
            left=std::max(left,interior.x); top=std::max(top,interior.y);
            right=std::min(right,interior.x+interior.w); bottom=std::min(bottom,interior.y+interior.h);
            around(interior,{left,top,right-left,bottom-top},fill);
        }
    }
    MapPoint marchPosition(Order order,float progress) const {
        const auto* road=findRoad(order);
        if (!road) return cityMapPoint(order.from,campaign.year());
        const auto line=routeLine(*road);
        std::array<float,roadBends+1> distances{};
        float total=0;
        for (int i=1;i<line.count;++i) {
            auto [x,y]=line.points[i-1];
            auto [u,v]=line.points[i];
            const float dx=u-x,dy=v-y;
            distances[i-1]=std::sqrt(dx*dx+dy*dy);
            total+=distances[i-1];
        }
        float distance=total*(order.from==road->from?progress:1-progress);
        for (int i=1;i<line.count;++i) {
            const float length=distances[i-1];
            if (distance<=length && length>0) {
                auto [x,y]=line.points[i-1];
                auto [u,v]=line.points[i];
                const float fraction=distance/length;
                return {x+(u-x)*fraction,y+(v-y)*fraction};
            }
            distance-=length;
        }
        return line.points[line.count-1];
    }
    void armyOnMap(pxa::game::Frame& f,Order order,const MapProjection& projection,bool own,float t) {
        if (!order) return;
        const auto* road=findRoad(order);
        const bool atSea=road && road->coastal;
        if (road) path(f,*road,projection,own?(atSea?seaRouteActive:mint):coral,2);
        auto [u,v]=marchPosition(order,t);
        int x=mapX(u),y=mapY(v);
        if (!mapWindow.contains(x-15,y-25)||!mapWindow.contains(x+15,y+9)) return;
        int pose=1+marchingMs/115%2;
        const int motion=pose==1?1:0;
        if (atSea) {
            contain(f,art::mapProps[coastalBoat],{x-16,y-20-motion,32,24},true);
        } else {
            const bool flip=marchPosition(order,std::min(1.f,t+.02f)).first<u;
            block(f,{x-14,y+1,28,6},ink);
            tile(f,art::marchers[!own][flip][pose],{x-16,y-24-motion,32,28},true);
        }
        uint16_t flag=own?mint:coral;
        block(f,{x-7,y-24-motion,1,11},ivory);
        block(f,{x-6,y-24-motion,10,5},flag);
        gauge(f,{x-13,y+4,26,5},marchingMs,travelDuration(),own?blueMarchGauge:redMarchGauge);
    }
    static constexpr int mapLabelSize=12;
    static constexpr Rect cityMarker{-8,-7,17,15};
    static constexpr int mapLandmarks[]={0,1,2,3,4,5,6,7,8,10,13,22,24,25,26,28,29,32,34,36,37,38,40,42};

    bool insideMap(Rect r) const {
        return mapWindow.contains(r.x,r.y)&&mapWindow.contains(r.x+r.w-1,r.y+r.h-1);
    }
    static float clearAtScale(Rect label,Rect obstacle,MapPoint delta,float from) {
        // Find the last scale where the projected boxes can intersect.
        float enter=from,exit=std::numeric_limits<float>::infinity();
        auto intersectAxis=[&](int first,int past,float distance) {
            if (distance==0) return first<0 && past>0;
            float near=first/distance,far=past/distance;
            if (near>far) std::swap(near,far);
            enter=std::max(enter,near);
            exit=std::min(exit,far);
            return enter<exit;
        };
        // Include one logical pixel for rounding each projected city position.
        if (!intersectAxis(label.x-obstacle.x-obstacle.w-1,label.x+label.w-obstacle.x+1,delta.first) ||
            !intersectAxis(label.y-obstacle.y-obstacle.h-1,label.y+label.h-obstacle.y+1,delta.second) ||
            enter>maxMapScale) return 0;
        return exit;
    }
    void planMapLabels(int year) {
        if (labelFontScale==scale && labelYear==year*4+campaign.season()) return;
        labelFontScale=scale;
        labelYear=year*4+campaign.season();
        cityLabelPlacements={};
        std::array<MapPoint,cityCount> centers;
        for (int i=0;i<cityCount;++i) centers[i]=cityMapPoint(i,year);
        const float fit=overviewScale();
        const int h=int(std::ceil((fontHeight(mapLabelSize)+2)/scale));
        auto plan=[&](int city) {
            const int w=int(std::ceil((textWidth(cityName(city),mapLabelSize)+3)/scale));
            const Rect choices[]={{-w/2,-h-8,w,h},{-w/2,9,w,h},{10,-h/2,w,h},{-w-10,-h/2,w,h}};
            auto& placement=cityLabelPlacements[city];
            placement={choices[0],std::numeric_limits<float>::infinity()};
            for (auto offset:choices) {
                float visible=fit;
                for (int j=0;j<cityCount;++j) {
                    if (j==city) continue;
                    const MapPoint delta{centers[j].first-centers[city].first,centers[j].second-centers[city].second};
                    visible=std::max(visible,clearAtScale(offset,cityMarker,delta,fit));
                    const auto& previous=cityLabelPlacements[j];
                    if (previous.offset.w && previous.minScale<=maxMapScale)
                        visible=std::max(visible,clearAtScale(offset,previous.offset,delta,previous.minScale));
                }
                if (visible<placement.minScale) placement={offset,visible};
            }
        };
        for (int city:mapLandmarks) plan(city);
        for (int i=0;i<cityCount;++i)
            if (!cityLabelPlacements[i].offset.w) plan(i);
    }
    Rect visibleMapLabel(int city,Rect islandLabel) const {
        Rect name=cityLabelPlacements[city].offset;
        const auto [x,y]=cityCenters[city];
        name.x+=x;
        name.y+=y;
        if (!insideMap(name) || name.overlaps(mapCompass) ||
            (fullAtlas() && (name.overlaps(atlasBack) || name.overlaps(atlasCity) || name.overlaps(atlasNames) ||
             name.overlaps(atlasMinus.shiftY(tallScene)) || name.overlaps(atlasPlus.shiftY(tallScene)))) ||
            (islandLabel.w && name.overlaps(islandLabel))) return {};
        return name;
    }
    void layoutMap(Rect islandLabel) {
        const int year=campaign.year();
        if (showMapNames) planMapLabels(year);
        const float zoom=mapScale(),left=mapLeft(),top=mapTop();
        mapCompass={mapWindow.x+mapWindow.w-47,mapWindow.y+mapWindow.h-51,42,46};
        mapLabels={};
        for (int i=0;i<cityCount;++i) {
            const auto [u,v]=cityMapPoint(i,year);
            cityCenters[i]={int(std::round(left+u*zoom)),int(std::round(top+v*zoom))};
        }
        // The Han River separates these two garrisons by less than a marker width.
        // Spread the gates north/south while road geometry stays at the true anchors.
        const int gap=cityCenters[6].second-cityCenters[fancheng].second;
        cityCenters[fancheng].second-=std::max(0,(cityMarker.h-gap+1)/2);
        cityCenters[6].second+=std::max(0,(cityMarker.h-gap)/2);
        for (int i=0;i<cityCount;++i) {
            const auto [x,y]=cityCenters[i];
            const Rect marker{x+cityMarker.x,y+cityMarker.y,cityMarker.w,cityMarker.h};
            mapMarkers[i]=insideMap(marker)?marker:Rect{};
            if (showMapNames && mapMarkers[i].w &&
                (i==selected || i==destination || zoom>=cityLabelPlacements[i].minScale))
                mapLabels[i]=visibleMapLabel(i,islandLabel);
        }
        if (!showMapNames) return;
        // Give selected names priority without moving neighbouring labels.
        for (int i=0;i<cityCount;++i)
            if (i!=selected && i!=destination && mapLabels[i].w &&
                ((mapLabels[selected].w && mapLabels[i].overlaps(mapLabels[selected])) ||
                 (destination>=0 && mapLabels[destination].w && mapLabels[i].overlaps(mapLabels[destination]))))
                mapLabels[i]={};
    }
    void drawCompass(pxa::game::Frame& f) {
        const auto r=mapCompass;
        if (!r.w) return;
        const bool canCenter=mapZoom && !isMarching();
        const bool focused=canCenter && registerControl(r,Command::centerMap,0);
        battlePanel(f,r,focused?jadePlaque:bronzePlaque);
        contain(f,art::mapProps[sinan],{r.cx()-11,r.y+5,22,22},true);
        boxText(f,canCenter?"回城":"南",{r.x+4,r.y+27,r.w-8,16},12,gold,1);
    }
    struct CityMarkers {
        std::array<pxa::game::Sprite,cityCount> gates;
        std::array<Rect,cityCount> stripes;
        std::array<uint16_t,cityCount> colors;
        size_t count=0;
    };
    void flushCityMarkers(pxa::game::Frame& f,CityMarkers& markers) const {
        if (markers.count==0) return;
        f.sprites({uint8_t(slots[art::mapProps[cityGate].page])},std::span(markers.gates).first(markers.count));
        for (size_t i=0;i<markers.count;++i) blockPixels(f,markers.stripes[i],markers.colors[i]);
        markers.count=0;
    }
    void drawCity(pxa::game::Frame& f,CityMarkers& markers,int i,Rect islandLabel,bool threatened) {
        if (!mapMarkers[i].w) return;
        const auto [x,y]=cityCenters[i];
        const int owner=campaign.cities[i].owner;
        const bool active=i==selected || i==destination;
        const bool emph=!isMarching() && registerControl({x-9,y-7,19,15},Command::select,i);
        const auto target=toPixels({x-7,y-8,13,15});
        if (target.w<=0 || target.h<=0) return;
        if (active || emph || threatened) {
            flushCityMarkers(f,markers);
            frame(f,{x-8,y-7,17,15},emph?ivory:threatened?coral:i==destination?mint:gold,ink);
        } else {
            for (const auto& gate:std::span(markers.gates).first(markers.count)) {
                if (target.overlaps({gate.x,gate.y,gate.width,gate.height})) {
                    // A later icon must cover the previous city's stripe in dense areas.
                    flushCityMarkers(f,markers);
                    break;
                }
            }
        }
        constexpr auto gate=art::mapProps[cityGate];
        const auto index=markers.count++;
        markers.gates[index]=slice(gate,target,0,0,gate.w,gate.h);
        markers.stripes[index]=toPixels({x-5,y+5,10,2});
        markers.colors[index]=colors[owner];
        if (showMapNames && emph && !mapLabels[i].w) mapLabels[i]=visibleMapLabel(i,islandLabel);
        const Rect name=mapLabels[i];
        if (!name.w) return;
        flushCityMarkers(f,markers);
        text(f,cityName(i),name.cx(),name.cy()-mapLabelSize/2,mapLabelSize,
             emph?ivory:i==destination?mint:active?gold:ivory,1,ink);
    }
    void drawCityPanel(pxa::game::Frame& f,const MarchRoute& route) {
        char buf[64];
        int inspected=destination<0?selected:destination;
        auto city=campaign.cities[inspected];
        skinFrame(f,cityBounds());
        const auto actions=[&](Rect r) {return r.shiftY(mapExtra());};
        boxText(f,cityName(inspected),{287,46,47,21},18,gold);
        boxText(f,terrains[sites[inspected].terrain],{336,47,32,20},12,dim,2);
        boxText(f,districtName(inspected),{287,69,81,15},12,dim);
        boxText(f,ownerName(city.owner),{287,84,81,16},13,colors[city.owner]);
        std::snprintf(buf,sizeof buf,"守军 %d",city.troops);
        boxText(f,buf,{287,100,81,15},13);
        std::snprintf(buf,sizeof buf,"%d",city.fields);
        iconText(f,farmIcon,buf,{287,115,37,15},12,ivory);
        std::snprintf(buf,sizeof buf,"%d",city.fort);
        iconText(f,wallsIcon,buf,{328,115,37,15},12,ivory);
        block(f,{287,130,81,1},edge);

        const bool editable=!isMarching();
        if (city.owner==campaign.player && destination<0) {
            button(f,actions({286,134,83,22}),"征募 80",Command::work,recruit,editable&&campaign.canWork(selected,recruit),true,recruitIcon);
            button(f,actions({286,158,83,24}),"屯田 95",Command::work,farm,editable&&campaign.canWork(selected,farm),false,farmIcon);
            button(f,actions({286,185,55,24}),"内政",Command::cityDesk,0,editable);
        } else {
            const bool transfer=city.owner==campaign.player;
            const bool ally=campaign.allied(city.owner);
            if (route) {
                std::snprintf(buf,sizeof buf,"可派 %d",campaign.own(selected)?campaign.dispatchTo(selected,inspected):0);
                boxText(f,buf,{287,132,81,15},13,dim,1);
                if (route.legs()>1) {
                    const int via=campaign.alliedWaypoint(route);
                    std::snprintf(buf,sizeof buf,"经%s",cityName(via>=0?via:route.cities[1]));
                    boxText(f,buf,{287,147,81,15},12,mint,1);
                } else if (const auto* road=findRoad(route.leg(0))) {
                    boxText(f,road->name?road->name:"区域通路",{287,147,81,15},12,
                            road->coastal?seaRouteActive:gold,1);
                }
            } else if (ally) boxText(f,"可借道通行",{287,137,81,20},12,mint,1);
            else if (!transfer && !campaign.ceasefire(city.owner))
                boxText(f,"道路不通",{287,137,81,20},12,dim,1);
            if (transfer)
                button(f,actions({286,163,83,20}),"设为起点",Command::origin,inspected,editable);
            else if (campaign.ceasefire(city.owner)) {
                std::snprintf(buf,sizeof buf,"%s%d季",ally?"结盟":"休战",
                              ally?campaign.alliance[city.owner]:campaign.truce[city.owner]);
                boxText(f,buf,{287,165,81,17},12,gold,1);
            } else if (campaign.canAskAlly(inspected)) {
                const bool planned=campaign.alliedRequest.target==inspected;
                button(f,actions({286,163,83,20}),planned?"盟军待发":"请盟出兵",
                       Command::alliedOffensive,inspected,editable,planned);
            }
            if (!campaign.ceasefire(city.owner))
                button(f,actions({286,185,55,24}),transfer?"调兵":"出征",Command::march,inspected,
                       editable&&campaign.routeLeader(route)>=0,true);
        }
        button(f,actions({344,185,25,24}),"",Command::gazetteer,inspected,editable,false,gazetteerIcon);
    }
    MapRoutes calculateMapRoutes() const {
        MapRoutes routes{};
        if (!isMarching() && destination>=0) routes.preview=campaign.marchRoute(selected,destination);
        for (size_t i=0;i<campaign.columns.size();++i) {
            const auto& unit=campaign.columns[i];
            if (!unit || !unit.eta) continue;
            const auto route=campaign.marchRoute(unit.road.from,unit.road.to);
            if (route) routes.camps[i]=marchPosition(route.leg(route.legs()-1),.65f);
        }
        return routes;
    }
    void drawMap(pxa::game::Frame& f) {
        mapBackdrop(f);
        char buf[60];
        if (!fullAtlas()) {
            const auto* era=historicalEra(campaign.player,campaign.year(),campaign.season());
            if (era) {
                const int eraYear=campaign.year()-era->year+1;
                if (eraYear==1)
                    std::snprintf(buf,sizeof buf,"%s元年 %s",era->name,seasons[campaign.season()]);
                else std::snprintf(buf,sizeof buf,"%s%d年 %s",era->name,eraYear,seasons[campaign.season()]);
            } else std::snprintf(buf,sizeof buf,"%d年 %s",campaign.year(),seasons[campaign.season()]);
            const Rect date=mapResources[0];
            const bool focused=registerControl(date,Command::chronicle,0,!isMarching());
            plaque(f,date,true,focused);
            boxText(f,buf,{date.x+5,date.y+1,date.w-10,15},12,gold,1);
            const int latest=latestChronicle(campaign.year(),campaign.season());
            const bool fresh=latest>chronicleRead && chronicle[latest].year==campaign.year() &&
                             chronicle[latest].season==campaign.season();
            boxText(f,fresh?"新纪事":"纪事",{date.x+6,date.y+15,date.w-12,12},11,fresh?mint:ivory,1);
            std::snprintf(buf,sizeof buf,"%d",campaign.gold);
            resourcePlate(f,mapResources[1],coinsIcon,buf,gold);
            std::snprintf(buf,sizeof buf,"%d",campaign.grain);
            resourcePlate(f,mapResources[2],grainIcon,buf);
            std::snprintf(buf,sizeof buf,"%d/%d",campaign.actions,campaign.actionCapacity());
            resourcePlate(f,mapResources[3],orderIcon,buf,mint);
        }

        const auto threat=isMarching()?Order{}:campaign.nextAttack();
        const auto alliedOrder=campaign.alliedOrder();
        if (!mapRoutes) mapRoutes=calculateMapRoutes();
        const auto& route=playerRoute?playerRoute:mapRoutes->preview;
        mapArt(f);
        prepareMapRoads();
        const auto projection=mapProjection();
        for (const auto& r:roads) {
            const bool near=selected==r.from || selected==r.to;
            if (near && !isMarching()) path(f,r,projection,ink,2);
            const auto color=r.coastal?(near?seaRouteActive:seaRoute):(near?gold:rgb(129,121,81));
            path(f,r,projection,color,1);
        }
        if (route && (!isMarching() || route.legs()>1))
            for (int i=0;i<route.legs();++i)
                if (const auto* road=findRoad(route.leg(i))) {
                    path(f,*road,projection,ink,3);
                    path(f,*road,projection,road->coastal?seaRouteActive:mint,1);
                }
        if (alliedOrder)
            if (const auto* road=findRoad(alliedOrder)) path(f,*road,projection,mint,2);
        if (threat) {
            for (int j=1;j<6;++j) {
                auto [a,b]=marchPosition(threat,j/6.f);
                const int x=mapX(a),y=mapY(b);
                if (mapWindow.contains(x-2,y-2) && mapWindow.contains(x+2,y+2))
                    block(f,{x-1,y-1,3,3},coral);
            }
        }
        Rect islandLabel{};
        if (showMapNames) {
            const int x=mapX(worldX(121.04f)),y=mapY(worldY(23.65f));
            const int half=int(std::ceil(textWidth("台湾岛",12)/(2*scale)))+2;
            islandLabel={x-half,y-8,half*2,16};
            if (!insideMap(islandLabel)) islandLabel={};
        }
        layoutMap(islandLabel);
        CityMarkers markers;
        for (int i=0;i<cityCount;++i)
            if (i!=selected && i!=destination) drawCity(f,markers,i,islandLabel,i==threat.to);
        drawCity(f,markers,selected,islandLabel,selected==threat.to);
        if (destination>=0) drawCity(f,markers,destination,islandLabel,destination==threat.to);
        flushCityMarkers(f,markers);
        if (islandLabel.w && !islandLabel.overlaps(mapCompass)) text(f,"台湾岛",islandLabel.cx(),islandLabel.cy()-6,12,dim,1,ink);
        for (const auto& position:mapRoutes->camps) {
            if (!position) continue;
            const auto [u,v]=*position;
            const int x=mapX(u),y=mapY(v);
            const Rect camp{x-9,y-12,19,17};
            if (!insideMap(camp)) continue;
            frame(f,camp,mint,ink);
            contain(f,art::mapProps[armyTent],{camp.x+2,camp.y+1,camp.w-4,camp.h-2},true);
        }
        if (playerRoute) {
            const auto [leg,progress]=playerMarchLeg();
            armyOnMap(f,leg,projection,true,progress);
        } else if (seasonMarch) {
            const float progress=float(marchingMs)/travelDuration();
            for (auto column:marching) if (column)
                armyOnMap(f,column,projection,campaign.allied(campaign.cities[column.from].owner),progress);
        }
        drawCompass(f);
        if (fullAtlas()) {
            const int inspected=destination>=0?destination:selected;
            std::snprintf(buf,sizeof buf,"%s · 城志",cityName(inspected));
            button(f,atlasBack,"返回城务",Command::atlas);
            button(f,atlasCity,buf,Command::gazetteer,inspected,true,false,gazetteerIcon);
            button(f,atlasNames,"城名",Command::mapNames,0,true,showMapNames,namesIcon);
            button(f,atlasMinus.shiftY(tallScene),"−",Command::zoom,-1,mapZoom>0);
            button(f,atlasPlus.shiftY(tallScene),"+",Command::zoom,1,mapZoom<2);
            return;
        }
        drawCityPanel(f,route);

        const auto footer=[&](Rect r) {return r.shiftY(mapExtra());};
        const bool twoRows=wrapSquareControls();
        battlePanel(f,footer(mapStatus));
        if (isMarching()) {
            count=0;
            focus=0;
            if (playerRoute) {
                const auto [from,to]=playerRoute.endpoints();
                if (const int via=campaign.alliedWaypoint(playerRoute);via>=0)
                    std::snprintf(buf,sizeof buf,"%s → %s · 经%s",cityName(from),cityName(to),cityName(via));
                else std::snprintf(buf,sizeof buf,"%s → %s",cityName(from),cityName(to));
            }
            else if (alliedOrder)
                std::snprintf(buf,sizeof buf,"盟军出击   %s → %s",cityName(alliedOrder.from),cityName(alliedOrder.to));
            else std::snprintf(buf,sizeof buf,"诸军正在进发");
            boxText(f,buf,footer({17,222,350,17}),12,playerRoute||alliedOrder?mint:coral);
            gauge(f,footer({12,twoRows?263:248,253,9}),
                  marchingMs,travelDuration(),blueMarchGauge);
            button(f,footer(twoRows?Rect{274,254,102,27}:Rect{274,243,102,19}),
                   "加速抵达",Command::skipMarch,0,true,true,horseIcon);
            return;
        }
        const char* status=nullptr;
        uint16_t tone=dim;
        switch (notice) {
            case Notice::recruited: status="征募完成，补充守军"; break;
            case Notice::cultivated: status="屯田完成，下季增粮"; break;
            case Notice::fortified: status="城防提升"; break;
            case Notice::moved: status="援军已抵达"; break;
            case Notice::income: status="钱粮入库"; break;
            case Notice::saved: status="战役已续上"; break;
            case Notice::saveError: status="存档写入失败"; tone=coral; break;
            case Notice::treaty: status="休战约定已立"; break;
            case Notice::allied: status="盟约已立，可借道通行";tone=mint;break;
            case Notice::supplies: status="商粮已入库"; break;
            case Notice::levies: status="边城乡勇已归队"; break;
            case Notice::civic: status="城中内政已更新"; break;
            case Notice::encamped: status="军队已扎营，次季抵达"; tone=mint; break;
            case Notice::withdrawn: status="围城军已撤回"; break;
            case Notice::routeBlocked: status="道路不通，军队已撤回";tone=coral;break;
            case Notice::goal: status="剧本悬赏已达成"; tone=gold; break;
            case Notice::prisonNews:
                std::snprintf(buf,sizeof buf,"敌逃%d将%d兵  我归%d将%d兵",prisonNews.escaped,prisonNews.enemiesFled,
                              prisonNews.returned,prisonNews.troopsReturned);
                status=buf; tone=gold; break;
            case Notice::none: break;
        }
        if (destination>=0 && (notice==Notice::none || notice==Notice::income)) {
            const int owner=campaign.cities[destination].owner;
            if (route) {
                const auto* first=findRoad(route.leg(0));
                if (route.legs()>1)
                    std::snprintf(buf,sizeof buf,"%s → %s · %d段",cityName(selected),cityName(destination),route.legs());
                else
                    std::snprintf(buf,sizeof buf,"%s → %s · %s",cityName(selected),cityName(destination),
                                  first && first->name?first->name:"直达");
                tone=mint;
            } else {
                const char* why=campaign.allied(owner)?"盟城可借道":campaign.ceasefire(owner)?"休战中":"道路不通";
                std::snprintf(buf,sizeof buf,"%s · %s",cityName(destination),why);
                tone=campaign.ceasefire(owner)?dim:coral;
            }
            status=buf;
        } else if (notice==Notice::none || notice==Notice::income) {
            if (const auto request=campaign.alliedRequest;request) {
                if (alliedOrder)
                    std::snprintf(buf,sizeof buf,"%s将攻%s",factions[request.side].title,cityName(request.target));
                else std::snprintf(buf,sizeof buf,"盟军暂缓出击%s",cityName(request.target));
                status=buf;
                tone=alliedOrder?mint:dim;
            } else if (const auto report=campaign.alliedReport;report.request) {
                const char* target=cityName(report.request.target);
                if (report.outcome==AlliedOutcome::captured)
                    std::snprintf(buf,sizeof buf,"%s攻取%s",factions[report.request.side].title,target);
                else if (report.outcome==AlliedOutcome::repelled)
                    std::snprintf(buf,sizeof buf,"盟军攻%s受挫",target);
                else std::snprintf(buf,sizeof buf,"盟军取消出兵，粮草已退");
                status=buf;
                tone=report.outcome==AlliedOutcome::captured?mint:gold;
            }
        }
        if (status) boxText(f,status,footer({17,222,threat?244:350,17}),12,tone);
        else {
            std::snprintf(buf,sizeof buf,"领地 %d/%d",campaign.holdings(campaign.player),cityCount);
            boxText(f,buf,footer({17,222,threat?83:93,17}),12,ivory);
            block(f,footer({threat?101:112,225,1,11}),edge);
            boxText(f,"季末",footer({threat?105:119,222,threat?35:42,17}),12,dim,1);
            for (int i=0;i<2;++i) {
                std::snprintf(buf,sizeof buf,i?"+%d粮":"+%d金",campaign.income(i!=0));
                if (threat) boxText(f,buf,footer({141+61*i,222,60,17}),12,i?mint:gold);
                else {
                    const int x=168+98*i;
                    contain(f,art::commandIcons[i?grainIcon:coinsIcon],footer({x,224,13,13}),true);
                    boxText(f,buf,footer({x+16,222,78,17}),12,i?mint:gold);
                }
            }
        }
        if (threat) {
            const bool focus=registerControl(footer({266,222,102,17}),Command::inspectThreat,0);
            block(f,footer({265,224,1,13}),edge);
            contain(f,art::commandIcons[ordersIcon],footer({270,224,12,12}),true);
            std::snprintf(buf,sizeof buf,"%s告急",cityName(threat.to));
            boxText(f,buf,footer({284,222,83,17}),12,focus?gold:coral);
        }
        const auto control=[&](int slot,Rect original) {
            return footer(twoRows?Rect{8+93*(slot%4),243+29*(slot/4),89,26}:original);
        };
        button(f,control(4,{8,243,41,19}),"战法",Command::guide);
        button(f,control(5,{51,243,41,19}),"封卷",Command::home);
        button(f,control(6,{94,243,41,19}),"军议",Command::council);
        button(f,control(0,{137,243,27,19}),"−",Command::zoom,-1,mapZoom>0);
        button(f,control(1,{166,243,27,19}),"+",Command::zoom,1,mapZoom<2);
        button(f,control(2,{195,243,42,19}),"城名",Command::mapNames,0,true,showMapNames,namesIcon);
        button(f,control(3,{239,243,42,19}),"全图",Command::atlas);
        button(f,control(7,{283,243,93,19}),"季末军议",Command::season,0,true,true);
    }
    static art::Tile troopTile(int side,Troop troop,int pose=0) {
        return troop<shields?art::troops[side][troop][pose]:art::mixedForces[3*side+troop-shields];
    }
    void drawDeploy(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const auto destinationCity=campaign.cities[departure.to];
        const int enemy=campaign.cities[battle.defending?departure.from:departure.to].owner;
        const int lead=plannedLeader();
        const bool adviser=lead>=0 && battleRole(campaign.leader(lead))==Role::strategist;
        const auto* kit=lead>=0?&campaign.generals[lead].kit:nullptr;
        const Gear namedWeapon=kit?(*kit)[armsSlot]:noGear;
        header(f,battle.defending?"守城布阵":"出征布阵",cityName(departure.to));
        plaque(f,{12,39,360,38});
        if (lead>=0) portrait(f,campaign.leader(lead).portrait,{18,42,31,31});
        const int nameLeft=lead>=0?55:21;
        boxText(f,lead>=0?campaign.leader(lead).name:"城中守备",{nameLeft,42,125-nameLeft,17},14,mint);
        number(f,battle.defending?destinationCity.troops:campaign.columns[arrivingColumn].troops,nameLeft,60,13);
        if (lead>=0) boxText(f,roleNames[int(battleRole(campaign.leader(lead)))],{91,59,35,16},12,dim,2);
        const int foeIndex=enemy<3?campaign.enemyOfficer(enemy,departure.to):-1;
        const auto* foe=foeIndex>=0?&officers[enemy][foeIndex]:nullptr;
        if (foe) portrait(f,foe->portrait,{336,42,31,31});
        const int nameRight=foe?330:361;
        boxText(f,foe?foe->name:ownerName(enemy),{257,42,nameRight-257,17},14,coral,2);
        number(f,battle.defending?campaign.dispatch(departure.from):destinationCity.troops,
               foe?329:361,59,13,ivory,2);
        const auto threat=Battle::enemyUnit(departure.to,enemy,foe);
        const bool siege=arrivingColumn>=0 && campaign.columns[arrivingColumn].blockade;
        char desc[58];
        const auto enemyLine=Battle::enemyFormationFor(departure.to,destinationCity.fort,
                                                       battle.defending,threat,campaign.difficulty);
        const auto enemyForces=Battle::enemyDeployment(departure.to,destinationCity.fort,battle.defending,
                                                      threat,campaign.difficulty);
        boxText(f,"敌军",{135,42,34,17},13,coral);
        for (int lane=0;lane<Battle::rankCount;++lane)
            contain(f,iconTile(troopIcons[enemyForces[lane]]),{175+23*lane,42,17,17},true);
        std::snprintf(desc,sizeof desc,"%s · %s%d级",terrains[sites[departure.to].terrain],
                      siege?"有效城防":"城防",std::max(0,destinationCity.fort-(siege?2:0)));
        boxText(f,desc,{128,59,128,16},12,dim,1);

        button(f,{12,80,97,23},"编队",Command::deployTab,0,true,!deploymentKit);
        button(f,{113,80,116,23},"阵法军械",Command::deployTab,1,true,deploymentKit);
        const bool engineAvailable=Battle::mayBringEngine(destinationCity.fort,battle.defending,siege);
        if (deploymentKit && engineAvailable) {
            std::snprintf(desc,sizeof desc,"器械随%s",rankNames[deployment.engineLane]);
            button(f,{250,80,120,23},desc,Command::cycleEngineLane);
        } else {
            std::snprintf(desc,sizeof desc,"敌阵 · %s",formations[enemyLine]);
            battlePanel(f,{234,80,137,23});
            boxText(f,desc,{242,82,121,19},12,dim,1);
        }
        if (!deploymentKit) {
            for (int lane=0;lane<Battle::rankCount;++lane) {
                std::snprintf(desc,sizeof desc,"%s · %s",rankNames[lane],troopNames[deployment.lineup[lane]]);
                button(f,{12+121*lane,107,117,24},desc,Command::deployLane,lane,true,lane==deploymentLane);
            }
            const auto opposing=enemyForces[deploymentLane];
            const auto troopEdge=[&](Troop troop) {
                return Battle::advantage(troop,opposing)-Battle::advantage(opposing,troop);
            };
            for (int i=0;i<troopCount;++i) {
                const Rect card{12+121*(i%3),135+40*(i/3),117,37};
                const bool focused=registerControl(card,Command::pickTroop,i);
                plaque(f,card,true,i==deployment.lineup[deploymentLane] || focused);
                tile(f,troopTile(0,Troop(i)),{card.x+4,card.y+1,34,34},true);
                boxText(f,troopNames[i],{card.x+40,card.y+2,72,16},14,ivory);
                boxText(f,troopHints[i],{card.x+40,card.y+19,72,16},12,
                        troopEdge(Troop(i))>0?mint:dim);
            }
            const auto troop=deployment.lineup[deploymentLane];
            const int matchup=troopEdge(troop);
            std::snprintf(desc,sizeof desc,"%s %s 对 %s · %s",rankNames[deploymentLane],troopNames[troop],
                          troopNames[opposing],matchup>0?"占优":matchup<0?"居下风":"相当");
            battlePanel(f,{12,213,359,18});
            boxText(f,desc,{19,214,345,16},12,matchup>0?mint:matchup<0?coral:dim,1);
        } else {
            battlePanel(f,{8,105,368,124});
            boxText(f,"阵法",{17,108,100,16},12,gold);
            boxText(f,formationHints[deployment.formation],{148,108,218,16},12,mint,2);
            for (int i=0;i<3;++i)
                button(f,{12+121*i,124,117,21},formations[i],Command::shift,i,true,
                       i==deployment.formation,formationIcons[i]);
            boxText(f,"武将兵器",{17,146,100,16},12,gold);
            char loadout[64]{};
            size_t used=0;
            for (const auto category:{armorSlot,mountSlot,strategySlot}) {
                if (!kit || (*kit)[category]==noGear) continue;
                const auto& item=gearSpecs[(*kit)[category]];
                const int written=std::snprintf(loadout+used,sizeof loadout-used,"%s%s",used?" · ":"",item.name);
                if (written<0 || size_t(written)>=sizeof loadout-used) break;
                used+=size_t(written);
            }
            const auto* weaponHint=lead<0?"没有领军武将":adviser?"军师可远射，后阵施策":weaponHints[deployment.weapon];
            boxText(f,namedWeapon==noGear && used?loadout:weaponHint,{125,146,241,16},12,dim,2);
            if (namedWeapon==noGear) {
                for (int i=0;i<3;++i)
                    button(f,{12+121*i,162,117,21},weaponNames[i],Command::pickWeapon,i,lead>=0 && (!adviser || i==longbow),
                           i==deployment.weapon,weaponIcons[i]);
            } else {
                battlePanel(f,{12,162,359,21},jadePlaque);
                boxText(f,gearSpecs[namedWeapon].name,{18,164,110,17},13,gold);
                boxText(f,used?loadout:"已装备 · 可在名册更换",{128,164,234,17},12,mint,2);
            }
            boxText(f,"攻城器械",{17,184,100,16},12,gold);
            const char* hint=!engineAvailable?"攻城且仍有城防时可带":
                deployment.engine==ram?"近城可撞两次 · 每次击破两格":
                deployment.engine==stoneThrower?"远投三次 · 压制城前守军":"每次耗14军令 · 每合一次";
            boxText(f,hint,{127,184,239,16},12,dim,2);
            for (int i=0;i<3;++i) {
                const Rect card{12+121*i,200,117,26};
                button(f,card,engineNames[i],Command::pickEngine,i,engineAvailable||i==0,i==deployment.engine);
                if (i) tile(f,art::mixedForces[5+i],{card.x+4,card.y+3,23,22},true);
            }
        }
        if (!battle.defending) button(f,{12,233,92,25},"撤军",Command::recall,arrivingColumn);
        else button(f,{12,233,92,25},"换将",Command::roster);
        const int force=battle.defending?destinationCity.troops:campaign.columns[arrivingColumn].troops;
        const auto lines=Battle::deployRanks(force,deployment.formation);
        std::snprintf(desc,sizeof desc,"%d / %d / %d",lines[0],lines[1],lines[2]);
        battlePanel(f,{107,233,131,25});
        boxText(f,desc,{114,235,117,21},12,mint,1);
        button(f,{241,231,130,28},"入场列阵",Command::engage,0,true,true);
    }

    int fieldX(int x) const {return x-int(std::round(battleCamera));}
    static bool fieldVisible(int x,int margin=0) {return x>=-margin && x<=sceneWidth+margin;}
    int laneOffset(int lane) const {return battleExtra()*(5+lane)/8;}
    int groundY(int lane) const {return std::max(167+19*lane+laneOffset(lane),fieldLand+14*lane);}
    int rankY(int lane) const {return 124+27*lane+laneOffset(lane);}
    bool hasFieldEffects() const {return fieldMotionArt && slots[art::battleEffects[0][0].page]>=0;}
    void contactShadow(pxa::game::Frame& f,int x,int foot,int w) const {
        block(f,{x-w/2,foot,w,2},rgb(64,49,33));
    }
    void dust(pxa::game::Frame& f,int x,int y,int direction,int phase) {
        if (hasFieldEffects()) {
            const int age=phase%14;
            const int point=x-direction*(5+age);
            if (age<12 && fieldVisible(point,14))
                tile(f,art::battleEffects[0][age/3],{point-13,y-11,26,13},true);
            return;
        }
        for (int i=0;i<2;++i) {
            const int age=(phase+i*7)%14;
            const int px=x-direction*(age*2+4);
            if (!fieldVisible(px) || age>10) continue;
            const int span=3+age/2;
            block(f,{px-span/2,y-1-age/3,span,age<7?2:1},
                  age<4?rgb(191,153,105):age<8?rgb(151,117,77):rgb(111,87,62));
        }
    }
    void sparks(pxa::game::Frame& f,int x,int y,int age,int count,bool rubble=false) {
        if (!fieldVisible(x,18) || age<0 || age>=12) return;
        if (hasFieldEffects() && !rubble) {
            const int size=std::clamp(13+count,18,32);
            tile(f,art::battleEffects[1][age/3],{x-size/2,y-size/2,size,size},true);
            return;
        }
        for (int i=0;i<count;++i) {
            const int direction=i%5-2;
            const int lift=1+i%3;
            const int px=x+direction*age*2+((i*13)%7-3);
            if (!fieldVisible(px)) continue;
            const int py=y-age*lift+age*age/6;
            const auto color=rubble?(i%2?rgb(208,170,117):rgb(145,111,77)):
                age<4?ivory:age<8?gold:rgb(176,99,53);
            if (!rubble && age<5 && i%2==0)
                line(f,px-direction*2,py+lift,px+direction,py-lift,color,1);
            else {
                const int size=age<7?2:1;
                block(f,{px,py,size,size},color);
            }
        }
    }
    void officerEffect(pxa::game::Frame& f,const Officer& actor,int side,int lane,int age) {
        const int direction=side?-1:1;
        const int origin=fieldX(side?battle.fronts[side][lane]+46:battle.generalPosition(lane,battle.atFront));
        const int target=fieldX(battle.fronts[1-side][lane]);
        const int y=groundY(lane)-27;
        const int wave=std::min(18,age);
        if (actor.power==crescentStrike) {
            const int x=origin+direction*std::min(76,12+age*6);
            if (hasFieldEffects()) {
                if (age<19 && fieldVisible(x,24))
                    facingTilePixels(f,art::battleEffects[2][std::min(3,age/4)],
                                     toPixels({x-26,y-35,52,61}),direction<0);
            } else {
                line(f,x,y-30,x+direction*13,y-17,mint,3);
                line(f,x+direction*13,y-17,x+direction*17,y+3,ivory,4);
                line(f,x+direction*17,y+3,x+direction*7,y+17,mint,3);
            }
            if (age>5) sparks(f,target,y,age-6,12);
        } else if (actor.power==bridgeRoar) {
            for (int i=0;i<3;++i) {
                const int x=origin+direction*(15+((age+i*7)%25)*5);
                line(f,x,y-25-i*3,x+11*direction,y,coral,2);
                line(f,x+11*direction,y,x,y+25+i*3,coral,2);
            }
        } else if (actor.power==eightArray || actor.power==rallyOath ||
                   actor.power==rallyGuard || actor.power==tigerBanner ||
                   actor.power==ironCommand || actor.power==eyeValor) {
            constexpr int around[][2]={{-2,0},{-2,-1},{-1,-2},{1,-2},{2,-1},{2,0},{1,1},{-1,1}};
            const uint16_t color=actor.power==eightArray?mint:
                actor.power==tigerBanner || actor.power==eyeValor?coral:gold;
            for (int row=0;row<Battle::rankCount;++row) {
                const int center=fieldX(battle.fronts[side][row]);
                if (!battle.ranks[side][row] || !fieldVisible(center,30)) continue;
                const int foot=groundY(row);
                if (actor.power==eightArray) {
                    for (int i=0;i<8;++i)
                        block(f,{center+around[i][0]*(6+wave/3),foot-13+around[i][1]*(3+wave/5),4,3},i%2?ivory:color);
                } else if (actor.power==tigerBanner || actor.power==ironCommand) {
                    const int top=foot-13-wave;
                    line(f,center,foot-4,center,top,ivory,2);
                    line(f,center,top,center+direction*(10+wave/3),top+4,color,4);
                } else {
                    line(f,center-8-wave,foot-4,center+8+wave,foot-4,color,2);
                    sparks(f,center,foot-12,age/2,6);
                }
            }
        } else if (actor.power==whiteSpear || actor.power==charge ||
                   actor.power==markVolley || actor.power==suddenRaid) {
            const int head=origin+(target-origin)*wave/18;
            const uint16_t color=actor.power==whiteSpear?ivory:actor.power==suddenRaid?coral:
                actor.power==markVolley || actor.portrait%2?gold:mint;
            for (int i=-1;i<=1;++i)
                line(f,head-direction*(25-i*5),y+i*8,head+direction*9,y+i*5,color,2);
            if (age>=14) sparks(f,target,y,age-14,7);
        } else if (actor.power==wildfire || actor.power==brushFire) {
            for (int i=0;i<8;++i) {
                const auto x=target+(i-4)*7+((age+i*2)%5);
                const int height=6+(age+i*7)%19;
                line(f,x,y+15,x+3,y+15-height,i%3?coral:gold,2);
            }
        } else sparks(f,origin,y,age/2,8+actor.portrait%5);
    }
    int battleCameraTarget() const {
        int left=Battle::fieldMargin,right=Battle::fieldLength-Battle::fieldMargin;
        for (int lane=0;lane<Battle::rankCount;++lane) {
            if (battle.ranks[0][lane]) left=std::max(left,battle.fronts[0][lane]);
            if (battle.ranks[1][lane]) right=std::min(right,battle.fronts[1][lane]);
        }
        return std::clamp(left+std::clamp((right-left)/2,18,48)-sceneWidth/2,0,fieldPanLimit);
    }
    int fieldArmyCenter(int side) const {
        int position=0,lanes=0;
        for (int lane=0;lane<Battle::rankCount;++lane) if (battle.ranks[side][lane]) {
            position+=battle.fronts[side][lane];
            ++lanes;
        }
        return lanes?position/lanes:side?Battle::fieldLength-Battle::campFront:Battle::campFront;
    }
    void viewBattleAt(int world) {
        followBattle=false;
        battleCamera=float(std::clamp(world-sceneWidth/2,0,fieldPanLimit));
        dirty=true;
        logState("battle-view");
    }
    void drawFieldMap(pxa::game::Frame& f,bool interactive) {
        constexpr Rect track=fieldTrack;
        battlePanel(f,fieldMapPanel);
        if (interactive)
            button(f,{14,67,65,21},followBattle?"随军":"回战线",Command::battleView,-1,true,followBattle);
        else boxText(f,"战线",{14,68,65,18},12,dim,1);
        const auto markerX=[&](int world) {return track.x+world*(track.w-4)/Battle::fieldLength;};
        const int windowLeft=markerX(int(battleCamera));
        const int windowRight=markerX(int(battleCamera)+sceneWidth)+3;
        block(f,track,rgb(36,28,23));
        block(f,{windowLeft,track.y,windowRight-windowLeft,track.h},rgb(78,60,38));
        block(f,{windowLeft,track.y,windowRight-windowLeft,1},gold);
        block(f,{windowLeft,track.y+track.h-1,windowRight-windowLeft,1},edge);
        if (battle.plan.engine!=noEngine && battle.wallPoints) {
            const int gate=markerX(Battle::gatePosition);
            const int first=markerX(Battle::gatePosition-battle.engineRange());
            block(f,{first,track.y+15,gate-first+1,1},battle.mayFireEngine()?gold:edge);
            block(f,{gate,track.y+2,2,14},gold);
        }
        for (int side=0;side<2;++side) if (battle.coordinating(side)) {
            const auto& support=battle.coordination(side);
            line(f,markerX(battle.fronts[side][support.source])+1,track.y+4+4*support.source,
                 markerX(battle.fronts[side][support.target])+1,track.y+4+4*support.target,side?coral:gold);
        }
        for (int lane=0;lane<Battle::rankCount;++lane)
            for (int side=0;side<2;++side)
                if (battle.ranks[side][lane])
                    block(f,{markerX(battle.fronts[side][lane]),track.y+3+4*lane,4,3},side?coral:mint);
        if (!interactive) return;
        button(f,{302,67,67,21},"观战",Command::fieldView);
        for (int i=0;i<3;++i) {
            const int left=track.w*i/3, right=track.w*(i+1)/3;
            const Rect part{track.x+left,track.y,right-left,track.h};
            if (registerControl(part,Command::battleView,i))
                block(f,{part.x,part.y+part.h-1,part.w,1},ivory);
        }
    }
    void army(pxa::game::Frame& f,int lane,int who) {
        const int current=battle.ranks[who][lane];
        if (!current) return;
        const Troop troop=battle.troopFor(who,lane);
        const Formation formation=who?battle.enemyFormation:battle.plan.formation;
        const Maneuver maneuver=battle.stance(who,lane);
        const int n=std::clamp(1+current*10/std::max(1,battle.initial[who]),1,4);
        const int forward=Battle::advanceDirection(who);
        const int lead=fieldX(battle.fronts[who][lane]);
        const int gap=formation==square?19:23;
        const int target=maneuver==fallBack?-1:battle.attackTarget(who,lane);
        const bool engaged=target>=0;
        const int heading=battle.rankFacing(who,lane,target);
        const int motion=battle.rankMotion[who][lane];
        const bool detailed=detailedArt && sansText;
        const bool largeFrame=troop<shields && detailedTroop(who?battle.enemyLineup:battle.plan.lineup)==troop;
        const auto& stances=largeFrame?art::largeTactics:art::tacticTroops;
        const bool moving=motion!=0;
        const bool mixedMotion=troop>=shields && hasFieldEffects();
        const int floor=groundY(lane);
        if (fieldVisible(lead,18) && moving && !engaged) {
            dust(f,lead-motion*15,floor,motion,battle.ticks+lane*4);
        }
        for (int i=n-1;i>=0;--i) {
            const int x=lead-forward*i*gap;
            if (!fieldVisible(x,24)) continue;
            const int foot=floor-i%2*3;
            const int stagger=(i*7+lane*4+who*3)%12;
            const int stride=battle.routed[who][lane]?2:4;
            int animation=moving?1+(battle.ticks/stride+stagger)%2:0;
            if (engaged) {
                const int action=(battle.roundClock+stagger)%Battle::period;
                animation=0;
                if (action<7) animation=4;
                else if (action<13) animation=5;
                else if (action>=31) animation=3;
                if (ranged(troop) && action<10 && i==0) {
                    const int end=fieldX(battle.fronts[1-who][target]);
                    const int shot=x+(end-x)*action/10;
                    const int height=foot-24+(groundY(target)-floor)*action/10-action%4;
                    line(f,shot,height,shot+heading*(troop==bow?9:13),height+1,gold,1);
                }
            }
            contactShadow(f,x,foot,troop==horse || troop==heavyHorse?26:18);
            auto sprite=mixedMotion?art::mixedTroopFrames[who][troop-shields][animation]:
                largeFrame?art::largeTroops[who][troop][animation]:
                detailed && troop<shields?art::detailTroops[who][troop][animation]:troopTile(who,troop,animation);
            if (detailed && troop<shields && (engaged || maneuver==hold || maneuver==press)) {
                const int phase=(engaged?battle.roundClock+stagger:battle.ticks*2+stagger)%Battle::period;
                if (maneuver==hold) sprite=stances[who][troop][phase<10?1:0];
                else if (maneuver==press) sprite=stances[who][troop][phase<11?3:2];
                else if (battle.round && phase<8 && battle.rankLosses[who][lane]>battle.rankLosses[1-who][lane]*2)
                    sprite=stances[who][troop][1];
            }
            groundedTile(f,sprite,x,foot,42,42,heading!=forward);
        }
    }
    void drawSiegeField(pxa::game::Frame& f) {
        if (battle.plan.engine==noEngine) return;
        const int gate=fieldX(Battle::gatePosition);
        const int x=fieldX(battle.enginePosition()),y=groundY(battle.plan.engineLane);
        const int gateFoot=groundY(2);
        if (battle.wallPoints && fieldVisible(gate,56))
            groundedTile(f,art::mixedForces[8],gate,gateFoot,100,96);
        if (battle.ranks[0][battle.plan.engineLane] && fieldVisible(x,32)) {
            contactShadow(f,x,y,35);
            groundedTile(f,art::mixedForces[5+battle.plan.engine],x,y,64,58);
        }
        if (!battle.siegeTicks) return;
        if (battle.plan.engine==stoneThrower && battle.siegeTicks>7) {
            const int t=23-battle.siegeTicks;
            const int rock=x+(gate-x)*t/16;
            const int height=y-40-(16-t)*t/2;
            if (fieldVisible(rock,4)) block(f,{rock-3,height-3,6,6},ivory);
        } else sparks(f,gate-10,gateFoot-34,battle.plan.engine==ram?(23-battle.siegeTicks)/2:7-battle.siegeTicks,9,true);
    }
    void drawFieldGeneral(pxa::game::Frame& f,int side,int lane) {
        const auto* person=side?battle.opponent():battle.commander>=0 && battle.generalHealth?&battle.officer():nullptr;
        if (!person) return;
        const int x=fieldX(side?battle.fronts[1][lane]+46:battle.generalPosition(lane,battle.atFront));
        if (!fieldVisible(x,23)) return;
        const int foot=groundY(lane)-1;
        const int y=foot-27;
        const auto job=battleRole(*person);
        const int portraitIndex=person->portrait-39;
        const int row=portraitIndex>=0 && portraitIndex<3?portraitIndex:
            job==Role::strategist?0:ranged(person->favored) && job==Role::vanguard?2:1;
        const bool action=side?battle.enemySkillTicks>0:battle.strikeTicks>0 || battle.effects>0;
        const int target=action?(side||!battle.strikeTicks?battle.targetRank(side,lane):battle.lastStrikeLane):-1;
        const int forward=Battle::advanceDirection(side);
        const int targetX=target>=0?fieldX(battle.fronts[1-side][target]):x;
        const int heading=targetX==x?battle.rankFacing(side,lane):targetX>x?1:-1;
        const int stride=battle.routed[side][lane]?3:5;
        const int signature=signatureFor(person);
        int pose=battle.rankMotion[side][lane]?1+(battle.ticks/stride)%2:0;
        if (action && zhaoAnimation(signature)) {
            const int age=side?22-battle.enemySkillTicks:
                          battle.strikeTicks?15-battle.strikeTicks:Battle::skillFrames-battle.effects;
            if (age<18) pose=age<4?3:age<11?4:5;
        } else if (action) pose=3;
        contactShadow(f,x,foot,signature>=0?25:20);
        const auto sprite=signature>=0?generalFrame(signature,side,pose):
            (detailedSharedLeaders()?art::fieldLeaders:art::compactFieldLeaders)[side][row][pose];
        const int size=signature>=0?54:46;
        groundedTile(f,sprite,x,foot,size,size,heading!=forward);
        if (!side && battle.strikeTicks) {
            const int travel=15-battle.strikeTicks;
            if (battle.plan.weapon==longbow) {
                const int flight=x+(targetX-x)*travel/14;
                const int height=y+(groundY(battle.lastStrikeLane)-groundY(battle.generalLane))*travel/14;
                line(f,flight,height+2,flight+19*heading,height+4,gold,2);
            } else line(f,x+(12+travel)*heading,y+12,x+(32+travel)*heading,y-6,gold,3);
        }
        if (!side) gauge(f,{x-13,foot+3,26,5},battle.generalHealth,100,jadeGauge);
    }
    void generalControls(pxa::game::Frame& f) {
        if (!battle.canMoveGeneral()) return;
        for (int lane=0;lane<3;++lane) for (int front=0;front<2;++front) {
            if (!battle.ranks[0][lane] || (lane==battle.generalLane && bool(front)==battle.atFront)) continue;
            const int center=fieldX(battle.generalPosition(lane,front));
            if (center<83 || center>301) continue;
            Rect zone{std::clamp(center-21,70,271),rankY(lane)+1,43,27};
            if (registerControl(zone,Command::moveGeneral,lane*2+front)) {
                battlePanel(f,zone,jadePlaque);
                boxText(f,front?"阵前":"后阵",zone.inset(4,4),11,gold,1);
            }
        }
    }
    void drawRanks(pxa::game::Frame& f,bool interactive) {
        for (int lane=0;lane<Battle::rankCount;++lane) {
            const int y=rankY(lane);
            const Rect own{8,y,60,24}, foe{316,y,60,24};
            const bool marked=interactive && registerControl(own,Command::battleRank,lane,!battle.result);
            const bool assisted=battle.coordinating() && battle.aid.target==lane;
            const bool led=battle.commander>=0 && battle.generalHealth && battle.generalLane==lane;
            battlePanel(f,own,led||marked||assisted||battle.rankOrders[lane].rounds?jadePlaque:bronzePlaque);
            if (led) block(f,{own.x+3,y+6,2,12},gold);
            const bool enemyAid=battle.coordinating(1) && battle.enemyAid.target==lane;
            battlePanel(f,foe,battle.enemyRank==lane||enemyAid?vermilionPlaque:bronzePlaque);
            char count[12];
            for (int side=0;side<2;++side) {
                const auto card=side?foe:own;
                const bool broken=battle.routed[side][lane] && battle.ranks[side][lane]>0;
                const auto icon=broken?routedIcon:troopIcons[battle.troopFor(side,lane)];
                contain(f,iconTile(icon),{card.x+5,y+3,17,17},true);
                std::snprintf(count,sizeof count,"%d",battle.ranks[side][lane]);
                const auto color=!battle.ranks[side][lane]?dim:broken?gold:side?coral:mint;
                boxText(f,count,{card.x+23,y+4,31,16},12,color,2);
            }
        }
    }

    void drawPause(pxa::game::Frame& f) {
        count=0;
        const auto center=[&](Rect r) {return r.shiftY(pageExtra()/2);};
        battlePanel(f,center({72,83,240,122}));
        if (retreatPrompt) {
            boxText(f,battle.defending?"确认弃城?":"确认撤军?",center({84,93,216,27}),20,coral,1);
            if (battle.exposed(0))
                boxText(f,"军势已乱，撤退仍可能被俘",center({84,120,216,18}),12,coral,1);
            button(f,center({87,144,101,30}),"返回",Command::back,0,true,true);
            button(f,center({196,144,101,30}),battle.defending?"弃城":"撤军",Command::confirmRetreat,0,true,false,withdrawIcon);
            return;
        }
        boxText(f,"会战暂停",center({84,93,216,27}),20,gold,1);
        button(f,center({87,132,101,28}),battle.awaitingOrders?"继续列阵":"继续战斗",Command::pause,0,true,true);
        button(f,center({196,132,101,28}),battle.defending?"弃城":"撤军",Command::retreat,0,true,false,withdrawIcon);
        button(f,center({142,168,100,25}),"音频",Command::audioMenu,0,true,false,musicIcon);
    }
    void combatCard(pxa::game::Frame& f,Rect card,const Officer* actor,
                    int value,int total,int morale,bool enemy,bool duel) {
        battlePanel(f,card,enemy?vermilionPlaque:jadePlaque);
        const Rect portraitBox{enemy?card.x+card.w-42:card.x+10,card.y+(duel?10:5),32,32};
        if (actor) portrait(f,art::battlePortraits[actor->portrait],portraitBox);
        else {
            frame(f,portraitBox,edge,ink);
            boxText(f,"守",portraitBox.inset(4,3),18,gold,1);
        }
        const int x=card.x+(enemy?9:51);
        const int align=enemy?2:0;
        char caption[32];
        std::snprintf(caption,sizeof caption,"%d",value);
        const char* name=actor?actor->name:(duel?"守阵校尉":"守军");
        boxText(f,duel?name:caption,{x,card.y+5,80,17},duel?14:16,enemy?coral:mint,align);
        if (!duel) boxText(f,name,{portraitBox.cx()-25,card.y+39,50,14},12,ivory,1);
        gauge(f,{x,card.y+23,80,9},value,total,enemy?redGauge:jadeGauge,enemy);
        std::snprintf(caption,sizeof caption,duel?"体力%d":"%d",duel?value:morale);
        if (!duel) contain(f,art::commandIcons[moraleIcon],{enemy?x+67:x,card.y+34,12,12},true);
        const uint16_t resolve=duel||morale>=42?ivory:morale<=28?coral:gold;
        boxText(f,caption,{x+(enemy||duel?0:15),card.y+33,duel?80:65,16},12,resolve,align);
    }
    void battleHud(pxa::game::Frame& f,bool duel=false) {
        combatCard(f,fieldCards[0],battle.commander>=0?&battle.officer():nullptr,
                   duel?battle.duel.health[0]:battle.troops[0],
                   duel?100:battle.initial[0],battle.morale[0],false,duel);
        combatCard(f,fieldCards[2],battle.opponent(),duel?battle.duel.health[1]:battle.troops[1],
                   duel?100:battle.initial[1],battle.morale[1],true,duel);
        battlePanel(f,fieldCards[1]);
        boxText(f,duel?"阵前斗将":cityName(battle.to),{159,9,66,18},16,gold,1);
        char line[32];
        if (duel)
            std::snprintf(line,sizeof line,"第%d合",std::min(3,battle.duel.round+1));
        else if (battle.plan.engine!=noEngine)
            std::snprintf(line,sizeof line,"城防%d格",battle.wallPoints);
        else std::snprintf(line,sizeof line,"%s",formations[battle.enemyFormation]);
        boxText(f,line,{159,28,66,16},13,duel?dim:coral,1);
        const bool approaching=!battle.round;
        constexpr int openingGap=Battle::fieldLength-2*Battle::campFront;
        const int progress=approaching?std::max(0,openingGap-battle.closestGap()):battle.roundClock;
        gauge(f,{162,47,60,7},duel?battle.duel.round:progress,
              duel?3:approaching?openingGap-std::max(battle.range(0),battle.range(1)):Battle::period,goldGauge);
    }
    const char* plannedEnemyOrder(char (&label)[60]) const {
        if (battle.enemyWithdrawalQueued) return "敌军将撤";
        if (battle.enemySkillQueued) return "敌将蓄势";
        if (battle.enemySiegeSortie) {
            std::snprintf(label,sizeof label,"敌%s扑向器械",rankNames[battle.enemyRank]);
        } else if (battle.enemyManeuver!=measured) {
            std::snprintf(label,sizeof label,"敌%s%s",battle.enemyRank<0?"军":rankNames[battle.enemyRank],
                          maneuvers[battle.enemyManeuver]);
        } else if (battle.coordinating(1)) {
            const auto& support=battle.enemyAid;
            const auto source=battle.troopFor(1,support.source);
            const char* order=source==shields?"掩护":ranged(source)?"援射":"夹击";
            std::snprintf(label,sizeof label,"敌%s%s%s",rankNames[support.source],order,rankNames[support.target]);
        } else return nullptr;
        return label;
    }
    void drawBattle(pxa::game::Frame& f) {
        if (battle.duel.visible()) {
            drawDuel(f);
            if (battle.paused) drawPause(f);
            return;
        }
        const auto footer=[&](Rect r) {return r.shiftY(battleExtra());};
        fieldBackdrop(f);
        drawSiegeField(f);
        char buf[44];
        const int enemyLeaderLane=Battle::nearestRank(battle.ranks[1],1);
        for (int lane=0;lane<Battle::rankCount;++lane) {
            army(f,lane,0);
            army(f,lane,1);
            if (lane==enemyLeaderLane) drawFieldGeneral(f,1,lane);
            if (lane==battle.generalLane) drawFieldGeneral(f,0,lane);
            if (battle.round && battle.roundClock<10)
                for (int side=0;side<2;++side)
                    if (battle.rankLosses[side][lane])
                        sparks(f,fieldX(battle.fronts[side][lane]),groundY(lane)-20,battle.roundClock,4);
        }
        const bool watching=fieldView && !battle.paused;
        const auto restoreHud=[&] {
            button(f,{274,229+tallScene,102,27},"恢复指挥",Command::fieldView,0,true,true);
        };
        if (!watching) battleHud(f);
        if (battle.awaitingOrders) {
            if (watching) {restoreHud();return;}
            count=0;
            drawRanks(f,false);
            if (battle.paused) {drawFieldMap(f,false);drawPause(f);return;}
            const auto opening=[&](Rect r) {return r.shiftY(tallScene);};
            battlePanel(f,opening({12,207,360,51}));
            boxText(f,"双方列阵，等待发令",opening({24,209,336,17}),13,gold,1);
            button(f,opening({21,228,138,25}),"下令进军",Command::launchOrders,0,true,true,horseIcon);
            button(f,opening({165,228,136,25}),"按兵待敌",Command::launchOrders,1,true,false,squareIcon);
            button(f,opening({307,228,55,25}),"暂停",Command::pause,0,true,false,pauseIcon);
            drawFieldMap(f,true);
            return;
        }
        const Rect cueArea{128,93,128,22};
        const auto caption=[&](Rect box,const char* label,BattlePlaque tone,uint16_t color) {
            if (watching) return;
            battlePanel(f,box,tone);
            boxText(f,label,box.inset(5,2),12,color,1);
        };
        const auto casualty=[&](int side,int losses) {
            if (!losses) return;
            char amount[12];
            std::snprintf(amount,sizeof amount,"-%d",losses);
            caption({side?259:72,93,53,22},amount,vermilionPlaque,coral);
        };
        if (battle.effects) {
            const int lane=battle.generalLane;
            const int x=fieldX(battle.generalPosition(lane,battle.atFront));
            const int age=Battle::skillFrames-battle.effects;
            officerEffect(f,battle.officer(),0,lane,age);
            if (battle.officer().power==fireArrow && battle.fireLane>=0) {
                const int target=fieldX(battle.fronts[1][battle.fireLane]);
                const int flight=std::min(12,age);
                const int arrow=x+(target-x)*flight/12;
                const int y=groundY(lane)-27-flight*(12-flight)/3;
                if (age<12) line(f,arrow-9,y+3,arrow+8,y,coral,3);
                else sparks(f,target,groundY(battle.fireLane)-21,age-12,14);
            }
            caption(cueArea,battle.officer().skill,jadePlaque,gold);
        } else if (battle.strikeTicks) {
            const int lane=battle.lastStrikeLane;
            sparks(f,fieldX(battle.fronts[1][lane]),groundY(lane)-23,15-battle.strikeTicks,6);
            casualty(1,battle.losses[1]);
        } else if (battle.round && battle.roundClock<15) {
            casualty(0,battle.losses[0]);
            casualty(1,battle.losses[1]);
        }
        if (!battle.effects) {
            char order[60];
            const bool rout=battle.routCue[0]>0 || battle.routCue[1]>0;
            const char* cue=rout?(battle.routCue[0]?"我军有一路溃退":"敌军有一路溃退"):plannedEnemyOrder(order);
            if (battle.enemySkillTicks && battle.opponent()) {
                if (enemyLeaderLane>=0)
                    officerEffect(f,*battle.opponent(),1,enemyLeaderLane,22-battle.enemySkillTicks);
                if (!cue) cue=battle.opponent()->skill;
            }
            if (cue) caption(cueArea,cue,vermilionPlaque,coral);
            else if (!battle.round && battle.ticks<24)
                caption(cueArea,battle.anchored?"全军驻守":"全军进军",jadePlaque,gold);
            else if (!battle.round) {
                std::snprintf(buf,sizeof buf,"离敌 %d 步",battle.closestGap());
                caption(cueArea,buf,bronzePlaque,ivory);
            }
        }
        if (battle.paused) {
            drawRanks(f,false);
            drawFieldMap(f,false);
            drawPause(f);
            return;
        }
        if (watching) {restoreHud();return;}
        if (tacticsOpen) {
            drawFieldMap(f,false);
            drawOrders(f);
            return;
        }
        battlePanel(f,footer(fieldStatus));
        gauge(f,footer({17,218,48,9}),battle.signal,100,goldGauge);
        std::snprintf(buf,sizeof buf,"令%d",battle.signal);
        boxText(f,buf,footer({71,214,51,17}),12,gold);
        if (battle.plan.engine!=noEngine && battle.wallPoints && battle.engineShots) {
            if (!battle.ranks[0][battle.plan.engineLane]) std::snprintf(buf,sizeof buf,"器械待援");
            else if (battle.lastShot==battle.round) std::snprintf(buf,sizeof buf,"下合再用");
            else if (battle.engineGap()>battle.engineRange())
                std::snprintf(buf,sizeof buf,"还差%d步",battle.engineGap()-battle.engineRange());
            else if (battle.signal<Battle::engineCost) std::snprintf(buf,sizeof buf,"需%d令",Battle::engineCost);
            else std::snprintf(buf,sizeof buf,"器械可用");
        }
        else if (battle.maneuverRounds)
            std::snprintf(buf,sizeof buf,"%s %d合",maneuvers[battle.maneuver],battle.maneuverRounds);
        else if (battle.anchored) std::snprintf(buf,sizeof buf,"全军驻守");
        else if (battle.armament!=noGear) std::snprintf(buf,sizeof buf,"%s",gearSpecs[battle.armament].name);
        else std::snprintf(buf,sizeof buf,"%s",troopNames[battle.troopFor(0,battle.generalLane)]);
        boxText(f,buf,footer({128,214,81,17}),12,battle.maneuverRounds?mint:dim);
        if (battle.plan.engine!=noEngine) {
            if (!battle.wallPoints) std::snprintf(buf,sizeof buf,"城防已破");
            else if (!battle.engineShots) std::snprintf(buf,sizeof buf,"器械用尽");
            else std::snprintf(buf,sizeof buf,"%s %d",battle.plan.engine==ram?"撞城":"投石",battle.engineShots);
            button(f,footer({216,210,99,25}),buf,Command::fireEngine,0,battle.mayFireEngine());
        } else if (battle.commander>=0) {
            if (battle.generalHealth)
                std::snprintf(buf,sizeof buf,"%s%s · %d",rankNames[battle.generalLane],
                              battle.atFront?"阵前":"后阵",battle.generalHealth);
            else std::snprintf(buf,sizeof buf,"武将退场");
            boxText(f,buf,footer({212,214,93,17}),12,battle.generalHealth>30?gold:coral,1);
        }
        const auto control=[&](int slot,Rect original) {
            return footer(wrapSquareControls()?Rect{8+124*(slot%3),237+28*(slot/3),120,25}:original);
        };
        for (int i=0;i<3;++i)
            button(f,control(i,{8+56*i,239,54,21}),formations[i],Command::shift,i,
                   !battle.result&&(i==battle.plan.formation||battle.signal>=Battle::commandCost),
                   i==battle.plan.formation,formationIcons[i]);
        button(f,control(3,{176,239,58,21}),weaponAttacks[battle.plan.weapon],Command::strike,0,
               battle.mayStrike(),false,weaponIcons[battle.plan.weapon]);
        button(f,control(4,{236,239,69,21}),battle.usedSkill?"将技已用":"将技",Command::skill,0,
               battle.mayRally(),true,skillIcon);
        button(f,control(5,{307,239,69,21}),"号令",Command::orders,0,!battle.result,false,ordersIcon);
        button(f,footer(fieldPause),"暂停",Command::pause,0,true,false,pauseIcon);
        drawFieldMap(f,!battle.result);
        drawRanks(f,true);
        generalControls(f);
        const auto commanderCard=fieldCards[0];
        if (battle.canMoveGeneral() && registerControl(commanderCard,Command::battleRank,battle.generalLane))
            around(commanderCard,commanderCard.inset(2,2),[&](Rect border) {block(f,border,gold);});
    }
    void drawReport(pxa::game::Frame& f) {
        fillBackground(f,reportSiege?art::siege:art::battlefields[sites[reportCity].terrain]);
        skinFrame(f,{39,22,306,223});
        const char* title=reportRetreat?(reportDefense?"弃城撤退":"收兵回城"):
                          reportWinner>0?(reportDefense?"守城告捷":"夺城告捷"):
                          reportDefense?"城池失守":"攻城受挫";
        contain(f,art::outcomes[reportWinner>0?0:1],{116,29,152,52},true);
        char line[80];
        std::snprintf(line,sizeof line,"%s · %s",cityName(reportCity),title);
        boxText(f,line,{59,82,266,23},15,reportWinner>0?gold:coral,1);
        block(f,{63,107,258,1},edge);
        int y=111;
        auto row=[&](UiIcon icon,uint16_t color) {
            iconText(f,icon,line,{61,y,262,16},13,color);
            y+=16;
        };
        std::snprintf(line,sizeof line,"我军伤亡 %d  ·  敌军伤亡 %d",reportLoss,battle.initial[1]-battle.troops[1]-battle.fled[1]);
        row(lossIcon,ivory);
        if (battle.fled[0] || battle.fled[1] || aftermath.dispersed) {
            if (aftermath.dispersed && (battle.fled[0] || battle.fled[1]))
                std::snprintf(line,sizeof line,"逃兵 我%d 敌%d · 我军溃散%d",battle.fled[0],battle.fled[1],aftermath.dispersed);
            else if (aftermath.dispersed)
                std::snprintf(line,sizeof line,"我军溃散 %d 人",aftermath.dispersed);
            else
                std::snprintf(line,sizeof line,"逃兵  我方 %d · 敌方 %d",battle.fled[0],battle.fled[1]);
            row(routedIcon,coral);
        }

        if (aftermath.ourCaptive>=0 || aftermath.ourPrisoners) {
            const char* actor=aftermath.ourCaptive>=0?campaign.leader(aftermath.ourCaptive).name:nullptr;
            if (actor && aftermath.ourPrisoners) std::snprintf(line,sizeof line,"%s与%d名士兵被俘",actor,aftermath.ourPrisoners);
            else if (actor) std::snprintf(line,sizeof line,"%s被俘",actor);
            else std::snprintf(line,sizeof line,"我方%d名士兵被俘",aftermath.ourPrisoners);
            row(captiveIcon,coral);
        }
        if (aftermath.captive>=0 || aftermath.enemyPrisoners) {
            const char* actor=battle.opponent()?battle.opponent()->name:nullptr;
            if (aftermath.captive>=0 && aftermath.enemyPrisoners)
                std::snprintf(line,sizeof line,"俘获%s与%d名敌兵",actor,aftermath.enemyPrisoners);
            else if (aftermath.captive>=0) std::snprintf(line,sizeof line,"俘获敌将%s",actor);
            else std::snprintf(line,sizeof line,"俘获敌兵%d人",aftermath.enemyPrisoners);
            row(captiveIcon,mint);
        }
        if (aftermath.rescuedGenerals || aftermath.rescuedTroops) {
            std::snprintf(line,sizeof line,"救回武将%d人 · 士兵%d人",aftermath.rescuedGenerals,aftermath.rescuedTroops);
            row(recruitIcon,mint);
        } else if (aftermath.enemyReturned && aftermath.enemyDispersed) {
            std::snprintf(line,sizeof line,"敌军撤回 %d · 敌军溃散 %d",aftermath.enemyReturned,aftermath.enemyDispersed);
            row(routedIcon,dim);
        } else if (aftermath.enemyReturned) {
            std::snprintf(line,sizeof line,"敌军 %d 人撤往%s",aftermath.enemyReturned,
                          cityName(reportDefense?battle.from:battle.enemyRetreatCity));
            row(withdrawIcon,dim);
        } else if (aftermath.enemyDispersed) {
            std::snprintf(line,sizeof line,"敌军溃散 %d 人",aftermath.enemyDispersed);
            row(routedIcon,dim);
        } else if (reportWinner>0 && !reportDefense) {
            std::snprintf(line,sizeof line,"入城缴获 115金 95粮");
            row(coinsIcon,dim);
        }
        if (aftermath.objective) std::snprintf(line,sizeof line,"剧本悬赏已达成");
        else if (aftermath.level) std::snprintf(line,sizeof line,"领军提升至等级 %d",aftermath.level);
        else std::snprintf(line,sizeof line,"领军历练 +%d",aftermath.xp);
        if (aftermath.xp || aftermath.objective) row(meritIcon,mint);

        const int tab=aftermath.captive>=0?0:aftermath.ourCaptive>=0?1:
            (aftermath.enemyPrisoners || aftermath.ourPrisoners)?2:-1;
        if (tab>=0) {
            button(f,{65,211,123,25},"返回舆图",Command::accept);
            button(f,{196,211,123,25},"俘虏营",Command::prisonCamp,tab,true,true,captiveIcon);
        } else button(f,{121,211,142,25},"返回舆图",Command::accept,0,true,true);
    }
    void drawHelp(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"行军用策","七十二季内一统州郡");
        constexpr const char* titles[]={"治城","进军","临阵","军令"};
        constexpr const char* a[]={"征募补兵，屯田加粮，扩地增令。",
            "选起点与目标，可调兵或出征。",
            "列阵后可下令进军，也可按兵待敌。",
            "点我方战场调将，军师可在后阵施策。"};
        constexpr const char* b[]={"城池内政可任太守、安民、搜寻。",
            "再点己城切换出发城。红线是敌袭。",
            "号令可先看预计伤亡。军心低会溃逃。",
            "猛将可单挑，统将的部队更能稳住战线。"};
        for (int i=0;i<4;++i) {
            int y=44+46*i;
            frame(f,{10,y,364,42});
            text(f,titles[i],20,y+11,15,gold);
            boxText(f,a[i],{80,y+2,282,18},13);
            boxText(f,b[i],{80,y+21,282,18},13,dim);
        }
        button(f,{263,232,111,24},"返回",Command::back,0,true,true);
    }
    void drawEnding(pxa::game::Frame& f) {
        fillBackground(f,titleBackground(),.57f);
        skinFrame(f,{36,40,312,181});
        contain(f,art::outcomes[campaign.won>0?0:1],{114,54,156,52},true);
        text(f,campaign.won>0?"州郡皆归帐下，山河重定。":
                campaign.holdings(campaign.player)?"七十二季已过，诸侯割据未平。":"城池尽失，大势已去。",192,122,15,ivory,1);
        button(f,{120,166,144,30},"返回标题",Command::home,0,true,true);
    }
    void drawReplace(pxa::game::Frame& f) {
        fillBackground(f,titleBackground(),.57f);
        skinFrame(f,{47,74,290,115});
        text(f,"另起一局",192,86,22,gold,1);
        text(f,"开战后会覆盖现有战役。",192,118,15,ivory,1);
        button(f,{57,148,119,29},"保留旧档",Command::back);
        button(f,{207,148,120,29},"确认起兵",Command::replace,0,true,true);
    }
    void drawCouncil(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        char state[68];
        std::snprintf(state,sizeof state,"%s · %s",factions[campaign.player].title,
                      difficultyNames[int(campaign.difficulty)]);
        header(f,"军议",state);
        std::snprintf(state,sizeof state,"%d",campaign.gold);
        resourcePlate(f,{10,41,95,25},coinsIcon,state,gold);
        std::snprintf(state,sizeof state,"%d",campaign.grain);
        resourcePlate(f,{110,41,95,25},grainIcon,state);
        std::snprintf(state,sizeof state,"%d/%d",campaign.actions,campaign.actionCapacity());
        resourcePlate(f,{210,41,95,25},orderIcon,state,mint);
        button(f,{312,41,62,25},"音频",Command::audioMenu,0,true,false,musicIcon);
        skinFrame(f,{10,70,178,148});
        skinFrame(f,{196,70,178,148});
        text(f,"筹粮练兵",20,79,16,gold);
        button(f,{110,75,69,23},"领地",Command::fiefs);
        text(f,"外交",206,79,16,gold);
        text(f,"花金120，购粮330",20,98,13,dim);
        button(f,{19,118,160,27},"通商购粮",Command::market,0,campaign.actions&&campaign.gold>=120);
        text(f,"边城各补105兵",20,151,12,dim);
        text(f,"金150  粮95",20,168,12,dim);
        button(f,{19,183,160,26},"巡边募兵",Command::militia,0,
               campaign.actions&&campaign.gold>=150&&campaign.grain>=95);
        boxText(f,"结盟四季 · 可借道",{205,97,160,17},12,dim);
        int n=0;
        for (int side=0;side<3;++side) {
            if (side==campaign.player) continue;
            const int y=117+46*n++;
            if (campaign.allied(side)) std::snprintf(state,sizeof state,"%s 结盟%d季",
                factions[side].title,campaign.alliance[side]);
            else if (campaign.ceasefire(side)) std::snprintf(state,sizeof state,"%s 休战%d季",
                factions[side].title,campaign.truce[side]);
            else std::snprintf(state,sizeof state,"%s",factions[side].title);
            boxText(f,state,{205,y,160,16},13,campaign.allied(side)?mint:ivory);
            std::snprintf(state,sizeof state,"议和%d金",truceCost);
            button(f,{205,y+17,77,25},state,Command::treat,side,campaign.mayTreat(side));
            std::snprintf(state,sizeof state,"%s%d金",campaign.allied(side)?"续盟":"结盟",allianceCost);
            button(f,{288,y+17,77,25},state,Command::ally,side,campaign.mayAlly(side));
        }
        button(f,{10,230,75,26},"返回",Command::back);
        button(f,{91,230,57,26},"军情",Command::forces);
        button(f,{154,230,70,26},"俘虏营",Command::prisonCamp,0,true,false,captiveIcon);
        button(f,{230,230,64,26},"州郡",Command::regions);
        button(f,{300,230,74,26},"武将",Command::roster,0,true,true);
    }
#include "expansion_ui.inc"

    void draw() {
        if (!ready || resize || !context->foreground() || loadingArt) return;
        auto wanted=pagesNeeded();
        for (unsigned i=0;i<wanted.size();++i) {
            if (wanted[i]==(slots[i]>=0) && (!sansText || slots[i]<fontSlots())) continue;
            loadingArt=true;
            auto task=context->tasks().start(syncArt(wanted));
            if (!task) {loadingArt=false;on_error(*context,task.error());}
            return;
        }
        count=0;
        drawOy=originY();
        pxa::game::Frame f(*renderer,commands.bytes());
        // Every page covers the surface with an opaque background.
        switch (page) {
            case Page::home: drawHome(f); break;
            case Page::choose: drawChoose(f); break;
            case Page::map: drawMap(f); break;
            case Page::deploy: drawDeploy(f); break;
            case Page::battle: drawBattle(f); break;
            case Page::report: drawReport(f); break;
            case Page::help: drawHelp(f); break;
            case Page::ending: drawEnding(f); break;
            case Page::replace: drawReplace(f); break;
            case Page::council: drawCouncil(f); break;
            case Page::roster: drawRoster(f); break;
            case Page::cityDesk: drawCityDesk(f);break;
            case Page::search: drawSearch(f);break;
            case Page::soundSettings: drawSoundSettings(f);break;
            case Page::gazetteer: drawGazetteer(f);break;
            case Page::chronicle: drawChronicle(f);break;
            case Page::seasonCouncil: drawSeasonCouncil(f);break;
            case Page::alliedOffensive: drawAlliedOffensive(f);break;
            case Page::regions: drawRegions(f);break;
            case Page::dispatch: drawDispatch(f);break;
            case Page::forces: drawForces(f);break;
            case Page::fiefs: drawFiefs(f);break;
            case Page::gear: drawGear(f);break;
            case Page::captive: drawCaptive(f);break;
        }
        mapFocusCity=-1;
        const int shownFocus=focus;
        if (controllerMode) ensureFocus();
        auto r=sansText?sans.upload(*renderer,fontSlots()):pxa::Result<void>{};
        if (r) r=f.submit();
        dirty=!r || (focus>=0 && focus!=shownFocus);
        if (!r && r.error()!=pxa::Error::would_block && drawErrors++<3) on_error(*context,r.error());
    }

    void show(Page nextPage,int first=0) {
        if (sansText && page!=nextPage) sans.reset();
        if (page==Page::deploy && nextPage!=Page::battle && nextPage!=Page::deploy)
            audio.releaseOpening(battle.defending);
        page=nextPage;
        mapRoutes.reset();
        mapFocusCity=-1;
        lookaheadDone=false;
        focus=first;
        count=0;
        repeatPadAt=0;
        resetPointer();
        dirty=true;
        previousTick=0;
        if (nextPage!=Page::soundSettings && nextPage!=Page::help) {
            const bool title=nextPage==Page::home || nextPage==Page::choose ||
                nextPage==Page::replace || nextPage==Page::ending;
            audio.scene(nextPage==Page::battle?Score::battle:title?Score::title:Score::atlas);
        }
        audio.pause(!context->foreground() ||
            ((nextPage==Page::battle || (nextPage==Page::soundSettings && audioFrom==Page::battle)) &&
            battle.paused));
    }
    void beginMarch(const MarchRoute& route) {
        mapRoutes.reset();
        playerRoute=route;
        marchingMs=0;
        previousTick=0;
        focus=0;
        if (!mapZoom) mapZoom=1;
        auto [x,y]=marchPosition(route.leg(0),route.legs()==1?.5f:0.f);
        mapCenterX=x;
        mapCenterY=y;
        clampCamera();
        logState(campaign.alliedWaypoint(route)>=0?"march-allied":"march");
        audio.cue(marchSound);
    }
    void beginSeason() {
        if (!campaign.canAdvance(seasonPlan)) return;
        show(Page::map,0);
        seasonMarch=true;
        marching=campaign.orders;
        marchingMs=0;
        previousTick=0;
        focus=0;
        Order first, threat;
        bool visible=false;
        for (auto order:marching) {
            if (!order) continue;
            if (!first) first=order;
            auto [x,y]=marchPosition(order,.5f);
            visible |= mapWindow.contains(mapX(x),mapY(y));
            if (campaign.cities[order.to].owner==campaign.player) threat=order;
        }
        if (mapZoom && (threat || (!visible && first))) {
            auto [x,y]=marchPosition(threat?threat:first,.5f);
            mapCenterX=x;
            mapCenterY=y;
            clampCamera();
        }
        if (!first) finishMarch();
        else {logState("enemy-march");audio.cue(marchSound);}
    }
    void finishMarch() {
        mapRoutes.reset();
        const Order order=playerRoute.endpoints();
        const bool season=seasonMarch;
        playerRoute={};
        marching={};
        seasonMarch=false;
        if (season) {
            prisonNews=campaign.advance(seasonPlan);
            seasonPlan=SeasonPlan::regular;
            notice=prisonNews?Notice::prisonNews:Notice::income;
            checkpoint();
            enterMap();
            logState("season");
        } else if (order && arrivingColumn>=0 && campaign.columns[arrivingColumn]) {
            if (campaign.own(order.to)) {
                campaign.arriveFriendly(arrivingColumn);
                selected=campaign.own(campaign.marchBase)?campaign.marchBase:order.to;
                notice=Notice::moved;
                checkpoint();
                enterMap();
                if (page==Page::map && !isMarching() && selected!=order.to) destination=order.to;
                logState("reinforce");
            } else if (campaign.ceasefire(campaign.cities[order.to].owner)) {
                campaign.recall(arrivingColumn);
                notice=Notice::withdrawn;
                checkpoint();
                enterMap();
            } else deploy(order,false);
        }
        dirty=true;
    }
    void deploy(Order order,bool defense) {
        departure=order;
        battle.defending=defense;
        if (defense) {
            arrivingColumn=-1;
            defenseLeader=campaign.fieldLeader(campaign.governor(order.to),order.to);
        }
        const int lead=plannedLeader();
        const auto troop=sites[order.to].terrain==river?bow:lead>=0?campaign.leader(lead).favored:spear;
        deployment={troop,square,preferredWeapon(troop)};
        if (lead>=0 && battleRole(campaign.leader(lead))==Role::strategist) deployment.weapon=longbow;
        deploymentLane=1;
        deploymentKit=false;
        show(Page::deploy,3);
    }
    void enterMap() {
        if (campaign.won) {show(Page::ending);return;}
        if (campaign.defense) {
            selected=campaign.defense.to;
            deploy(campaign.defense,true);
            logState("defend");
            return;
        }
        for (arrivingColumn=campaign.readyColumn();arrivingColumn>=0;arrivingColumn=campaign.readyColumn()) {
            const Order order=campaign.columns[arrivingColumn].road;
            const auto route=campaign.marchRoute(order.from,order.to);
            if (!route) {
                campaign.recall(arrivingColumn);
                notice=Notice::routeBlocked;
                checkpoint();
                logState("march-route-blocked");
                continue;
            }
            selected=order.from;
            destination=order.to;
            show(Page::map);
            beginMarch(route);
            return;
        }
        if (!campaign.own(selected)) selected=campaign.preferredOrigin();
        destination=-1;
        show(Page::map,-1);
    }
    void conclude(bool retreat=false) {
        reportWinner=battle.result;
        reportCity=battle.to;
        reportDefense=battle.defending;
        reportRetreat=retreat;
        reportSiege=battle.walls>=3;
        aftermath=battle.settle(campaign);
        reportLoss=battle.initial[0]-battle.troops[0]-battle.fled[0];
        if (!battle.defending && battle.result>0)
            selected=campaign.own(campaign.marchBase)?campaign.marchBase:battle.to;
        if (notice!=Notice::prisonNews) notice=aftermath.objective?Notice::goal:Notice::none;
        checkpoint();
        show(Page::report);
        audio.cue(battle.result>0?victorySound:defeatSound);
        audio.cue(battle.result>0?victoryVoice:defeatVoice);
        logState(battle.enemyWithdrew?"enemy-withdrew":battle.result>0?"victory":"defeat");
        if (aftermath.ourCaptive>=0 || aftermath.ourPrisoners) logState("our-captured");
        if (aftermath.captive>=0 || aftermath.enemyPrisoners) logState("enemy-captured");
    }
    void openOrders(int rank) {
        tacticsRank=rank;
        proposedOrder.reset();
        tacticsOpen=true;
        focus=rank+1;
        previousTick=0;
    }
    void closeOrders() {
        proposedOrder.reset();
        tacticsOpen=false;
        focus=battle.plan.formation;
        previousTick=0;
    }
    void pauseBattle() {
        proposedOrder.reset();
        tacticsOpen=false;
        retreatPrompt=false;
        battle.paused=!battle.paused;
        audio.pause(battle.paused);
        focus=battle.paused?0:battle.plan.formation;
        previousTick=0;
        dirty=true;
    }
    void recallColumn(int id) {
        if (!campaign.recall(id)) return;
        notice=Notice::withdrawn;
        checkpoint();
        logState("recall");
        if (page==Page::deploy) enterMap();
    }
    pxa::BackAction on_back() {
        mapMotion={};
        switch (page) {
            case Page::home:return pxa::BackAction::close;
            case Page::choose:
            case Page::ending:show(Page::home);break;
            case Page::replace:show(Page::choose,side);break;
            case Page::map:
                if (isMarching()) finishMarch();
                else if (fullAtlas()) {setMapWindow(standardMap);focus=-1;dirty=true;clampCamera();}
                else if (destination>=0) {destination=-1;mapRoutes.reset();focus=-1;dirty=true;}
                else show(Page::home);
                break;
            case Page::council:
            case Page::dispatch:
            case Page::gazetteer:
            case Page::seasonCouncil:show(Page::map,-1);break;
            case Page::cityDesk:
                if (deskMapOrigin>=0) {
                    selected=deskMapOrigin;
                    deskMapOrigin=-1;
                    show(Page::fiefs,-1);
                } else show(Page::map,-1);
                break;
            case Page::regions:show(Page::council);break;
            case Page::chronicle:show(chronicleFrom,-1);break;
            case Page::fiefs:show(Page::council);break;
            case Page::alliedOffensive:show(alliedFrom,-1);break;
            case Page::forces:show(Page::council);break;
            case Page::gear:show(Page::roster,rosterPosition()-rosterStart());break;
            case Page::search:show(Page::cityDesk);break;
            case Page::soundSettings:show(audioFrom);break;
            case Page::roster:show(rosterFrom,rosterFrom==Page::deploy?3:0);break;
            case Page::deploy:
                if (battle.defending) show(Page::home);
                else recallColumn(arrivingColumn);
                break;
            case Page::battle:
                if (retreatPrompt) {retreatPrompt=false;focus=0;dirty=true;}
                else if (tacticsOpen) {
                    if (proposedOrder) {proposedOrder.reset();focus=tacticsRank+1;}
                    else closeOrders();
                    dirty=true;
                }
                else if (fieldView && !battle.paused) input({{},Command::fieldView,0,true});
                else pauseBattle();
                break;
            case Page::report:enterMap();break;
            case Page::captive:
                if (prisonFrom==Page::map) enterMap();
                else show(prisonFrom);
                break;
            case Page::help:show(helpFrom,helpFrom==Page::map?-1:0);break;
        }
        return pxa::BackAction::stay;
    }
    void input(Control control) {
        if (!control.enabled) return;
        mapRoutes.reset();
        mapMotion={};
        const int arg=control.data;
        dirty=true;
        switch (control.command) {
            case Command::resume:
                selected=campaign.preferredOrigin();
                if (selected>=0) focusCamera(selected);
                notice=campaign.alliedRequest||campaign.alliedReport.request?Notice::none:Notice::saved;
                enterMap();
                break;
            case Command::newGame:
                audio.cue(confirmSound);
                chosenChapter=0;side=0;chosenDifficulty=Difficulty::standard;
                seasonPlan=SeasonPlan::regular;
                show(Page::choose);
                break;
            case Command::guide:helpFrom=page;show(Page::help);break;
            case Command::audioMenu:audioFrom=page;show(Page::soundSettings);break;
            case Command::audioToggle:audio.toggle(unsigned(arg));audio.cue(confirmSound);break;
            case Command::back:(void)on_back();break;
            case Command::chapter:
                chosenChapter=arg;
                break;
            case Command::difficulty:
                chosenDifficulty=Difficulty(arg);
                break;
            case Command::side:side=arg;break;
            case Command::seat:
                if (hasSave||damagedSave) {show(Page::replace);break;}
                [[fallthrough]];
            case Command::replace:
                campaign.start(side,chosenChapter,chosenDifficulty);
                selected=campaign.capital();
                setMapWindow(standardMap);mapZoom=0;showMapNames=false;chronicleRead=-1;focusCamera(selected);
                discovery={};
                regionalChoice=0;
                notice=Notice::none;
                checkpoint();
                enterMap();
                logState("campaign");
                break;
            case Command::select: {
                int target=-1;
                if (arg!=selected && !(arg==destination && campaign.own(arg))) target=arg;
                if (arg==destination && campaign.own(arg)) {
                    selected=arg;
                    if (campaign.setOrigin(arg)) checkpoint();
                    logState("march-origin");
                }
                destination=target;
                if (controllerMode) focus=-1;
                notice=Notice::none;
                break;
            }
            case Command::origin:
                if (campaign.own(arg)) {
                    selected=arg;
                    destination=-1;
                    notice=Notice::none;
                    focus=-1;
                    if (campaign.setOrigin(arg)) checkpoint();
                    focusCamera(arg);
                    if (page==Page::fiefs) show(Page::map,-1);
                    audio.cue(confirmSound);
                    logState("march-origin");
                }
                break;
            case Command::work:
                if (campaign.develop(selected,Work(arg))) {
                    notice=arg==recruit?Notice::recruited:arg==farm?Notice::cultivated:
                        arg==walls?Notice::fortified:Notice::civic;
                    campaign.makeOrders();
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("work");
                }
                break;
            case Command::cityDesk:
                deskMapOrigin=-1;
                show(Page::cityDesk);
                break;
            case Command::fiefs:
                fiefsPage=0;
                show(Page::fiefs);
                break;
            case Command::fiefPage:
                fiefsPage=std::clamp(fiefsPage+arg,0,(campaign.holdings(campaign.player)-1)/fiefRows);
                break;
            case Command::fiefDesk:
                if (campaign.own(arg)) {
                    deskMapOrigin=selected;
                    selected=arg;
                    show(Page::cityDesk);
                }
                break;
            case Command::cityPolicy:
                if (campaign.setPolicy(selected,CityPolicy(arg))) {
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("city-policy");
                }
                break;
            case Command::gazetteer:gazetteerCity=arg;show(Page::gazetteer);break;
            case Command::browseCity:gazetteerCity=(gazetteerCity+arg+cityCount)%cityCount;break;
            case Command::chronicle:
                chronicleFrom=page;
                if (page==Page::map) chronicleRead=latestChronicle(campaign.year(),campaign.season());
                chronicleCursor=latestChronicle(campaign.year(),campaign.season(),arg?gazetteerCity:-1);
                show(Page::chronicle,1);
                break;
            case Command::chronicleStep:
                chronicleCursor=arg;
                break;
            case Command::chronicleDeed:
                if (campaign.dispatchDeed(arg)) {
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("chronicle-deed");
                }
                break;
            case Command::chronicleMap:
                destination=arg==selected?-1:arg;
                if (!mapZoom) mapZoom=1;
                focusCamera(arg);
                show(Page::map,-1);
                break;
            case Command::openSearch:
                searchingOfficer=campaign.searchLead(selected);
                discovery={};
                show(Page::search);
                break;
            case Command::searchOfficer:
                rosterFrom=Page::search;
                officerSlot=searchingOfficer>=0?searchingOfficer:campaign.commander;
                show(Page::roster,rosterPosition()-rosterStart());
                break;
            case Command::survey:
                if (campaign.canSearch(selected,searchingOfficer)) {
                    discovery=campaign.search(selected,searchingOfficer);
                    checkpoint();
                    audio.cue(confirmSound);
                    logState(discovery.kind==talentFound?"talent-found":"search");
                }
                break;
            case Command::welcomeTalent:
                if (const auto result=campaign.welcome(selected,arg);result!=Offer::unavailable) {
                    checkpoint();
                    if (result==Offer::joined) audio.cue(confirmSound);
                    logState(result==Offer::joined?"talent-recruited":"talent-refused");
                }
                break;
            case Command::march: {
                const auto route=campaign.marchRoute(selected,arg);
                if (const int lead=campaign.routeLeader(route);lead>=0) {
                    departure={selected,arg};
                    dispatchLead=lead;
                    dispatchSize=legion;
                    if (!campaign.canSend(route,dispatchSize,lead)) dispatchSize=division;
                    if (!campaign.canSend(route,dispatchSize,lead)) dispatchSize=vanguard;
                    stageSiege=false;
                    show(Page::dispatch,1+dispatchSize);
                }
                break;
            }
            case Command::chooseSize:dispatchSize=DispatchSize(arg);break;
            case Command::approach:stageSiege=bool(arg);break;
            case Command::send:
                if (campaign.send(departure.from,departure.to,dispatchSize,dispatchLead,stageSiege)>=0) {
                    if (stageSiege) notice=Notice::encamped;
                    checkpoint();
                    enterMap();
                    logState(stageSiege?"encamp":"send");
                }
                break;
            case Command::recall:recallColumn(arg);break;
            case Command::season:
                show(Page::seasonCouncil);
                break;
            case Command::inspectThreat: {
                const auto threat=campaign.nextAttack();
                if (!threat) break;
                selected=threat.to;
                destination=-1;
                setMapWindow(standardMap);
                mapZoom=1;
                focusCamera(selected);
                if (page==Page::seasonCouncil) show(Page::map,-1);
                focus=-1;
                break;
            }
            case Command::choosePlan:seasonPlan=SeasonPlan(arg);break;
            case Command::advanceSeason:beginSeason();break;
            case Command::skipMarch:finishMarch();break;
            case Command::zoom: {
                const bool overview=mapZoom==0;
                mapZoom=zoomStep(arg);
                if (overview) focusCamera(destination<0?selected:destination);
                else clampCamera();
                focus=-1;
                break;
            }
            case Command::centerMap:
                focusCamera(selected);
                focus=-1;
                break;
            case Command::mapNames:
                showMapNames=!showMapNames;
                break;
            case Command::atlas:
                setMapWindow(fullAtlas()?standardMap:fullMap);
                clampCamera();
                focus=-1;
                break;
            case Command::council:show(Page::council);break;
            case Command::forces:show(Page::forces);break;
            case Command::regions:show(Page::regions,regionalChoice);break;
            case Command::chooseRegion:regionalChoice=arg;break;
            case Command::locateRegionCity:
                destination=arg==selected?-1:arg;
                setMapWindow(standardMap);
                mapZoom=1;
                focusCamera(arg);
                notice=Notice::none;
                show(Page::map,-1);
                break;
            case Command::claimRegion:
                if (campaign.claimRegional(arg)) {
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("regional-reward");
                }
                break;
            case Command::roster:
                rosterFrom=page;
                officerSlot=page==Page::dispatch?dispatchLead:
                    page==Page::deploy?defenseLeader:
                    page==Page::cityDesk?campaign.governor(selected):campaign.commander;
                if (officerSlot<0) officerSlot=campaign.commander;
                show(Page::roster,rosterPosition()-rosterStart());
                break;
            case Command::officer:officerSlot=arg;break;
            case Command::rosterPage:
                officerSlot=rosterSlot(std::clamp(rosterStart()+arg*rosterSize,0,rosterCount()-1));
                focus=0;
                break;
            case Command::hire:
                if (const auto result=campaign.hire(arg);result!=Offer::unavailable) {
                    checkpoint();
                    if (result==Offer::joined) audio.cue(confirmSound);
                    logState(result==Offer::joined?"hire":"hire-refused");
                }
                break;
            case Command::chooseOfficer:
                if (eligibleOfficer(arg)) {
                    if (rosterFrom==Page::dispatch) dispatchLead=arg;
                    else if (rosterFrom==Page::deploy) {
                        defenseLeader=arg;
                        if (battleRole(campaign.leader(arg))==Role::strategist) deployment.weapon=longbow;
                    }
                    else if (rosterFrom==Page::search) searchingOfficer=arg;
                    else {
                        if (rosterFrom==Page::cityDesk) campaign.assign(selected,arg);
                        else campaign.commander=arg;
                        checkpoint();
                    }
                    logState("appoint");
                    (void)on_back();
                }
                break;
            case Command::gear:
                chooseGearCategory(armsSlot);
                show(Page::gear);
                break;
            case Command::gearCategory:chooseGearCategory(GearSlot(arg));break;
            case Command::pickGear:gearCursor=arg;break;
            case Command::equip:
                if (campaign.equip(officerSlot,Gear(arg))) {checkpoint();logState("equip");}
                break;
            case Command::unequip:
                if (campaign.unequip(officerSlot,GearSlot(arg))) {checkpoint();logState("unequip");}
                break;
            case Command::buyGear:
                if (campaign.buyGear(Gear(arg))) {checkpoint();logState("gear-bought");}
                break;
            case Command::drill:
                if (campaign.drill(arg)) {checkpoint();logState("drill");}
                break;
            case Command::treat:
                if (campaign.treat(arg)) {notice=Notice::treaty;checkpoint();logState("treaty");}
                break;
            case Command::ally:
                if (campaign.ally(arg)) {
                    notice=Notice::allied;
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("alliance");
                }
                break;
            case Command::alliedOffensive:
                if (campaign.canAskAlly(arg) || (campaign.alliedRequest && campaign.alliedRequest.target==arg)) {
                    alliedTarget=arg;
                    alliedFrom=page;
                    show(Page::alliedOffensive,-1);
                }
                break;
            case Command::requestAllied:
                if (campaign.requestAlliedAttack(arg,alliedTarget)) {
                    notice=Notice::none;
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("allied-offensive");
                }
                break;
            case Command::cancelAllied:
                if (campaign.cancelAlliedAttack()) {
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("allied-cancel");
                }
                break;
            case Command::market:
                if (campaign.market()) {notice=Notice::supplies;checkpoint();logState("market");}
                break;
            case Command::militia:
                if (campaign.militia()) {notice=Notice::levies;checkpoint();logState("levy");}
                break;
            case Command::engage:
                battle.start(campaign,departure,arrivingColumn,deployment,defenseLeader);
                battle.prepare();
                followBattle=true;
                fieldView=false;
                battleCamera=float(battleCameraTarget());
                elapsed=0;
                tacticsOpen=false;
                retreatPrompt=false;
                fieldMotionArt=zhaoFieldArt=true;
                nativeBattlePictures=sharpBattlePictures=true;
                show(Page::battle);
                audio.cue(drumSound);
                logState("battle");
                break;
            case Command::launchOrders:
                if (battle.launch(bool(arg))) {
                    previousTick=0;
                    focus=battle.plan.formation;
                    audio.cue(arg?holdVoice:chargeVoice);
                    logState(arg?"battle-stand":"battle-advance");
                }
                break;
            case Command::anchorArmy:
                if (battle.anchor()) {
                    closeOrders();
                    audio.cue(holdVoice);
                    logState("battle-anchor");
                }
                break;
            case Command::deployTab:deploymentKit=bool(arg);break;
            case Command::deployLane:deploymentLane=arg;break;
            case Command::pickTroop:
                deployment.lineup[deploymentLane]=Troop(arg);
                logState("deploy-troop");
                break;
            case Command::pickEngine:
                deployment.engine=Engine(arg);
                logState("deploy-engine");
                break;
            case Command::cycleEngineLane:deployment.engineLane=(deployment.engineLane+1)%Battle::rankCount;break;
            case Command::fireEngine:
                if (battle.fireEngine()) {
                    audio.cue(battle.plan.engine==ram?clashSound:drumSound);
                    logState("siege-engine");
                }
                break;
            case Command::pickWeapon:deployment.weapon=Weapon(arg);break;
            case Command::shift:
                if (page==Page::deploy) deployment.formation=Formation(arg);
                else if (battle.order(Formation(arg))) {
                    audio.cue(formationSound);
                    info("han-banners formation");
                }
                break;
            case Command::moveGeneral:
                if (battle.moveGeneral(arg/2,arg%2)) {
                    if (tacticsOpen) closeOrders();
                    logState("general-move");
                }
                break;
            case Command::strike:
                if (battle.strike()) {
                    audio.cue(battle.plan.weapon==longbow?arrowSound:clashSound);
                    logState("weapon");
                }
                break;
            case Command::skill:
                if (battle.rally()) {
                    const auto power=battle.officer().power;
                    audio.cue(power==charge || power==whiteSpear || power==crescentStrike?breakVoice:
                              Battle::supportPower(power)?rallyVoice:skillVoice);
                    audio.cue(ranged(battle.troopFor(0,battle.generalLane))?arrowSound:clashSound);
                    info("han-banners skill");
                }
                break;
            case Command::fieldView:
                fieldView=!fieldView;
                focus=0;
                logState(fieldView?"field-view":"field-controls");
                break;
            case Command::orders:
                if (tacticsOpen) closeOrders();
                else openOrders(-1);
                break;
            case Command::battleRank:
                openOrders(arg);
                break;
            case Command::battleView: {
                if (arg<0) {
                    followBattle=true;
                    battleCamera=float(battleCameraTarget());
                    logState("battle-follow");
                } else {
                    const int center=arg==1?(fieldArmyCenter(0)+fieldArmyCenter(1))/2:fieldArmyCenter(arg/2);
                    viewBattleAt(center);
                }
                break;
            }
            case Command::rankSupport:
                if (battle.supportRank(tacticsRank)) {
                    closeOrders();
                    audio.cue(confirmSound);
                    logState("rank-support");
                }
                break;
            case Command::rankAssist:
                if (battle.directAid(arg,tacticsRank)) {
                    audio.cue(confirmSound);
                    logState("rank-assist");
                }
                break;
            case Command::clearRank:
                if (battle.clearRankOrder(tacticsRank)) closeOrders();
                break;
            case Command::previewOrder:
                proposedOrder=Maneuver(arg);
                logState("order-preview");
                break;
            case Command::cancelOrder:
                proposedOrder.reset();
                focus=tacticsRank+1;
                break;
            case Command::maneuver:
                if (battle.issueOrder(tacticsRank,Maneuver(arg))) {
                    closeOrders();
                    constexpr Clip calls[]={noClip,chargeVoice,holdVoice,flankVoice,holdVoice};
                    if (tacticsRank<0 && arg!=measured) {
                        audio.cue(drumSound);
                        audio.cue(calls[arg]);
                    } else audio.cue(confirmSound);
                    logState(tacticsRank<0?"maneuver":"rank-order");
                }
                break;
            case Command::duel:
                if (battle.challenge()) {
                    tacticsOpen=false;
                    focus=0;
                    previousTick=0;
                    audio.cue(challengeVoice);
                    logState("duel");
                }
                break;
            case Command::duelMove:
                if (battle.duelOrder(DuelMove(arg))) {
                    constexpr Clip calls[]={thrustVoice,parryVoice,feintVoice};
                    audio.cue(calls[arg]);
                    if (battle.duel.recoil>=0) audio.cue(clashSound);
                    logState("duel-move");
                }
                break;
            case Command::pause:pauseBattle();break;
            case Command::retreat:
                retreatPrompt=true;
                focus=0;
                break;
            case Command::confirmRetreat:
                battle.result=-1;
                conclude(true);
                break;
            case Command::accept:enterMap();break;
            case Command::prisonCamp:
                openPrison(arg);
                break;
            case Command::prisonTab:
                prisonMessage=nullptr;
                prisonTab=arg;
                prisonIndex=nextPrisoner(-1,1);
                break;
            case Command::prisonPage:
                prisonMessage=nullptr;
                prisonIndex=nextPrisoner(prisonIndex,arg);
                break;
            case Command::captiveChoice:
                actOnPrisoner(arg);
                break;
            case Command::home:show(Page::home);break;
        }
    }
    bool focusAvailable() const {
        return focus>=0 && focus<count && controls[focus].enabled;
    }
    void ensureFocus() {
        if (focusAvailable()) return;
        focus=-1;
        for (int i=0;i<count;++i) if (controls[i].enabled) {focus=i;break;}
    }
    void moveFocus(int dx,int dy) {
        ensureFocus();
        if (focus<0) return;
        const auto source=controls[focus].box;
        int best=std::numeric_limits<int>::max(), picked=focus;
        for (int i=0;i<count;++i) {
            if (!controls[i].enabled) continue;
            const auto dest=controls[i].box;
            int x=dest.cx()-source.cx(), y=dest.cy()-source.cy();
            int along=x*dx+y*dy;
            if (along<=0) continue;
            int across=std::abs(x*dy-y*dx);
            int score=along*along+across*across*4;
            if (score<best) {best=score;picked=i;}
        }
        if (picked==focus) {
            // Wrap so every action remains reachable on a directional pad.
            const int direction=dx+dy>0?1:count-1;
            for (int i=1;i<count;++i) {
                const int next=(focus+i*direction)%count;
                if (controls[next].enabled) {picked=next;break;}
            }
        }
        if (picked==focus) return;
        focus=picked;
        if (page!=Page::map || !mapZoom || controls[focus].command!=Command::select) return;
        const auto& marker=controls[focus];
        const auto safe=mapWindow.inset(32,25);
        const int x=marker.box.cx(),y=marker.box.cy();
        const float zoom=mapScale();
        const float scrollX=(x-std::clamp(x,safe.x,safe.x+safe.w))/zoom;
        const float scrollY=(y-std::clamp(y,safe.y,safe.y+safe.h))/zoom;
        if (moveMapCamera(scrollX,scrollY)) {
            mapFocusCity=marker.data;
            focus=-1;
        }
    }
    void movePadFocus() {
        const int dy=gamepad.vertical(), dx=gamepad.horizontal();
        if (page==Page::battle && fieldView && !battle.paused && dx && !dy) {
            viewBattleAt(int(battleCamera)+sceneWidth/2+72*dx);
            return;
        }
        if (dy) moveFocus(0,dy);
        else if (dx) moveFocus(dx,0);
    }
    void repeatPadFocus(uint64_t now) {
        if (!controllerMode || !repeatPadAt || now<repeatPadAt) return;
        const int before=focus;
        movePadFocus();
        repeatPadAt=now+110'000;
        dirty|=focus!=before;
    }
    void on_controller(pxa::Context&,const pxa::input::ControllerState& s) {
        using namespace pxa::input;
        if (!gamepad.update(s)) return;
        if (!gamepad.down(padDirections)) repeatPadAt=0;
        if (!gamepad.connected()) {
            dirty|=controllerMode;
            controllerMode=false;
            seenPadShortcuts=0;
            return;
        }
        constexpr auto actions=controller_a|controller_b|controller_start|padDirections|padShortcuts;
        if (!ready || resize || !context->foreground() || !gamepad.pressed(actions)) return;
        if (!controllerMode) {
            resetPointer();
            controllerMode=true;
            info("han-banners input-gamepad");
        }
        seenPadShortcuts|=gamepad.buttons()&padShortcuts;
        dirty=true;
        if (loadingArt) return;
        if (gamepad.pressed(controller_b)) {
            repeatPadAt=0;
            if (page==Page::home) (void)context->window().request_back();
            else (void)on_back();
        } else if (gamepad.pressed(padDirections)) {
            movePadFocus();
            repeatPadAt=s.timestamp_us+340'000;
        } else {
            repeatPadAt=0;
            if (gamepad.pressed(controller_start) && page==Page::battle) {
                if (!tacticsOpen && !retreatPrompt && !battle.result) pauseBattle();
            } else if (gamepad.pressed(controller_a|controller_start)) {
                if (focusAvailable()) input(controls[focus]);
                else ensureFocus();
            } else {
                for (int i=0;i<count;++i) {
                    const auto& control=controls[i];
                    if (!control.enabled || !gamepad.pressed(padKeyFor(control.command,control.data)&padShortcuts))
                        continue;
                    input(control);
                    break;
                }
            }
        }
        draw();
    }
    void resetPointer() {
        pointerDown=false;
        mapDragging=false;
        battleDragging=false;
        pressedControl.reset();
        pinch.reset();
        mapMotion={};
        mapTapTime=0;
    }
    void panBattle(float x) {
        const float previous=battleCamera;
        if (followBattle) logState("battle-pan");
        followBattle=false;
        battleCamera=std::clamp(battleCamera-(x-dragX)*1.6f,0.f,float(fieldPanLimit));
        dragX=x;
        dirty|=battleCamera!=previous;
    }
    void updatePinch() {
        const auto& gesture=*pinch;
        const float cx=(pointerX+gesture.x)*.5f,cy=(pointerY+gesture.y)*.5f;
        const float dx=pointerX-gesture.x,dy=pointerY-gesture.y;
        const float zoom=zoomLevel(gesture.scale*std::sqrt(dx*dx+dy*dy)/gesture.distance);
        const float previousX=mapCenterX,previousY=mapCenterY,previousZoom=mapZoom;
        mapZoom=zoom;
        const float worldScale=mapScale();
        mapCenterX=gesture.worldX-(cx-mapWindow.cx())/worldScale;
        mapCenterY=gesture.worldY-(cy-mapWindow.cy())/worldScale;
        clampCamera();
        dirty|=mapCenterX!=previousX || mapCenterY!=previousY || mapZoom!=previousZoom;
    }
    bool handlePinch(const pxa::input::Pointer& p,float x,float y) {
        using P=pxa::input::PointerPhase;
        if (pinch) {
            const bool primary=p.pointer_id==pointerId;
            if ((!primary && p.pointer_id!=pinch->id) || p.action()==P::down) return true;
            if (p.action()==P::cancel) {
                resetPointer();
                return true;
            }
            if (primary) {pointerX=x;pointerY=y;}
            else {pinch->x=x;pinch->y=y;}
            updatePinch();
            if (p.action()==P::up) {
                // The remaining contact can drag immediately and cannot activate a city.
                if (primary) {
                    pointerId=pinch->id;
                    pointerX=pinch->x;
                    pointerY=pinch->y;
                }
                dragX=pointerX;dragY=pointerY;
                mapMotion={.dragAt=p.timestamp_us};
                pinch.reset();
            }
            return true;
        }
        if (!pointerDown || p.pointer_id==pointerId) return false;
        if (p.action()==P::down && mapDragging && mapWindow.contains(int(x),int(y)) &&
            mapWindow.contains(int(pointerX),int(pointerY))) {
            const float dx=pointerX-x,dy=pointerY-y;
            const float worldScale=mapScale();
            pinch=MapPinch{p.pointer_id,x,y,std::max(8.f,std::sqrt(dx*dx+dy*dy)),worldScale,
                mapCenterX+((pointerX+x)*.5f-mapWindow.cx())/worldScale,
                mapCenterY+((pointerY+y)*.5f-mapWindow.cy())/worldScale};
            dirty|=pressedControl.has_value();
            pressedControl.reset();
            mapTapTime=0;
            mapMotion={};
            dragMoved=true;
        }
        return true;
    }
    void on_pointer(pxa::Context&,const pxa::input::Pointer& p) {
        if (!ready || resize) return;
        using P=pxa::input::PointerPhase;
        const float x=(float(pxa::ui::canvas_to_surface_coordinate(p.x,display))*width/display.width-ox)/scale;
        const float y=(float(pxa::ui::canvas_to_surface_coordinate(p.y,display))*height/display.height-originY())/scale;
        if (handlePinch(p,x,y)) return;
        if (p.action()==P::move) {
            if (!pointerDown) return;
            pointerX=x;pointerY=y;
            if (!dragMoved && std::abs(x-pressX)+std::abs(y-pressY)>6.f) {
                dragMoved=true;
                mapTapTime=0;
                dirty|=pressedControl.has_value();
            }
            if (dragMoved && mapDragging && mapZoom>0) panMap(x,y,p.timestamp_us);
            if (dragMoved && battleDragging) panBattle(x);
            return;
        }
        int hit=-1, nearest=50000;
        if ((p.action()==P::down || (p.action()==P::up && !dragMoved)) && x>=0 && y>=0) {
            for (int i=count-1;i>=0;--i) {
                const auto& control=controls[i];
                if (!control.enabled) continue;
                if (page==Page::map && control.command==Command::select) {
                    const bool marker=control.box.contains(int(x),int(y));
                    if (!marker && !mapLabels[control.data].contains(int(x),int(y))) continue;
                    const auto [cx,cy]=cityCenters[control.data];
                    const int dx=int(x)-cx,dy=int(y)-cy;
                    const int distance=(marker?0:10000)+dx*dx+dy*dy;
                    if (distance<nearest) {nearest=distance;hit=i;}
                } else if (control.box.contains(int(x),int(y))) {
                    hit=i;
                    break;
                }
            }
        }
        if (p.action()==P::down && !pointerDown) {
            dirty|=controllerMode || hit>=0;
            if (controllerMode) info("han-banners input-touch");
            controllerMode=false;
            repeatPadAt=0;
            mapFocusCity=-1;
            mapMotion={.dragAt=p.timestamp_us};
            pointerDown=true;
            if (hit>=0) {
                pressedControl=controls[hit];
                mapTapTime=0;
            }
            pointerId=p.pointer_id;
            mapDragging=page==Page::map && !isMarching() && mapWindow.contains(int(x),int(y));
            battleDragging=page==Page::battle && !battle.paused && !battle.duel.visible() &&
                !battle.result && !tacticsOpen &&
                (fieldView?Rect{0,0,384,229+tallScene}:Rect{69,92,246,117+battleExtra()}).contains(int(x),int(y));
            pressX=dragX=pointerX=x;pressY=dragY=pointerY=y;dragMoved=false;
        } else if (pointerDown && p.ended()) {
            if (p.action()==P::up && mapDragging && dragMoved && mapZoom>0) {
                panMap(x,y,p.timestamp_us,false);
                releaseMap(p.timestamp_us);
            }
            if (p.action()==P::up && battleDragging && dragMoved) panBattle(x);
            pointerDown=false;
            mapDragging=false;
            battleDragging=false;
            const auto pressed=pressedControl;
            pressedControl.reset();
            if (!pressed) {
                const bool mapTap=p.action()==P::up && !dragMoved && hit<0 && page==Page::map &&
                    !isMarching() && mapZoom<2 && mapWindow.contains(int(x),int(y)) &&
                    mapWindow.contains(int(pressX),int(pressY)) &&
                    std::abs(x-pressX)+std::abs(y-pressY)<=6.f;
                if (mapTap && mapTapTime && p.timestamp_us>=mapTapTime &&
                    p.timestamp_us-mapTapTime<=350000 && std::abs(x-mapTapX)+std::abs(y-mapTapY)<20.f) {
                    zoomAt(x,y);
                    mapTapTime=0;
                    draw();
                } else {
                    mapTapTime=mapTap?p.timestamp_us:0;
                    mapTapX=x;
                    mapTapY=y;
                }
                return;
            }
            dirty=true;
            if (p.action()==P::up && !dragMoved &&
                std::abs(x-pressX)+std::abs(y-pressY)<=6.f &&
                ((hit>=0 && controls[hit].command==pressed->command && controls[hit].data==pressed->data) ||
                 (hit<0 && pressed->box.contains(int(pressX),int(pressY))))) {
                // Accept a small release drift, but only while this action is still available.
                for (int i=0;i<count;++i) if (controls[i].command==pressed->command &&
                    controls[i].data==pressed->data && controls[i].enabled) {
                    focus=i;
                    if (pressed->command==Command::battleView && pressed->data>=0) {
                        const int world=int(std::lround((x-fieldTrack.x)*Battle::fieldLength/(fieldTrack.w-4)));
                        viewBattleAt(world);
                    } else input(controls[i]);
                    draw();
                    break;
                }
            }
        }
    }
    void on_background(pxa::Context&) {
        audio.pause(true);
        gamepad.reset();
        controllerMode=false;
        seenPadShortcuts=0;
        repeatPadAt=0;
        resetPointer();
        if (page==Page::battle) {battle.paused=true;focus=0;}
        previousTick=0;
        (void)context->clock().set_period(0);
    }
    void on_foreground(pxa::Context&) {
        audio.pause((page==Page::battle || (page==Page::soundSettings && audioFrom==Page::battle)) &&
                    battle.paused);
        immersive();
        previousTick=0;
        dirty=true;
        if (ready) (void)context->clock().set_period(clockPeriod);
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event) {
        if (audio.event(event)) return true;
        if (auto d=pxa::ui::decode_display_metrics(event)) {
            resize|=display.width!=d->width || display.height!=d->height;
            display=*d;
            if (ready && !resize) layout();
            resetPointer();
            dirty=true;
            return true;
        }
        auto tick=pxa::decode_clock_tick(event);
        if (!tick) return false;
        if (!ready || !context->foreground()) return true;
        audio.tick(tick->timestamp_us);
        if (resize && !loadingArt) {
            auto task=context->tasks().start(openRenderer());
            if (!task) return std::unexpected(task.error());
            return true;
        }
        if (loadingArt) return true;
        repeatPadFocus(tick->timestamp_us);
        if (page==Page::deploy && count) audio.warmOpening(battle.defending);
        warmNextScene();
        if (page==Page::map && !isMarching()) glideMap(tick->timestamp_us);
        if (page==Page::map && isMarching()) {
            auto now=tick->timestamp_us;
            const int duration=travelDuration();
            if (previousTick && now>previousTick)
                marchingMs=std::min(duration,marchingMs+int(std::min<uint64_t>((now-previousTick)/1000,100)));
            previousTick=now;
            if (marchingMs>=duration) finishMarch();
            else if (playerRoute.legs()>1) {
                const auto [leg,progress]=playerMarchLeg();
                const auto [x,y]=marchPosition(leg,progress);
                mapCenterX=x;
                mapCenterY=y;
                clampCamera();
            }
            dirty=true;
        }
        if (page==Page::battle && !battle.paused && !tacticsOpen && !battle.awaitingOrders) {
            auto now=tick->timestamp_us;
            if (previousTick && now>previousTick) elapsed+=unsigned(std::min<uint64_t>(now-previousTick,100000));
            previousTick=now;
            const int round=battle.round;
            const int fled=battle.fled[0]+battle.fled[1];
            const bool animated=!battle.duel.ready() && elapsed>=33333;
            while (elapsed>=33333 && (!battle.result || battle.duel.visible() || battle.effects ||
                                      battle.strikeTicks || battle.siegeTicks)) {
                battle.step();
                if (followBattle && !battle.duel.visible())
                    battleCamera+=std::clamp(float(battleCameraTarget())-battleCamera,-7.f,7.f);
                elapsed-=33333;
            }
            if (battle.result && !battle.duel.visible() && !battle.effects && !battle.strikeTicks &&
                !battle.siegeTicks) conclude();
            else if (battle.round!=round && !battle.duel.visible()) {
                if (fled!=battle.fled[0]+battle.fled[1]) logState("battle-rout");
                if (!round) logState("battle-contact");
                if (battle.enemySiegeSortie) logState("enemy-sortie");
                if (battle.enemyAid.rounds==2) logState("enemy-assist");
                bool arrows=false;
                for (int side=0;side<2;++side) for (int lane=0;lane<Battle::rankCount;++lane)
                    arrows|=ranged(battle.troopFor(side,lane)) && battle.canReach(side,lane);
                audio.cue(arrows?arrowSound:clashSound);
            }
            dirty|=animated;
        }
        if (dirty) draw();
        return true;
    }
};
}
PXA_APPLICATION(han::App)
