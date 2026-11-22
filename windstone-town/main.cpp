#include "combat_animation.hpp"
#include "world_view.hpp"

#include <pxa/app.hpp>
#include <pxa_surface_geometry.h>

#include <bit>
#include <cstdio>
#include <optional>

namespace wind {
namespace {

enum class Screen { Title, Town, Tutorial, Build, Roster, Hero, Quests, Party, Battle, Commissions, Ledger, Treasures, Journal, Shop, Help, Victory,
                    Loadout, Growth, Pack, Stock, ItemDetail, ExpeditionReport, Homes, Residents,
                    Disciplines, Workshop, Recipe, TownPlan, Business, Frontier, TownEvents, TownEventDetail };
enum Action {
    Begin=1, Close, OpenBuild, OpenRoster, OpenQuests, OpenLedger, OpenHelp,
    Speed, MapFocus, Previous, Next, ConfirmBuild, Hire, Gift, OpenHomes,
    Dispatch, Upgrade, Promote, OpenTownEvents, Craft, OpenTreasures, OpenJournal, ViewShops, ViewTerrain,
    OpenCommissions, AcceptSurvey, AcceptTown, AbandonCommission, ClaimCommission, FollowCommission, UseSupply, Retreat,
    Train, PlanBalanced, PlanAssault, PlanGuard, CyclePlan,
    OpenLoadout, OpenGrowth, OpenSupplies, OpenStock, BuyItem, SellItem, EquipItem, RemoveItem, UseItem,
    SpendArms, SpendGrit, SpendTravel,
    ToggleDepths, ShowExpeditionReport, OpenResidents, OpenDisciplines, OpenWorkshop, Refine,
    TrackHero, StopTracking, InspectTracked, OpenTownPlan, PlanExpedition, OpenBusiness,
    TakeMainPath, ExploreSideCave, OpenFrontier, LocateFrontier, PrepareFrontier, HostTownEvent, LocateSquare,
    BuildOption=100, HeroOption=120, QuestOption=140, PartyOption=160, GearOption=180, ItemOption=200,
    HomeOption=ItemOption+ItemCount, ResidentOption=HomeOption+MaxBuildings,
    RecipeOption=ResidentOption+HeroCount, DisciplineOption=RecipeOption+std::size(Recipes),
    PolicyOption=DisciplineOption+std::size(Disciplines),
    TownEventOption=PolicyOption+std::size(ServicePlans)
};
struct Button {Rect box; int action;};
constexpr const char* Seasons[]{"春芽","长夏","金秋","霜冬"};
constexpr const char* Activities[]{
    "散步","歇脚","前往铺面","正在到访","回家途中","在家休整","前去打野","讨伐野怪",
    "前往灯会","广场灯会","远征出发","矿洞交战","远征返程","探索矿道","前往南岭","南岭讨伐",
    "活动途中","广场集训","正在赴宴"
};
constexpr const char* Treasures[]{"萤苔琥珀","双响金铃","潮纹古镜","听风铜羽","霜脊王冠","风核之心"};
constexpr const char* TreasureLore[]{
    "一盏永不熄灭的苔灯，封存在树脂里。",
    "轻触铜铃，两声回响却来自不同方向。",
    "水面平静时，镜里仍有涟漪。",
    "越过山脊的每一阵风都刻在羽纹里。",
    "冰霜在王冠上悄悄长成六瓣。",
    "沉睡一百年的风，在掌心重新跳动。"
};

struct Format {
    char bytes[128]{};
    operator Text() const {return bytes;}
};
template<class... Args> Format fmt(const char* text,Args... args) {
    Format f;
    std::snprintf(f.bytes,sizeof(f.bytes),text,args...);
    return f;
}

struct App {
    static constexpr pxa::game::LoopOptions loop_options{
        .simulation_step_us=50000,.frame_period_ms=34,.maximum_updates=1};
    pxa::Context* ctx=nullptr;
    std::optional<pxa::Surface> surface;
    pxa::WindowMetrics window{};
    pxa::ui::DisplayMetrics display{};
    pxa::input::Controller controller;
    pxa::input::PointerCapture capture;
    Game game;
    std::array<Button,30> buttons{};
    int button_count=0,focus=0;
    bool pad=false, map_focus=false, placing=false, terrain_tab=false;
    bool ready=false, existing=false, foreground=false, opening=false, resize=true;
    bool dirty=true, save_pending=false, saving=false, save_error=false, save_protected=false;
    unsigned now=0, last_render=0, last_step=0, paint_time=0, paused_interpolation=0;
    CombatAnimation combat_animation;
    unsigned save_at=0, surface_at=0, toast_until=0, saved_until=0;
    unsigned long long frame_number=1;
    int scale=1, image_x=0,image_y=0, speed=1, page=0, hero_id=0, quest_id=0, shop_id=0;
    unsigned team=0;
    Tactic tactic=Tactic::Balanced;
    Route route=Route::Standard;
    Route cave_route=Route::Standard;
    Screen report_back=Screen::Quests;
    ItemGroup pack_group=ItemGroup::Weapon;
    Item item=Item::None;
    Screen item_source=Screen::Pack;
    int item_list_page=0;
    int recipe_id=0, workshop_page=0;
    int followed=-1;
    Kind build_kind=Forge;
    Screen screen=Screen::Title, help_back=Screen::Title, commission_back=Screen::Town;
    Screen hero_back=Screen::Roster, roster_back=Screen::Town, quests_back=Screen::Town;
    Screen frontier_back=Screen::Town;
    Screen events_back=Screen::Ledger;
    TownEvent town_event=TownEvent::Lanterns;
    Point camera=HomeCamera, selection{-1,-1};
    Rect safe{}, viewport{}, card{};
    Point down{}, last{};
    bool down_map=false, panning=false;
    Text toast{}, last_notice{};
    Save snapshot{};
    std::array<std::byte,SaveSize+100> storage_packet{};

    auto view() {
        return pxa::ui::Canvas().input_only().on_pointer(
            [this](const pxa::ui::CanvasPointer& event){pointer(event);});
    }
    pxa::Result<void> on_start(pxa::Context& context,std::span<const std::byte> config) {
        ctx=&context;
        if (auto v=pxa::ui::decode_start_display(config)) display=*v;
        (void)ctx->window().configure({.edge_to_edge=true,.status_bar_mode=pxa::WindowBarMode::hidden,
                                     .navigation_bar_mode=pxa::WindowBarMode::hidden});
        return ctx->tasks().start(load());
    }
    pxa::Task<void> load() {
        const auto result=co_await ctx->storage().get("windstone-v1",snapshot);
        bool previous_map=false;
        existing=result && decode(game.s,std::span{snapshot}.first(*result),&previous_map);
        if (existing) game.restore(previous_map);
        else {
            game.reset();
            if (result || result.error()!=pxa::Error::not_found) save_protected=true;
        }
        ready=true; dirty=true;
        co_return pxa::Result<void>{};
    }
    pxa::Task<void> write() {
        const auto result=co_await ctx->storage().set("windstone-v1",snapshot,storage_packet);
        saving=false;
        save_error=!result;
        if (result) saved_until=now+2200;
        else {save_pending=true; save_at=now+3000;}
        dirty=true;
        co_return pxa::Result<void>{};
    }
    void schedule_save() {
        if (!save_pending) save_at=now+500;
        save_pending=true;
        existing=true;
    }
    void flush() {
        if (!save_pending || saving || save_protected || now<save_at) return;
        snapshot=encode(game.s);
        saving=true; save_pending=false;
        if (!ctx->tasks().start(write())) {
            saving=false; save_pending=true; save_at=now+1000;
        }
    }
    void on_foreground(pxa::Context&) {
        foreground=true; last_step=0; dirty=true;
    }
    void on_background(pxa::Context&) {
        foreground=false;
        capture.reset(); controller.reset();
        if (ready && existing && screen!=Screen::Title) {
            schedule_save();
            save_at=now;
            flush();
        }
    }
    pxa::Result<bool> on_event(pxa::Context&,const pxa::Event& e) {
        if (e.service!=3 || e.opcode!=0x8002 || e.token!=0) return false;
        auto v=pxa::ui::decode_display_metrics(e);
        if (!v) return std::unexpected(v.error());
        display=*v;
        capture.reset();
        dirty=true;
        return true;
    }
    static int preferred_scale(const pxa::WindowMetrics& v) {
        const auto& inset=v.safe_insets;
        const int usable=std::min(int(v.pixel_width)-int(inset.left)-int(inset.right),
                                  int(v.pixel_height)-int(inset.top)-int(inset.bottom));
        if (usable>=840) return 4;
        if (usable>=420) return 2;
        return 1;
    }
    void on_window_changed(const pxa::WindowMetrics& v) {
        resize |= window.pixel_width!=v.pixel_width || window.pixel_height!=v.pixel_height ||
                  preferred_scale(window)!=preferred_scale(v);
        window=v;
        dirty=true;
    }
    pxa::Task<void> open_surface() {
        if (!window.pixel_width || !window.pixel_height) {
            const auto value=co_await ctx->window().snapshot();
            if (!value) {opening=false; surface_at=now+1000; co_return std::unexpected(value.error());}
            if (value->revision>=window.revision) on_window_changed(*value);
        }
        const auto w=window.pixel_width,h=window.pixel_height;
        const int divisor=preferred_scale(window);
        const auto sw=uint16_t(w/divisor), sh=uint16_t(h/divisor);
        surface.reset();
        auto result=co_await ctx->surface().create_mapped({
            .width=sw,.height=sh,.buffers=2,.max_buffer_bytes=1024*1024});
        if (!result) {
            opening=false; surface_at=now+1000;
            (void)ctx->log().write(pxa::LogLevel::error,"Windstone surface allocation failed");
            co_return std::unexpected(result.error());
        }
        const auto configured=co_await result->configure({.width=sw,.height=sh});
        if (configured) {
            scale=int(pxa_surface_fit_scale(sw,sh,w,h));
            image_x=(w-sw*scale)/2; image_y=(h-sh*scale)/2;
            surface.emplace(std::move(*result));
        } else surface_at=now+1000;
        resize=w!=window.pixel_width || h!=window.pixel_height || divisor!=preferred_scale(window);
        dirty=true; opening=false;
        co_return configured;
    }
    unsigned simulation_interpolation() const {
        return speed?std::min(now-last_step,SimulationBeatMs-1)*unsigned(speed):paused_interpolation;
    }
    void on_frame(pxa::Context&,pxa::game::FrameTick tick) {
        if (!foreground) return;
        now=unsigned(tick.timestamp_us/1000);
        if ((!surface || resize) && !opening && now>=surface_at) {
            opening=true;
            if (!ctx->tasks().start(open_surface())) opening=false;
        }
        const bool running=ready && screen!=Screen::Title && screen!=Screen::Tutorial &&
                           screen!=Screen::Help && screen!=Screen::Victory && speed;
        if (!last_step || !running) last_step=now;
        if (running && now-last_step>=SimulationBeatMs) {
            const int updates=std::min(3,int((now-last_step)/SimulationBeatMs));
            last_step=now-(now-last_step)%SimulationBeatMs;
            const int day=game.s.day,phase=game.s.trip.phase, result=game.s.trip.result;
            const auto progress=game.s.commission.progress;
            const auto attendance=game.s.event_attended;
            const auto trail=game.s.trip.distance/20;
            const auto patrol=game.s.trip.patrol_hp, cache=game.s.trip.cache_found;
            const auto side_cache_found=game.s.trip.side_cache_found;
            const auto route_choice=game.s.trip.cave_path;
            const bool at_fork=waiting_at_fork(game.s.trip);
            for (int i=0;i<updates*speed;++i) {
                const auto prior=game.s.trip;
                std::array<uint8_t,HeroCount> health{};
                if (prior.phase==Fighting)
                    for (int id=0;id<HeroCount;++id) health[id]=game.s.heroes[id].hp;
                game.step(screen==Screen::Battle || screen==Screen::Quests);
                if (prior.round!=game.s.trip.round) combat_animation.record(prior.hp,health,game.s);
                else combat_animation.step();
            }
            if (game.s.day!=day || game.s.trip.phase!=phase || game.s.trip.result!=result ||
                game.s.commission.progress!=progress || game.s.event_attended!=attendance || game.s.trip.distance/20!=trail ||
                game.s.trip.patrol_hp!=patrol || game.s.trip.cache_found!=cache ||
                game.s.trip.side_cache_found!=side_cache_found || game.s.trip.cave_path!=route_choice ||
                waiting_at_fork(game.s.trip)!=at_fork) schedule_save();
            if (game.journal[0].message!=last_notice &&
                !(screen==Screen::Battle && combat_animation.finishing(game.s.trip,combat_animation.age))) {
                last_notice=game.journal[0].message;
                if (!last_notice.empty()) show_toast(last_notice);
            }
        }
        if (toast_until && now>=toast_until) {toast_until=0; dirty=true;}
        if (saved_until && now>=saved_until) {saved_until=0; dirty=true;}
        flush();
        const bool animated=screen!=Screen::Title && screen!=Screen::Help;
        if (!surface || opening || (!dirty && (!animated || now-last_render<52))) return;
        auto frame=surface->acquire();
        if (!frame) return;
        const auto pixels=frame->rgb565();
        if (!pixels) return;
        Draw draw(*pixels);
        paint_time=now;
        render(draw);
        if (frame->present(frame_number)) {++frame_number; dirty=false; last_render=now;}
    }

    void layout(Draw& d) {
        const auto& in=window.safe_insets;
        const int l=(std::max(0,int(in.left)-image_x)+scale-1)/scale;
        const int r=(std::max(0,int(in.right)-image_x)+scale-1)/scale;
        const int t=(std::max(0,int(in.top)-image_y)+scale-1)/scale;
        const int b=(std::max(0,int(in.bottom)-image_y)+scale-1)/scale;
        safe={l,t,d.clip.w-l-r,d.clip.h-t-b};
        viewport={0,safe.y+31,d.clip.w,safe.h-72};
    }
    bool focused(int action) const {
        return pad && !map_focus && button_count==focus && action;
    }
    void add(Rect r,int action) {
        if (action && button_count<int(buttons.size())) buttons[button_count++]={r,action};
    }
    void button(Draw& d,Rect r,Text text,int action,bool primary=false,int mark=-1) {
        const bool active=focused(action);
        d.skin(r,!action?InactiveButton:active?AmberButton:primary?TealButton:WheatButton);
        const auto color=!action?Quiet:active?TextInk:primary?Paper:TextInk;
        const auto content=r.inset(4);
        if (mark<0) d.center(content,text,color);
        else if (text.empty()) {
            const int size=std::min(content.h,content.w)>=14?14:10;
            if (std::min(content.h,content.w)>=size)
                icon(d,Icon(mark),r.x+(r.w-size)/2,r.y+(r.h-size)/2,size);
        }
        else {
            const auto label=Draw::fit({0,0,content.w-13,content.h},text);
            if (label.ink.w && content.h>=10) {
                const int x=content.x+(content.w-13-label.ink.w)/2;
                icon(d,Icon(mark),x,r.y+(r.h-10)/2);
                d.line({x+13,content.y,label.ink.w,content.h},label,color);
            } else d.center(content,text,color);
        }
        add(r,action);
    }
    void title(Draw& d) {
        d.fill(d.clip,PanelShade);
        const int middle=safe.x+safe.w/2;
        const Rect source=safe.h>safe.w?Rect{0,200,240,300}:Rect{0,0,320,200};
        constexpr int inset=7;
        const int height=std::min(safe.h-43,source.h+2*inset),width=std::min(safe.w-6,source.w+2*inset);
        const int top=safe.y+std::max(3,(safe.h-height-41)/2);
        const Rect frame{middle-width/2,top,width,height};
        const auto scene=frame.inset(inset);
        d.skin(frame,MarblePaper);
        d.crop(art::title,source,scene);
        const Rect name{middle-86,scene.y+5,172,36};
        d.skin(name,TitleRail);
        const auto title_label=name.inset(5);
        d.center({title_label.x+1,title_label.y+1,title_label.w,title_label.h},"秘宝冒险镇",PanelShade,24);
        d.center(title_label,"秘宝冒险镇",Brass,24);
        const int buttonY=std::min(safe.bottom()-36,frame.bottom()+7);
        d.skin({safe.x+4,buttonY-4,safe.w-8,36},StatusRail);
        button(d,{middle-93,buttonY,123,28},!ready?"读档中":existing?"继续冒险":"抵达风石镇",
               ready?Begin:0,true,Play);
        button(d,{middle+36,buttonY,58,28},"玩法",OpenHelp);
        if (save_protected)
            d.center({safe.x,buttonY+27,safe.w,15},"无法读取存档，本次不会写入",Paper);
    }
    void hud_control(Draw& d,Rect box,Icon mark,int action) {
        const bool active=focused(action);
        if (active) d.skin(box,AmberButton);
        icon(d,mark,box.x+(box.w-14)/2,box.y+(box.h-14)/2,14);
        add(box,action);
    }
    void hud_resource(Draw& d,Rect box,Icon mark,int amount,Color color) {
        const auto value=fmt("%d",amount);
        const int width=box.w-22;
        const int size=Draw::measure(value,16)<=width?16:12;
        icon(d,mark,box.x+4,box.y+(box.h-12)/2,12);
        d.label({box.x+19,box.y+2,width,box.h-4},value,color,size);
    }
    void hud(Draw& d) {
        d.fill({0,0,d.clip.w,viewport.y},PanelShade);
        const Rect rail{safe.x+3,safe.y+2,safe.w-6,27};
        d.skin(rail,StatusRail);
        const int controls=screen==Screen::Town?2:screen==Screen::Battle?1:0;
        const int control_w=safe.w<260?24:28;
        const int right=rail.right()-4;
        const int info_end=right-(controls?controls*(control_w+4):40);
        const int y=rail.y+3,left=rail.x+4;
        const int number_w=std::min(75,(info_end-left)*3/10);
        const int gold_w=std::max(number_w,Draw::measure(fmt("%d",game.s.gold))+22);
        const int fame_w=std::max(number_w,Draw::measure(fmt("%d",game.s.fame))+22);
        hud_resource(d,{left,y,gold_w,21},Coin,game.s.gold,Paper);
        hud_resource(d,{left+gold_w+3,y,fame_w,21},Star,game.s.fame,Brass);
        const int day_x=left+gold_w+fame_w+6;
        for (const int boundary:{left+gold_w,day_x-2,info_end+1})
            d.fill({boundary,y+4,1,13},RailDivider);
        const Rect date{day_x,y,info_end-day_x,21};
        const auto day=fmt("%s %d日",Seasons[(game.s.day-1)/12%4],game.s.day);
        const auto period=game.s.clock>=Game::EveningBeat?"暮":game.s.clock<Game::DayBeats/4?"晨":"昼";
        const auto full_date=fmt("%s %s",day.bytes,period);
        const Text time_label=Draw::measure(full_date)<=date.w?Text(full_date):Text(day);
        const int width=Draw::measure(time_label);
        if (date.w>=width+18) {
            const int x=date.x+(date.w-width-16)/2;
            icon(d,Sun,x,y+4,12);
            d.text(x+16,y+4,time_label,Paper);
        } else {
            d.center(date,width<=date.w?time_label:Text(fmt("%d日",game.s.day)),Paper);
        }
        if (controls) {
            const int x=right-control_w;
            hud_control(d,{x,y,control_w,21},speed==0?Stop:speed==2?Fast:Play,Speed);
            if (controls==2)
                hud_control(d,{x-control_w-4,y,control_w,21},Map,MapFocus);
        } else {
            d.center({right-36,y,36,21},fmt("%d星",game.s.rank),Brass);
        }
    }
    void show_toast(Text text) {
        toast=text; toast_until=now+3400; dirty=true;
    }
    void message(Draw& d,Rect r,Text text,Color color=TextInk) {
        d.skin(r,MessagePlaque);
        d.label({r.x+7,r.y+3,r.w-14,r.h-6},text,color);
    }
    static Text placement_hint(Placement state) {
        switch (state) {
        case Placement::Good: return "地块可建，确认后扣除金币";
        case Placement::Occupied: return "选择空地，避开河道、树林和已有设施";
        case Placement::Locked: return "升镇后开放这片土地";
        case Placement::NoRoad: return "需要同一高度且与广场相通的道路";
        case Placement::Funds: return "金币不足";
        case Placement::Limit: return "已达设施数量上限";
        case Placement::ElevationLimit: return "已到达地形高度上限或下限";
        case Placement::NoStairLink: return "石阶需紧邻高差一层、已通行的道路";
        }
        return {};
    }
    void dock_button(Draw& d,Rect box,Icon mark,Text label,int action,bool primary=false) {
        const bool active=focused(action);
        if (active || primary) d.skin(box,active?AmberButton:DockAction);
        const int size=box.w>=58?18:14;
        const auto inside=box.inset(4);
        const auto line=Draw::fit({0,0,inside.w-size-3,inside.h},label);
        const int x=inside.x+(inside.w-size-line.ink.w-3)/2;
        if (inside.h>=size && inside.w>=size) icon(d,mark,std::max(x,inside.x),box.y+(box.h-size)/2,size);
        d.line({x+size+3,inside.y,line.ink.w,inside.h},line,active?TextInk:Paper);
        add(box,action);
    }
    int dock_tab() const {
        constexpr std::array tabs{Screen::Build,Screen::Roster,Screen::Quests,Screen::Ledger};
        const auto tab=std::find(tabs.begin(),tabs.end(),screen);
        return tab==tabs.end()?-1:int(tab-tabs.begin());
    }
    void town_bar(Draw& d) {
        const int bottom=viewport.bottom();
        d.fill({0,bottom,d.clip.w,d.clip.h-bottom},PanelShade);
        const Rect rail{safe.x+3,bottom+3,safe.w-6,safe.bottom()-bottom-6};
        d.skin(rail,StatusRail);
        const int tab=dock_tab();
        if (screen!=Screen::Town && tab<0) return;
        const Rect area{rail.x+4,rail.y+3,rail.w-8,rail.h-6};
        if (screen==Screen::Town && placing) {
            const auto validity=inside(selection)?game.placement(selection,build_kind):Placement::Occupied;
            const auto& building=Types[build_kind];
            const Rect stripe{safe.x+6,bottom-21,safe.w-12,18};
            if (earthwork(build_kind) && validity==Placement::Good) {
                const int h=game.terrain(selection).height;
                message(d,stripe,fmt("高度 %d → %d　余 %d 金",h,h+(build_kind==RaiseGround?1:-1),
                                     game.s.gold-building.cost),Mint);
            } else if (validity==Placement::Good) {
                message(d,stripe,fmt("建造后剩余 %d 金",game.s.gold-building.cost),Mint);
            } else message(d,stripe,inside(selection)?placement_hint(validity):
                           "点选空地，或用方向键移动施工标记");
            button(d,{area.x,area.y,66,area.h},"取消",Close);
            button(d,{area.x+72,area.y,area.w-72,area.h},
                   fmt("%s%s  %d金",earthwork(build_kind)?"":"建造",building.name,building.cost),
                   validity==Placement::Good?ConfirmBuild:0,true,Hammer);
            return;
        }
        const bool show_save=screen==Screen::Town && (save_error || saved_until);
        if (screen==Screen::Town && toast_until) {
            const int max=safe.w-12-(show_save?60:0);
            const int width=std::min(max,Draw::measure(toast)+15);
            message(d,{safe.x+6,bottom-21,width,18},toast);
        }
        if (show_save)
            message(d,{safe.right()-60,bottom-21,54,18},save_error?"存档失败":"已存档",save_error?Rust:Mint);
        constexpr const char* labels[]{"建造","旅人","寻宝","镇务"};
        constexpr int actions[]{OpenBuild,OpenRoster,OpenQuests,OpenLedger};
        constexpr Icon marks[]{Hammer,People,Compass,Book};
        for (int i=0;i<4;++i) {
            const int a=area.x+area.w*i/4,b=area.x+area.w*(i+1)/4;
            if (i) d.fill({a-2,area.y+5,1,area.h-10},RailDivider);
            const bool selected=i==tab;
            dock_button(d,{a,area.y,b-a-3,area.h},marks[i],labels[i],selected?Close:actions[i],selected);
        }
    }
    void panel(Draw& d,Text name) {
        d.shade({card.x+3,card.y+3,card.w,card.h},PanelShade,9);
        d.skin(card,MarblePaper);
        d.label({card.x+12,card.y+6,card.w-50,19},name,TextInk,16);
        d.fill({card.x+8,card.y+27,card.w-16,1},d.paper_rule());
        const Rect close{card.right()-31,card.y+4,24,22};
        if (focused(Close)) d.skin(close,FocusedRow);
        icon(d,CloseIcon,close.x+5,close.y+4,14);
        add(close,Close);
    }
    void heading(Draw& d,Text name,Text hint={}) {
        d.shade(viewport,DeepShade,6);
        const int w=std::min(safe.w-12,330);
        const int h=std::min(viewport.h-6,191);
        card={safe.x+(safe.w-w)/2,viewport.y+(viewport.h-h)/2,w,h};
        panel(d,name);
        if (!hint.empty()) d.text(card.x+9,card.y+33,hint,Quiet,12,card.w-18);
    }
    int row_count() const {return card.h>=153?3:2;}
    Rect list_row(int i) const {
        return {card.x+8,card.y+49+i*25,card.w-16,23};
    }
    void list(Draw& d,int i,Text left,Text right,int action,int sprite=-1,bool selected=false) {
        const auto r=list_row(i);
        const bool active=focused(action);
        if (active || selected)
            d.skin(r,active?FocusedRow:SelectedRow);
        else d.fill({r.x+3,r.bottom()-1,r.w-6,1},d.paper_rule());
        if (sprite>=0) {
            const Point p{r.x+14,r.bottom()-2};
            if (sprite>=200) item_icon(d,static_cast<Item>(sprite-199),{r.x+4,r.y+2});
            else if (sprite>=100) hero_sprite(d,sprite-100,p);
            else building_sprite(d,Kind(sprite),p,BuildingArt::List);
        }
        const int shift=sprite>=0?27:7;
        const int rw=std::min(Draw::measure(right),r.w/2);
        d.label({r.x+shift,r.y+3,r.w-rw-shift-10,r.h-6},left,action?TextInk:Quiet);
        d.label({r.right()-rw-7,r.y+3,rw,r.h-6},right,action?Mint:Quiet);
        add(r,action);
    }
    Rect actions_area() const {return {card.x+8,card.bottom()-27,card.w-16,22};}
    void pagination(Draw& d,int total,int columns,Text label={}) {
        const int count=(total+columns-1)/columns;
        const auto r=actions_area();
        button(d,{r.x,r.y,50,r.h},"上一页",page>0?Previous:0);
        button(d,{r.right()-50,r.y,50,r.h},"下一页",page+1<count?Next:0);
        d.center({r.x+53,r.y,r.w-106,r.h},label.empty()?Text(fmt("%d / %d",page+1,count)):label,Quiet);
    }
    void split(Draw& d,Text a,int action_a,Text b,int action_b,bool primary=true) {
        const auto r=actions_area();
        button(d,{r.x,r.y,(r.w-5)/2,r.h},a,action_a);
        button(d,{r.x+(r.w+5)/2,r.y,(r.w-5)/2,r.h},b,action_b,primary);
    }
    void build_tab(Draw& d,Rect box,Text name,bool chosen,int action) {
        const bool active=focused(action);
        d.folder_tab(box,chosen,active);
        // Use the shared clear area below the bevel and above the page rim.
        d.center({box.x+4,box.y+6,box.w-8,box.h-7},name,TextInk,10);
        add(box,action);
    }
    void catalog_card(Draw& d,Rect box,Kind kind) {
        const auto& item=Types[kind];
        const bool unlocked=item.rank<=game.s.rank;
        const bool affordable=game.s.gold>=item.cost;
        const int action=unlocked?BuildOption+int(kind):0;
        if (focused(action)) d.skin(box.inset(2),FocusedRow);
        constexpr const char* names[]{"公会","旅店","铁匠铺","药房","民居","集市","矿洞","祭坛",
                                      "小路","石阶","抬高","降低"};
        const bool large=box.h>=60 && kind<Road;
        const int picture=large?32:21;
        const int top=box.y+std::max(1,(box.h-picture-26)/2);
        const int middle=box.x+box.w/2;
        const auto old_clip=d.clip;
        d.clip={box.x+2,box.y+1,box.w-4,top+picture-box.y};
        building_sprite(d,kind,{middle,top+picture},large?BuildingArt::Preview:BuildingArt::List);
        if (!unlocked) d.shade(d.clip,d.paper_color(),5);
        d.clip=old_clip;
        d.center({box.x+2,top+picture+1,box.w-4,13},names[kind],unlocked?TextInk:Quiet);
        const Rect price{box.x+5,top+picture+14,box.w-10,12};
        if (unlocked) {
            const auto cost=fmt("%d",item.cost);
            const int x=middle-(Draw::measure(cost)+13)/2;
            icon(d,Coin,x,price.y+1);
            d.label({x+13,price.y,Draw::measure(cost),price.h},cost,affordable?Mint:Rust);
        } else d.center(price,fmt("%d星解锁",item.rank),Quiet);
        add(box,action);
    }
    void catalog_page(Draw& d,Rect box,Text label,int action) {
        if (focused(action)) d.skin(box,FocusedRow);
        d.center(box,label,action?Mint:Quiet);
        add(box,action);
    }
    void build_menu(Draw& d) {
        const bool side=safe.w>=280 && safe.w>safe.h;
        const int w=side?std::min(242,safe.w/2+16):std::min(244,safe.w-12);
        const int h=std::min(viewport.h-4,safe.h>safe.w?211:185);
        const int x=side?safe.right()-w-6:safe.x+(safe.w-w)/2;
        card={x,viewport.y+(viewport.h-h)/2,w,h};
        panel(d,"建造");
        constexpr int tab_gap=2;
        const Rect tabs{card.x+8,card.y+29,w-16,Draw::FolderTabHeight};
        const int tab_w=(tabs.w-tab_gap)/2;
        const Rect shops{tabs.x,tabs.y,tab_w,tabs.h};
        const Rect terrain{shops.right()+tab_gap,tabs.y,tabs.w-tab_w-tab_gap,tabs.h};
        const Rect footer{card.x+8,card.bottom()-17,w-16,14};
        d.folder_page({tabs.x,tabs.bottom()-1,tabs.w,footer.y-tabs.bottom()},terrain_tab?terrain:shops);
        build_tab(d,shops,"铺面",!terrain_tab,ViewShops);
        build_tab(d,terrain,"地形与道路",terrain_tab,ViewTerrain);
        const std::span<const Kind> catalog=terrain_tab?std::span<const Kind>{GroundList}:std::span<const Kind>{ShopList};
        const int columns=!terrain_tab && w>=204?3:2,rows=h>=158?2:1;
        const int per=columns*rows,count=(int(catalog.size())+per-1)/per;
        page=std::min(page,count-1);
        const int start=tabs.bottom(),tile_h=(footer.y-3-start)/rows;
        const int visible=std::min(per,int(catalog.size())-page*per);
        for (int i=0;i<visible;++i) {
            const int c=i%columns,r=i/columns;
            const int from=footer.x+c*footer.w/columns,to=footer.x+(c+1)*footer.w/columns;
            const Rect box{from,start+r*tile_h,to-from,tile_h};
            if (c) d.fill({from,box.y+4,1,box.h-8},d.paper_rule());
            if (r) d.fill({from+3,box.y,box.w-6,1},d.paper_rule());
            catalog_card(d,box,catalog[page*per+i]);
        }
        if (count>1) {
            catalog_page(d,{footer.x,footer.y,49,footer.h},"上一页",page>0?Previous:0);
            catalog_page(d,{footer.right()-49,footer.y,49,footer.h},"下一页",page+1<count?Next:0);
            d.center({footer.x+50,footer.y,footer.w-100,footer.h},fmt("%d / %d",page+1,count),Quiet);
        } else d.center(footer,"选好后在地图上放置",Quiet);
    }
    void roster(Draw& d) {
        const int residents=std::count_if(game.s.heroes.begin(),game.s.heroes.end(),
            [](const Hero& hero){return hero.level && hero.home!=Game::NoHome;});
        heading(d,"旅人名册",fmt("已定居 %d / %d",residents,game.hero_count()));
        const int per=row_count();
        for (int i=0;i<per && page*per+i<HeroCount;++i) {
            const int id=page*per+i;
            const auto& h=game.s.heroes[id];
            list(d,i,fmt("%s  %s",Heroes[id].name,Heroes[id].role),
                 h.level?Text(fmt("Lv.%d  %s",h.level,Activities[int(game.activity(id))])):
                         Text(fmt("声望 %d",Heroes[id].fame)),HeroOption+id,100+id);
        }
        pagination(d,HeroCount,per);
    }
    void hero_detail(Draw& d) {
        const auto& h=game.s.heroes[hero_id];
        const auto& a=Heroes[hero_id];
        heading(d,fmt("%s · %s",a.name,a.role));
        const auto r=actions_area();
        const Point portrait{card.x+34,std::min(card.y+84,r.y-47)};
        hero_sprite(d,hero_id,portrait,true);
        travel_outfit(d,h,portrait+Point{2,-18},true);
        const int x=card.x+62, y=card.y+39;
        if (!h.level) {
            d.text(x,y,"等待加入小镇",TextInk);
            d.text(x,y+19,fmt("需要声望 %d、金币 %d",a.fame,a.fee),Quiet,12,card.right()-x-7);
            split(d,"返回名册",OpenRoster,fmt("邀请 %d金",a.fee),
                  game.s.fame>=a.fame && game.s.gold>=a.fee?Hire:0);
            return;
        }
        const int width=card.right()-x-7;
        const auto stats=game.stats(hero_id);
        d.label({x,y,width,14},fmt("Lv.%d  战力 %d  满意 %d",h.level,stats.rating(),h.mood),TextInk);
        d.label({x,y+16,width,14},fmt("生命 %d  体力 %d  钱袋 %d",h.hp,h.energy,h.wallet),Mint);
        const bool available=game.eligible(hero_id);
        button(d,{portrait.x-23,portrait.y+1,46,20},"定位",TrackHero);
        if (card.h>=148) {
            const auto attributes=game.prepared(hero_id)?
                fmt("攻击 %d  防护 %d  整备 −%d",stats.attack,stats.armor,PreparedDiscount):
                fmt("攻击 %d  防护 %d  减耗 %d",stats.attack,stats.armor,stats.stamina);
            d.label({x,y+32,width,14},attributes,TextInk);
        }
        if (card.h>=154)
            d.label({x,y+48,width,14},h.level==LevelLimit?Text("已达最高等级"):
                    Text(fmt("经验 %d/%d  训练加 %d",h.xp,experience_goal(h.level),game.training_experience())),Quiet);
        const int half=r.w/2;
        button(d,{r.x,r.y-25,half-2,21},"装备与道具",OpenLoadout,false,Chest);
        button(d,{r.x+half+2,r.y-25,r.w-half-2,21},fmt("成长 · %d点",game.growth_points(hero_id)),OpenGrowth,false,Book);
        const int w=r.w/3;
        button(d,{r.x,r.y,w-3,r.h},"馈赠 34金",available && game.s.gold>=34?Gift:0);
        const auto train_label=fmt("训练 %d金",Game::TrainingCost);
        button(d,{r.x+w,r.y,w-3,r.h},h.level==LevelLimit && !h.practicing()?"已满级":game.trained(hero_id)?"今日已练":
               !available?"远征中":h.hp<Game::TrainingHealth || h.energy<Game::TrainingEnergy?"待休整":Text(train_label),
               game.can_train(hero_id)?Train:0,true);
        button(d,{r.x+w*2,r.y,r.w-w*2,r.h},
               h.home==Game::NoHome?Text("安排住处"):Text(fmt("民居 %d",home_number(h.home))),OpenHomes);
    }
    Format activity_detail(int id) const {
        const auto activity=game.activity(id);
        if (activity==Activity::ToShop || activity==Activity::Shop) {
            const auto& building=game.s.buildings[game.walkers[id].shop];
            return fmt("%s%s",activity==Activity::ToShop?"去":"在",Types[building.kind].name);
        }
        return fmt("%s",Activities[int(activity)]);
    }
    void track_camera() {
        const auto activity=game.activity(followed);
        if (activity==Activity::Home)
            camera=tile_center(game,game.s.buildings[game.s.heroes[followed].home].at());
        else if (activity==Activity::Expedition || activity==Activity::CaveTrail)
            camera=tile_center(game,expedition_entrance(game.s.trip.route));
        else camera=walker_feet(game,followed,simulation_interpolation());
        clamp_camera();
    }
    void tracking_banner(Draw& d) {
        const Rect r{safe.x+6,viewport.y+4,std::min(244,safe.w-12),21};
        const auto status=activity_detail(followed);
        button(d,{r.x,r.y,r.w-52,r.h},fmt("%s · %s",Heroes[followed].name,status.bytes),InspectTracked,true,People);
        button(d,{r.right()-48,r.y,48,r.h},"停止",StopTracking);
    }
    int home_number(int house) const {
        return int(std::count_if(game.s.buildings.begin(),game.s.buildings.begin()+house+1,
                                [](const Building& b){return b.kind==House;}));
    }
    void homes(Draw& d) {
        const auto& h=game.s.heroes[hero_id];
        const auto title=fmt("%s · 住处",Heroes[hero_id].name);
        const auto current=fmt("现居 民居 %d，可选择空屋搬入",h.home==Game::NoHome?0:home_number(h.home));
        const auto mood=fmt("满意 %d/%d，达标后可入住",h.mood,Game::SettleMood);
        heading(d,title,!game.eligible(hero_id)?Text("远征结束后可安排住处"):
                 h.home!=Game::NoHome?Text(current):h.mood<Game::SettleMood?Text(mood):Text("选择一间空置民居"));
        std::array<int,MaxBuildings> empty{};
        int count=0;
        for (int i=1;i<game.s.count;++i)
            if (game.s.buildings[i].kind==House && !game.occupants(i)) empty[count++]=i;
        if (!count) {
            d.text(card.x+10,card.y+56,"没有空置民居，可再建一间。",TextInk,12,card.w-20);
            split(d,"返回",Close,"建造民居",BuildOption+int(House));
            return;
        }
        const int per=row_count();
        for (int i=0;i<per && page*per+i<count;++i) {
            const int house=empty[page*per+i], level=game.s.buildings[house].level;
            list(d,i,fmt("民居 %d  ·  Lv.%d",home_number(house),level),fmt("日租 %d",room_rent(level)),
                 game.can_assign_home(hero_id,house)?HomeOption+house:0,House);
        }
        pagination(d,count,per);
    }
    void residents(Draw& d) {
        const auto& house=game.s.buildings[shop_id];
        heading(d,fmt("民居 %d · 分配住户",home_number(shop_id)),fmt("一人一户  ·  日租 %d",room_rent(house.level)));
        std::array<int,HeroCount> people{};
        int count=0;
        for (int i=0;i<HeroCount;++i)
            if (game.s.heroes[i].level) people[count++]=i;
        const int per=row_count();
        for (int i=0;i<per && page*per+i<count;++i) {
            const int id=people[page*per+i];
            const auto& h=game.s.heroes[id];
            const auto address=fmt("从民居 %d 搬入",h.home==Game::NoHome?0:home_number(h.home));
            const auto mood=fmt("满意 %d/%d",h.mood,Game::SettleMood);
            list(d,i,Heroes[id].name,!game.eligible(id)?Text("远征中"):
                 h.home!=Game::NoHome?Text(address):h.mood<Game::SettleMood?Text(mood):Text("可入住"),
                 game.can_assign_home(id,shop_id)?ResidentOption+id:0,100+id);
        }
        pagination(d,count,per);
    }
    void loadout(Draw& d) {
        const auto& hero=game.s.heroes[hero_id];
        heading(d,fmt("%s · 装备",Heroes[hero_id].name),fmt("武器强化 +%d  ·  更换装备保留强化",hero.gear));
        constexpr Text slots[]{"武器","护甲","饰物"};
        const int per=card.h>=149?3:2;
        for (int i=0;i<per && page*per+i<GearSlots;++i) {
            const int slot=page*per+i;
            const Item equipped=hero.equipment[slot];
            list(d,i,slots[slot],equipped==Item::None?Text("空栏"):Text(item_type(equipped).name),
                 GearOption+slot,equipped==Item::None?-1:200+item_index(equipped),equipped!=Item::None);
        }
        const auto r=actions_area();
        const int center=r.w-104;
        button(d,{r.x,r.y,48,r.h},"上页",page>0?Previous:0);
        button(d,{r.x+52,r.y,center,r.h},"仓库道具",OpenSupplies,true,Chest);
        button(d,{r.right()-48,r.y,48,r.h},"下页",(page+1)*per<GearSlots?Next:0);
    }
    void growth(Draw& d) {
        const auto& hero=game.s.heroes[hero_id];
        const int points=game.growth_points(hero_id);
        heading(d,fmt("%s · 成长",Heroes[hero_id].name),
                fmt("可分配 %d 点  ·  每升一级获得一点",points));
        constexpr Text names[]{"武艺","坚韧","行装"};
        constexpr const char* benefits[]{"攻击 +2","防护 +1","减耗 +1"};
        const int per=row_count();
        for (int i=0;i<per && page*per+i<TalentCount;++i) {
            const int talent=page*per+i;
            const bool available=game.can_invest(hero_id,static_cast<Talent>(talent));
            list(d,i,fmt("%s  %d/%d",names[talent].data(),hero.talents[talent],TalentLimit),
                 fmt("%s%s",benefits[talent],available?" · 加点":""),available?SpendArms+talent:0);
        }
        const auto r=actions_area();
        const auto specialization=fmt("专精 · %s",valid_discipline(hero.discipline)?
                                      Disciplines[unsigned(hero.discipline)-1].name:"未选");
        if (per>=TalentCount) button(d,r,specialization,OpenDisciplines,true,Book);
        else {
            button(d,{r.x,r.y,46,r.h},"上页",page>0?Previous:0);
            button(d,{r.x+50,r.y,r.w-100,r.h},specialization,OpenDisciplines,true);
            button(d,{r.right()-46,r.y,46,r.h},"下页",(page+1)*per<TalentCount?Next:0);
        }
    }
    void disciplines(Draw& d) {
        const auto& h=game.s.heroes[hero_id];
        const auto required=fmt("旅人达到 %d 级可修习",DisciplineLevel);
        heading(d,fmt("%s · 公会专精",Heroes[hero_id].name),h.level<DisciplineLevel?Text(required):
                 !game.eligible(hero_id)?Text("远征结束后可切换专精"):Text("训练与讨伐提升当前专精"));
        const int per=row_count();
        const int count=int(std::size(Disciplines));
        for (int i=0;i<per && page*per+i<count;++i) {
            const int choice=page*per+i;
            const auto& info=Disciplines[choice];
            const auto type=static_cast<Discipline>(choice+1);
            const int progress=h.practice[choice];
            const auto learning=fmt("熟练 %d/%d",progress,MasteryLimit);
            const auto cost=fmt("修习 %d金",DisciplineCost);
            list(d,i,fmt("%s · %s",info.name,info.benefit),
                 progress==MasteryLimit?Text("精通"):progress?Text(learning):Text(cost),
                 game.can_choose_discipline(hero_id,type)?DisciplineOption+choice:0,-1,type==h.discipline);
        }
        if (per<count) pagination(d,count,per,"精通加成常驻");
        else {
            const auto r=actions_area();
            d.center(r,"精通加成常驻，已修习可免费切换",Quiet);
        }
    }
    void item_list(Draw& d,bool shopping) {
        constexpr const char* groups[]{"武器仓库","护甲仓库","饰物仓库","道具仓库"};
        heading(d,shopping?Types[game.s.buildings[shop_id].kind].name:groups[int(pack_group)],
                shopping?Text("购入后收入共享仓库"):Text("选中物品可查看、分配或出售"));
        std::array<Item,ItemCount> entries{};
        int count=0;
        const auto& equipped=game.s.heroes[hero_id].equipment;
        const Item worn=pack_group==ItemGroup::Consumable?Item::None:equipped[int(pack_group)];
        for (int n=1;n<=ItemCount;++n) {
            const auto candidate=static_cast<Item>(n);
            if (shopping?game.sells(shop_id,candidate):
                        item_type(candidate).group==pack_group && (game.owned(candidate)||worn==candidate))
                entries[count++]=candidate;
        }
        const int per=row_count();
        page=std::min(page,std::max(0,(count+per-1)/per-1));
        for (int i=0;i<per && page*per+i<count;++i) {
            const Item option=entries[page*per+i];
            const auto& type=item_type(option);
            const auto detail=shopping?(game.s.rank<type.rank?fmt("%d星解锁",type.rank):
                game.s.buildings[shop_id].level<stock_level(option)?fmt("铺面升至%d级",stock_level(option)):
                type.relics?fmt("%d金 %d遗物",type.price,type.relics):fmt("%d金",type.price)):
                (option==worn?fmt("已装备 · %d件",game.owned(option)):fmt("%d件",game.owned(option)));
            list(d,i,type.name,detail,ItemOption+item_index(option),200+item_index(option),!shopping && option==worn);
        }
        if (!count) d.text(card.x+12,card.y+54,"仓库暂无这类物品，可逛铺面或进矿洞。",Quiet,12,card.w-24);
        pagination(d,count,per,count?Text{}:Text("暂无物品"));
    }
    void item_detail(Draw& d) {
        const auto& type=item_type(item);
        const bool shopping=item_source==Screen::Stock;
        const bool consumable=type.group==ItemGroup::Consumable;
        const bool worn=!consumable && game.s.heroes[hero_id].equipment[int(type.group)]==item;
        heading(d,type.name,fmt("仓库库存 %d 件",game.owned(item)));
        item_icon(d,item,{card.x+19,card.y+53},true);
        const int x=card.x+68, width=card.right()-x-8;
        if (consumable) {
            const char* effect=item==Item::LeafPotion?"恢复生命":item==Item::BlueTonic?"恢复体力":
                               item==Item::HoneyBun?"增加满意度":"增加经验";
            d.label({x,card.y+54,width,15},fmt("%s %d",effect,type.amount),TextInk);
            d.label({x,card.y+75,width,15},"可给在镇内的旅人使用",Quiet);
        } else {
            d.label({x,card.y+54,width,15},fmt("攻击 +%d  防护 +%d",type.attack,type.armor),TextInk);
            d.label({x,card.y+75,width,15},fmt("体力减耗 +%d",type.stamina),Quiet);
        }
        if (shopping) {
            const auto cost=type.relics?fmt("炼成 %d金 %d遗物",type.price,type.relics):fmt("购入 %d金",type.price);
            if (card.h>=145)
                d.label({x,card.y+94,width,15},fmt("需要 %d星 · 铺面%d级",type.rank,stock_level(item)),Quiet);
            split(d,"返回货架",Close,cost,game.can_buy(shop_id,item)?BuyItem:0);
            return;
        }
        const bool removable=worn && game.eligible(hero_id) && game.owned(item)<StockLimit;
        split(d,fmt("售出 %d金",type.price/3),game.owned(item) && game.s.gold<60000?SellItem:0,
              consumable?"使用":worn?"卸下":"装备",consumable?(game.can_use(hero_id,item)?UseItem:0):
              (worn?(removable?RemoveItem:0):(game.can_equip(hero_id,item)?EquipItem:0)));
    }
    void workshop(Draw& d) {
        heading(d,"炼成工坊","用仓库装备与遗物炼成新装备");
        std::array<int,std::size(Recipes)> entries{};
        int count=0;
        for (int i=0;i<int(std::size(Recipes));++i)
            if (game.offers_recipe(shop_id,i)) entries[count++]=i;
        const int per=row_count();
        page=std::min(page,std::max(0,(count+per-1)/per-1));
        for (int i=0;i<per && page*per+i<count;++i) {
            const int index=entries[page*per+i];
            const auto& r=Recipes[index];
            const auto requirement=game.s.rank<r.rank?fmt("%d星解锁",r.rank):
                game.s.buildings[shop_id].level<r.level?fmt("铺面需%d级",r.level):
                fmt("%d金 · %d遗物",r.gold,r.relics);
            list(d,i,item_type(r.output).name,requirement,RecipeOption+index,200+item_index(r.output));
        }
        pagination(d,count,per);
    }
    void recipe_detail(Draw& d) {
        const auto& r=Recipes[recipe_id];
        const int x_offset=37;
        heading(d,fmt("炼成 · %s",item_type(r.output).name),
                fmt("需 %d星 · 铺面 %d级",r.rank,r.level));
        item_icon(d,r.output,{card.x+12,card.y+53});
        const int x=card.x+x_offset, y=card.y+49, w=card.w-x_offset-8;
        const bool ready=game.can_refine(shop_id,recipe_id);
        d.label({x,y,w,15},fmt("%s ×1  仓库 %d",item_type(r.input).name,game.owned(r.input)),TextInk);
        d.label({x,y+18,w,15},fmt("金币 %d  遗物 %d / %d",r.gold,game.s.relics,r.relics),TextInk);
        const Text status=ready?"炼成后收入共享仓库":
            game.s.rank<r.rank || game.s.buildings[shop_id].level<r.level?"尚未解锁":
            !game.owned(r.input)?"仓库缺少原装备":game.s.relics<r.relics?"遗物不足":
            game.s.gold<r.gold?"金币不足":"成品库存已满";
        d.label({x,y+36,w,15},status,ready?Mint:Rust);
        split(d,"返回配方",Close,"炼成",ready?Refine:0);
    }
    void quests(Draw& d) {
        if (game.s.trip.phase!=Idle) {battle(d); return;}
        const bool deeper=cave_route==Route::Depths;
        const auto progress=fmt("险境已攻破 %d/6",std::popcount(game.s.depth_cleared));
        heading(d,deeper?"山脊裂隙 · 险境":"回声矿洞");
        d.label({card.x+9,card.y+32,card.w-91,16},
                deeper?Text(progress):Text("六层遗迹等你寻宝"),Quiet);
        button(d,{card.right()-78,card.y+31,68,17},"南岭讨伐",OpenFrontier,game.frontier_visible());
        const int per=row_count();
        for (int i=0;i<per && page*per+i<QuestCount;++i) {
            const int floor=page*per+i;
            const auto& q=Quests[floor];
            const bool open=game.floor_open(floor,cave_route);
            const auto reward=fmt("%s%d金",deeper && !(game.s.depth_cleared&(1u<<floor))?"首破 ":"赏金 ",
                                  game.preview_reward(floor,cave_route).gold);
            const auto required=fmt("%d星解锁",q.rank);
            const Text status=!open?(deeper?Text("先通关常规"):q.rank>game.s.rank?Text(required):Text("前层未通")):
                              (!deeper && (game.s.cleared&(1u<<floor)))?Text("已寻获"):Text(reward);
            list(d,i,fmt("%dF  %s",floor+1,q.name),status,open?QuestOption+floor:0);
        }
        const auto r=actions_area();
        const bool switchable=game.s.cleared;
        const bool has_report=game.s.trip.result;
        const int pager=switchable || has_report?41:49;
        const int segments=1+int(switchable)+int(has_report);
        const int start=r.x+pager+4, total=r.w-pager*2-8;
        int segment=0;
        auto middle=[&](Text label,int action,bool primary=false,int icon=-1) {
            const int x=start+segment*total/segments;
            ++segment;
            button(d,{x,r.y,start+segment*total/segments-x-3,r.h},label,action,primary,icon);
        };
        button(d,{r.x,r.y,pager,r.h},"上页",page>0?Previous:0);
        middle(segments==1?(game.s.commission.complete()?"委托可领奖":"公会委托"):"委托",
               OpenCommissions,true,segments==1?Book:-1);
        if (switchable) middle(deeper?"常规":"险境",ToggleDepths,deeper);
        if (has_report) middle("战报",ShowExpeditionReport);
        button(d,{r.right()-pager,r.y,pager,r.h},"下页",(page+1)*per<QuestCount?Next:0);
    }
    void frontier(Draw& d) {
        const bool open=game.floor_open(0,Route::Frontier);
        const bool available=open && game.s.trip.phase==Idle;
        const auto next=fmt("第 %d 日再次出现",game.s.frontier_next_day);
        const Text status=game.s.rank<FrontierHunt.rank?"小镇达到二星后出现":
            !open?Text(next):available?"南岭林道发现首领":"已有队伍在外行动";
        heading(d,FrontierHunt.name,status);
        const auto reward=game.preview_reward(0,Route::Frontier);
        const int y=card.y+51, x=card.x+62, width=card.right()-x-9;
        d.blit(art::monsters,{0,FrontierSpecies*54,54,54},{card.x+6,y});
        d.label({x,y,width,17},fmt("%s  ·  生命 %d",FrontierHunt.monster,FrontierHunt.hp),TextInk);
        d.label({x,y+19,width,17},fmt("每 %d 回合重击",FrontierHeavyEvery),Rust);
        d.label({x,y+37,width,17},fmt("%d金  %d声望  %d遗物",reward.gold,reward.fame,reward.relics),Mint);
        d.label({x,y+55,width,17},fmt("掉落 %s",item_type(reward.loot).name),Quiet);
        split(d,"定位南岭",LocateFrontier,"召集队伍",available?PrepareFrontier:0);
    }
    static Format commission_description(Commission order) {
        switch (order.kind) {
        case CommissionKind::Survey: return fmt("赢得 %dF %s",order.target+1,Quests[order.target].name);
        case CommissionKind::Trade: return fmt("付费到访 %d 次",order.target);
        case CommissionKind::Build: return fmt("新建或扩建 %d 处",order.target);
        case CommissionKind::Hunt: return fmt("讨伐 %d 只野怪",order.target);
        default: return {};
        }
    }
    void commission_card(Draw& d,Rect r,Commission order,int accept=0) {
        const auto prize=commission_prize(order);
        if (order.complete()) d.skin(r,SelectedRow);
        else d.fill({r.x+4,r.bottom()-1,r.w-8,1},d.paper_rule());
        commission_icon(d,order.kind,{r.x+6,r.y+8});
        const int x=r.x+31, width=r.w-40-(accept?42:0);
        d.text(x,r.y+5,commission_description(order),TextInk,12,width);
        d.text(x,r.y+19,fmt("奖励 %d金  声望+%d",prize.gold,prize.fame),Mint,12,width);
        if (accept) button(d,{r.right()-45,r.y+8,39,22},"接取",accept,true);
        else {
            d.bar({x,r.y+37,r.right()-x-56,6},order.progress,order.goal());
            d.text(r.right()-48,r.y+33,fmt("%d/%d",order.progress,order.goal()),Mint);
        }
    }
    void commissions(Draw& d) {
        const Commission order=game.s.commission;
        const bool claimed=game.s.commission_reward_day==game.s.day;
        const bool active=order.kind!=CommissionKind::None;
        heading(d,"公会委托",claimed?"今日委托已结算":order.complete()?"委托完成，领取公会奖励":
                active?"进行中的委托可跨日完成":"选择一项委托，每日可结算一次");
        if (active) {
            commission_card(d,{card.x+8,card.y+48,card.w-16,53},order);
            if (order.complete()) split(d,"返回",Close,"领取奖励",ClaimCommission);
            else split(d,"放弃委托",AbandonCommission,
                       order.kind==CommissionKind::Survey?"前往寻宝":order.kind==CommissionKind::Build?"前往建造":"返回小镇",
                       FollowCommission);
        } else if (claimed) {
            d.text(card.x+12,card.y+65,"明天来看看新的委托。",Quiet);
            button(d,actions_area(),"返回",Close,true);
        } else {
            const auto offers=game.commission_offers();
            for (int i=0;i<int(offers.size());++i)
                commission_card(d,{card.x+8,card.y+48+i*39,card.w-16,37},offers[i],i==0?AcceptSurvey:AcceptTown);
        }
    }
    void party(Draw& d) {
        const bool deeper=route==Route::Depths;
        const auto reward=game.preview_reward(quest_id,route);
        const auto& target=expedition_target(quest_id,route);
        const auto title=fmt("%s · %s",target.name,item_type(reward.loot).name);
        heading(d,deeper?Text(title):route==Route::Frontier?Text(fmt("召集讨伐队 · %s",target.monster)):Text(target.name));
        const auto info=tactic_info(tactic);
        const int opener=game.opening_strike(quest_id,team,tactic,0,route);
        const int upper=game.opening_strike(quest_id,team,tactic,6,route);
        const int hp=guardian_health(quest_id,route);
        const auto& risk=Depths[quest_id];
        const auto forecast=deeper?fmt("血%d  甲%d  %d轮重击  首击%d-%d",hp,risk.armor,risk.heavy_every,opener,upper):
            route==Route::Frontier?fmt("首领 %d血  %d轮重击  首击%d-%d",hp,FrontierHeavyEvery,opener,upper):
            fmt("%s  首击 %d-%d  守卫 %d",info.hint,opener,upper,hp);
        const bool can_dispatch=game.can_launch(quest_id,team,tactic,route);
        d.label({card.x+9,card.y+30,card.w-18,17},
                !team?Text("选择一至三位旅人"):can_dispatch?Text(forecast):Text("有旅人需休整或调整战术"),
                team && !can_dispatch?Rust:Quiet);
        std::array<int,HeroCount> candidates{};
        int count=0;
        for (int i=0;i<HeroCount;++i) if (game.s.heroes[i].level) candidates[count++]=i;
        const int per=std::max(2,row_count()-1);
        for (int i=0;i<per && page*per+i<count;++i) {
            const int id=candidates[page*per+i]; const auto& h=game.s.heroes[id];
            const bool rested=game.ready_for_trip(id,tactic,route);
            const bool chosen=team&(1u<<id);
            const auto stamina=fmt("%s体力 %d/%d",game.prepared(id)?"整备  ":game.guards(id)?"护卫  ":"",
                                   h.energy,game.required_energy(id,tactic,route));
            list(d,i,fmt("%s  %s",Heroes[id].name,Heroes[id].role),
                 h.hp<25?"伤势未愈":Text(stamina),rested || chosen?PartyOption+id:0,100+id,chosen);
        }
        const auto r=actions_area();
        const bool separate_tactics=card.h>=151;
        if (separate_tactics) {
            const int w=r.w/3;
            for (int i=0;i<int(std::size(Tactics));++i) {
                const auto& option=Tactics[i];
                button(d,{r.x+i*w,r.y-27,w-3,22},fmt("%s 耗%d",option.name,tactic_energy(static_cast<Tactic>(i),route)),
                       PlanBalanced+i,unsigned(tactic)==unsigned(i));
            }
        }
        const int pager=separate_tactics?41:33;
        const int plan_width=separate_tactics?0:(r.w-pager*2)/2;
        button(d,{r.x,r.y,pager,r.h},"上页",page>0?Previous:0);
        button(d,{r.x+pager+5,r.y,pager,r.h},"下页",(page+1)*per<count?Next:0);
        if (!separate_tactics)
            button(d,{r.x+pager*2+9,r.y,plan_width-5,r.h},info.name,CyclePlan);
        const int dispatch_x=pager*2+plan_width+12;
        button(d,{r.x+dispatch_x,r.y,r.w-dispatch_x,r.h},fmt("出发  %d/3",std::popcount(team)),
               can_dispatch?Dispatch:0,true,separate_tactics?Compass:-1);
    }
    void damage_number(Draw& d,Point p,int loss,unsigned time,unsigned impact) {
        if (!loss || time<impact || time>=impact+155) return;
        const int y=p.y-int((time-impact)/25);
        const auto label=fmt("-%d",loss);
        d.center({p.x-15,y+1,30,13},label,DeepShade,10);
        d.center({p.x-15,y,30,13},label,rgb(0xffd188),10);
    }
    void cave_trail(Draw& d) {
        const auto& t=game.s.trip;
        const int base=cave_length(t.floor,t.route), length=cave_distance(t);
        const bool inside_cave=t.phase==Exploring;
        const bool fork=waiting_at_fork(t);
        const bool side=in_side_cave(t);
        const bool known_side_cache=(game.s.side_caches[unsigned(t.route)]&(1u<<t.floor))!=0;
        const bool can_detour=fork && game.can_choose_cave_path(CavePath::Side);
        const bool cache=inside_cave && showing_cache(t);
        const bool opened_this_trip=side?t.side_cache_found:t.cache_found;
        const int species=patrol_species(t);
        const auto encounter=fmt("%s挡路 · %d/%d",Quests[species].monster,t.patrol_hp,current_patrol_health(t));
        const auto progress=fmt("%s探索 · 已走 %d%%",side?"侧洞":"矿道",int(t.distance)*100/length);
        const Text status=fork?"队伍在岔路等你决定":!speed?"经营已暂停，点右上角继续":!inside_cave?"队伍正赶往洞口":
            t.patrol_hp?Text(encounter):cache?(opened_this_trip?"已开启宝箱":"此处宝箱已开启"):Text(progress);
        heading(d,fmt("%dF  %s · %s",t.floor+1,Quests[t.floor].name,
                      fork?"岔路":side?"侧洞":t.route==Route::Depths?"险境":"矿道"),status);
        const Rect stage{card.x+8,card.y+49,card.w-16,card.h-77};
        const auto previous=d.clip;
        d.clip=stage;
        d.fill(stage,Ink);
        const auto interpolation=simulation_interpolation();
        const int pan=3*t.distance+(inside_cave && !t.patrol_hp && !fork?int(interpolation*3/SimulationBeatMs):0);
        const int height=std::min(116,stage.h);
        const int scene=side?(t.floor+1)%QuestCount:t.floor;
        const Rect source{scene%2*320,scene/2*116+116-height,320,height};
        for (int x=stage.x-pan%320, chunk=pan/320;x<stage.right();x+=320,++chunk)
            d.blit(art::caves,source,{x,stage.bottom()-height},bool(chunk%2));
        const int ground=stage.bottom()-17;
        const bool large=stage.h>=70;
        const int stride=std::clamp((stage.w-95)/3,21,38);
        int party_index=0;
        for (int id=0;id<HeroCount;++id) if (t.team&(1u<<id)) {
            const int x=stage.x+24+stride*party_index++;
            const auto& hero=game.s.heroes[id];
            const Point feet{x,ground};
            if (t.patrol_hp) {
                const int pose=!hero.hp?0:t.patrol_timer<3?0:t.patrol_timer<8?1:2;
                combat_hero(d,hero,id,pose,feet,large);
            } else {
                const auto& walker=game.walkers[id];
                const int frame=fork?-1:inside_cave?int((t.distance*SimulationBeatMs+interpolation)/125)%4:
                                            walking_phase(walker,walking_elapsed(walker,interpolation));
                if (frame>=0 && hero.hp) adventurer_sprite(d,art::walking,walk_frame(id,frame),feet,large);
                else hero_sprite(d,id,feet,large);
                travel_outfit(d,hero,feet+Point{large?2:1,large?-20:-11},large);
            }
            d.bar({x-9,ground+1,18,4},hero.hp,100);
        }
        const int encounter_x=stage.right()-35;
        if (t.patrol_hp) {
            const int pose=t.patrol_timer>=7?3:0;
            d.blit(art::monsters,{pose*54,species*54,54,54},{encounter_x-27,ground-54});
            d.bar({encounter_x-17,ground+1,34,4},t.patrol_hp,current_patrol_health(t),true);
        } else if (cache) {
            d.blit_double(art::icons,{int(Chest)*18,36,16,16},{encounter_x-16,ground-32});
        }
        const Rect trail{stage.x+9,stage.bottom()-14,stage.w-18,14};
        d.shade(trail,DeepShade,12);
        d.fill({trail.x+5,trail.y+7,trail.w-10,1},Quiet);
        const auto marker=[&](int distance,Icon mark) {
            const int x=trail.x+5+(trail.w-10)*distance/length;
            icon(d,mark,x-5,trail.y+2);
        };
        marker(base/4,Compass);
        marker(base/2,Chest);
        marker(cave_fork(t),Map);
        if (second_patrol(t.floor,t.route)) marker(3*base/4,Compass);
        if (t.cave_path==CavePath::Side) {
            marker(side_patrol(t),Compass);
            marker(side_cache(t),Chest);
        }
        marker(length,Star);
        marker(t.distance,People);
        if (fork) {
            d.shade({stage.x,stage.y,stage.w,37},DeepShade,13);
            d.label({stage.x+8,stage.y+4,stage.w-16,14},"主路直达守卫，侧洞多一场战斗",Paper);
            const auto reward=fmt("%d遗物＋%s",cave_cache_relics(t.route,true),t.route==Route::Depths?"笔记":"补剂");
            const auto side_hint=fmt("每人耗 %d 体力 · %s",Game::SideCaveCost,known_side_cache?"宝箱已取":reward.bytes);
            d.label({stage.x+8,stage.y+20,stage.w-16,14},can_detour?Text(side_hint):Text("体力不足，可沿主路继续"),Brass);
        }
        d.clip=previous;
        const auto r=actions_area();
        const int third=r.w/3;
        if (fork) {
            button(d,{r.x,r.y,third-3,r.h},"走主路",TakeMainPath,true);
            button(d,{r.x+third,r.y,third-3,r.h},"探侧洞",can_detour?ExploreSideCave:0);
            button(d,{r.x+2*third,r.y,r.w-2*third,r.h},"撤回",Retreat);
            return;
        }
        button(d,{r.x,r.y,third-3,r.h},t.supplied?Text("已补给"):Text(fmt("补给 %d金",Game::SupplyCost)),
               game.can_supply()?UseSupply:0);
        button(d,{r.x+third,r.y,third-3,r.h},"撤回",inside_cave?Retreat:0);
        button(d,{r.x+2*third,r.y,r.w-2*third,r.h},"回小镇",Close,true);
    }
    void frontier_march(Draw& d) {
        int last_member=-1, remaining=-1;
        for (int id=0;id<HeroCount;++id) {
            if (!(game.s.trip.team&(1u<<id))) continue;
            const auto& walker=game.walkers[id];
            const int left=walker.length-walker.next+int(walker.walk_beats>0);
            if (left>remaining) {last_member=id; remaining=left;}
        }
        heading(d,"讨伐队前往南岭",!speed?Text("经营已暂停，点右上角继续"):
                Text(fmt("距集结处还有 %d 格",std::max(0,remaining))));
        const Rect stage{card.x+8,card.y+49,card.w-16,card.h-77};
        const auto interpolation=simulation_interpolation();
        const Point focus=last_member<0?tile_center(game,FrontierGround):walker_feet(game,last_member,interpolation);
        draw_world(d,game,stage,focus-Point{0,16},paint_time,interpolation,{-1,-1},false,Road);
        button(d,actions_area(),"回小镇",Close,true);
    }
    void battle(Draw& d) {
        const auto& t=game.s.trip;
        const bool outdoors=t.route==Route::Frontier;
        if (outdoors && t.phase==Outbound) {frontier_march(d); return;}
        if (t.phase==Outbound || t.phase==Exploring) {cave_trail(d); return;}
        const auto& q=expedition_target(t.floor,t.route);
        const unsigned interpolation=simulation_interpolation();
        const unsigned elapsed=combat_animation.age+interpolation;
        const bool in_combat=t.phase==Fighting || combat_animation.finishing(t,elapsed);
        const int guardian_hp=combat_animation.guardian_hp(t,elapsed);
        const int maximum=guardian_health(t.floor,t.route);
        const bool deeper=t.route==Route::Depths;
        const auto fighting=fmt("第 %d 回合  %s %d/%d%s",std::max(1,int(t.round)),outdoors?"首领":"守卫",guardian_hp,maximum,
                                !t.result && heavy_round(t.floor,t.route,t.round+1)?"  下轮重击":"");
        const Text status=t.phase==Idle?(t.result==1?"远征成功，奖励已入账":"队伍已返回小镇"):
                          speed==0?"经营已暂停，点右上角继续":in_combat?Text(fighting):
                          t.result==1?(outdoors?"首领已击败，队伍正在返程":"寻宝成功，队伍正在返程"):
                          "队伍撤回镇里休整";
        heading(d,outdoors?fmt("%s · %s",q.name,q.monster):
                fmt("%dF  %s · %s",t.floor+1,q.name,deeper?"险境":tactic_info(t.tactic).name),status);
        const Rect stage{card.x+8,card.y+49,card.w-16,card.h-77};
        const auto old=d.clip;
        const int floor=stage.bottom()-7;
        const int enemyX=outdoors?stage.center().x+std::min(68,stage.w/3):stage.right()-34;
        if (outdoors) {
            const auto focus=tile_center(game,FrontierGround)+stage.center()-Point{enemyX,floor};
            draw_world(d,game,stage,focus,paint_time,simulation_interpolation(),{-1,-1},false,Road,true);
        }
        d.clip=stage;
        if (!outdoors) {
            d.fill(stage,Ink);
            d.crop(art::caves,{t.floor%2*320,t.floor/2*116,320,116},stage,true);
            effect(d,Torch,int(paint_time/112%4),{stage.x+stage.w/2,stage.y+36});
        }
        int i=0;
        const int attack=combat_animation.ally_pose(elapsed);
        const int counter=combat_animation.guardian_pose(elapsed);
        const bool large=stage.h>=60;
        Point hit_anchor{};
        for (int id=0;id<HeroCount;++id) if (t.team&(1u<<id)) {
            const Point on_road=outdoors?tile_center(game,frontier_party_tile(t.team,id))-
                                         tile_center(game,FrontierGround):Point{};
            const int x=outdoors?enemyX+on_road.x:stage.x+27+std::clamp((stage.w-100)/3,23,42)*i++;
            const int feet=outdoors?floor+on_road.y:floor;
            const auto& hero=game.s.heroes[id];
            const int hp=in_combat?combat_animation.ally_hp(hero,id,elapsed):hero.hp;
            const bool alive=hp>0;
            if (in_combat) {
                const int pose=alive?attack:0;
                const bool hit=id==combat_animation.target && elapsed>=CombatAnimation::Counter && elapsed<365;
                combat_hero(d,hero,id,pose,{x+(pose==2?4:hit?-3:0),feet-2},large);
                if (id==combat_animation.target) hit_anchor={x,feet};
            } else {
                const auto& walker=game.walkers[id];
                const int step=alive && t.phase!=Idle?
                    walking_phase(walker,walking_elapsed(walker,interpolation)):-1;
                if (step>=0)
                    adventurer_sprite(d,art::walking,walk_frame(id,step),{x,feet-2},large,t.phase==Returning);
                else hero_sprite(d,id,{x,feet-2},large);
            }
            if (!in_combat) {
                const bool returning=t.phase==Returning;
                const int side=returning?-1:1;
                travel_outfit(d,hero,{x+side*(large?2:1),feet-(large?20:11)},large,false,returning);
            }
            d.bar({x-10,feet+1,20,4},hp,100);
        }
        if (in_combat || (!outdoors && t.result!=1)) {
            const int enemyFrame=in_combat?counter:0;
            const int recoil=enemyFrame==1?2:enemyFrame==3?-4:0;
            const int species=outdoors?FrontierSpecies:t.floor;
            d.blit(art::monsters,{enemyFrame*54,species*54,54,54},{enemyX-27+recoil,floor-54});
            d.bar({enemyX-17,floor+1,34,4},guardian_hp,maximum,true);
            if (in_combat)
                damage_number(d,{enemyX,stage.y+7},combat_animation.guardian_loss,elapsed,CombatAnimation::Strike);
        } else if (!outdoors) {
            relic(d,t.floor,{enemyX-17,floor-35,34,32});
        }
        if (hit_anchor.x)
            damage_number(d,{hit_anchor.x,std::max(stage.y+7,hit_anchor.y-(large?52:34))},
                          combat_animation.ally_loss,elapsed,CombatAnimation::Counter);
        d.clip=old;
        const auto r=actions_area();
        if (t.phase==Fighting) {
            const int close=std::min(92,r.w/3),leave=49,supply=r.w-close-leave-8;
            button(d,{r.x,r.y,supply,r.h},t.supplied?Text("已补给"):Text(fmt("补给 %d金",Game::SupplyCost)),
                   game.can_supply()?UseSupply:0,false,Medicine);
            button(d,{r.x+supply+4,r.y,leave,r.h},"撤回",Retreat);
            button(d,{r.right()-close,r.y,close,r.h},"回小镇",Close,true);
            return;
        }
        const bool show_loot=t.result==1 && valid_item(t.loot) && !in_combat;
        const int exit_width=r.w>=260?92:76, report_width=51;
        const int loot_width=r.w-exit_width-report_width-9;
        if (show_loot) {
            item_icon(d,t.loot,{r.x+2,r.y+2});
            d.label({r.x+24,r.y,loot_width-24,r.h},fmt("%s%s",r.w>=260?"入库 · ":"",item_type(t.loot).name),Mint);
        } else if (!in_combat) {
            d.text(r.x+3,r.y+5,t.result==1 && !outdoors?Text(Treasures[t.floor]):Text(q.monster),
                   t.result==1?Mint:Quiet,12,loot_width);
        }
        if (t.result && !in_combat)
            button(d,{r.right()-exit_width-report_width-4,r.y,report_width,r.h},"战报",ShowExpeditionReport);
        button(d,{r.right()-exit_width,r.y,exit_width,r.h},t.phase==Idle?(outdoors?"回南岭":"回矿洞"):"回到小镇",Close,true);
    }
    void expedition_report(Draw& d) {
        const auto& t=game.s.trip;
        const bool outdoors=t.route==Route::Frontier;
        const auto& q=expedition_target(t.floor,t.route);
        heading(d,t.result==1?"远征告捷":"远征撤回",outdoors?fmt("%s · %s · %d轮",q.name,q.monster,t.round):
                fmt("%dF %s · %s · %d轮",t.floor+1,q.name,t.route==Route::Depths?"险境":"常规",t.round));
        const int x=card.x+12, width=card.w-24, y=card.y+49;
        if (t.result==1 && t.reward_xp) {
            d.label({x,y,width,18},fmt("金币 +%d    声望 +%d",t.reward_gold,t.reward_fame),Mint);
            d.label({x,y+23,width,18},fmt("%s遗物 +%d  每位经验 +%d",outdoors?"首领":"守卫",t.reward_relics,t.reward_xp),TextInk);
        } else if (t.result==1) {
            d.label({x,y,width,18},"旧记录仅保留远征结果和入库物品",Quiet);
        } else {
            d.label({x,y,width,18},outdoors?"队伍撤回，未击败首领。":"队伍撤回，未击败守卫。",Quiet);
        }
        if (valid_item(t.loot)) {
            item_icon(d,t.loot,{x,y+47});
            d.label({x+24,y+45,width-24,20},fmt("入库 · %s",item_type(t.loot).name),Mint);
        } else if (t.result==1 && t.reward_xp) {
            d.label({x,y+45,width,20},"仓库已满，战利品已折入金币。",Quiet);
        }
        if (t.cache_found || t.side_cache_found)
            d.label({x,y+(t.result==1?65:24),width,15},
                    t.side_cache_found?(t.cache_found?"途中与侧洞宝箱已开启":"侧洞宝箱已开启"):"途中宝箱已开启",Mint);
        const auto r=actions_area();
        button(d,r,"返回",Close,true);
    }
    void ledger(Draw& d) {
        heading(d,fmt("%d星 · %s",game.s.rank,Ranks[game.s.rank-1].name));
        int y=card.y+34, x=card.x+10;
        d.label({x,y,card.w-20,15},fmt("昨收 %d  日租 %d  遗物 %d",game.s.yesterday,game.daily_rent(),game.s.relics),Mint);
        button(d,{x,y+16,card.w-20,20},game.can_promote()?"发展指引 · 可晋级":"发展指引",OpenTownPlan,false,Compass);
        const int half=(card.w-22)/2;
        button(d,{x,y+39,half,22},game.s.festival?"活动进行中":"镇上活动",OpenTownEvents,false,People);
        button(d,{x+half+3,y+39,half,22},fmt("炼成护符 %d/8",game.s.charms),
               game.s.gold>=28 && game.s.relics>=4 && game.s.charms<8 &&
               std::any_of(game.s.buildings.begin(),game.s.buildings.begin()+game.s.count,
                           [](const Building& b){return b.kind==Shrine;})?Craft:0);
        const auto r=actions_area();
        const int w=r.w/3;
        button(d,{r.x,r.y,w-3,r.h},"秘宝图鉴",OpenTreasures,true,Gem);
        button(d,{r.x+w,r.y,w-3,r.h},"镇志",OpenJournal,false,Book);
        button(d,{r.x+w*2,r.y,r.w-w*2,r.h},"晋升",game.can_promote()?Promote:0,true,Star);
    }
    void town_events(Draw& d) {
        const bool recent=valid_event(game.s.event);
        const auto summary=recent?fmt("%s%s · 到场 %d 人",game.s.festival?"正在举办 ":"上次 ",
                                      event_info(game.s.event).name,std::popcount(game.s.event_attended)):Format{};
        heading(d,"镇上活动",recent?Text(summary):Text("在广场举办活动，旅人会自行赴约"));
        const int per=row_count(), count=int(std::size(TownEvents));
        for (int row=0;row<per && page*per+row<count;++row) {
            const int index=page*per+row;
            const auto& info=TownEvents[index];
            const bool active=game.s.festival && unsigned(game.s.event)==unsigned(index+1);
            const auto cost=fmt("%d金",info.gold), lock=fmt("%d星解锁",info.rank);
            list(d,row,info.name,active?Text("进行中"):game.s.rank<info.rank?Text(lock):Text(cost),
                 TownEventOption+index,-1,active);
        }
        if (per<count) pagination(d,count,per);
        else button(d,actions_area(),"定位广场",LocateSquare,false,Compass);
    }
    void town_event_detail(Draw& d) {
        const auto& info=event_info(town_event);
        const bool active=game.s.festival && game.s.event==town_event;
        const auto guests=std::popcount(game.s.event_attended);
        const auto attendance=active?fmt("第 %d 日结束 · 到场 %d 人",
                                         std::min(9999,int(game.s.day)+game.s.festival-1),guests):
                                      fmt("上次活动 · 到场 %d 人",guests);
        const auto rank=fmt("小镇达到 %d 星解锁",info.rank);
        const Text status=game.s.rank<info.rank?Text(rank):active?Text(attendance):
            game.s.festival?"已有活动在广场举行":game.s.gold<info.gold?"金币不足":
            game.s.event==town_event?Text(attendance):"活动持续到次日结束";
        heading(d,info.name,status);
        const int x=card.x+12,y=card.y+51,w=card.w-24;
        if (town_event==TownEvent::Lanterns) {
            d.label({x,y,w,16},fmt("开场时获得 %d 声望",LanternFame),Mint);
            d.label({x,y+18,w,16},fmt("全镇旅人满意度 +%d",LanternMood),TextInk);
            d.label({x,y+36,w,16},"日间空闲的旅人会来广场赏灯",Quiet);
        } else if (town_event==TownEvent::Drill) {
            d.label({x,y,w,16},fmt("到场后经验 +%d  专精熟练 +%d",DrillExperience,DrillPractice),Mint);
            d.label({x,y+18,w,16},fmt("每人练一次，消耗 %d 体力",DrillEnergy),TextInk);
            d.label({x,y+36,w,16},"空闲且体力充足的旅人可参加",Quiet);
        } else {
            d.label({x,y,w,16},fmt("到场后体力 +%d  满意度 +%d",FeastEnergy,FeastMood),Mint);
            d.label({x,y+18,w,16},fmt("下次出征最多少耗 %d 体力",PreparedDiscount),TextInk);
            d.label({x,y+36,w,16},"整备效果保留到出征时使用",Quiet);
        }
        if (card.h>=156 && game.s.event==town_event && guests) {
            d.label({x,y+54,46,22},"已到场",Quiet,10);
            int position=0;
            const int gap=std::min(20,(w-49)/HeroCount);
            for (int id=0;id<HeroCount;++id)
                if (game.s.event_attended&(1u<<id))
                    hero_sprite(d,id,{x+52+gap*position++,y+76});
        }
        split(d,"定位广场",LocateSquare,active?"活动进行中":Text(fmt("举办 %d金",info.gold)),
              game.can_host_event(town_event)?HostTownEvent:0);
    }
    void town_plan(Draw& d) {
        const int rank=game.s.rank;
        const auto goal_name=fmt("下一目标 · %d星 %s",rank+1,Ranks[std::min(rank,4)].name);
        heading(d,"发展指引",rank==5?Text("已达五星，可继续挑战山脊险境"):Text(goal_name));
        const bool has_house=std::any_of(game.s.buildings.begin(),game.s.buildings.begin()+game.s.count,
                                        [](const Building& b){return b.kind==House;});
        if (rank==5) {
            split(d,has_house?"安排住处":"建造民居",has_house?OpenRoster:BuildOption+int(House),"查看寻宝",OpenQuests);
            return;
        }
        const auto& goal=Ranks[rank];
        const int amounts[]{game.s.fame,game.hero_count(),std::popcount(game.s.cleared)};
        const int targets[]{goal.fame,goal.people,goal.floors};
        constexpr const char* names[]{"声望","旅人","秘宝"};
        constexpr const char* moves[]{"查看委托","招募旅人","前往组队"};
        constexpr int actions[]{OpenCommissions,OpenRoster,PlanExpedition};
        const int per=row_count();
        for (int i=0;i<per && page*per+i<3;++i) {
            const int item=page*per+i;
            const bool met=amounts[item]>=targets[item];
            list(d,i,fmt("%s %d / %d",names[item],amounts[item],targets[item]),
                 met?"已达成":moves[item],met?0:actions[item],-1,met);
        }
        if (per<3) pagination(d,3,per);
        else split(d,has_house?"安排住处":"建造民居",has_house?OpenRoster:BuildOption+int(House),
                   game.can_promote()?"晋升城镇":"达标后晋升",game.can_promote()?Promote:0);
    }
    void treasures(Draw& d) {
        const int i=std::clamp(page,0,QuestCount-1);
        const bool found=game.s.cleared&(1u<<i);
        heading(d,"秘宝图鉴",fmt("已寻回 %d / %d",std::popcount(game.s.cleared),QuestCount));
        const Rect plate{card.x+10,card.y+50,37,38};
        d.skin(plate,found?TealButton:InactiveButton);
        if (found) relic(d,i,plate.inset(5));
        else icon(d,Chest,plate.x+12,plate.y+13,14);
        d.text(plate.right()+9,plate.y,found?Treasures[i]:"未寻获",found?Mint:Quiet,16);
        d.text(plate.right()+9,plate.y+20,fmt("%dF  %s",i+1,Quests[i].name),Quiet);
        if (card.h>=147)
            d.text(card.x+10,card.y+92,found?TreasureLore[i]:"击败遗迹守卫后，秘宝会登记在此。",TextInk,12,card.w-20);
        pagination(d,QuestCount,1);
    }
    void journal(Draw& d) {
        heading(d,"风石镇志","最近发生的事");
        for (int i=0;i<row_count();++i) {
            if (game.journal[i].message.empty()) break;
            const auto& n=game.journal[i];
            list(d,i,n.message,n.value?Text(fmt("+%d",n.value)):Text{},0);
        }
        split(d,"玩法",OpenHelp,"返回镇务",OpenLedger);
    }
    void shop(Draw& d) {
        const auto& b=game.s.buildings[shop_id];
        const auto& type=Types[b.kind];
        const bool house=b.kind==House;
        const int cost=game.upgrade_cost(shop_id);
        const auto upgrade=fmt("扩建 %d金",cost);
        const Text upgrade_label=cost?Text(upgrade):Text("已满级");
        const int upgrade_action=cost && game.s.gold>=cost?Upgrade:0;
        heading(d,house?Text(fmt("灯窗民居 %d",home_number(shop_id))):Text(type.name));
        building_sprite(d,Kind(b.kind),{card.x+37,card.y+99});
        const int x=card.x+67, max=card.right()-x-7;
        if (house) {
            Format names;
            const int rooms=game.occupants(shop_id);
            for (int i=0;i<HeroCount;++i)
                if (game.s.heroes[i].level && game.s.heroes[i].home==shop_id)
                    names=fmt("%s%s%s",names.bytes,names.bytes[0]?"、":"",Heroes[i].name);
            d.label({x,card.y+43,max,15},rooms?Text(fmt("住户 %s",names.bytes)):Text("空置，可分配一位旅人"),TextInk);
            d.label({x,card.y+64,max,15},fmt("民居等级 %d / 3  日租 %d",b.level,rooms*room_rent(b.level)),Mint);
            d.label({x,card.y+83,max,15},"扩建提升休整效果",Quiet);
            split(d,rooms?"已入住":"分配住户",rooms?0:OpenResidents,upgrade_label,upgrade_action);
            return;
        }
        d.text(x,card.y+43,type.description,TextInk,12,max);
        if (paid_service(b.kind)) {
            const int price=service_price(b,b.account.policy);
            d.label({x,card.y+63,max,15},fmt("铺面 %d级  到访 %d金",b.level,price),Mint);
            button(d,{x,std::min(card.y+82,actions_area().y-22),max,20},"经营账本",OpenBusiness,false,Book);
        } else {
            d.text(x,card.y+64,fmt("铺面等级 %d / 3",b.level),Mint,12,max);
            d.label({x,card.y+83,max,15},"旅人会自行到访。",Quiet);
        }
        const bool has_stock=std::any_of(std::begin(Items),std::end(Items),
                                         [&](const ItemType& item){return item.vendor==b.kind;});
        const bool can_work=std::any_of(std::begin(Recipes),std::end(Recipes),
                                       [&](const Recipe& recipe){return recipe.workshop==b.kind;});
        if (can_work) {
            const auto r=actions_area();
            const int w=r.w/3;
            button(d,{r.x,r.y,w-3,r.h},"货架",OpenStock);
            button(d,{r.x+w,r.y,w-3,r.h},"炼成工坊",OpenWorkshop);
            button(d,{r.x+2*w,r.y,r.w-2*w,r.h},upgrade_label,upgrade_action,true);
        } else split(d,has_stock?"查看货架":"回到小镇",has_stock?OpenStock:Close,upgrade_label,upgrade_action);
    }
    void business(Draw& d) {
        const auto& b=game.s.buildings[shop_id];
        const auto& account=b.account;
        heading(d,fmt("%s · 经营",Types[b.kind].name));
        const int half=card.w/2;
        d.label({card.x+9,card.y+32,half-12,15},
                fmt("今 %d次  %d金",account.today.visits,account.today.revenue),Mint);
        d.label({card.x+half+4,card.y+32,card.w-half-13,15},
                fmt("昨 %d次  %d金",account.yesterday.visits,account.yesterday.revenue),Quiet);
        const int per=row_count(), count=int(std::size(ServicePlans));
        for (int row=0;row<per && page*per+row<count;++row) {
            const int index=page*per+row;
            const auto policy=static_cast<ServicePolicy>(index);
            const auto& terms=ServicePlans[index];
            const bool selected=policy==account.policy;
            const auto label=policy==ServicePolicy::Value?fmt("%s · 满意+%d",terms.name,terms.mood):
                policy==ServicePolicy::Premium?(b.kind==Forge?fmt("%s · 强化上限+%d",terms.name,terms.forge):
                    fmt("%s · 恢复+%d",terms.name,terms.recovery)):fmt("%s",terms.name);
            list(d,row,label,fmt("%d金/次",service_price(b,policy)),
                 selected?0:PolicyOption+index,-1,selected);
        }
        if (per<count) pagination(d,count,per);
        else d.center(actions_area(),"旅人到访时按当前价格支付",Quiet);
    }
    void lesson(Draw& d) {
        const bool guide=screen==Screen::Tutorial;
        heading(d,guide?"抵达风石镇":"经营手册");
        const char* lines[]{
            "先在道路旁的同高空地建一间铁匠铺。",
            "建好民居后，点房屋给旅人安排住处。",
            "点「寻宝」组队探洞，首通奖励秘宝。",
            "点「镇务」查看晋级目标，解锁新设施。"
        };
        if (!guide) {
            lines[0]="拖动看地图，点建筑查看或扩建。";
            lines[1]="点铺面查看货架，装备在旅人页分配。";
            lines[2]="建好民居后，点房屋给旅人安排住处。";
            lines[3]="每日可练一次。守卫和骑士会替队友挡反击。";
        }
        for (int i=0;i<4;++i)
            d.text(card.x+10,card.y+36+i*17,lines[i],i==0?Mint:TextInk,12,card.w-20);
        const auto r=actions_area();
        button(d,{r.x,r.y,r.w,r.h},guide?"开始经营":"返回",Close,true,guide?Play:-1);
    }
    void victory(Draw& d) {
        heading(d,"风石谷的新名城");
        const int x=card.x+11,y=card.y+39;
        d.text(x,y,"六件秘宝已安放，风石镇获评五星。",Mint,12,card.w-22);
        d.text(x,y+21,fmt("第 %d 日  ·  %d 位旅人  ·  %d 次到访",
                          game.s.day,game.hero_count(),game.s.visits),TextInk,12,card.w-22);
        d.text(x,y+42,"山谷还会迎来新的旅人。",Mint);
        split(d,"秘宝图鉴",OpenTreasures,"继续经营",Close);
    }
    void render(Draw& d) {
        layout(d);
        button_count=0;
        if (screen==Screen::Title) {title(d); focus=std::clamp(focus,0,std::max(0,button_count-1)); return;}
        const Rect all=d.clip;
        if (screen==Screen::Help && help_back==Screen::Title) {
            d.fill(all,Ink);
        } else {
            if (screen==Screen::Town && followed>=0) track_camera();
            draw_world(d,game,viewport,camera,paint_time,simulation_interpolation(),
                       screen==Screen::Town && (map_focus || placing)?selection:Point{-1,-1},placing,build_kind,
                       false,&combat_animation);
        }
        hud(d);
        switch (screen) {
        case Screen::Tutorial: lesson(d); break;
        case Screen::Build: build_menu(d); break;
        case Screen::Roster: roster(d); break;
        case Screen::Hero: hero_detail(d); break;
        case Screen::Homes: homes(d); break;
        case Screen::Residents: residents(d); break;
        case Screen::Loadout: loadout(d); break;
        case Screen::Growth: growth(d); break;
        case Screen::Disciplines: disciplines(d); break;
        case Screen::Workshop: workshop(d); break;
        case Screen::Recipe: recipe_detail(d); break;
        case Screen::Pack: item_list(d,false); break;
        case Screen::Stock: item_list(d,true); break;
        case Screen::ItemDetail: item_detail(d); break;
        case Screen::ExpeditionReport: expedition_report(d); break;
        case Screen::Quests: quests(d); break;
        case Screen::Frontier: frontier(d); break;
        case Screen::Party: party(d); break;
        case Screen::Battle: battle(d); break;
        case Screen::Commissions: commissions(d); break;
        case Screen::Ledger: ledger(d); break;
        case Screen::TownEvents: town_events(d); break;
        case Screen::TownEventDetail: town_event_detail(d); break;
        case Screen::TownPlan: town_plan(d); break;
        case Screen::Treasures: treasures(d); break;
        case Screen::Journal: journal(d); break;
        case Screen::Shop: shop(d); break;
        case Screen::Business: business(d); break;
        case Screen::Help:
            if (help_back!=Screen::Title) lesson(d);
            else {
                heading(d,"经营手册");
                const char* intro[]{"建设临街铺面，让旅人消费和休息。",
                                    "组一至三人队伍，寻回矿洞六层的秘宝。",
                                    "提升声望，从驿站经营到五星名城。",
                                    "触摸操作；或用方向键、A确认、B返回。"};
                for (int i=0;i<4;++i)
                    d.text(card.x+10,card.y+37+17*i,intro[i],TextInk,12,card.w-20);
                const auto r=actions_area();
                button(d,{r.x,r.y,r.w,r.h},"返回",Close,true);
            }
            break;
        case Screen::Victory: victory(d); break;
        default: break;
        }
        if (screen==Screen::Town && followed>=0) tracking_banner(d);
        town_bar(d);
        if (screen!=Screen::Town && dock_tab()<0 && toast_until) {
            const int width=std::min(safe.w-14,Draw::measure(toast)+15);
            message(d,{safe.x+7,viewport.bottom()+10,width,18},toast);
        }
        focus=std::clamp(focus,0,std::max(0,button_count-1));
    }

    void show(Screen value) {
        screen=value; page=0; focus=0; button_count=0; dirty=true;
    }
    void inspect_hero(int id,Screen back) {
        hero_id=id;
        hero_back=back;
        show(Screen::Hero);
    }
    void begin_placement(Kind type) {
        followed=-1;
        placing=true; build_kind=type;
        selection=pad?Point{7,11}:Point{-1,-1};
        map_focus=pad;
        show(Screen::Town);
        if (pad) track_selection();
    }
    void track_selection() {
        if (!inside(selection)) selection=Square;
        const auto p=tile_center(game,selection);
        const int x=std::max(10,viewport.w/2-44);
        const int y=std::max(10,viewport.h/2-34);
        camera.x=std::clamp(camera.x,p.x-x,p.x+x);
        camera.y=std::clamp(camera.y,p.y-y,p.y+y);
        clamp_camera();
    }
    void clamp_camera() {
        camera.x=std::clamp(camera.x,grid_anchor({0,Height-3}).x,grid_anchor({Width-3,0}).x);
        camera.y=std::clamp(camera.y,25,grid_anchor({Width-3,Height-3}).y);
    }
    void close_screen() {
        switch (screen) {
        case Screen::Title: return;
        case Screen::Town:
            if (followed>=0) {followed=-1; dirty=true; return;}
            if (placing || map_focus) {
                placing=false; map_focus=false; selection={-1,-1}; dirty=true;
                return;
            }
            if (existing) schedule_save();
            show(Screen::Title); return;
        case Screen::Tutorial:
            schedule_save(); show(Screen::Town); return;
        case Screen::Hero: show(hero_back); return;
        case Screen::Roster: show(roster_back); return;
        case Screen::Quests: show(quests_back); return;
        case Screen::Frontier: show(frontier_back); return;
        case Screen::TownPlan: show(Screen::Ledger); return;
        case Screen::TownEvents: show(events_back); return;
        case Screen::TownEventDetail:
            show(Screen::TownEvents);
            return;
        case Screen::Homes: show(Screen::Hero); return;
        case Screen::Residents: show(Screen::Shop); return;
        case Screen::Loadout: case Screen::Growth: show(Screen::Hero); return;
        case Screen::Disciplines: show(Screen::Growth); return;
        case Screen::Workshop: show(Screen::Shop); return;
        case Screen::Recipe: show(Screen::Workshop); page=workshop_page; return;
        case Screen::Pack: show(Screen::Loadout); return;
        case Screen::Stock: show(Screen::Shop); return;
        case Screen::Business: show(Screen::Shop); return;
        case Screen::ItemDetail: show(item_source); page=item_list_page; return;
        case Screen::ExpeditionReport: show(report_back); return;
        case Screen::Party: show(route==Route::Frontier?Screen::Frontier:Screen::Quests); return;
        case Screen::Commissions: show(commission_back); return;
        case Screen::Treasures: case Screen::Journal: show(Screen::Ledger); return;
        case Screen::Help: show(help_back); return;
        case Screen::Battle:
            if (waiting_at_fork(game.s.trip)) show_toast("队伍将在后台沿主路继续");
            show(game.s.trip.phase!=Idle?Screen::Town:
                 game.s.trip.route==Route::Frontier?Screen::Frontier:Screen::Quests); return;
        default: show(Screen::Town); return;
        }
    }
    pxa::BackAction on_back() {
        if (screen==Screen::Title) return pxa::BackAction::close;
        close_screen();
        return pxa::BackAction::stay;
    }
    void activate(int action) {
        if (action>=TownEventOption && action<TownEventOption+int(std::size(TownEvents))) {
            town_event=static_cast<TownEvent>(action-TownEventOption+1);
            show(Screen::TownEventDetail);
            return;
        }
        if (action>=PolicyOption && action<PolicyOption+int(std::size(ServicePlans))) {
            const auto policy=static_cast<ServicePolicy>(action-PolicyOption);
            if (game.set_service_policy(shop_id,policy)) schedule_save();
            return;
        }
        if (action>=DisciplineOption && action<DisciplineOption+int(std::size(Disciplines))) {
            if (game.choose_discipline(hero_id,static_cast<Discipline>(action-DisciplineOption+1))) {
                schedule_save();
                show_toast(game.journal[0].message);
            }
            return;
        }
        if (action>=RecipeOption && action<RecipeOption+int(std::size(Recipes))) {
            recipe_id=action-RecipeOption;
            workshop_page=page;
            show(Screen::Recipe);
            return;
        }
        if (action>=ResidentOption && action<ResidentOption+HeroCount) {
            if (game.assign_home(action-ResidentOption,shop_id)) {
                schedule_save();
                show(Screen::Shop);
                show_toast(game.journal[0].message);
            }
            return;
        }
        if (action>=HomeOption && action<HomeOption+MaxBuildings) {
            if (game.assign_home(hero_id,action-HomeOption)) {
                schedule_save();
                show(Screen::Hero);
                show_toast(game.journal[0].message);
            }
            return;
        }
        if (action>=ItemOption && action<ItemOption+ItemCount) {
            item=static_cast<Item>(action-ItemOption+1);
            item_source=screen;
            item_list_page=page;
            show(Screen::ItemDetail);
            return;
        }
        if (action>=GearOption && action<GearOption+GearSlots) {
            pack_group=static_cast<ItemGroup>(action-GearOption);
            show(Screen::Pack);
            return;
        }
        if (action>=PartyOption && action<PartyOption+HeroCount) {
            const unsigned bit=1u<<(action-PartyOption);
            if (team&bit || std::popcount(team)<3) team^=bit;
            else show_toast("远征队最多三人");
            dirty=true;
            return;
        }
        if (action>=QuestOption && action<QuestOption+QuestCount) {
            quest_id=action-QuestOption;
            route=cave_route;
            tactic=Tactic::Balanced;
            team=game.suggest_team(quest_id,tactic,route);
            show(Screen::Party);
            return;
        }
        if (action>=HeroOption && action<HeroOption+HeroCount) {
            inspect_hero(action-HeroOption,Screen::Roster);
            return;
        }
        if (action>=BuildOption+int(Inn) && action<=BuildOption+int(LowerGround)) {
            begin_placement(Kind(action-BuildOption));
            return;
        }
        switch (action) {
        case Begin:
            if (ready)
                show(!existing?Screen::Tutorial:waiting_at_fork(game.s.trip)?Screen::Battle:Screen::Town);
            break;
        case Close: close_screen(); break;
        case OpenBuild: terrain_tab=false; show(Screen::Build); break;
        case ViewShops: case ViewTerrain:
            terrain_tab=action==ViewTerrain; page=0; dirty=true; break;
        case OpenRoster:
            if (screen!=Screen::Hero) roster_back=screen==Screen::TownPlan?Screen::TownPlan:Screen::Town;
            show(Screen::Roster);
            break;
        case TrackHero:
            followed=hero_id;
            map_focus=false;
            selection={-1,-1};
            show(Screen::Town);
            break;
        case StopTracking: followed=-1; dirty=true; break;
        case InspectTracked: if (followed>=0) inspect_hero(followed,Screen::Town); break;
        case OpenHomes: show(Screen::Homes); break;
        case OpenResidents: show(Screen::Residents); break;
        case OpenLoadout: show(Screen::Loadout); break;
        case OpenGrowth: show(Screen::Growth); break;
        case OpenDisciplines: show(Screen::Disciplines); break;
        case OpenWorkshop: show(Screen::Workshop); break;
        case OpenBusiness: show(Screen::Business); break;
        case Refine:
            if (game.refine(shop_id,recipe_id)) {schedule_save(); show_toast(game.journal[0].message);}
            break;
        case OpenSupplies: pack_group=ItemGroup::Consumable; show(Screen::Pack); break;
        case OpenStock: show(Screen::Stock); break;
        case BuyItem:
            if (game.buy(shop_id,item)) {schedule_save(); show_toast(game.journal[0].message);}
            break;
        case SellItem:
            if (game.sell(item)) {schedule_save(); show_toast("物品已售出");}
            break;
        case EquipItem:
            if (game.equip(hero_id,item)) {schedule_save(); show_toast("装备已更换");}
            break;
        case RemoveItem:
            if (game.unequip(hero_id,pack_group)) {schedule_save(); show_toast("装备已放回仓库");}
            break;
        case UseItem:
            if (game.use(hero_id,item)) {schedule_save(); show_toast("道具已使用");}
            break;
        case SpendArms: case SpendGrit: case SpendTravel:
            if (game.invest(hero_id,static_cast<Talent>(action-SpendArms))) {schedule_save(); dirty=true;}
            break;
        case OpenQuests:
            quests_back=screen==Screen::TownPlan?Screen::TownPlan:Screen::Town;
            show(game.s.trip.phase==Idle?Screen::Quests:Screen::Battle);
            break;
        case OpenFrontier:
            frontier_back=screen==Screen::Quests?Screen::Quests:Screen::Town;
            show(game.s.trip.phase!=Idle && game.s.trip.route==Route::Frontier?Screen::Battle:Screen::Frontier);
            break;
        case LocateFrontier:
            followed=-1;
            camera=tile_center(game,FrontierGround)-Point{0,24};
            selection=map_focus?FrontierGround:Point{-1,-1};
            show(Screen::Town);
            break;
        case PrepareFrontier:
            route=Route::Frontier;
            quest_id=0;
            tactic=Tactic::Balanced;
            team=game.suggest_team(quest_id,tactic,route);
            show(Screen::Party);
            break;
        case PlanExpedition:
            quests_back=Screen::TownPlan;
            cave_route=Route::Standard;
            if (game.s.trip.phase!=Idle) {show(Screen::Battle); break;}
            for (int floor=0;floor<QuestCount;++floor)
                if (!(game.s.cleared&(1u<<floor)) && game.floor_open(floor)) {
                    activate(QuestOption+floor);
                    return;
                }
            show(Screen::Quests);
            break;
        case ToggleDepths:
            cave_route=cave_route==Route::Depths?Route::Standard:Route::Depths;
            page=0; focus=0; dirty=true;
            break;
        case ShowExpeditionReport:
            if (game.s.trip.result) {report_back=screen; show(Screen::ExpeditionReport);}
            break;
        case OpenCommissions:
            commission_back=screen==Screen::Quests || screen==Screen::TownPlan?screen:Screen::Town;
            show(Screen::Commissions);
            break;
        case AcceptSurvey: case AcceptTown:
            if (game.accept_commission(action==AcceptSurvey?0:1)) schedule_save();
            break;
        case AbandonCommission:
            if (game.abandon_commission()) schedule_save();
            break;
        case ClaimCommission:
            if (game.claim_commission()) {schedule_save(); show(commission_back);}
            break;
        case FollowCommission:
            if (game.s.commission.kind==CommissionKind::Survey) {
                if (game.s.trip.phase==Idle) {
                    cave_route=Route::Standard;
                    activate(QuestOption+game.s.commission.target);
                }
                else show(Screen::Battle);
            } else if (game.s.commission.kind==CommissionKind::Build) activate(OpenBuild);
            else show(Screen::Town);
            break;
        case UseSupply:
            if (game.supply()) schedule_save();
            break;
        case TakeMainPath: case ExploreSideCave:
            if (game.choose_cave_path(action==TakeMainPath?CavePath::Main:CavePath::Side))
                schedule_save();
            break;
        case Retreat:
            if (game.retreat()) {combat_animation={}; schedule_save();}
            break;
        case OpenLedger: show(Screen::Ledger); break;
        case OpenTownEvents:
            events_back=screen==Screen::Ledger?Screen::Ledger:Screen::Town;
            show(Screen::TownEvents);
            break;
        case HostTownEvent:
            if (game.host_event(town_event)) {schedule_save(); dirty=true;}
            break;
        case LocateSquare:
            followed=-1;
            camera=tile_center(game,Square)-Point{0,12};
            selection=map_focus?Square:Point{-1,-1};
            show(Screen::Town);
            break;
        case OpenTownPlan: show(Screen::TownPlan); break;
        case OpenHelp: help_back=screen; show(Screen::Help); break;
        case OpenTreasures: show(Screen::Treasures); break;
        case OpenJournal: show(Screen::Journal); break;
        case Previous: --page; dirty=true; break;
        case Next: ++page; dirty=true; break;
        case Speed:
            if (speed==2) paused_interpolation=simulation_interpolation();
            if (!speed) last_step=now-paused_interpolation;
            speed=(speed+1)%3;
            show_toast(speed==0?"经营暂停":speed==1?"正常速度":"经营加速");
            break;
        case MapFocus:
            followed=-1;
            if (pad) {
                map_focus=!map_focus;
                if (map_focus) track_selection();
            } else {
                camera=camera==HomeCamera?RidgeCamera:camera==RidgeCamera?SouthernCamera:HomeCamera;
                selection={-1,-1};
                show_toast(camera==RidgeCamera?"山腰、观测营和第三层山顶":
                           camera==SouthernCamera?"南岭林道，可沿路建设":"回到谷地镇区");
            }
            dirty=true;
            break;
        case ConfirmBuild:
            if (game.build(selection,build_kind)) {
                schedule_save();
                if (build_kind<Road) {placing=false; map_focus=false;}
                show_toast(game.journal[0].message);
            }
            break;
        case Hire:
            if (game.recruit(hero_id)) {schedule_save(); show_toast(game.journal[0].message);}
            break;
        case Gift:
            if (game.gift(hero_id)) {schedule_save(); show_toast(game.journal[0].message);}
            else show_toast("这位旅人已拥有足够的装备");
            break;
        case Train:
            if (game.train(hero_id)) {schedule_save(); show_toast(game.journal[0].message);}
            break;
        case PlanBalanced: case PlanAssault: case PlanGuard:
            tactic=static_cast<Tactic>(action-PlanBalanced);
            dirty=true;
            break;
        case CyclePlan:
            tactic=static_cast<Tactic>((unsigned(tactic)+1)%std::size(Tactics));
            dirty=true;
            break;
        case Upgrade:
            if (game.upgrade(shop_id)) {schedule_save(); show_toast(game.journal[0].message);}
            break;
        case Dispatch:
            if (game.launch(quest_id,team,tactic,route)) {combat_animation={}; schedule_save(); show(Screen::Battle);}
            else show_toast("队伍体力不足，或路线尚未开放");
            break;
        case Promote:
            if (game.promote()) {
                schedule_save(); show_toast(game.journal[0].message);
                if (game.s.victory) show(Screen::Victory);
            }
            break;
        case Craft:
            if (game.craft()) {schedule_save(); show_toast(game.journal[0].message);}
            break;
        default: break;
        }
    }
    void examine(Point location,int shop=-1) {
        if (shop<0) shop=game.building(location);
        if (location==Cave || location==DepthsCave) {
            cave_route=location==DepthsCave?Route::Depths:Route::Standard;
            activate(OpenQuests);
            return;
        }
        if (location==FrontierGround && game.frontier_visible()) {activate(OpenFrontier); return;}
        if (location==Square && game.s.festival) {activate(OpenTownEvents); return;}
        if (location==GuildBoard && shop<0) {activate(OpenCommissions); return;}
        if (shop>=0) {
            if (game.s.buildings[shop].kind==Guild) activate(OpenCommissions);
            else {shop_id=shop; show(Screen::Shop);}
        } else if (inside(location)) {
            const auto tile=game.terrain(location);
            if (tile.ward>game.s.rank && (tile.ground==Grass || tile.ground==Plot))
                show_toast("晋级后可在这片台地建造");
            else if (tile.ground==Stair) show_toast("石阶连接两个高度");
            else if (tile.ground==Bridge) show_toast("这座桥通向东岸的矿洞");
            else {
                constexpr Text layers[]{"谷底空地，可以铺路或建店","第一层台地，沿路建造",
                                        "第二层台地，石阶通往更高处","第三层台地，已到最高处"};
                show_toast(layers[tile.height]);
            }
        }
    }
    void tap(Point p) {
        for (int i=0;i<button_count;++i) if (buttons[i].box.contains(p)) {
            activate(buttons[i].action); return;
        }
        if (screen!=Screen::Town || !viewport.contains(p)) return;
        const Point world=p+camera-viewport.center();
        const auto at=hit_tile(game,world);
        if (placing) {selection=at; dirty=true; return;}
        const auto frontier=tile_center(game,FrontierGround);
        if (game.frontier_visible() && Rect{frontier.x-22,frontier.y-56,44,61}.contains(world)) {
            examine(FrontierGround); return;
        }
        for (const auto entrance:{Cave,DepthsCave}) {
            const auto cave=tile_center(game,entrance);
            if (Rect{cave.x-13,cave.y-29,29,35}.contains(world)) {
                examine(entrance); return;
            }
        }
        int front=-1;
        for (int i=0;i<game.s.count;++i) {
            const auto origin=tile_center(game,game.s.buildings[i].at());
            if (Rect{origin.x-13,origin.y-25,28,31}.contains(world) &&
                (front<0 || origin.y>tile_center(game,game.s.buildings[front].at()).y))
                front=i;
        }
        const auto board=tile_center(game,GuildBoard)+Point{5,5};
        if (Rect{board.x-15,board.y-26,30,26}.contains(world) &&
            (front<0 || board.y>=tile_center(game,game.s.buildings[front].at()).y)) {
            activate(OpenCommissions);
            return;
        }
        int walker=-1, frontmost_y=-9999;
        for (int id=0;id<HeroCount;++id) {
            if (!walker_visible(game,id)) continue;
            const auto p=walker_feet(game,id,simulation_interpolation());
            if (!Rect{p.x-6,p.y-19,13,20}.contains(world) || p.y<frontmost_y) continue;
            if (front>=0) {
                const auto& building=game.s.buildings[front];
                const auto center=tile_center(game,building.at());
                const auto& w=game.walkers[id];
                const int actor_depth=std::max(w.at.x+w.at.y,w.from.x+w.from.y);
                const int building_depth=building.x+building.y;
                if ((building_depth>actor_depth || (building_depth==actor_depth && center.y>p.y)) &&
                    building_covers(Kind(building.kind),center+Point{0,7},world)) continue;
            }
            walker=id;
            frontmost_y=p.y;
        }
        if (walker>=0) {inspect_hero(walker,Screen::Town); return;}
        examine(at,front);
    }
    void pointer(const pxa::ui::CanvasPointer& e) {
        using pxa::input::PointerPhase;
        const auto phase=e.action();
        const Point p{(pxa::ui::canvas_to_surface_coordinate(e.x,display)-image_x)/scale,
                      (pxa::ui::canvas_to_surface_coordinate(e.y,display)-image_y)/scale};
        if (phase==PointerPhase::down) {
            if (!capture.try_capture(e)) return;
            pad=false; map_focus=false; dirty=true;
            down=last=p; panning=false;
            down_map=screen==Screen::Town && viewport.contains(p);
            return;
        }
        if (!capture.owns(e)) return;
        const auto motion=p-down;
        if (phase==PointerPhase::move && down_map) {
            panning|=std::abs(motion.x)+std::abs(motion.y)>5;
            if (panning) {followed=-1; camera=camera+last-p; clamp_camera(); dirty=true;}
            last=p;
        }
        if (e.ended()) {
            capture.release(e);
            if (phase==PointerPhase::up && !panning && std::abs(motion.x)+std::abs(motion.y)<13) tap(p);
        }
    }
    void move_focus(Point vector) {
        if (screen==Screen::Town && map_focus) {
            const auto p=selection+vector;
            if (inside(p)) selection=p;
            track_selection(); dirty=true; return;
        }
        if (button_count<2) return;
        const int anchor=std::clamp(focus,0,button_count-1);
        const auto p=buttons[anchor].box.center();
        int best=-1,score=99999;
        for (int i=0;i<button_count;++i) {
            if (i==anchor) continue;
            const auto delta=buttons[i].box.center()-p;
            const int forward=delta.x*vector.x+delta.y*vector.y;
            if (forward<=0) continue;
            const int test=forward+3*std::abs(delta.x*vector.y-delta.y*vector.x);
            if (test<score) {best=i; score=test;}
        }
        if (best>=0) focus=best;
        dirty=true;
    }
    void on_controller(pxa::Context&,const pxa::input::ControllerState& state) {
        using namespace pxa::input;
        if (!controller.update(state) ||
            !controller.pressed(controller_up | controller_down | controller_left | controller_right |
                                controller_a | controller_b | controller_y | controller_start)) return;
        pad=true; dirty=true;
        if (controller.pressed(controller_up)) move_focus({0,-1});
        if (controller.pressed(controller_down)) move_focus({0,1});
        if (controller.pressed(controller_left)) move_focus({-1,0});
        if (controller.pressed(controller_right)) move_focus({1,0});
        if (controller.pressed(controller_b)) {close_screen(); return;}
        if (screen==Screen::Town && controller.pressed(controller_y)) {
            activate(MapFocus);
            return;
        }
        if ((screen==Screen::Town || screen==Screen::Battle) && controller.pressed(controller_start)) {
            activate(Speed);
            return;
        }
        if (!controller.pressed(controller_a)) return;
        if (screen==Screen::Town && map_focus) {
            if (placing) {
                if (game.placement(selection,build_kind)==Placement::Good) activate(ConfirmBuild);
                else show_toast(placement_hint(game.placement(selection,build_kind)));
            } else {
                map_focus=false;
                examine(selection);
            }
        } else if (button_count) activate(buttons[std::clamp(focus,0,button_count-1)].action);
    }
};

}
}
PXA_GAME(wind::App)
