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
    Rect inset(int dx,int dy) const {return {x+dx,y+dy,w-2*dx,h-2*dy};}
    bool contains(int u,int v) const {return u>=x && v>=y && u<x+w && v<y+h;}
    bool overlaps(Rect r) const {return x<r.x+r.w && r.x<x+w && y<r.y+r.h && r.y<y+h;}
};
constexpr Rect standardMap{8,42,264,177};
constexpr Rect wideMap{8,42,368,177};

enum class Page {home, choose, map, deploy, battle, report, help, ending, replace, council, roster,
                 cityDesk, dispatch, forces, gear, captive, gazetteer, seasonCouncil, regions, search,
                 soundSettings};
enum class Command {resume, newGame, guide, back, side, seat, work, march, season, select,
                    engage, shift, skill, pause, retreat, accept, home, replace, zoom, council,
                    roster, officer, hire, chooseOfficer, treat, market, militia, skipMarch, atlas,
                    chapter, difficulty, cityDesk, survey, forces, chooseSize, approach, send, recall,
                    rosterPage, gear, pickGear, equip, drill, orders, maneuver, duel, duelMove,
                    captiveChoice, gazetteer, browseCity, choosePlan, advanceSeason, inspectThreat,
                    regions, chooseRegion, locateRegionCity, claimRegion,
                    openSearch, searchOfficer, welcomeTalent, audioMenu, audioToggle, origin,
                    battleRank, rankSupport, clearRank,
                    confirmRetreat, centerMap, mapNames, pickTroop, pickWeapon, moveGeneral, strike,
                    battleView,deployTab,deployLane,pickEngine,cycleEngineLane,fireEngine};
struct Control {
    Rect box;
    Command command;
    int data;
    bool enabled;
};
enum UiIcon {coinsIcon,grainIcon,orderIcon,recruitIcon,farmIcon,wallsIcon,marketIcon,
             pacifyIcon,surveyIcon,spearIcon,bowIcon,horseIcon,namesIcon,squareIcon,wedgeIcon,
             wingsIcon,bladeIcon,skillIcon,ordersIcon,pauseIcon,moraleIcon,duelIcon,flankIcon,withdrawIcon};
enum MapProp {cityGate,sinan,armyTent,mapScroll,coastalBoat};
enum BattlePlaque {bronzePlaque,jadePlaque,vermilionPlaque};
enum GaugeTone {jadeGauge=1,redGauge,goldGauge,blueMarchGauge,redMarchGauge};
constexpr UiIcon formationIcons[]={squareIcon,wedgeIcon,wingsIcon};
constexpr UiIcon weaponIcons[]={spearIcon,bladeIcon,bowIcon};
inline constexpr const char* seasons[]={"春","夏","秋","冬"};
inline constexpr const char* rankNames[]={"上路","中路","下路"};
inline constexpr const char* troopMarks[]={"枪","弓","骑","盾","弩","重"};
inline constexpr const char* difficultyNames[]={"入门","常规","群雄"};
inline constexpr const char* difficultyHints[]={
    "开局钱粮更多，前四季可安心备战",
    "前两季备战，敌军会补防和下令",
    "敌军更早出击，攻城粮耗更高",
};
enum class Notice {none, recruited, cultivated, fortified, moved, income, saved, saveError, treaty, supplies, levies,
                    civic, encamped, withdrawn, goal};

struct App {
    pxa::Context* context = nullptr;
    pxa::ui::DisplayMetrics display{};
    std::optional<pxa::game::Renderer> renderer;
    std::array<int,art::pageCount> slots{};
    bool loadingArt=false;
    Rect mapWindow=standardMap;
    bool showMapNames=false;
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
    float scale = 1, ox = 0, oy = 0;
    bool sansText = false, texelAlpha = false;
    SansText sans;
    Page page = Page::home, helpFrom = Page::home;
    Page audioFrom=Page::home;
    GameAudio audio;
    Notice notice = Notice::none;
    Campaign campaign;
    Battle battle;
    Order departure;
    int chosenChapter=0, dispatchLead=-1, defenseLeader=-1, arrivingColumn=-1, gearCursor=0;
    Difficulty chosenDifficulty=Difficulty::standard;
    bool stageSiege=false, tacticsOpen=false, retreatPrompt=false;
    int tacticsRank=-1;
    static constexpr int fieldPanLimit=Battle::fieldLength-sceneWidth;
    float battleCamera=0;
    bool followBattle=true,battleDragging=false;
    bool warmingArt=false, lookaheadDone=false;
    DispatchSize dispatchSize=legion;
    int searchingOfficer=-1;
    int gazetteerCity=0;
    int regionalChoice=0;
    SeasonPlan seasonPlan=SeasonPlan::regular;
    Discovery discovery{};
    Settlement aftermath{};
    float mapCenterX=worldWidth/2.f, mapCenterY=worldHeight/2.f;
    float mapZoom=0;
    int officerSlot=0;
    Page rosterFrom=Page::council;
    std::array<Order,ownerCount> marching{};
    Order playerMarch{};
    bool seasonMarch=false, mapDragging=false, dragMoved=false;
    int marchingMs=0;
    float dragX=0, dragY=0;
    static constexpr int marchDuration=2500;
    bool isMarching() const {return seasonMarch || bool(playerMarch);}

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
    float mapTapX=0,mapTapY=0;
    uint64_t mapTapTime=0;
    int reportWinner = 0, reportCity = 0, reportLoss = 0;
    bool reportDefense = false, reportRetreat = false, reportSiege = false;
    uint64_t previousTick = 0;
    unsigned clockPeriod = 33;
    unsigned elapsed = 0, drawErrors = 0;
    pxa::input::Controller gamepad;

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
        Save data{};
        for (const auto& slot:saveSlots) {
            auto result=co_await context->storage().get(slot.key,std::span(data).first(slot.size));
            if (!result && result.error()==pxa::Error::not_found) continue;
            hasSave=result && *result==slot.size && restoreSave(data,slot.size,campaign);
            damagedSave=!hasSave;
            break;
        }
        co_return co_await openRenderer();
    }
    bool nativeWidePanel() const {
        return std::min(display.width,display.height)==480 &&
               std::max(display.width,display.height)==854;
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
        const bool useSans=scale/fx>=1.45f && fontSlots()>=10;
        if (sansText!=useSans || sansText) sans.reset();
        sansText=useSans && sans.init();
        labelFontScale=0;
        ox = std::round(left+(cw-sceneWidth*scale)/2);
        oy = std::round(top+(ch-sceneHeight*scale)/2);
    }
    pxa::Task<void> openRenderer() {
        ready = false;
        resize = false;
        renderer.reset();
        pxa::game::RenderOptions opt;
        const bool large=std::max(display.width,display.height)>480;
        const bool nativeWide=nativeWidePanel();
        const int divisor=!nativeWide && unsigned(display.width)*display.height>=800u*450u?2:1;
        width=std::max(1,int(display.width)/divisor);
        height=std::max(1,int(display.height)/divisor);
        opt.width = width;
        opt.height = height;
        opt.buffers = nativeWide||divisor==2?3:2;
        opt.direct_scanout = true;
        // On the 854x480 panel, rasterize while the display composes the previous frame.
        opt.borrow_commands=nativeWide;
        opt.max_draw_bytes = commands.capacity();
        auto r = co_await context->game().create(opt);
        if (r) {
            renderer.emplace(std::move(*r));
        } else if (opt.buffers==3 && (r.error()==pxa::Error::resource_limit ||
                                     r.error()==pxa::Error::unsupported ||
                                     (opt.borrow_commands && r.error()==pxa::Error::invalid_argument))) {
            opt.buffers=2;
            opt.borrow_commands=false;
            auto fallback=co_await context->game().create(opt);
            if (fallback) renderer.emplace(std::move(*fallback));
            else if (nativeWide && fallback.error()==pxa::Error::resource_limit) {
                width=opt.width=display.width/2;
                height=opt.height=display.height/2;
                auto smaller=co_await context->game().create(opt);
                if (!smaller) co_return std::unexpected(smaller.error());
                renderer.emplace(std::move(*smaller));
            } else co_return std::unexpected(fallback.error());
        } else if (large && divisor==1 && r.error()==pxa::Error::resource_limit) {
            width=opt.width=std::max(1,int(display.width)/2);
            height=opt.height=std::max(1,int(display.height)/2);
            auto smaller=co_await context->game().create(opt);
            if (!smaller) co_return std::unexpected(smaller.error());
            renderer.emplace(std::move(*smaller));
        } else {
            co_return std::unexpected(r.error());
        }
        texelAlpha=renderer->supports(pxa::game::RenderCapability::sprite_texel_alpha);
        auto palette = pxa::game::Upload(*renderer,commands.bytes()).palette(art::palette);
        if (!palette) co_return palette;
        slots.fill(-1);
        detailedArt=width>=640 && height>=360 && renderer->info().max_texture_dimension>=256;
        layout();
        ready = true;
        dirty = true;
        if (!audio.bank.context) audio.start(*context);
        clockPeriod=nativeWide||divisor==2?16:33;
        (void)context->clock().set_period(clockPeriod);
        logState("ready");
        draw();
        co_return pxa::Result<void>{};
    }
    art::Tile titleBackground() const {
        return width*2>=height*3 ? art::coverWide : art::coverSquare;
    }

    bool detailedArt=false;
    art::Backdrop backdropTiles(art::Tile image) const {
        if (detailedArt && sansText)
            for (const auto& backdrop:art::detailedBackdrops)
                if (backdrop.compact==image.page) return backdrop;
        return {image.page,image.page,1,1,image.w,image.h};
    }

    int fontSlots() const {
        return int(std::min<unsigned>(16,renderer->info().max_textures))-SansText::banks;
    }

    using Pages=std::array<bool,art::pageCount>;
    bool hasColumns() const {
        for (const auto& unit:campaign.columns) if (unit) return true;
        return false;
    }
    static bool usesClassicTroops(const Lineup& lineup) {
        return std::any_of(lineup.begin(),lineup.end(),[](Troop troop){return troop<shields;});
    }
    art::Tile battlefield() const {
        return battle.walls>=3?art::siege:art::battlefields[sites[battle.to].terrain];
    }
    static void atlasPath(char (&name)[48],unsigned page) {
        std::snprintf(name,sizeof name,"assets/atlas-%u.pxr",page);
    }
    pxa::Task<void> warmArt(Page source,std::array<art::Tile,4> tiles) {
        for (auto tile:tiles) {
            if (page!=source || (source==Page::battle && !tacticsOpen)) break;
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
            tiles={city.fort>=3?art::siege:art::battlefields[sites[departure.to].terrain],
                   art::troops[1][0][0],ownLegacy?art::tacticTroops[0][0][0]:art::Tile{},
                   art::tacticTroops[1][0][0]};
        } else if (page==Page::battle && tacticsOpen && battle.mayDuel()) {
            tiles={art::siege,art::duelFrames[0][0],art::duelFrames[1][0]};
        } else return;
        const auto backdrop=backdropTiles(tiles[0]);
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
        auto backdrop=[&](art::Tile image) {
            const auto tiles=backdropTiles(image);
            for (int i=0;i<tiles.columns*tiles.rows;++i) set[tiles.first+i]=true;
        };
        require(art::plaques[0]);
        auto room=[&](art::Tile background=art::warHall) {
            backdrop(background);
            require(art::windows[0]);
        };
        auto actor=[&](int slot) {
            if (slot>=0) require(art::portraits[campaign.leader(slot).portrait]);
        };
        switch (page) {
            case Page::home:backdrop(titleBackground());require(art::logo);break;
            case Page::replace:backdrop(titleBackground());require(art::windows[0]);break;
            case Page::ending:
                backdrop(titleBackground());require(art::windows[0]);
                require(art::outcomes[campaign.won>0?0:1]);break;
            case Page::choose:room();require(art::portraits[0]);break;
            case Page::roster:
                room();
                for (int i=rosterStart();i<std::min(rosterCount(),rosterStart()+rosterSize);++i)
                    actor(rosterSlot(i));
                break;
            case Page::map:
                room();
                // Keep the tiles resident while panning across quadrant seams.
                for (int n=0;n<4;++n) require(mapTile(n));
                if (isMarching() || hasColumns()) require(art::marchers[0][0][0]);
                break;
            case Page::gazetteer:
            case Page::seasonCouncil:
            case Page::soundSettings:
            case Page::regions:room();break;
            case Page::dispatch:room();actor(dispatchLead);break;
            case Page::cityDesk:room(art::cityCourtyard);actor(campaign.governor(selected));break;
            case Page::search:
                room(art::cityCourtyard);
                actor(searchingOfficer);
                if (const int id=campaign.localTalent(selected);campaign.knowsTalent(id))
                    require(art::portraits[talents[id].officer.portrait]);
                break;
            case Page::deploy: {
                room();
                actor(plannedLeader());
                const int enemy=campaign.cities[battle.defending?departure.from:departure.to].owner;
                if (enemy<3) require(art::portraits[officers[enemy][campaign.enemyOfficer(enemy,departure.to)].portrait]);
                require(art::troops[0][0][0]);
                break;
            }
            case Page::battle:
                require(art::portraits[battle.officer().portrait]);
                if (auto* foe=battle.opponent()) require(art::portraits[foe->portrait]);
                require(art::windows[0]);require(art::hudBars[0]);
                if (battle.duel.visible()) {
                    backdrop(art::siege);
                    require(art::duelFrames[0][0]);require(art::duelFrames[1][0]);
                } else {
                    backdrop(battlefield());
                    for (int side=0;side<2;++side)
                        if (usesClassicTroops(side?battle.enemyLineup:battle.plan.lineup)) {
                            require(art::troops[side][0][0]);
                            require(art::tacticTroops[side][0][0]);
                        }
                }
                break;
            case Page::report:
                require(art::windows[0]);require(art::outcomes[reportWinner>0?0:1]);
                backdrop(reportSiege?art::siege:art::battlefields[sites[reportCity].terrain]);break;
            case Page::gear:
            case Page::forces:room();require(art::equipment[0]);break;
            case Page::captive:
                room();
                if (campaign.prisoner)
                    require(art::portraits[officers[campaign.prisoner.side][campaign.prisoner.officer].portrait]);
                break;
            case Page::help:
            case Page::council:room();break;
        }
        return set;
    }
    pxa::Task<void> syncArt(Pages needed) {
        std::array<bool,16> occupied{};
        std::array<std::byte,1> blank{};
        pxa::Result<void> status{};
        const auto max=sansText?size_t(fontSlots()):
            std::min<size_t>(occupied.size(),renderer->info().max_textures);
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
                if (i>=art::firstDetailedPage &&
                    (status.error()==pxa::Error::quota_exceeded || status.error()==pxa::Error::resource_limit)) {
                    detailedArt=false;
                    status={};
                    info("han-banners using compact backgrounds");
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
            // The v7 roster makes the payload larger than the default 512-byte request.
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

    int sx(float x) const {return int(std::floor(ox+x*scale+.5f));}
    int sy(float y) const {return int(std::floor(oy+y*scale+.5f));}
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
    void backdropRegion(pxa::game::Frame& f,art::Tile image,Rect region,float anchor,float zoom=1.f) const {
        if (region.w<=0 || region.h<=0) return;
        const auto tiles=backdropTiles(image);
        const int sourceWidth=tiles.w*tiles.columns,sourceHeight=tiles.h*tiles.rows;
        const float aspect=float(width)/height;
        const int w=std::max(1,int(std::round(std::min(float(sourceWidth),sourceHeight*aspect)/zoom)));
        const int h=std::max(1,int(std::round(std::min(float(sourceHeight),sourceWidth/aspect)/zoom)));
        const int cx=int(std::round((sourceWidth-w)*anchor)),cy=(sourceHeight-h)/2;
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
        backdropRegion(f,image,{0,0,width,height},anchor);
    }
    template<class Paint>
    static void around(Rect outer,Rect hole,Paint paint) {
        paint(Rect{outer.x,outer.y,outer.w,hole.y-outer.y});
        paint(Rect{outer.x,hole.y+hole.h,outer.w,outer.y+outer.h-hole.y-hole.h});
        paint(Rect{outer.x,hole.y,hole.x-outer.x,hole.h});
        paint(Rect{hole.x+hole.w,hole.y,outer.x+outer.w-hole.x-hole.w,hole.h});
    }
    void mapBackdrop(pxa::game::Frame& f) const {
        around({0,0,width,height},toPixels(mapWindow),[&](Rect r) {
            backdropRegion(f,art::warHall,r,.42f);
        });
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
        drawStrip(f,marching?art::marchBars[0]:art::hudBars[0],p);
        const int border=std::max(1,p.h/6);
        const int room=p.w-2*border;
        if (value<=0 || maximum<=0 || room<=0 || p.h<=2*border) return;
        const int amount=std::max(1,room*std::min(value,maximum)/maximum);
        const int x=reverse?p.x+p.w-border-amount:p.x+border;
        const auto fill=marching?art::marchBars[1+tone-blueMarchGauge]:art::hudBars[tone];
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
    const art::Glyph& glyph(uint32_t code) const {
        const auto& face=art::glyphs;
        const auto it=std::lower_bound(face.begin(),face.end(),code,
            [](const art::Glyph& item,uint32_t key){return item.code<key;});
        return it!=face.end() && it->code==code ? *it : face[art::fallbackGlyph];
    }
    int fontScale(int requested) const {
        const float native=scale*requested/art::fontPixels;
        return std::max(1,int(requested>=28?std::round(native):std::floor(native)));
    }
    int fontHeight(int size) const {
        if (!sansText) return art::fontPixels*fontScale(size);
        return std::clamp(int(std::round(scale*std::max(11.f,size*.85f))),9,42);
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
        if (enabled && focus<0 && (action!=Command::select || data==selected)) focus=index;
        controls[index]={box,action,data,enabled};
        return enabled && ((controllerMode && focus==index) ||
               (pressedControl && !dragMoved && pressedControl->command==action && pressedControl->data==data));
    }
    void button(pxa::game::Frame& f,Rect r,const char* label,Command action,int data=0,
                bool enabled=true,bool main=false,int icon=-1) {
        const bool focused=registerControl(r,action,data,enabled);
        plaque(f,r,enabled,main||focused);
        if (focused && r.w>60) block(f,{r.x+4,r.y+5,2,r.h-10},gold);
        auto labelBox=r.inset(r.w<=60?3:6,r.h<21?1:r.h<24?2:3);
        const int iconSize=r.h<=21?12:16;
        const int iconGap=iconSize+4;
        if (icon>=0 && textWidth(label,14)<=toPixels(labelBox).w-(sx(iconGap)-sx(0))) {
            contain(f,art::commandIcons[icon],{labelBox.x+1,r.cy()-iconSize/2,iconSize,iconSize},true);
            labelBox.x+=iconGap;
            labelBox.w-=iconGap;
        }
        boxText(f,label,labelBox,14,enabled?ivory:dim,1);
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
    void portrait(pxa::game::Frame& f,int actor,Rect r) {
        frame(f,{r.x-1,r.y-1,r.w+2,r.h+2},edge,ink);
        contain(f,art::portraits[actor],r);
    }
    void drawHome(pxa::game::Frame& f) {
        fillBackground(f,titleBackground(),.57f);
        block(f,{14,14,26,2},gold);
        block(f,{14,14,2,15},gold);
        block(f,{344,248,26,2},gold);
        block(f,{368,235,2,15},gold);
        contain(f,art::logo,{25,59,213,57},true);
        block(f,{28,119,104,1},edge);
        if (hasSave) {
            button(f,{28,140,136,29},"继续征途",Command::resume,0,true,true);
            button(f,{28,175,136,28},"新建战役",Command::newGame);
            button(f,{28,210,136,27},"玩法说明",Command::guide);
        } else {
            button(f,{28,158,136,31},"新建战役",Command::newGame,0,true,true);
            button(f,{28,197,136,29},"玩法说明",Command::guide);
            if (damagedSave) text(f,"存档无法读取",28,232,13,coral);
        }
        button(f,{274,211,87,27},"音频",Command::audioMenu);
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
            portrait(f,i,{card.cx()-29,card.y+5,58,58});
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
    const char* cityName(int i) const {return cityLabel(i,campaign.year());}
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
    void panMap(float x,float y) {
        const float previousX=mapCenterX,previousY=mapCenterY;
        const float zoom=mapScale();
        mapCenterX-=(x-dragX)/zoom;
        mapCenterY-=(y-dragY)/zoom;
        dragX=x;dragY=y;
        clampCamera();
        dirty|=mapCenterX!=previousX || mapCenterY!=previousY;
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
    };
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
    void path(pxa::game::Frame& f,const Road& road,uint16_t color,int stroke) {
        const auto line=routeLine(road);
        for (int i=1;i<line.count;++i) {
            auto [x,y]=line.points[i-1];
            auto [u,v]=line.points[i];
            drawSegment(f,mapX(x),mapY(y),mapX(u),mapY(v),color,stroke);
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
        for (int n=0;n<4;++n) {
            const Rect crop=mapTileCrop(n);
            if (crop.w<=0 || crop.h<=0) continue;
            const auto base=mapTile(n);
            const int l=(crop.x*base.w+w/2)/w,r=((crop.x+crop.w)*base.w+w/2)/w;
            const int t=(crop.y*base.h+h/2)/h,b=((crop.y+crop.h)*base.h+h/2)/h;
            if (l>=r || t>=b) continue;
            const int wx=(n%2)*w+crop.x,wy=(n/2)*h+crop.y;
            const int x=mapX(wx),y=mapY(wy);
            const int endX=mapX(wx+crop.w),endY=mapY(wy+crop.h);
            if (x>=endX || y>=endY) continue;
            const Rect dst{sx(x),sy(y),sx(endX)-sx(x),sy(endY)-sy(y)};
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
    void armyOnMap(pxa::game::Frame& f,Order order,bool own) {
        if (!order) return;
        const auto* road=findRoad(order);
        const bool atSea=road && road->coastal;
        if (road) path(f,*road,own?(atSea?seaRouteActive:mint):coral,2);
        const float t=std::clamp(float(marchingMs)/marchDuration,0.f,1.f);
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
        gauge(f,{x-13,y+4,26,5},marchingMs,marchDuration,own?blueMarchGauge:redMarchGauge);
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
        if (labelFontScale==scale && labelYear==year) return;
        labelFontScale=scale;
        labelYear=year;
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
            const int x=int(std::round(left+u*zoom)),y=int(std::round(top+v*zoom));
            cityCenters[i]={x,y};
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
    void drawCity(pxa::game::Frame& f,int i,Rect islandLabel,bool threatened) {
        if (!mapMarkers[i].w) return;
        const auto [x,y]=cityCenters[i];
        const int owner=campaign.cities[i].owner;
        bool active=i==selected || i==destination;
        bool emph=!isMarching() && registerControl({x-9,y-7,19,15},Command::select,i);
        if (active || emph || threatened) frame(f,{x-8,y-7,17,15},emph?ivory:threatened?coral:gold,ink);
        contain(f,art::mapProps[cityGate],{x-7,y-8,13,14},true);
        block(f,{x-6,y+4,12,3},ink);
        block(f,{x-5,y+5,10,2},colors[owner]);
        if (showMapNames && emph && !mapLabels[i].w) mapLabels[i]=visibleMapLabel(i,islandLabel);
        const Rect name=mapLabels[i];
        if (!name.w) return;
        text(f,cityName(i),name.cx(),name.cy()-mapLabelSize/2,mapLabelSize,emph?ivory:active?gold:ivory,1,ink);
    }
    void drawCityPanel(pxa::game::Frame& f) {
        char buf[64];
        int inspected=destination<0?selected:destination;
        auto city=campaign.cities[inspected];
        skinFrame(f,{279,42,97,177});
        boxText(f,cityName(inspected),{287,46,47,21},18,gold);
        boxText(f,terrains[sites[inspected].terrain],{336,47,32,20},12,dim,2);
        boxText(f,cityDistrict(inspected,campaign.year()),{287,69,81,15},12,dim);
        boxText(f,ownerName(city.owner),{287,84,81,16},13,colors[city.owner]);
        std::snprintf(buf,sizeof buf,"守军 %d",city.troops);
        boxText(f,buf,{287,100,81,15},13);
        std::snprintf(buf,sizeof buf,"田%d  城防%d",city.fields,city.fort);
        boxText(f,buf,{287,115,81,15},12,dim);
        block(f,{287,130,81,1},edge);

        const bool editable=!isMarching();
        if (city.owner==campaign.player && destination<0) {
            button(f,{286,134,83,22},"征募 80",Command::work,recruit,editable&&campaign.canWork(selected,recruit),true,recruitIcon);
            button(f,{286,158,83,24},"屯田 95",Command::work,farm,editable&&campaign.canWork(selected,farm),false,farmIcon);
            button(f,{286,185,55,24},"内政",Command::cityDesk,0,editable);
        } else {
            const bool local=adjacent(selected,inspected);
            const bool transfer=city.owner==campaign.player;
            if (local) {
                std::snprintf(buf,sizeof buf,"可派 %d",campaign.own(selected)?campaign.dispatch(selected):0);
                boxText(f,buf,{287,132,81,15},13,dim,1);
                const auto* road=findRoad({selected,inspected});
                boxText(f,road && road->name?road->name:"区域通路",{287,147,81,15},12,
                        road && road->coastal?seaRouteActive:gold,1);
            } else if (!transfer) boxText(f,"非相邻城池",{287,137,81,20},12,dim,1);
            if (transfer)
                button(f,{286,163,83,20},"设为起点",Command::origin,inspected,editable);
            if (!transfer && campaign.ceasefire(city.owner))
                boxText(f,"盟约生效",{287,165,81,17},12,gold,1);
            button(f,{286,185,55,24},transfer?"调兵":"出征",Command::march,inspected,
                   editable&&campaign.canMarch(selected,inspected),true);
        }
        button(f,{344,185,25,24},"志",Command::gazetteer,inspected,editable);
    }
    void drawMap(pxa::game::Frame& f) {
        mapBackdrop(f);
        char buf[60];
        const int year=campaign.year();
        if (year<220) std::snprintf(buf,sizeof buf,"建安%d年 %s",year-195,seasons[campaign.season()]);
        else std::snprintf(buf,sizeof buf,"%d年 %s",year,seasons[campaign.season()]);
        plaque(f,{8,6,122,28});
        text(f,buf,69,13,14,gold,1);
        std::snprintf(buf,sizeof buf,"%d",campaign.gold);
        resourcePlate(f,{138,6,72,28},coinsIcon,buf,gold);
        std::snprintf(buf,sizeof buf,"%d",campaign.grain);
        resourcePlate(f,{216,6,72,28},grainIcon,buf);
        std::snprintf(buf,sizeof buf,"%d/%d",campaign.actions,campaign.actionCapacity());
        resourcePlate(f,{295,6,81,28},orderIcon,buf,mint);

        const auto threat=isMarching()?Order{}:campaign.nextAttack();
        mapArt(f);
        for (const auto& r:roads) {
            const bool near=selected==r.from || selected==r.to;
            if (near && !isMarching()) path(f,r,ink,2);
            const auto color=r.coastal?(near?seaRouteActive:seaRoute):(near?gold:rgb(129,121,81));
            path(f,r,color,1);
        }
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
        for (int i=0;i<cityCount;++i)
            if (i!=selected && i!=destination) drawCity(f,i,islandLabel,i==threat.to);
        drawCity(f,selected,islandLabel,selected==threat.to);
        if (destination>=0) drawCity(f,destination,islandLabel,destination==threat.to);
        if (islandLabel.w && !islandLabel.overlaps(mapCompass)) text(f,"台湾岛",islandLabel.cx(),islandLabel.cy()-6,12,dim,1,ink);
        for (const auto& unit:campaign.columns) {
            if (!unit || !unit.eta) continue;
            auto [u,v]=marchPosition(unit.road,.45f);
            const int x=mapX(u),y=mapY(v);
            const Rect camp{x-9,y-12,19,17};
            if (!insideMap(camp)) continue;
            frame(f,camp,mint,ink);
            contain(f,art::mapProps[armyTent],{camp.x+2,camp.y+1,camp.w-4,camp.h-2},true);
        }
        if (isMarching()) {
            for (auto column:marching) armyOnMap(f,column,false);
            armyOnMap(f,playerMarch,true);
        }
        drawCompass(f);
        if (mapWindow.w==standardMap.w) drawCityPanel(f);

        battlePanel(f,{8,220,368,21});
        if (isMarching()) {
            count=0;
            focus=0;
            if (playerMarch) std::snprintf(buf,sizeof buf,"%s → %s",
                                         cityName(playerMarch.from),cityName(playerMarch.to));
            else std::snprintf(buf,sizeof buf,"诸军正在进发");
            boxText(f,buf,{17,222,350,17},12,playerMarch?mint:coral);
            gauge(f,{12,248,253,9},marchingMs,marchDuration,blueMarchGauge);
            button(f,{274,243,102,19},"加速抵达",Command::skipMarch,0,true,true,horseIcon);
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
            case Notice::supplies: status="商粮已入库"; break;
            case Notice::levies: status="边城乡勇已归队"; break;
            case Notice::civic: status="城中内政已更新"; break;
            case Notice::encamped: status="军队已扎营，次季抵达"; tone=mint; break;
            case Notice::withdrawn: status="围城军已撤回"; break;
            case Notice::goal: status="剧本悬赏已达成"; tone=gold; break;
            case Notice::none: break;
        }
        if (status) boxText(f,status,{17,222,threat?244:350,17},12,tone);
        else {
            std::snprintf(buf,sizeof buf,"领地 %d/%d",campaign.holdings(campaign.player),cityCount);
            boxText(f,buf,{17,222,threat?83:93,17},12,ivory);
            block(f,{threat?101:112,225,1,11},edge);
            boxText(f,"季末",{threat?105:119,222,threat?35:42,17},12,dim,1);
            for (int i=0;i<2;++i) {
                std::snprintf(buf,sizeof buf,i?"+%d粮":"+%d金",campaign.income(i!=0));
                if (threat) boxText(f,buf,{141+61*i,222,60,17},12,i?mint:gold);
                else {
                    const int x=168+98*i;
                    contain(f,art::commandIcons[i?grainIcon:coinsIcon],{x,224,13,13},true);
                    boxText(f,buf,{x+16,222,78,17},12,i?mint:gold);
                }
            }
        }
        if (threat) {
            const bool focus=registerControl({266,222,102,17},Command::inspectThreat,0);
            block(f,{265,224,1,13},edge);
            contain(f,art::commandIcons[ordersIcon],{270,224,12,12},true);
            std::snprintf(buf,sizeof buf,"%s告急",cityName(threat.to));
            boxText(f,buf,{284,222,83,17},12,focus?gold:coral);
        }
        button(f,{8,243,41,19},"战法",Command::guide);
        button(f,{51,243,41,19},"封卷",Command::home);
        button(f,{94,243,41,19},"军议",Command::council);
        button(f,{137,243,27,19},"−",Command::zoom,-1,mapZoom>0);
        button(f,{166,243,27,19},"+",Command::zoom,1,mapZoom<2);
        button(f,{195,243,42,19},"城名",Command::mapNames,0,true,showMapNames,namesIcon);
        button(f,{239,243,42,19},mapWindow.w==wideMap.w?"城务":"全图",Command::atlas);
        button(f,{283,243,93,19},"季末军议",Command::season,0,true,true);
    }
    static art::Tile troopTile(int side,Troop troop,int pose=0) {
        return troop<shields?art::troops[side][troop][pose]:art::mixedForces[3*side+troop-shields];
    }
    void drawDeploy(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const auto destinationCity=campaign.cities[departure.to];
        const int enemy=campaign.cities[battle.defending?departure.from:departure.to].owner;
        const int lead=plannedLeader();
        header(f,battle.defending?"守城布阵":"出征布阵",cityName(departure.to));
        plaque(f,{12,39,360,38});
        if (lead>=0) portrait(f,campaign.leader(lead).portrait,{18,42,31,31});
        const int nameLeft=lead>=0?55:21;
        boxText(f,lead>=0?campaign.leader(lead).name:"城中守备",{nameLeft,42,125-nameLeft,17},14,mint);
        number(f,battle.defending?destinationCity.troops:campaign.columns[arrivingColumn].troops,nameLeft,60,13);
        const auto* foe=enemy<3?&officers[enemy][campaign.enemyOfficer(enemy,departure.to)]:nullptr;
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
        std::snprintf(desc,sizeof desc,"敌 %s / %s / %s",troopMarks[enemyForces[0]],
                      troopMarks[enemyForces[1]],troopMarks[enemyForces[2]]);
        boxText(f,desc,{128,42,128,17},14,coral,1);
        std::snprintf(desc,sizeof desc,"%s · %s%d",terrains[sites[departure.to].terrain],
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
            for (int i=0;i<troopCount;++i) {
                const Rect card{12+121*(i%3),135+40*(i/3),117,37};
                const bool focused=registerControl(card,Command::pickTroop,i);
                plaque(f,card,true,i==deployment.lineup[deploymentLane] || focused);
                tile(f,troopTile(0,Troop(i)),{card.x+4,card.y+1,34,34},true);
                boxText(f,troopNames[i],{card.x+40,card.y+2,72,16},14,ivory);
                boxText(f,troopHints[i],{card.x+40,card.y+19,72,16},12,
                        Battle::advantage(Troop(i),opposing)>=115?mint:dim);
            }
            std::snprintf(desc,sizeof desc,"%s对位 · 敌%s",rankNames[deploymentLane],troopNames[opposing]);
            battlePanel(f,{12,213,359,18});
            boxText(f,desc,{19,214,345,16},12,dim,1);
        } else {
            battlePanel(f,{8,105,368,124});
            boxText(f,"阵法",{17,108,100,16},12,gold);
            boxText(f,formationHints[deployment.formation],{148,108,218,16},12,mint,2);
            for (int i=0;i<3;++i)
                button(f,{12+121*i,124,117,21},formations[i],Command::shift,i,true,
                       i==deployment.formation,formationIcons[i]);
            boxText(f,"武将兵器",{17,146,100,16},12,gold);
            boxText(f,lead>=0?weaponHints[deployment.weapon]:"没有领军武将",{148,146,218,16},12,dim,2);
            for (int i=0;i<3;++i)
                button(f,{12+121*i,162,117,21},weaponNames[i],Command::pickWeapon,i,lead>=0,
                       i==deployment.weapon,weaponIcons[i]);
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
        button(f,{241,231,130,28},"击鼓开战",Command::engage,0,true,true);
    }

    int fieldX(int x) const {return x-int(std::round(battleCamera));}
    static bool fieldVisible(int x,int margin=0) {return x>=-margin && x<=sceneWidth+margin;}
    static int groundY(int lane) {return 167+19*lane;}
    void dust(pxa::game::Frame& f,int x,int y,int direction,int phase) {
        for (int i=0;i<2;++i) {
            const int age=(phase+i*7)%14;
            const int px=x-direction*(age+6);
            if (!fieldVisible(px) || age>10) continue;
            block(f,{px,y-age/3,age<5?3:2,2},age<5?rgb(179,142,94):rgb(119,93,65));
        }
    }
    void sparks(pxa::game::Frame& f,int x,int y,int age,int count,bool rubble=false) {
        if (!fieldVisible(x,18) || age>=12) return;
        for (int i=0;i<count;++i) {
            const int px=x+(i%5-2)*age*2+((i*13)%7-3);
            if (!fieldVisible(px)) continue;
            const int py=y-age*(1+i%3)+age*age/6;
            const int size=age<6?2+(i%2):2;
            const auto color=rubble?(i%2?rgb(208,170,117):rgb(145,111,77)):
                age<5?ivory:gold;
            block(f,{px,py,size,size},color);
        }
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
    void drawFieldMap(pxa::game::Frame& f,bool interactive) {
        constexpr Rect track{87,69,282,17};
        battlePanel(f,{8,64,368,27});
        if (interactive)
            button(f,{14,67,65,21},"随军",Command::battleView,-1,true,followBattle);
        else boxText(f,"战线",{14,68,65,18},12,dim,1);
        const auto markerX=[&](int world) {return track.x+world*(track.w-4)/Battle::fieldLength;};
        const int windowLeft=markerX(int(battleCamera));
        const int windowRight=markerX(int(battleCamera)+sceneWidth)+3;
        block(f,track,rgb(36,28,23));
        block(f,{windowLeft,track.y,windowRight-windowLeft,track.h},rgb(78,60,38));
        block(f,{windowLeft,track.y,windowRight-windowLeft,1},gold);
        for (int lane=0;lane<Battle::rankCount;++lane)
            for (int side=0;side<2;++side)
                if (battle.ranks[side][lane])
                    block(f,{markerX(battle.fronts[side][lane]),track.y+3+4*lane,4,3},side?coral:mint);
        if (!interactive) return;
        for (int i=0;i<3;++i) {
            const Rect part{track.x+i*94,track.y,94,track.h};
            if (registerControl(part,Command::battleView,i))
                block(f,{part.x,part.y+part.h-1,part.w,1},ivory);
        }
    }
    void army(pxa::game::Frame& f,int lane,int who) {
        const int current=battle.ranks[who][lane];
        if (!current) return;
        const bool engaged=battle.canReach(who,lane);
        const Troop troop=battle.troopFor(who,lane);
        const Formation formation=who?battle.enemyFormation:battle.plan.formation;
        const Maneuver maneuver=battle.stance(who,lane);
        const int n=std::clamp(1+current*10/std::max(1,battle.initial[who]),1,4);
        const int direction=who?-1:1;
        const int lead=fieldX(battle.fronts[who][lane]);
        const int gap=formation==square?19:23;
        const int target=engaged?battle.attackTarget(who,lane):-1;
        for (int i=n-1;i>=0;--i) {
            const int x=lead-direction*i*gap;
            if (!fieldVisible(x,24)) continue;
            const int extraStep=troop>=shields && maneuver!=hold?(battle.ticks/3+i)%3:0;
            const int foot=groundY(lane)-i%2*3;
            const int y=foot-extraStep;
            const int stagger=(i*7+lane*4+who*3)%12;
            int animation=maneuver==hold?0:1+(battle.ticks/4+stagger)%2;
            if (engaged) {
                const int action=(battle.roundClock+stagger)%Battle::period;
                animation=0;
                if (action<7) animation=4;
                else if (action<13) animation=5;
                else if (action>=31) animation=3;
                if (ranged(troop) && action<10 && i==0) {
                    const int end=fieldX(battle.fronts[1-who][target]);
                    const int shot=x+(end-x)*action/10;
                    const int height=y-24+(groundY(target)-groundY(lane))*action/10-action%4;
                    line(f,shot,height,shot+direction*(troop==bow?9:13),height+1,gold,1);
                }
            }
            block(f,{x-11,foot-1,22,3},rgb(57,46,31));
            auto sprite=troopTile(who,troop,animation);
            if (troop<shields && (engaged || maneuver==hold || maneuver==fallBack || maneuver==press)) {
                const int phase=(engaged?battle.roundClock+stagger:battle.ticks*2+stagger)%Battle::period;
                if (battle.morale[who]<24) sprite=art::tacticTroops[who][troop][phase<18?4:5];
                else if (maneuver==hold || maneuver==fallBack) sprite=art::tacticTroops[who][troop][phase<10?1:0];
                else if (maneuver==press) sprite=art::tacticTroops[who][troop][phase<11?3:2];
                else if (battle.round && phase<8 && battle.rankLosses[who][lane]>battle.rankLosses[1-who][lane]*2)
                    sprite=art::tacticTroops[who][troop][1];
            }
            tile(f,sprite,{x-21,y-41,42,42},true);
        }
        if (fieldVisible(lead,18) && !engaged && maneuver!=hold)
            dust(f,lead-direction*15,groundY(lane),direction,battle.ticks+lane*4);
    }
    void drawSiegeField(pxa::game::Frame& f) {
        if (battle.plan.engine==noEngine) return;
        const int gate=fieldX(Battle::gatePosition);
        const int x=fieldX(battle.enginePosition()),y=groundY(battle.plan.engineLane);
        if (battle.wallPoints && fieldVisible(gate,56))
            tile(f,art::mixedForces[8],{gate-50,105,100,96},true);
        if (battle.ranks[0][battle.plan.engineLane] && fieldVisible(x,32))
            tile(f,art::mixedForces[5+battle.plan.engine],{x-32,y-57,64,58},true);
        if (!battle.siegeTicks) return;
        if (battle.plan.engine==stoneThrower && battle.siegeTicks>7) {
            const int t=23-battle.siegeTicks;
            const int rock=x+(gate-x)*t/16;
            const int height=y-40-(16-t)*t/2;
            if (fieldVisible(rock,4)) block(f,{rock-3,height-3,6,6},ivory);
        } else sparks(f,gate-10,171,battle.plan.engine==ram?(23-battle.siegeTicks)/2:7-battle.siegeTicks,9,true);
    }
    void drawFieldGeneral(pxa::game::Frame& f) {
        if (battle.commander<0 || !battle.generalHealth) return;
        const int x=fieldX(battle.generalPosition(battle.generalLane,battle.atFront));
        if (!fieldVisible(x,24)) return;
        const int y=groundY(battle.generalLane)-27;
        if (battle.strikeTicks) {
            const int travel=15-battle.strikeTicks;
            if (battle.plan.weapon==longbow) {
                const int target=fieldX(battle.fronts[1][battle.lastStrikeLane]);
                const int flight=x+(target-x)*travel/14;
                const int height=y+(groundY(battle.lastStrikeLane)-groundY(battle.generalLane))*travel/14;
                line(f,flight,height+2,flight+19,height+4,gold,2);
            } else line(f,x+12+travel,y+12,x+32+travel,y-6,gold,3);
        }
        portrait(f,battle.officer().portrait,{x-12,y-12,24,24});
        gauge(f,{x-12,y+12,24,5},battle.generalHealth,100,jadeGauge);
    }
    void generalControls(pxa::game::Frame& f) {
        if (!battle.canMoveGeneral()) return;
        for (int lane=0;lane<3;++lane) for (int front=0;front<2;++front) {
            if (!battle.ranks[0][lane] || (lane==battle.generalLane && bool(front)==battle.atFront)) continue;
            const int center=fieldX(battle.generalPosition(lane,front));
            if (center<83 || center>301) continue;
            Rect zone{std::clamp(center-21,70,271),125+27*lane,43,27};
            if (registerControl(zone,Command::moveGeneral,lane*2+front)) {
                battlePanel(f,zone,jadePlaque);
                boxText(f,front?"阵前":"后阵",zone.inset(4,4),11,gold,1);
            }
        }
    }
    void drawRanks(pxa::game::Frame& f) {
        for (int lane=0;lane<Battle::rankCount;++lane) {
            const int y=124+27*lane;
            const Rect own{8,y,60,24}, foe{316,y,60,24};
            const bool marked=registerControl(own,Command::battleRank,lane,!battle.result);
            battlePanel(f,own,marked||battle.rankOrders[lane].rounds?jadePlaque:bronzePlaque);
            battlePanel(f,foe,battle.enemyRank==lane?vermilionPlaque:bronzePlaque);
            char count[12];
            for (int side=0;side<2;++side) {
                const auto card=side?foe:own;
                boxText(f,troopMarks[battle.troopFor(side,lane)],{card.x+5,y+4,17,16},12,dim,1);
                std::snprintf(count,sizeof count,"%d",battle.ranks[side][lane]);
                boxText(f,count,{card.x+23,y+4,31,16},12,side?coral:mint,2);
            }
        }
    }

    void drawPause(pxa::game::Frame& f) {
        count=0;
        battlePanel(f,{72,83,240,122});
        if (retreatPrompt) {
            boxText(f,battle.defending?"确认弃城?":"确认撤军?",{84,93,216,27},20,coral,1);
            button(f,{87,144,101,30},"返回",Command::back,0,true,true);
            button(f,{196,144,101,30},battle.defending?"弃城":"撤军",Command::confirmRetreat,0,true,false,withdrawIcon);
            return;
        }
        boxText(f,"会战暂停",{84,93,216,27},20,gold,1);
        button(f,{87,132,101,28},"继续战斗",Command::pause,0,true,true);
        button(f,{196,132,101,28},battle.defending?"弃城":"撤军",Command::retreat,0,true,false,withdrawIcon);
        button(f,{142,168,100,25},"音频",Command::audioMenu);
    }
    void combatCard(pxa::game::Frame& f,Rect card,const Officer* actor,
                    int value,int total,int morale,bool enemy,bool duel) {
        battlePanel(f,card,enemy?vermilionPlaque:jadePlaque);
        const Rect portraitBox{enemy?card.x+card.w-42:card.x+10,card.y+10,32,32};
        if (actor) portrait(f,actor->portrait,portraitBox);
        else {
            frame(f,portraitBox,edge,ink);
            boxText(f,"守",portraitBox.inset(4,3),18,gold,1);
        }
        const int x=card.x+(enemy?9:51);
        const int align=enemy?2:0;
        char caption[32];
        std::snprintf(caption,sizeof caption,"%d",value);
        boxText(f,duel?(actor?actor->name:"守阵校尉"):caption,{x,card.y+5,80,17},
                duel?14:16,enemy?coral:mint,align);
        gauge(f,{x,card.y+23,80,9},value,total,enemy?redGauge:jadeGauge,enemy);
        std::snprintf(caption,sizeof caption,duel?"体力%d":"军心%d",duel?value:morale);
        if (!duel) contain(f,art::commandIcons[moraleIcon],{enemy?x+67:x,card.y+34,12,12},true);
        boxText(f,caption,{x+(enemy||duel?0:15),card.y+33,duel?80:65,16},12,ivory,align);
    }
    void battleHud(pxa::game::Frame& f,bool duel=false) {
        combatCard(f,{8,5,140,55},&battle.officer(),duel?battle.duel.health[0]:battle.troops[0],
                   duel?100:battle.initial[0],battle.morale[0],false,duel);
        combatCard(f,{236,5,140,55},battle.opponent(),duel?battle.duel.health[1]:battle.troops[1],
                   duel?100:battle.initial[1],battle.morale[1],true,duel);
        battlePanel(f,{153,5,78,55});
        boxText(f,duel?"阵前斗将":cityName(battle.to),{159,9,66,18},16,gold,1);
        char line[32];
        if (duel)
            std::snprintf(line,sizeof line,"第%d合",std::min(3,battle.duel.round+1));
        else if (battle.plan.engine!=noEngine)
            std::snprintf(line,sizeof line,"城防 %d",battle.wallPoints);
        else std::snprintf(line,sizeof line,"%s",formations[battle.enemyFormation]);
        boxText(f,line,{159,28,66,16},13,duel?dim:coral,1);
        const bool approaching=!battle.round;
        constexpr int openingGap=Battle::fieldLength-2*Battle::campFront;
        const int progress=approaching?std::max(0,openingGap-battle.closestGap()):battle.roundClock;
        gauge(f,{162,47,60,7},duel?battle.duel.round:progress,
              duel?3:approaching?openingGap-std::max(battle.range(0),battle.range(1)):Battle::period,goldGauge);
    }
    void drawBattle(pxa::game::Frame& f) {
        if (battle.duel.visible()) {
            drawDuel(f);
            if (battle.paused) drawPause(f);
            return;
        }
        backdropRegion(f,battlefield(),{0,0,width,height},battleCamera/fieldPanLimit,1.22f);
        drawSiegeField(f);
        char buf[44];
        for (int lane=0;lane<3;++lane) {
            army(f,lane,0);
            army(f,lane,1);
        }
        if (battle.round && battle.roundClock<10)
            for (int side=0;side<2;++side) for (int lane=0;lane<Battle::rankCount;++lane)
                if (battle.rankLosses[side][lane])
                    sparks(f,fieldX(battle.fronts[side][lane]),groundY(lane)-20,battle.roundClock,4);
        drawFieldGeneral(f);
        battleHud(f);
        const Rect cueArea{128,93,128,22};
        const auto caption=[&](Rect box,const char* label,BattlePlaque tone,uint16_t color) {
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
            sparks(f,x,groundY(lane)-23,(25-battle.effects)/2,10);
            caption(cueArea,battle.officer().skill,jadePlaque,gold);
        } else if (battle.strikeTicks) {
            const int lane=battle.lastStrikeLane;
            sparks(f,fieldX(battle.fronts[1][lane]),groundY(lane)-23,15-battle.strikeTicks,6);
            casualty(1,battle.losses[1]);
        } else if (battle.round && battle.roundClock<15) {
            casualty(0,battle.losses[0]);
            casualty(1,battle.losses[1]);
        }
        if (!battle.effects && (battle.enemySkillQueued || battle.enemySkillTicks ||
                                 battle.enemyManeuver!=measured)) {
            const char* cue=battle.enemyWithdrawalQueued?"敌军将撤":
                            battle.enemySkillTicks?battle.opponent()->skill:
                            battle.enemySkillQueued?"敌将蓄势":nullptr;
            if (!cue) {
                std::snprintf(buf,sizeof buf,"敌%s%s",battle.enemyRank<0?"军":rankNames[battle.enemyRank],
                              maneuvers[battle.enemyManeuver]);
                cue=buf;
            }
            caption(cueArea,cue,vermilionPlaque,coral);
        } else if (!battle.effects && !battle.round) {
            std::snprintf(buf,sizeof buf,"离敌 %d 步",battle.closestGap());
            caption(cueArea,buf,bronzePlaque,ivory);
        }
        if (tacticsOpen && !battle.paused) {
            drawFieldMap(f,false);
            drawOrders(f);
            return;
        }
        battlePanel(f,{8,210,307,25});
        gauge(f,{17,218,48,9},battle.signal,100,goldGauge);
        std::snprintf(buf,sizeof buf,"令%d",battle.signal);
        boxText(f,buf,{71,214,51,17},12,gold);
        if (battle.plan.engine!=noEngine && battle.wallPoints && battle.engineShots &&
            battle.engineGap()>battle.engineRange())
            std::snprintf(buf,sizeof buf,"还差%d步",battle.engineGap()-battle.engineRange());
        else if (battle.maneuverRounds)
            std::snprintf(buf,sizeof buf,"%s %d合",maneuvers[battle.maneuver],battle.maneuverRounds);
        else std::snprintf(buf,sizeof buf,"%s",troopNames[battle.troopFor(0,battle.generalLane)]);
        boxText(f,buf,{128,214,81,17},12,battle.maneuverRounds?mint:dim);
        if (battle.plan.engine!=noEngine) {
            if (!battle.wallPoints) std::snprintf(buf,sizeof buf,"城防已破");
            else if (!battle.engineShots) std::snprintf(buf,sizeof buf,"器械用尽");
            else std::snprintf(buf,sizeof buf,"%s %d",battle.plan.engine==ram?"撞城":"投石",battle.engineShots);
            button(f,{216,210,99,25},buf,Command::fireEngine,0,battle.mayFireEngine());
        } else if (battle.commander>=0) {
            const char* status=!battle.generalHealth?"武将退场":
                battle.atFront?"阵前损体力":"点路牌分兵";
            boxText(f,status,{212,214,93,17},12,battle.generalHealth?dim:coral,1);
        }
        for (int i=0;i<3;++i)
            button(f,{8+56*i,239,54,21},formations[i],Command::shift,i,
                   !battle.result&&(i==battle.plan.formation||battle.signal>=Battle::commandCost),
                   i==battle.plan.formation,formationIcons[i]);
        button(f,{176,239,58,21},weaponAttacks[battle.plan.weapon],Command::strike,0,
               battle.mayStrike(),false,weaponIcons[battle.plan.weapon]);
        button(f,{236,239,69,21},battle.usedSkill?"将技已用":"将技",Command::skill,0,
               battle.mayRally(),true,skillIcon);
        button(f,{307,239,69,21},"号令",Command::orders,0,!battle.result,false,ordersIcon);
        button(f,{320,210,56,25},"暂停",Command::pause,0,true,false,pauseIcon);
        drawFieldMap(f,!battle.paused && !battle.result);
        drawRanks(f);
        if (battle.paused) drawPause(f);
        else generalControls(f);
    }
    void drawReport(pxa::game::Frame& f) {
        fillBackground(f,reportSiege?art::siege:art::battlefields[sites[reportCity].terrain]);
        skinFrame(f,{39,27,306,207});
        const char* title=reportRetreat?(reportDefense?"弃城撤退":"收兵回城"):
                          reportWinner>0?(reportDefense?"守城告捷":"夺城告捷"):
                          reportDefense?"城池失守":"攻城受挫";
        contain(f,art::outcomes[reportWinner>0?0:1],{116,42,152,54},true);
        char heading[56];
        std::snprintf(heading,sizeof heading,"%s · %s",cityName(reportCity),title);
        boxText(f,heading,{59,101,266,23},14,reportWinner>0?gold:coral,1);
        block(f,{67,123,250,1},edge);
        text(f,"我军伤亡",78,139,15);
        number(f,reportLoss,308,138,17,ivory,2);
        const char* extra=reportWinner<=0?(reportLoss==battle.initial[0]?"全军覆没":"余部已撤离前线"):
                           reportDefense?"守军留驻城内":"入城缴获 115金 95粮";
        if (aftermath.enemyReturned) {
            char withdrawal[64];
            std::snprintf(withdrawal,sizeof withdrawal,"敌军 %d 人撤往%s",
                          aftermath.enemyReturned,cityName(battle.enemyRetreatCity));
            boxText(f,withdrawal,{60,155,264,23},14,dim,1);
        } else text(f,extra,192,159,14,dim,1);
        char merit[56];
        if (aftermath.objective) std::snprintf(merit,sizeof merit,"剧本悬赏已达成");
        else if (aftermath.level) std::snprintf(merit,sizeof merit,"领军提升至等级 %d",aftermath.level);
        else std::snprintf(merit,sizeof merit,"领军历练 +%d",aftermath.xp);
        if (aftermath.xp || aftermath.objective) text(f,merit,192,180,13,mint,1);
        button(f,{121,197,142,25},aftermath.captured?"处置俘将":"返回舆图",Command::accept,0,true,true);
    }
    void drawHelp(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"行军用策","七十二季内一统州郡");
        constexpr const char* titles[]={"治城","进军","临阵","军令"};
        constexpr const char* a[]={"征募补兵，屯田加粮，扩地增令。",
            "点邻城可调兵，围城次季抵达。",
            "枪克骑，骑克弓，弓克枪。",
            "点我方战场调将，兵器出招耗12令。"};
        constexpr const char* b[]={"城池内政可任太守、安民、搜寻。",
            "再点己城切换出发城。红线是敌袭。",
            "战前选兵种、阵法和武将兵器。",
            "号令控制攻守，单挑影响军心。"};
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
        button(f,{312,41,62,25},"音频",Command::audioMenu);
        skinFrame(f,{10,70,178,148});
        skinFrame(f,{196,70,178,148});
        text(f,"筹粮练兵",20,79,16,gold);
        text(f,"使节议和",206,79,16,gold);
        text(f,"花金120，购粮330",20,98,13,dim);
        button(f,{19,118,160,27},"通商购粮",Command::market,0,campaign.actions&&campaign.gold>=120);
        text(f,"边城各补105兵",20,151,12,dim);
        text(f,"金150  粮95",20,168,12,dim);
        button(f,{19,183,160,26},"巡边募兵",Command::militia,0,
               campaign.actions&&campaign.gold>=150&&campaign.grain>=95);
        text(f,"金180 · 停战三季",206,98,13,dim);
        int n=0;
        for (int side=0;side<3;++side) {
            if (side==campaign.player) continue;
            if (campaign.ceasefire(side)) std::snprintf(state,sizeof state,"%s 休战%d季",
                factions[side].title,campaign.truce[side]);
            else std::snprintf(state,sizeof state,"与%s议和",factions[side].title);
            button(f,{205,118+40*n++,160,28},state,Command::treat,side,campaign.mayTreat(side));
        }
        button(f,{10,230,75,26},"返回",Command::back);
        button(f,{91,230,75,26},"军情",Command::forces);
        button(f,{172,230,95,26},"州郡军略",Command::regions);
        button(f,{273,230,101,26},"武将名册",Command::roster,0,true,true);
    }
#include "expansion_ui.inc"

    void draw() {
        if (!ready || !context->foreground() || loadingArt) return;
        auto wanted=pagesNeeded();
        for (unsigned i=0;i<wanted.size();++i) {
            if (wanted[i]==(slots[i]>=0) && (!sansText || slots[i]<fontSlots())) continue;
            loadingArt=true;
            auto task=context->tasks().start(syncArt(wanted));
            if (!task) {loadingArt=false;on_error(*context,task.error());}
            return;
        }
        count=0;
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
            case Page::seasonCouncil: drawSeasonCouncil(f);break;
            case Page::regions: drawRegions(f);break;
            case Page::dispatch: drawDispatch(f);break;
            case Page::forces: drawForces(f);break;
            case Page::gear: drawGear(f);break;
            case Page::captive: drawCaptive(f);break;
        }
        auto r=sansText?sans.upload(*renderer,fontSlots()):pxa::Result<void>{};
        if (r) r=f.submit();
        dirty=!r;
        if (!r && r.error()!=pxa::Error::would_block && drawErrors++<3) on_error(*context,r.error());
    }

    void show(Page nextPage,int first=0) {
        if (sansText && page!=nextPage) sans.reset();
        if (page==Page::deploy && nextPage!=Page::battle && nextPage!=Page::deploy)
            audio.releaseOpening(battle.defending);
        page=nextPage;
        lookaheadDone=false;
        focus=first;
        count=0;
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
    void beginMarch(Order order) {
        playerMarch=order;
        marchingMs=0;
        previousTick=0;
        focus=0;
        if (!mapZoom) mapZoom=1;
        auto [x,y]=marchPosition(order,.5f);
        mapCenterX=x;
        mapCenterY=y;
        clampCamera();
        logState("march");
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
        const Order order=playerMarch;
        const bool season=seasonMarch;
        playerMarch={};
        marching={};
        seasonMarch=false;
        if (season) {
            campaign.advance(seasonPlan);
            seasonPlan=SeasonPlan::regular;
            notice=Notice::income;
            checkpoint();
            enterMap();
            logState("season");
        } else if (order && arrivingColumn>=0 && campaign.columns[arrivingColumn]) {
            if (campaign.own(order.to)) {
                campaign.arriveFriendly(arrivingColumn);
                selected=order.to;
                notice=Notice::moved;
                checkpoint();
                enterMap();
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
        deploymentLane=1;
        deploymentKit=false;
        show(Page::deploy,3);
    }
    void enterMap() {
        if (campaign.won) {show(Page::ending);return;}
        if (campaign.prisoner) {show(Page::captive);return;}
        if (campaign.defense) {
            selected=campaign.defense.to;
            deploy(campaign.defense,true);
            logState("defend");
            return;
        }
        arrivingColumn=campaign.readyColumn();
        if (arrivingColumn>=0) {
            const Order order=campaign.columns[arrivingColumn].road;
            selected=order.from;
            destination=order.to;
            show(Page::map);
            beginMarch(order);
            return;
        }
        if (!campaign.own(selected)) {
            for (int i=0;i<cityCount;++i) if (campaign.own(i)) {selected=i;break;}
        }
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
        reportLoss=battle.initial[0]-battle.troops[0]+aftermath.dispersed;
        if (!battle.defending && battle.result>0) selected=battle.to;
        notice=aftermath.objective?Notice::goal:Notice::none;
        checkpoint();
        show(Page::report);
        audio.cue(battle.result>0?victorySound:defeatSound);
        audio.cue(battle.result>0?victoryVoice:defeatVoice);
        logState(aftermath.enemyReturned?"enemy-withdrew":battle.result>0?"victory":"defeat");
    }
    void openOrders(int rank) {
        tacticsRank=rank;
        tacticsOpen=true;
        lookaheadDone=false;
        focus=rank+1;
        previousTick=0;
    }
    void closeOrders() {
        tacticsOpen=false;
        focus=battle.plan.formation;
        previousTick=0;
    }
    void pauseBattle() {
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
        switch (page) {
            case Page::home:return pxa::BackAction::close;
            case Page::choose:
            case Page::ending:show(Page::home);break;
            case Page::replace:show(Page::choose,side);break;
            case Page::map:
                if (isMarching()) finishMarch();
                else if (mapWindow.w==wideMap.w) {mapWindow=standardMap;focus=-1;dirty=true;clampCamera();}
                else if (destination>=0) {destination=-1;focus=-1;dirty=true;}
                else show(Page::home);
                break;
            case Page::council:
            case Page::dispatch:
            case Page::gazetteer:
            case Page::seasonCouncil:
            case Page::cityDesk:show(Page::map,-1);break;
            case Page::regions:show(Page::council);break;
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
                else if (tacticsOpen) {closeOrders();dirty=true;}
                else pauseBattle();
                break;
            case Page::report:enterMap();break;
            case Page::captive:show(Page::home);break;
            case Page::help:show(helpFrom,helpFrom==Page::map?-1:0);break;
        }
        return pxa::BackAction::stay;
    }
    void input(Control control) {
        if (!control.enabled) return;
        const int arg=control.data;
        dirty=true;
        switch (control.command) {
            case Command::resume:
                selected=campaign.capital();
                notice=Notice::saved;
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
                mapWindow=standardMap;mapZoom=0;showMapNames=false;focusCamera(selected);
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
                if (arg==destination && campaign.own(arg)) selected=arg;
                destination=target;
                notice=Notice::none;
                break;
            }
            case Command::origin:
                if (campaign.own(arg)) {
                    selected=arg;
                    destination=-1;
                    notice=Notice::none;
                    focus=-1;
                    audio.cue(confirmSound);
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
            case Command::cityDesk:show(Page::cityDesk);break;
            case Command::gazetteer:gazetteerCity=arg;show(Page::gazetteer);break;
            case Command::browseCity:gazetteerCity=(gazetteerCity+arg+cityCount)%cityCount;break;
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
                if (campaign.welcome(selected,arg)) {
                    checkpoint();
                    audio.cue(confirmSound);
                    logState("talent-recruited");
                }
                break;
            case Command::march:
                if (campaign.canMarch(selected,arg)) {
                    departure={selected,arg};
                    dispatchLead=campaign.routeLeader(selected,arg);
                    dispatchSize=legion;
                    if (!campaign.canSend(selected,arg,dispatchSize,dispatchLead)) dispatchSize=division;
                    if (!campaign.canSend(selected,arg,dispatchSize,dispatchLead)) dispatchSize=vanguard;
                    stageSiege=false;
                    show(Page::dispatch,1+dispatchSize);
                }
                break;
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
                mapWindow=standardMap;
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
                mapWindow=mapWindow.w==wideMap.w?standardMap:wideMap;
                if (mapWindow.w==wideMap.w && !mapZoom) mapZoom=1;
                focusCamera(destination<0?selected:destination);
                focus=-1;
                break;
            case Command::council:show(Page::council);break;
            case Command::forces:show(Page::forces);break;
            case Command::regions:show(Page::regions,regionalChoice);break;
            case Command::chooseRegion:regionalChoice=arg;break;
            case Command::locateRegionCity:
                destination=arg==selected?-1:arg;
                mapWindow=standardMap;
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
                if (campaign.hire(arg)) {checkpoint();logState("hire");}
                break;
            case Command::chooseOfficer:
                if (eligibleOfficer(arg)) {
                    if (rosterFrom==Page::dispatch) dispatchLead=arg;
                    else if (rosterFrom==Page::deploy) defenseLeader=arg;
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
                gearCursor=campaign.generals[officerSlot].gear==noGear?0:campaign.generals[officerSlot].gear;
                show(Page::gear,gearCursor);
                break;
            case Command::pickGear:gearCursor=arg;break;
            case Command::equip:
                if (campaign.equip(officerSlot,Gear(arg))) {checkpoint();logState("equip");}
                break;
            case Command::drill:
                if (campaign.drill(arg)) {checkpoint();logState("drill");}
                break;
            case Command::treat:
                if (campaign.treat(arg)) {notice=Notice::treaty;checkpoint();logState("treaty");}
                break;
            case Command::market:
                if (campaign.market()) {notice=Notice::supplies;checkpoint();logState("market");}
                break;
            case Command::militia:
                if (campaign.militia()) {notice=Notice::levies;checkpoint();logState("levy");}
                break;
            case Command::engage:
                battle.start(campaign,departure,arrivingColumn,deployment,defenseLeader);
                followBattle=true;
                battleCamera=float(battleCameraTarget());
                elapsed=0;
                tacticsOpen=false;
                retreatPrompt=false;
                show(Page::battle,deployment.formation);
                audio.cue(drumSound);
                audio.cue(battle.defending?holdVoice:chargeVoice);
                logState("battle");
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
                    audio.cue(power==charge?breakVoice:power==guard || power==inspire?rallyVoice:skillVoice);
                    audio.cue(ranged(battle.troopFor(0,battle.generalLane))?arrowSound:clashSound);
                    info("han-banners skill");
                }
                break;
            case Command::orders:
                if (tacticsOpen) closeOrders();
                else openOrders(-1);
                break;
            case Command::battleRank:
                openOrders(arg);
                break;
            case Command::battleView: {
                followBattle=arg<0;
                if (followBattle) battleCamera=float(battleCameraTarget());
                else {
                    const int center=arg==1?(fieldArmyCenter(0)+fieldArmyCenter(1))/2:fieldArmyCenter(arg/2);
                    battleCamera=float(std::clamp(center-sceneWidth/2,0,fieldPanLimit));
                }
                logState(followBattle?"battle-follow":"battle-view");
                break;
            }
            case Command::rankSupport:
                if (battle.supportRank(tacticsRank)) {
                    closeOrders();
                    audio.cue(confirmSound);
                    logState("rank-support");
                }
                break;
            case Command::clearRank:
                if (battle.clearRankOrder(tacticsRank)) closeOrders();
                break;
            case Command::maneuver:
                if (tacticsRank<0?battle.direct(Maneuver(arg)):battle.directRank(tacticsRank,Maneuver(arg))) {
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
            case Command::captiveChoice:
                if (arg==2 && !campaign.admit()) break;
                if (arg!=2) campaign.disposeCaptive(arg==1);
                checkpoint();
                logState(arg==2?"guest":"captive");
                enterMap();
                break;
            case Command::home:show(Page::home);break;
        }
    }
    void moveFocus(int dx,int dy) {
        if (!count) return;
        focus=std::clamp(focus,0,count-1);
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
        focus=picked;
    }
    void on_controller(pxa::Context&,const pxa::input::ControllerState& s) {
        if (!gamepad.update(s) || !ready) return;
        using namespace pxa::input;
        if (!gamepad.buttons()) return;
        controllerMode=true;
        dirty=true;
        if (gamepad.pressed(controller_b)) {
            if (page==Page::home) (void)context->window().request_back();
            else (void)on_back();
        } else if (gamepad.pressed(controller_up)) moveFocus(0,-1);
        else if (gamepad.pressed(controller_down)) moveFocus(0,1);
        else if (gamepad.pressed(controller_left)) moveFocus(-1,0);
        else if (gamepad.pressed(controller_right)) moveFocus(1,0);
        else if ((gamepad.pressed(controller_a)||gamepad.pressed(controller_start))&&focus>=0&&focus<count)
            input(controls[focus]);
        draw();
    }
    void resetPointer() {
        pointerDown=false;
        mapDragging=false;
        battleDragging=false;
        pressedControl.reset();
        pinch.reset();
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
            dragMoved=true;
        }
        return true;
    }
    void on_pointer(pxa::Context&,const pxa::input::Pointer& p) {
        if (!ready) return;
        using P=pxa::input::PointerPhase;
        const float x=(float(pxa::ui::canvas_to_surface_coordinate(p.x,display))*width/display.width-ox)/scale;
        const float y=(float(pxa::ui::canvas_to_surface_coordinate(p.y,display))*height/display.height-oy)/scale;
        if (handlePinch(p,x,y)) return;
        if (p.action()==P::move) {
            if (!pointerDown) return;
            pointerX=x;pointerY=y;
            if (!dragMoved && std::abs(x-pressX)+std::abs(y-pressY)>6.f) {
                dragMoved=true;
                mapTapTime=0;
                dirty|=pressedControl.has_value();
            }
            if (dragMoved && mapDragging && mapZoom>0) panMap(x,y);
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
            controllerMode=false;
            pointerDown=true;
            if (hit>=0) {
                pressedControl=controls[hit];
                mapTapTime=0;
            }
            pointerId=p.pointer_id;
            mapDragging=page==Page::map && !isMarching() && mapWindow.contains(int(x),int(y));
            battleDragging=page==Page::battle && !battle.paused && !battle.duel.visible() &&
                !battle.result && !tacticsOpen && Rect{69,92,246,117}.contains(int(x),int(y));
            pressX=dragX=pointerX=x;pressY=dragY=pointerY=y;dragMoved=false;
        } else if (pointerDown && p.ended()) {
            if (p.action()==P::up && mapDragging && dragMoved && mapZoom>0) panMap(x,y);
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
                    input(controls[i]);
                    draw();
                    break;
                }
            }
        }
    }
    void on_background(pxa::Context&) {
        audio.pause(true);
        gamepad.reset();
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
            if (ready) layout();
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
        if (page==Page::deploy && count) audio.warmOpening(battle.defending);
        warmNextScene();
        if (page==Page::map && isMarching()) {
            auto now=tick->timestamp_us;
            if (previousTick && now>previousTick)
                marchingMs=std::min(marchDuration,marchingMs+int(std::min<uint64_t>((now-previousTick)/1000,100)));
            previousTick=now;
            if (marchingMs>=marchDuration) finishMarch();
            dirty=true;
        }
        if (page==Page::battle && !battle.paused && !tacticsOpen) {
            auto now=tick->timestamp_us;
            if (previousTick && now>previousTick) elapsed+=unsigned(std::min<uint64_t>(now-previousTick,100000));
            previousTick=now;
            const int round=battle.round;
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
                if (!round) logState("battle-contact");
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
