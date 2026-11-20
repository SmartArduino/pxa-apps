#include <pxa/app.hpp>
#include <pxa/events.hpp>
#include <pxa/game_upload.hpp>
#include <pxa/ui_display.hpp>
#include <cmath>
#include <cstdio>
#include <limits>
#include "battle.hpp"
#include "save.hpp"
#include "art.hpp"
#include "sans_text.hpp"

namespace han {

constexpr int sceneWidth = 384, sceneHeight = 264;
constexpr uint16_t rgb(int r, int g, int b) {
    return uint16_t(((r & 248) << 8) | ((g & 252) << 3) | (b >> 3));
}
constexpr uint16_t ink = rgb(13,20,22), panel = rgb(25,36,35), edge = rgb(104,91,64);
constexpr uint16_t ivory = rgb(241,226,188), dim = rgb(175,170,146), gold = rgb(244,196,111);
constexpr uint16_t mint = rgb(128,222,180), coral = rgb(251,142,101);
constexpr uint16_t colors[] = {mint, rgb(151,180,243), coral, rgb(221,203,153),
    rgb(205,181,241), rgb(243,196,137), rgb(240,228,168), rgb(163,218,218)};

struct Rect {
    int x,y,w,h;
    int cx() const {return x + w/2;}
    int cy() const {return y + h/2;}
    bool contains(int u,int v) const {return u>=x && v>=y && u<x+w && v<y+h;}
    bool overlaps(Rect r) const {return x<r.x+r.w && r.x<x+w && y<r.y+r.h && r.y<y+h;}
};
constexpr Rect standardMap{8,42,264,177};
constexpr Rect wideMap{8,42,368,177};

enum class Page {home, choose, map, deploy, battle, report, help, ending, replace, council, roster,
                 cityDesk, dispatch, forces, gear, captive};
enum class Command {resume, newGame, guide, back, side, seat, work, march, season, select,
                    engage, shift, skill, pause, retreat, accept, home, replace, zoom, council,
                    roster, officer, hire, chooseOfficer, treat, market, militia, skipMarch, atlas,
                    chapter, cityDesk, survey, forces, chooseSize, approach, send, recall,
                    rosterPage, gear, pickGear, equip, drill, orders, maneuver, duel, duelMove,
                    captiveChoice};
struct Control {
    Rect box;
    Command command;
    int data;
    bool enabled;
};
enum UiIcon {coinsIcon,grainIcon,orderIcon,recruitIcon,farmIcon,wallsIcon,marketIcon,
             pacifyIcon,surveyIcon,spearIcon,bowIcon,horseIcon};
enum MapProp {cityGate,compassRose,armyTent,mapScroll};
inline constexpr const char* seasons[]={"春","夏","秋","冬"};
enum class Notice {none, recruited, cultivated, fortified, moved, income, saved, saveError, treaty, supplies, levies,
                    civic, encamped, withdrawn, goal};

struct App {
    pxa::Context* context = nullptr;
    pxa::ui::DisplayMetrics display{};
    std::optional<pxa::game::Renderer> renderer;
    std::array<int,art::pageCount> slots{};
    bool loadingArt=false;
    Rect mapWindow=standardMap;
    std::array<Rect,cityCount> mapLabels{};
    pxa::game::DrawBuffer<16384> commands;
    std::array<Control,48> controls{};
    int count = 0, focus = 0, selected = 2, destination = -1, side = 0, formation = spear;
    int width = 0, height = 0;
    float scale = 1, ox = 0, oy = 0;
    bool sansText = false, textAlpha = false;
    SansText sans;
    Page page = Page::home, helpFrom = Page::home;
    Notice notice = Notice::none;
    Campaign campaign;
    Battle battle;
    Order departure;
    int chosenChapter=0, dispatchLead=-1, defenseLeader=-1, arrivingColumn=-1, gearCursor=0;
    bool stageSiege=false, tacticsOpen=false;
    DispatchSize dispatchSize=legion;
    int surveyCity=-1;
    Discovery discovery{};
    Settlement aftermath{};
    float mapCenterX=worldWidth/2.f, mapCenterY=worldHeight/2.f;
    int mapZoom=0, officerSlot=0;
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
    int pointerIndex = -1, pointerId = -1;
    int reportWinner = 0, reportCity = 0, reportLoss = 0;
    bool reportDefense = false, reportRetreat = false, reportSiege = false;
    uint64_t previousTick = 0;
    unsigned elapsed = 0, drawErrors = 0;
    pxa::input::Controller gamepad;

    void info(std::string_view message) { (void)context->log().write(pxa::LogLevel::info,message); }
    void logState(const char* action) {
        char body[160];
        std::snprintf(body,sizeof body,"han-banners %s page=%d turn=%d side=%d cities=%d actions=%d chapter=%d",
                      action,int(page),campaign.turn,campaign.player,campaign.holdings(campaign.player),
                      campaign.actions,campaign.chapter);
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
        auto r = co_await context->storage().get(saveKey,data);
        hasSave = r && *r == data.size() && decode(data,campaign);
        damagedSave = !hasSave && (r || r.error() != pxa::Error::not_found);
        if (!r && r.error()==pxa::Error::not_found) {
            SecondSave previous{};
            const auto old=co_await context->storage().get("campaign-v2",previous);
            hasSave=old && *old==previous.size() && migrate(previous,campaign);
            damagedSave=!hasSave && (old || old.error()!=pxa::Error::not_found);
            if (!old && old.error()==pxa::Error::not_found) {
                LegacySave legacy{};
                const auto ancient=co_await context->storage().get("campaign-v1",legacy);
                hasSave=ancient && *ancient==legacy.size() && migrate(legacy,campaign);
                damagedSave=!hasSave && (ancient || ancient.error()!=pxa::Error::not_found);
            }
        }
        co_return co_await openRenderer();
    }
    void layout() {
        const float fx = float(width)/display.width;
        const float fy = float(height)/display.height;
        float left = fx * std::max(display.safe.left,display.system_bars.left);
        float right = fx * std::max(display.safe.right,display.system_bars.right);
        float top = fy * std::max(display.safe.top,display.system_bars.top);
        float bottom = fy * std::max(display.safe.bottom,display.system_bars.bottom);
        float cw = std::max(1.f,width-left-right), ch = std::max(1.f,height-top-bottom);
        scale = std::min(cw/sceneWidth,ch/sceneHeight);
        const bool useSans=scale/fx>=1.45f && fontSlots()>=10;
        if (sansText!=useSans || sansText) sans.reset();
        sansText=useSans && sans.init();
        ox = std::round(left+(cw-sceneWidth*scale)/2);
        oy = std::round(top+(ch-sceneHeight*scale)/2);
    }
    pxa::Task<void> openRenderer() {
        ready = false;
        resize = false;
        renderer.reset();
        pxa::game::RenderOptions opt;
        const bool large=std::max(display.width,display.height)>480;
        // Keep text at panel resolution on common larger PXA displays.
        const int divisor=unsigned(display.width)*display.height>800u*600u?2:1;
        width=std::max(1,int(display.width)/divisor);
        height=std::max(1,int(display.height)/divisor);
        opt.width = width;
        opt.height = height;
        opt.buffers = 2;
        opt.direct_scanout = true;
        opt.max_draw_bytes = commands.capacity();
        auto r = co_await context->game().create(opt);
        if (r) {
            renderer.emplace(std::move(*r));
        } else if (large && divisor==1 && r.error()==pxa::Error::resource_limit) {
            width=opt.width=std::max(1,int(display.width)/2);
            height=opt.height=std::max(1,int(display.height)/2);
            auto smaller=co_await context->game().create(opt);
            if (!smaller) co_return std::unexpected(smaller.error());
            renderer.emplace(std::move(*smaller));
        } else {
            co_return std::unexpected(r.error());
        }
        textAlpha=renderer->supports(pxa::game::RenderCapability::sprite_texel_alpha);
        auto palette = pxa::game::Upload(*renderer,commands.bytes()).palette(art::palette);
        if (!palette) co_return palette;
        slots.fill(-1);
        layout();
        ready = true;
        dirty = true;
        (void)context->clock().set_period(33);
        logState("ready");
        draw();
        co_return pxa::Result<void>{};
    }
    art::Tile titleBackground() const {
        return width*2>=height*3 ? art::coverWide : art::coverSquare;
    }

    int fontSlots() const {
        return int(std::min<unsigned>(16,renderer->info().max_textures))-SansText::banks;
    }

    using Pages=std::array<bool,art::pageCount>;
    bool hasColumns() const {
        for (const auto& unit:campaign.columns) if (unit) return true;
        return false;
    }
    art::Tile battlefield() const {
        return battle.walls>=3?art::siege:art::battlefields[sites[battle.to].terrain];
    }
    Pages pagesNeeded() const {
        Pages set{};
        if (!sansText)
            for (int i=art::firstFontPage;i<art::pageCount;++i) set[i]=true;
        auto require=[&](art::Tile image){set[image.page]=true;};
        require(art::plaques[0]);
        auto room=[&](art::Tile background=art::warHall) {
            require(background);
            require(art::skin);
        };
        auto actor=[&](int slot) {
            if (slot>=0) require(art::portraits[campaign.leader(slot).portrait]);
        };
        switch (page) {
            case Page::home:require(titleBackground());require(art::logo);break;
            case Page::replace:require(titleBackground());break;
            case Page::ending:
                require(titleBackground());require(art::skin);
                require(art::outcomes[campaign.won>0?0:1]);break;
            case Page::choose:room();require(art::portraits[0]);break;
            case Page::roster:
                room();
                for (int i=rosterStart();i<std::min(rosterCount(),rosterStart()+rosterSize);++i) actor(i);
                break;
            case Page::map:
                room();
                for (auto t:art::mapTiles) require(t);
                if (isMarching() || hasColumns()) require(art::marchers[0][0][0]);
                break;
            case Page::dispatch:room();actor(dispatchLead);break;
            case Page::cityDesk:room(art::cityCourtyard);actor(campaign.governor(selected));break;
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
                require(art::skin);require(art::hudBars[0]);
                if (!sansText || !battle.duel.visible()) {
                    require(battlefield());
                    require(art::troops[0][0][0]);require(art::troops[1][0][0]);
                    require(art::tacticTroops[0][0][0]);require(art::tacticTroops[1][0][0]);
                }
                if (!sansText || battle.duel.visible()) {
                    require(art::siege);
                    require(art::duelFrames[0][0]);require(art::duelFrames[1][0]);
                }
                break;
            case Page::report:
                require(art::skin);require(art::outcomes[reportWinner>0?0:1]);
                require(reportSiege?art::siege:art::battlefields[sites[reportCity].terrain]);break;
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
            if (empty==end) {status=std::unexpected(pxa::Error::resource_limit);break;}
            char name[48];
            std::snprintf(name,sizeof name,"assets/atlas-%u.pxr",i);
            auto asset=co_await context->assets().load(pxa::AssetKind::texture,name);
            if (!asset) {status=std::unexpected(asset.error());info(name);break;}
            int slot=empty-occupied.begin();
            status=renderer->bind_asset(*asset,{uint8_t(slot)});
            if (status) {slots[i]=slot;*empty=true;}
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
            auto written = co_await context->storage().set(saveKey,data);
            if (!written) {
                notice = Notice::saveError;
                dirty = true;
                info("han-banners save failed");
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
        float length = std::max(1.f,std::hypot(float(e-l),float(b-t)));
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
    void coverPixels(pxa::game::Frame& f,art::Tile image,Rect dest,float anchor=.5f) const {
        if (dest.w<=0 || dest.h<=0) return;
        const float aspect=float(dest.w)/dest.h;
        const int w=std::clamp(int(std::round(image.h*aspect)),1,int(image.w));
        const int h=std::clamp(int(std::round(image.w/aspect)),1,int(image.h));
        image.x+=int(std::round((image.w-w)*anchor));
        image.y+=(image.h-h)/2;
        image.w=w;
        image.h=h;
        tilePixels(f,image,dest);
    }
    void fillBackground(pxa::game::Frame& f,art::Tile image,float anchor=.5f) const {
        coverPixels(f,image,{0,0,width,height},anchor);
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
    void skinFrame(pxa::game::Frame& f,Rect area,uint16_t fill=ink) const {
        const auto p=toPixels(area);
        if (p.w<=0 || p.h<=0) return;
        blockPixels(f,p,fill);
        const int border=std::max(2,int(13*scale+.5f));
        nineSlice(f,art::skin,p,{20,20,border,border},false);
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
    void healthBar(pxa::game::Frame& f,Rect area,int value,int maximum,bool enemy=false) const {
        Rect p=toPixels(area);
        drawStrip(f,art::hudBars[0],p);
        const int inset=p.h>=6?2:1;
        const int room=p.w-2*inset;
        if (value<=0 || room<=0 || p.h<=2) return;
        const int amount=std::clamp(room*value/std::max(1,maximum),1,room);
        const int x=enemy?p.x+p.w-inset-amount:p.x+inset;
        drawStrip(f,art::hudBars[enemy?2:1],{x,p.y+1,amount,p.h-2});
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
    int textWidth(std::string_view s,int size) const {
        const int height=fontHeight(size);
        int n=0;
        while (!s.empty()) n+=glyphAdvance(next(s),height);
        return n;
    }
    struct TextStamp {
        art::Tile tile{};
        Rect rect{};
        uint8_t slot=0;
    };

    TextStamp stamp(uint32_t code,int height,int x,int y) {
        if (sansText) {
            const auto* letter=sans.glyph(code,height,textAlpha);
            if (!letter) return {};
            const auto w=uint16_t(letter->width),h=uint16_t(letter->height);
            return {{0,uint16_t(letter->x),uint16_t(letter->y),w,h},
                    {x+letter->left,y+(17*height+10)/20+letter->top,w,h},
                    uint8_t(fontSlots()+letter->bank)};
        }
        const auto tile=glyph(code).tile;
        return {tile,{x,y,tile.w*(height/art::fontPixels),height},uint8_t(slots[tile.page])};
    }

    void text(pxa::game::Frame& f,std::string_view s,int x,int y,int size=14,
              uint16_t color=ivory,int align=0,uint16_t outline=0) {
        const int h=fontHeight(size);
        int u=sx(x);
        const int v=(sy(y)+sy(y+size)-h)/2;
        if (align) u-=textWidth(s,size)/(align==1?2:1);
        const bool alpha=sansText && textAlpha;
        if (outline) {
            int left=u;
            const int stroke=std::max(1,h/12);
            auto line=s;
            while (!line.empty()) {
                const auto code=next(line);
                if (code!=' ') {
                    const auto [tile,r,slot]=stamp(code,h,left,v);
                    if (tile.w) {
                        const std::array edges={
                            slice(tile,{r.x-stroke,r.y,r.w,r.h},0,0,tile.w,tile.h),
                            slice(tile,{r.x+stroke,r.y,r.w,r.h},0,0,tile.w,tile.h),
                            slice(tile,{r.x,r.y-stroke,r.w,r.h},0,0,tile.w,tile.h),
                            slice(tile,{r.x,r.y+stroke,r.w,r.h},0,0,tile.w,tile.h),
                        };
                        f.sprites({slot},edges,{.solid_color=true,.texel_alpha=alpha,.color={outline}});
                    }
                }
                left+=glyphAdvance(code,h);
            }
        }
        while (!s.empty()) {
            const auto code=next(s);
            if (code!=' ') {
                const auto [tile,rect,slot]=stamp(code,h,u,v);
                if (tile.w) {
                    const auto sprite=slice(tile,rect,0,0,tile.w,tile.h);
                    f.sprites({slot},{&sprite,1},{.solid_color=true,.texel_alpha=alpha,.color={color}});
                }
            }
            u+=glyphAdvance(code,h);
        }
    }
    void number(pxa::game::Frame& f,int n,int x,int y,int size,uint16_t color=ivory,int align=0) {
        char str[24];
        std::snprintf(str,sizeof str,"%d",n);
        text(f,str,x,y,size,color,align);
    }
    bool registerControl(Rect box,Command action,int data,bool enabled=true) {
        int index=count++;
        if (focus<0 && (action!=Command::select || data==selected)) focus=index;
        controls[index]={box,action,data,enabled};
        return (controllerMode && focus==index) ||
               (pointerDown && !dragMoved && pointerIndex==index);
    }
    void button(pxa::game::Frame& f,Rect r,const char* label,Command action,int data=0,
                bool enabled=true,bool main=false,int icon=-1) {
        const bool focused=registerControl(r,action,data,enabled);
        plaque(f,r,enabled,main||focused);
        if (focused) block(f,{r.x+4,r.y+5,2,r.h-10},gold);
        if (icon>=0 && sx(r.w)-sx(0)>=textWidth(label,14)+sx(20)-sx(0)+5) {
            contain(f,art::commandIcons[icon],{r.x+5,r.cy()-8,16,16},true);
            text(f,label,r.cx()+8,r.cy()-7,14,enabled?ivory:dim,1);
        } else text(f,label,r.cx(),r.cy()-7,14,enabled?ivory:dim,1);
    }
    void header(pxa::game::Frame& f,const char* label,const char* sub=nullptr) {
        plaque(f,{7,3,370,31});
        block(f,{16,10,2,16},gold);
        text(f,label,25,8,19,gold);
        if (sub) text(f,sub,365,11,13,ivory,2);
    }
    void resourcePlate(pxa::game::Frame& f,Rect area,UiIcon icon,
                       std::string_view value,uint16_t color=ivory) {
        plaque(f,area);
        contain(f,art::commandIcons[icon],{area.x+5,area.cy()-8,16,16},true);
        text(f,value,area.cx()+7,area.cy()-7,14,color,1);
    }
    void portrait(pxa::game::Frame& f,int actor,Rect r) {
        frame(f,{r.x-1,r.y-1,r.w+2,r.h+2},edge,ink);
        contain(f,art::portraits[actor],r);
    }
    void bar(pxa::game::Frame& f,Rect r,int value,int maximum,uint16_t color) {
        block(f,r,rgb(56,53,43));
        block(f,{r.x,r.y,r.w*value/std::max(1,maximum),r.h},color);
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
            Rect card{10+122*i,71,118,130};
            bool emphasis=registerControl(card,Command::side,i);
            const bool selectedRuler=side==i;
            skinFrame(f,card,selectedRuler||emphasis?rgb(70,50,30):ink);
            portrait(f,i,{card.cx()-38,card.y+5,76,76});
            block(f,{card.x+7,card.y+83,104,2},colors[i]);
            text(f,rulers[i],card.cx(),card.y+88,18,selectedRuler?gold:ivory,1);
            char base[48];
            std::snprintf(base,sizeof base,"%s · %d城",cityLabel(chapter.base[i],chapter.year),holdings[i]);
            text(f,base,card.cx(),card.y+111,12,colors[i],1);
        }
        for (int i=0;i<chapterCount;++i)
            button(f,{10+122*i,41,118,24},chapters[i].name,Command::chapter,i,true,i==chosenChapter);
        plaque(f,{10,204,364,22});
        text(f,chapter.description,192,209,12,ivory,1);
        button(f,{10,231,85,25},"返回",Command::back);
        button(f,{239,231,135,25},"起兵",Command::seat,0,true,true,orderIcon);
    }
    const char* ownerName(int o) const {return factions[o].title;}
    const char* cityName(int i) const {return cityLabel(i,campaign.year());}
    float mapScale() const {
        constexpr float sizes[]={.425f,.74f,1.13f};
        return sizes[mapZoom];
    }
    void clampCamera() {
        float halfW=std::min(worldWidth/2.f,mapWindow.w/(mapScale()*2));
        float halfH=std::min(worldHeight/2.f,mapWindow.h/(mapScale()*2));
        mapCenterX=std::clamp(mapCenterX,halfW,worldWidth-halfW);
        mapCenterY=std::clamp(mapCenterY,halfH,worldHeight-halfH);
    }
    void focusCamera(int city) {
        mapCenterX=sites[city].x();
        mapCenterY=sites[city].y();
        clampCamera();
    }
    float mapLeft() const {return mapWindow.cx()-mapCenterX*mapScale();}
    float mapTop() const {return mapWindow.cy()-mapCenterY*mapScale();}
    int mapX(float x) const {return int(std::round(mapLeft()+x*mapScale()));}
    int mapY(float y) const {return int(std::round(mapTop()+y*mapScale()));}
    int cityX(int index) const {return mapX(sites[index].x());}
    int cityY(int index) const {return mapY(sites[index].y());}
    const Road* findRoad(Order order) const {
        for (const auto& r:roads) if (r.joins(order.from,order.to)) return &r;
        return nullptr;
    }
    void drawSegment(pxa::game::Frame& f,int x,int y,int u,int v,uint16_t color,int stroke) {
        const int dx=u-x, dy=v-y;
        float start=0,end=1;
        auto cut=[&](int p,int q) {
            if (!p) return q>=0;
            const float t=float(q)/p;
            if (p<0) start=std::max(t,start);
            else end=std::min(t,end);
            return start<=end;
        };
        if (!cut(-dx,x-mapWindow.x-1) || !cut(dx,mapWindow.x+mapWindow.w-2-x) ||
            !cut(-dy,y-mapWindow.y-1) || !cut(dy,mapWindow.y+mapWindow.h-2-y)) return;
        int a=x+int(std::round(start*dx)), b=y+int(std::round(start*dy));
        int c=x+int(std::round(end*dx)), d=y+int(std::round(end*dy));
        if (a!=c || b!=d) line(f,a,b,c,d,color,stroke);
    }
    void path(pxa::game::Frame& f,const Road& edgeRoute,uint16_t color,int stroke) {
        const int x=cityX(edgeRoute.from),y=cityY(edgeRoute.from);
        const int u=cityX(edgeRoute.to),v=cityY(edgeRoute.to);
        if (edgeRoute.lon) {
            int a=mapX(worldX(edgeRoute.lon)), b=mapY(worldY(edgeRoute.lat));
            drawSegment(f,x,y,a,b,color,stroke);
            drawSegment(f,a,b,u,v,color,stroke);
        } else drawSegment(f,x,y,u,v,color,stroke);
    }
    void mapArt(pxa::game::Frame& f) {
        frame(f,mapWindow,edge,rgb(25,44,43));
        for (int n=0;n<4;++n) {
            auto base=art::mapTiles[n];
            const int wx=(n%2)*256, wy=(n/2)*208;
            const auto m=mapScale();
            const int l=std::clamp(int(std::ceil((mapWindow.x+1-mapLeft())/m))-wx,0,int(base.w));
            const int r=std::clamp(int(std::floor((mapWindow.x+mapWindow.w-1-mapLeft())/m))-wx,0,int(base.w));
            const int t=std::clamp(int(std::ceil((mapWindow.y+1-mapTop())/m))-wy,0,int(base.h));
            const int b=std::clamp(int(std::floor((mapWindow.y+mapWindow.h-1-mapTop())/m))-wy,0,int(base.h));
            if (l>=r||t>=b) continue;
            int x=mapX(wx+l),y=mapY(wy+t);
            tile(f,{base.page,uint16_t(l),uint16_t(t),uint16_t(r-l),uint16_t(b-t)},
                  {x,y,mapX(wx+r)-x,mapY(wy+b)-y});
        }
    }
    std::pair<float,float> marchPosition(Order order,float t) const {
        const auto &left=sites[order.from], &right=sites[order.to];
        float ax=left.x(), ay=left.y(), bx=right.x(), by=right.y();
        if (auto road=findRoad(order);road && road->lon) {
            float cx=worldX(road->lon), cy=worldY(road->lat);
            float k=std::hypot(cx-ax,cy-ay)/(std::hypot(cx-ax,cy-ay)+std::hypot(bx-cx,by-cy));
            if (t<k) {bx=cx;by=cy;t/=k;}
            else {ax=cx;ay=cy;t=(t-k)/(1-k);}
        }
        return {ax+(bx-ax)*t,ay+(by-ay)*t};
    }
    void armyOnMap(pxa::game::Frame& f,Order order,bool own) {
        if (!order) return;
        if (auto r=findRoad(order)) path(f,*r,own?mint:coral,2);
        const float t=std::clamp(float(marchingMs)/marchDuration,0.f,1.f);
        auto [u,v]=marchPosition(order,t);
        int x=mapX(u),y=mapY(v);
        if (!mapWindow.contains(x-15,y-25)||!mapWindow.contains(x+15,y+9)) return;
        bool flip=sites[order.to].lon<sites[order.from].lon;
        int pose=1+marchingMs/115%2;
        int motion=pose==1?1:0;
        block(f,{x-14,y+1,28,6},ink);
        tile(f,art::marchers[!own][flip][pose],{x-16,y-24-motion,32,28},true);
        uint16_t flag=own?mint:coral;
        block(f,{x-7,y-24-motion,1,11},ivory);
        block(f,{x-6,y-24-motion,10,5},flag);
        bar(f,{x-13,y+5,26,2},marchingMs,marchDuration,flag);
    }
    bool taggedCity(int i) const {
        constexpr int anchors[]={0,2,3,4,5,8,9,10,13,25,26,28,29};
        return std::find(std::begin(anchors),std::end(anchors),i)!=std::end(anchors);
    }
    Rect labelAt(int i,int direction) const {
        const int w=int(std::ceil((textWidth(cityName(i),12)+3)/scale));
        const int h=int(std::ceil((fontHeight(12)+2)/scale));
        const int x=cityX(i),y=cityY(i);
        const int mx=std::clamp(x-w/2,mapWindow.x+1,mapWindow.x+mapWindow.w-w-1);
        const int my=std::clamp(y-h/2,mapWindow.y+1,mapWindow.y+mapWindow.h-h-1);
        switch (direction) {
            case 1: return {mx,y-h-6,w,h};
            case 2: return {x+7,my,w,h};
            case 3: return {x-w-7,my,w,h};
            default:return {mx,y+5,w,h};
        }
    }
    bool insideMap(Rect r) const {
        return mapWindow.contains(r.x,r.y)&&mapWindow.contains(r.x+r.w-1,r.y+r.h-1);
    }
    void placeLabel(int i,bool selectedCity=false) {
        int lowest=std::numeric_limits<int>::max();
        Rect best{};
        for (int d=0;d<4;++d) {
            const auto name=labelAt(i,d);
            if (!insideMap(name)) continue;
            int overlaps=0;
            for (int j=0;j<cityCount;++j) {
                if (j==i || !mapWindow.contains(cityX(j),cityY(j))) continue;
                if (name.overlaps({cityX(j)-5,cityY(j)-5,10,9})) ++overlaps;
                if (mapLabels[j].w && name.overlaps(mapLabels[j])) overlaps+=3;
            }
            if (overlaps<lowest) {lowest=overlaps;best=name;}
            if (lowest==0) break;
        }
        if (selectedCity||lowest==0) mapLabels[i]=best;
    }
    void drawCompass(pxa::game::Frame& f) {
        const int left=mapWindow.x+4,right=mapWindow.x+mapWindow.w-30;
        const int top=mapWindow.y+4,bottom=mapWindow.y+mapWindow.h-42;
        const Rect corners[]={{right,bottom,26,37},{left,bottom,26,37},
                              {right,top,26,37},{left,top,26,37}};
        for (auto r:corners) {
            bool covered=false;
            for (int i=0;i<cityCount && !covered;++i) {
                const Rect marker{cityX(i)-8,cityY(i)-8,17,16};
                covered=r.overlaps(marker) || (mapLabels[i].w && r.overlaps(mapLabels[i]));
            }
            if (covered) continue;
            contain(f,art::mapProps[compassRose],{r.x,r.y,r.w,24},true);
            text(f,"北",r.cx(),r.y+25,12,gold,1,ink);
            return;
        }
    }
    void drawCity(pxa::game::Frame& f,int i) {
        int x=cityX(i), y=cityY(i), owner=campaign.cities[i].owner;
        if (!mapWindow.contains(x-8,y-7) || !mapWindow.contains(x+8,y+7)) return;
        bool active=i==selected || i==destination;
        bool emph=!isMarching() && registerControl({x-9,y-7,19,15},Command::select,i);
        if (active||emph) frame(f,{x-8,y-7,17,15},emph?ivory:gold,ink);
        contain(f,art::mapProps[cityGate],{x-7,y-8,13,14},true);
        block(f,{x-6,y+4,12,3},ink);
        block(f,{x-5,y+5,10,2},colors[owner]);
        if (emph && !mapLabels[i].w) placeLabel(i,true);
        const Rect name=mapLabels[i];
        if (!name.w) return;
        text(f,cityName(i),name.cx(),name.cy()-6,12,emph?ivory:active?gold:ivory,1,ink);
    }
    void drawCityPanel(pxa::game::Frame& f) {
        char buf[64];
        int inspected=destination<0?selected:destination;
        auto city=campaign.cities[inspected];
        skinFrame(f,{279,42,97,177},panel);
        text(f,cityName(inspected),288,48,18,gold);
        text(f,ownerName(city.owner),289,69,13,colors[city.owner]);
        const bool terrainInOwnerRow=textWidth(ownerName(city.owner),13)+
            textWidth(terrains[sites[inspected].terrain],12)+2<=sx(369)-sx(289);
        std::string_view region=sites[inspected].region;
        if (region.ends_with("郡")) region.remove_suffix(std::string_view("郡").size());
        text(f,terrainInOwnerRow?region:terrains[sites[inspected].terrain],369,49,11,dim,2);
        if (terrainInOwnerRow) text(f,terrains[sites[inspected].terrain],369,70,12,dim,2);
        std::snprintf(buf,sizeof buf,"守军 %d",city.troops);
        text(f,buf,289,88,14);
        std::snprintf(buf,sizeof buf,"田%d  城防%d",city.fields,city.fort);
        text(f,buf,289,107,12,dim);
        block(f,{287,125,81,1},edge);

        if (city.owner==campaign.player && destination<0) {
            button(f,{286,131,83,24},"征募 80",Command::work,recruit,campaign.canWork(selected,recruit),true,recruitIcon);
            button(f,{286,158,83,24},"屯田 95",Command::work,farm,campaign.canWork(selected,farm),false,farmIcon);
            button(f,{286,185,83,24},"城池内政",Command::cityDesk,selected,true,false,wallsIcon);
        } else {
            bool local=adjacent(selected,inspected);
            bool transfer=city.owner==campaign.player;
            if (local) {
                std::snprintf(buf,sizeof buf,"可派 %d",campaign.own(selected)?campaign.dispatch(selected):0);
                text(f,buf,327,134,13,dim,1);
                if (!transfer) {
                    std::snprintf(buf,sizeof buf,"相邻可出兵");
                    text(f,buf,327,153,13,dim,1);
                }
            } else text(f,"道路未连通",327,144,13,dim,1);
            if (!transfer && campaign.ceasefire(city.owner))
                text(f,"盟约生效",327,164,12,gold,1);
            button(f,{286,182,83,27},transfer?"调兵":"出征",Command::march,inspected,
                   campaign.canMarch(selected,inspected),true);
        }
    }
    void drawMap(pxa::game::Frame& f) {
        fillBackground(f,art::warHall,.42f);
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

        mapArt(f);
        for (const auto& r:roads) {
            const bool near=selected==r.from || selected==r.to;
            if (near && !isMarching()) path(f,r,ink,2);
            path(f,r,near?gold:rgb(129,121,81),1);
        }
        for (auto o:campaign.orders) {
            if (!o || campaign.cities[o.to].owner!=campaign.player || isMarching()) continue;
            for (int j=1;j<6;++j) {
                auto [a,b]=marchPosition(o,j/6.f);
                const int x=mapX(a),y=mapY(b);
                if (mapWindow.contains(x-2,y-2) && mapWindow.contains(x+2,y+2))
                    block(f,{x-1,y-1,3,3},coral);
            }
        }
        mapLabels={};
        placeLabel(selected,true);
        if (destination>=0) placeLabel(destination,true);
        for (int pass=0;pass<3;++pass) {
            for (int i=0;i<cityCount;++i) {
                if (mapLabels[i].w || !mapWindow.contains(cityX(i),cityY(i))) continue;
                if (pass==0 && !adjacent(selected,i)) continue;
                if (pass==1 && !taggedCity(i)) continue;
                if (pass==2 && !mapZoom) continue;
                placeLabel(i);
            }
        }
        for (int i=0;i<cityCount;++i) if (i!=selected && i!=destination) drawCity(f,i);
        drawCity(f,selected);
        if (destination>=0) drawCity(f,destination);
        drawCompass(f);
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
        if (mapWindow.w==standardMap.w) drawCityPanel(f);

        block(f,{7,221,370,39},ink);
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
        for (auto o:campaign.orders) if (o && campaign.cities[o.to].owner==campaign.player &&
                                        notice==Notice::none) {
            std::snprintf(buf,sizeof buf,"%s下季受袭",cityName(o.to));
            status=buf;
            tone=coral;
        }
        if (!status) {
            std::snprintf(buf,sizeof buf,"领地 %d/%d  下季 +%d金 +%d粮",
                          campaign.holdings(campaign.player),cityCount,campaign.income(false),campaign.income(true));
            status=buf;
        }
        if (isMarching()) {
            count=0;
            if (focus<0 || focus>=1) focus=0;
            if (playerMarch) std::snprintf(buf,sizeof buf,"%s → %s",
                                         cityName(playerMarch.from),cityName(playerMarch.to));
            else std::snprintf(buf,sizeof buf,"诸军正在进发");
            text(f,buf,9,226,13,playerMarch?mint:coral);
            bar(f,{9,247,232,4},marchingMs,marchDuration,playerMarch?mint:coral);
            button(f,{274,226,102,32},"加速抵达",Command::skipMarch,0,true,true);
            return;
        }
        text(f,status,9,224,12,tone);
        button(f,{8,240,43,21},"战法",Command::guide);
        button(f,{54,240,43,21},"封卷",Command::home);
        button(f,{100,240,43,21},"军议",Command::council);
        button(f,{146,240,30,21},"−",Command::zoom,-1,mapZoom>0);
        button(f,{179,240,30,21},"+",Command::zoom,1,mapZoom<2);
        button(f,{212,240,57,21},mapWindow.w==wideMap.w?"城务":"全图",Command::atlas);
        button(f,{274,226,102,32},"推进一季",Command::season,0,true,true);
    }
    void drawDeploy(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        const auto destinationCity=campaign.cities[departure.to];
        const int enemy=campaign.cities[battle.defending?departure.from:departure.to].owner;
        const int lead=plannedLeader();
        header(f,battle.defending?"守城布阵":"出征布阵",cityName(departure.to));
        if (lead>=0) portrait(f,campaign.leader(lead).portrait,{13,48,63,63});
        text(f,lead>=0?campaign.leader(lead).name:"城中守备",84,50,17,gold);
        const bool siege=arrivingColumn>=0 && campaign.columns[arrivingColumn].blockade;
        text(f,siege?"围城已削城防":lead>=0?campaign.leader(lead).skill:"预备兵临阵",84,77,13,dim);
        if (enemy<3) {
            const auto& foe=officers[enemy][campaign.enemyOfficer(enemy,departure.to)];
            portrait(f,foe.portrait,{308,48,63,63});
            text(f,foe.name,298,51,17,colors[enemy],2);
        } else text(f,ownerName(enemy),298,51,17,ivory,2);
        text(f,terrains[sites[departure.to].terrain],298,78,13,dim,2);
        plaque(f,{12,122,360,35});
        text(f,"我军",21,132,14,mint);
        number(f,battle.defending?destinationCity.troops:campaign.columns[arrivingColumn].troops,65,129,19);
        text(f,"敌军",241,132,14,coral);
        number(f,battle.defending?campaign.dispatch(departure.from):destinationCity.troops,365,129,19,ivory,2);
        char desc[48];
        std::snprintf(desc,sizeof desc,"城防 %d",destinationCity.fort);
        text(f,desc,191,132,14,gold,1);
        for (int i=0;i<3;++i) {
            Rect r{12+122*i,170,116,45};
            bool hi=registerControl(r,Command::shift,i);
            plaque(f,r,true,i==formation||hi);
            tile(f,art::troops[0][i][0],{r.x+3,r.y+2,37,37},true);
            text(f,formations[i],r.x+43,r.y+6,15,ivory);
            text(f,formationHints[i],r.x+43,r.y+26,12,dim);
        }
        if (!battle.defending) button(f,{12,230,91,26},"撤军",Command::recall,arrivingColumn);
        else button(f,{12,230,91,26},"换将",Command::roster);
        button(f,{241,228,131,29},"击鼓开战",Command::engage,0,true,true);
    }

    void army(pxa::game::Frame& f,int lane,int who) {
        int current=battle.troops[who];
        if (!current) return;
        int step=battle.ticks-Battle::approach;
        Formation stance=who?battle.intent:battle.stance;
        int n=std::clamp(1+current*4/std::max(1,battle.initial[who]),1,4);
        for (int i=n-1;i>=0;--i) {
            int stagger=(i*7+lane*4+who*3)%12;
            int approach=std::min(96,battle.ticks*96/Battle::approach);
            int animation=step<0?1+(battle.ticks/4+stagger)%2:0;
            if (step>=0) {
                int action=(step+stagger)%Battle::period;
                if (action<7) animation=4;
                else if (action<13) animation=5;
                else if (action>=31) animation=3;
                if (stance==bow && action<8) {
                    int x=who?254-action*8:130+action*8;
                    int y=128+lane*23-action%4*2;
                    line(f,x,y,x+(who?8:-8),y+1,gold,1);
                }
            }
            int x=(battle.defending?45:43)+approach+(stance==bow?-22:stance==horse?8:0)-i*23+lane*5;
            if (!who) {
                if (battle.maneuver==press) x+=14;
                else if (battle.maneuver==hold) x-=12;
                else if (battle.maneuver==flank) x+=lane==1?-12:19;
            } else x=384-x;
            const int y=150+lane*22-i%2*4;
            block(f,{x-11,y-4,22,3},rgb(57,46,31));
            auto sprite=art::troops[who][stance][animation];
            if (step>=0) {
                const int phase=(step+stagger)%Battle::period;
                if (battle.morale[who]<24) sprite=art::tacticTroops[who][stance][phase<18?4:5];
                else if (!who && battle.maneuver==hold) sprite=art::tacticTroops[who][stance][phase<10?1:0];
                else if (!who && battle.maneuver==press) sprite=art::tacticTroops[who][stance][phase<11?3:2];
                else if (battle.round && phase<8 && battle.losses[who]>battle.losses[1-who]*2)
                    sprite=art::tacticTroops[who][stance][1];
            }
            tile(f,sprite,{x-21,y-40,42,42},true);
        }
    }
    void drawPause(pxa::game::Frame& f) {
        count=0;
        skinFrame(f,{72,83,240,96});
        text(f,"会战暂停",192,96,20,gold,1);
        button(f,{89,132,98,30},"继续战斗",Command::pause,0,true,true);
        button(f,{196,132,99,30},battle.defending?"弃城":"撤军",Command::retreat);
    }
    void drawBattle(pxa::game::Frame& f) {
        if (battle.duel.visible()) {
            drawDuel(f);
            if (battle.paused) drawPause(f);
            return;
        }
        fillBackground(f,battlefield());
        block(f,{0,0,384,53},ink);
        portrait(f,battle.officer().portrait,{8,7,36,36});
        if (auto* foe=battle.opponent()) portrait(f,foe->portrait,{340,7,36,36});
        else {
            frame(f,{339,6,38,38});
            text(f,"守",358,13,20,gold,1);
        }
        number(f,battle.troops[0],51,6,17,mint);
        number(f,battle.troops[1],333,6,17,coral,2);
        healthBar(f,{51,27,97,9},battle.troops[0],battle.initial[0]);
        healthBar(f,{236,27,97,9},battle.troops[1],battle.initial[1],true);
        text(f,"军心",51,37,11,dim);
        number(f,battle.morale[0],77,36,12);
        text(f,"军心",310,37,11,dim);
        number(f,battle.morale[1],307,36,12,ivory,2);
        char buf[44];
        std::snprintf(buf,sizeof buf,"%s会战",cityName(battle.to));
        text(f,buf,192,4,16,gold,1);
        std::snprintf(buf,sizeof buf,"敌令 %s",formations[battle.intent]);
        text(f,buf,192,23,14,coral,1);
        int progress=battle.ticks<Battle::approach?battle.ticks:
            (battle.ticks-Battle::approach)%Battle::period;
        bar(f,{157,42,70,3},progress,battle.ticks<Battle::approach?Battle::approach:Battle::period,coral);
        for (int lane=0;lane<3;++lane) {
            army(f,lane,0);
            army(f,lane,1);
        }
        if (battle.effects) {
            int trail=25-battle.effects;
            for (int i=0;i<8;++i) {
                int x=115+trail*5+i*13, y=124+i*19%64;
                line(f,x,y,x+28,y-10,i%2?gold:ivory,2);
            }
            frame(f,{121,66,142,26},gold,ink);
            text(f,battle.officer().skill,192,70,17,gold,1);
        } else if (battle.round && (battle.ticks-Battle::approach)%Battle::period<17) {
            number(f,-battle.losses[0],111,92,15,coral,1);
            number(f,-battle.losses[1],273,92,15,coral,1);
        } else if (battle.ticks<Battle::approach) {
            text(f,"两军列阵",192,69,16,ivory,1);
        }
        plaque(f,{4,212,376,51});
        bar(f,{11,219,97,4},battle.signal,100,gold);
        std::snprintf(buf,sizeof buf,"令%d",battle.signal);
        text(f,buf,116,214,12,gold);
        text(f,battle.maneuver==measured?formationHints[battle.stance]:maneuvers[battle.maneuver],
             156,214,12,dim);
        if (battle.maneuverRounds) number(f,battle.maneuverRounds,198,214,12,gold);
        for (int i=0;i<3;++i) {
            Rect r{8+63*i,233,58,24};
            button(f,r,formations[i],Command::shift,i,i==battle.stance||battle.signal>=Battle::commandCost,
                   i==battle.stance,spearIcon+i);
        }
        button(f,{199,233,107,24},battle.usedSkill?"将技已用":battle.officer().skill,Command::skill,
               0,battle.commander>=0&&!battle.usedSkill&&battle.signal>=Battle::skillCost,true);
        button(f,{311,233,65,24},"号令",Command::orders);
        if (battle.paused) drawPause(f);
        else if (tacticsOpen) drawOrders(f);
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
        text(f,heading,192,105,14,reportWinner>0?gold:coral,1);
        block(f,{67,123,250,1},edge);
        text(f,"我军伤亡",78,139,15);
        number(f,reportLoss,308,138,17,ivory,2);
        const char* extra=reportWinner<=0?(reportLoss==battle.initial[0]?"全军覆没":"余部已撤离前线"):
                           reportDefense?"守军留驻城内":"入城缴获 115金 95粮";
        text(f,extra,192,159,14,dim,1);
        char merit[56];
        if (aftermath.objective) std::snprintf(merit,sizeof merit,"剧本悬赏已达成");
        else if (aftermath.level) std::snprintf(merit,sizeof merit,"领军提升至等级 %d",aftermath.level);
        else std::snprintf(merit,sizeof merit,"领军历练 +%d",aftermath.xp);
        if (aftermath.xp || aftermath.objective) text(f,merit,192,180,13,mint,1);
        button(f,{121,199,142,27},aftermath.captured?"处置俘将":"返回舆图",Command::accept,0,true,true);
    }
    void drawHelp(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"行军用策","六十四季内一统州郡");
        constexpr const char* titles[]={"治城","进军","临阵","军令"};
        constexpr const char* a[]={"征募补兵，屯田加粮，扩地增令。",
            "点邻城可调兵，围城次季抵达。",
            "枪克骑，骑克弓，弓克枪。看敌令。",
            "换阵耗14，将技耗46。军令会回升。"};
        constexpr const char* b[]={"城内政可任太守、安民、巡访。",
            "再点己城切换出发城。红线是敌袭。",
            "山地利枪，水乡利弓，平原利骑。",
            "号令控制攻守，单挑影响军心。"};
        for (int i=0;i<4;++i) {
            int y=44+46*i;
            frame(f,{10,y,364,42});
            text(f,titles[i],20,y+11,15,gold);
            text(f,a[i],60,y+3,13);
            text(f,b[i],60,y+23,13,dim);
        }
        button(f,{263,232,111,24},"返回",Command::back,0,true,true);
    }
    void drawEnding(pxa::game::Frame& f) {
        fillBackground(f,titleBackground(),.57f);
        skinFrame(f,{36,40,312,181});
        contain(f,art::outcomes[campaign.won>0?0:1],{114,54,156,52},true);
        text(f,campaign.won>0?"州郡皆归帐下，山河重定。":
                campaign.holdings(campaign.player)?"六十四季已过，诸侯割据未平。":"城池尽失，大势已去。",192,122,15,ivory,1);
        button(f,{120,166,144,30},"返回标题",Command::home,0,true,true);
    }
    void drawReplace(pxa::game::Frame& f) {
        fillBackground(f,titleBackground(),.57f);
        frame(f,{47,74,290,115},gold,ink);
        text(f,"另起一局",192,86,22,gold,1);
        text(f,"开战后会覆盖现有战役。",192,118,15,ivory,1);
        button(f,{57,148,119,29},"保留旧档",Command::back);
        button(f,{207,148,120,29},"确认起兵",Command::replace,0,true,true);
    }
    void drawCouncil(pxa::game::Frame& f) {
        fillBackground(f,art::warHall);
        header(f,"军议",factions[campaign.player].title);
        char state[68];
        std::snprintf(state,sizeof state,"%d",campaign.gold);
        resourcePlate(f,{10,41,95,25},coinsIcon,state,gold);
        std::snprintf(state,sizeof state,"%d",campaign.grain);
        resourcePlate(f,{110,41,95,25},grainIcon,state);
        std::snprintf(state,sizeof state,"%d/%d",campaign.actions,campaign.actionCapacity());
        resourcePlate(f,{210,41,95,25},orderIcon,state,mint);
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
        button(f,{10,230,102,25},"返回舆图",Command::back);
        button(f,{125,230,102,25},"军情",Command::forces);
        button(f,{236,229,138,27},"武将名册",Command::roster,0,true,true);
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
        f.clear({ink});
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
        page=nextPage;
        focus=first;
        count=0;
        pointerDown=false;
        mapDragging=false;
        dirty=true;
        previousTick=0;
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
    }
    void beginSeason() {
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
        else logState("enemy-march");
    }
    void finishMarch() {
        const Order order=playerMarch;
        const bool season=seasonMarch;
        playerMarch={};
        marching={};
        seasonMarch=false;
        if (season) {
            campaign.advance();
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
        formation=sites[order.to].terrain==river?bow:lead>=0?campaign.leader(lead).favored:spear;
        show(Page::deploy,formation);
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
        logState(battle.result>0?"victory":"defeat");
    }
    void pauseBattle() {
        tacticsOpen=false;
        battle.paused=!battle.paused;
        focus=battle.paused?0:battle.stance;
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
            case Page::cityDesk:show(Page::map,-1);break;
            case Page::forces:show(Page::council);break;
            case Page::gear:show(Page::roster,officerSlot-rosterStart());break;
            case Page::roster:show(rosterFrom,rosterFrom==Page::deploy?formation:0);break;
            case Page::deploy:
                if (battle.defending) show(Page::home);
                else recallColumn(arrivingColumn);
                break;
            case Page::battle:
                if (tacticsOpen) {tacticsOpen=false;focus=battle.stance;previousTick=0;dirty=true;}
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
                chosenChapter=0;side=0;
                show(Page::choose);
                break;
            case Command::guide:helpFrom=page;show(Page::help);break;
            case Command::back:(void)on_back();break;
            case Command::chapter:
                chosenChapter=arg;
                break;
            case Command::side:side=arg;break;
            case Command::seat:
                if (hasSave||damagedSave) {show(Page::replace);break;}
                [[fallthrough]];
            case Command::replace:
                campaign.start(side,chosenChapter);
                selected=campaign.capital();
                mapWindow=standardMap;mapZoom=0;focusCamera(selected);
                surveyCity=-1;
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
            case Command::work:
                if (campaign.develop(selected,Work(arg))) {
                    notice=arg==recruit?Notice::recruited:arg==farm?Notice::cultivated:
                        arg==walls?Notice::fortified:Notice::civic;
                    surveyCity=-1;
                    campaign.makeOrders();
                    checkpoint();
                    logState("work");
                }
                break;
            case Command::cityDesk:show(Page::cityDesk);break;
            case Command::survey:
                if (campaign.canSurvey(selected)) {
                    discovery=campaign.survey(selected);
                    surveyCity=selected;
                    checkpoint();
                    logState("survey");
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
            case Command::season:beginSeason();break;
            case Command::skipMarch:finishMarch();break;
            case Command::zoom:
                mapZoom=std::clamp(mapZoom+arg,0,2);
                focusCamera(destination<0?selected:destination);
                focus=-1;
                break;
            case Command::atlas:
                mapWindow=mapWindow.w==wideMap.w?standardMap:wideMap;
                if (mapWindow.w==wideMap.w && !mapZoom) mapZoom=1;
                focusCamera(destination<0?selected:destination);
                focus=-1;
                break;
            case Command::council:show(Page::council);break;
            case Command::forces:show(Page::forces);break;
            case Command::roster:
                rosterFrom=page;
                officerSlot=page==Page::dispatch?dispatchLead:
                    page==Page::deploy?defenseLeader:
                    page==Page::cityDesk?campaign.governor(selected):campaign.commander;
                if (officerSlot<0) officerSlot=campaign.commander;
                show(Page::roster,officerSlot-rosterStart());
                break;
            case Command::officer:officerSlot=arg;break;
            case Command::rosterPage:
                officerSlot=std::clamp(rosterStart()+arg*rosterSize,0,rosterCount()-1);
                focus=0;
                break;
            case Command::hire:
                if (campaign.hire(arg)) {checkpoint();logState("hire");}
                break;
            case Command::chooseOfficer:
                if (eligibleOfficer(arg)) {
                    if (rosterFrom==Page::dispatch) dispatchLead=arg;
                    else if (rosterFrom==Page::deploy) defenseLeader=arg;
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
                battle.start(campaign,departure,arrivingColumn,Formation(formation),defenseLeader);
                elapsed=0;
                tacticsOpen=false;
                show(Page::battle,formation);
                logState("battle");
                break;
            case Command::shift:
                if (page==Page::deploy) formation=arg;
                else if (battle.order(Formation(arg))) info("han-banners formation");
                break;
            case Command::skill:
                if (battle.rally()) {
                    info("han-banners skill");
                    if (battle.result) conclude();
                }
                break;
            case Command::orders:
                tacticsOpen=!tacticsOpen;
                focus=tacticsOpen?0:battle.stance;
                previousTick=0;
                break;
            case Command::maneuver:
                if (battle.direct(Maneuver(arg))) {
                    tacticsOpen=false;
                    focus=battle.stance;
                    previousTick=0;
                    logState("maneuver");
                }
                break;
            case Command::duel:
                if (battle.challenge()) {
                    tacticsOpen=false;
                    focus=0;
                    previousTick=0;
                    logState("duel");
                }
                break;
            case Command::duelMove:
                if (battle.duelOrder(DuelMove(arg))) logState("duel-move");
                break;
            case Command::pause:pauseBattle();break;
            case Command::retreat:
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
            picked=(focus+(dx+dy>0?1:count-1))%count;
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
    void on_pointer(pxa::Context&,const pxa::input::Pointer& p) {
        if (!ready) return;
        using P=pxa::input::PointerPhase;
        const float x=(pxa::ui::canvas_to_surface_coordinate(p.x,display)*width/display.width-ox)/scale;
        const float y=(pxa::ui::canvas_to_surface_coordinate(p.y,display)*height/display.height-oy)/scale;
        int hit=-1, nearest=50000;
        if (x>=0 && y>=0) for (int i=count-1;i>=0;--i) {
            const auto& control=controls[i];
            if (page==Page::map && control.command==Command::select) {
                const bool marker=control.box.contains(int(x),int(y));
                if (!marker && !mapLabels[control.data].contains(int(x),int(y))) continue;
                const int dx=int(x)-cityX(control.data),dy=int(y)-cityY(control.data);
                const int distance=(marker?0:10000)+dx*dx+dy*dy;
                if (distance<nearest) {nearest=distance;hit=i;}
            } else if (control.box.contains(int(x),int(y))) {
                hit=i;
                break;
            }
        }
        if (p.action()==P::down && !pointerDown) {
            pointerDown=true;
            pointerIndex=hit;
            pointerId=p.pointer_id;
            mapDragging=page==Page::map && !isMarching() && mapZoom>0 && mapWindow.contains(int(x),int(y));
            dragX=x;dragY=y;dragMoved=false;
            dirty=hit>=0 || dirty;
        } else if (pointerDown && pointerId==p.pointer_id && p.action()==P::move && mapDragging) {
            if (dragMoved || std::abs(x-dragX)+std::abs(y-dragY)>6) {
                dragMoved=true;
                mapCenterX-=(x-dragX)/mapScale();
                mapCenterY-=(y-dragY)/mapScale();
                dragX=x;dragY=y;
                clampCamera();
                controllerMode=false;
                dirty=true;
            }
        } else if (pointerDown && pointerId==p.pointer_id && p.ended()) {
            pointerDown=false;
            mapDragging=false;
            dirty=pointerIndex>=0 || dirty;
            if (p.action()==P::up && !dragMoved && hit>=0 && hit==pointerIndex) {
                controllerMode=false;
                input(controls[hit]);
                draw();
            }
        }
    }
    void on_background(pxa::Context&) {
        gamepad.reset();
        pointerDown=false;
        if (page==Page::battle) {battle.paused=true;focus=0;}
        previousTick=0;
        (void)context->clock().set_period(0);
    }
    void on_foreground(pxa::Context&) {
        immersive();
        previousTick=0;
        dirty=true;
        if (ready) (void)context->clock().set_period(33);
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& event) {
        if (auto d=pxa::ui::decode_display_metrics(event)) {
            resize|=display.width!=d->width || display.height!=d->height;
            display=*d;
            if (ready) layout();
            pointerDown=false;
            dirty=true;
            return true;
        }
        auto tick=pxa::decode_clock_tick(event);
        if (!tick) return false;
        if (!ready || !context->foreground()) return true;
        if (resize && !loadingArt) {
            auto task=context->tasks().start(openRenderer());
            if (!task) return std::unexpected(task.error());
            return true;
        }
        if (loadingArt) return true;
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
            while (elapsed>=33333 && (!battle.result || battle.duel.visible())) {
                battle.step();
                elapsed-=33333;
            }
            if (battle.result && !battle.duel.visible()) conclude();
            dirty=true;
        }
        if (dirty) draw();
        return true;
    }
};
}
PXA_APPLICATION(han::App)
