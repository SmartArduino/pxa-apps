#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <string_view>

namespace wind {
using Text = std::string_view;

struct Point {
    int x, y;
    constexpr bool operator==(const Point&) const = default;
    constexpr Point operator+(Point b) const { return {x+b.x,y+b.y}; }
    constexpr Point operator-(Point b) const { return {x-b.x,y-b.y}; }
};

constexpr int Width=20, OriginalHeight=17, Height=23, Cells=Width*Height, MaxElevation=3;
constexpr int OriginalCells=Width*OriginalHeight, OriginalCellFlags=(OriginalCells+7)/8, CellFlags=(Cells+7)/8;
constexpr unsigned SimulationBeatMs=50, WalkTileBeats=7, ExpeditionTileBeats=5;
constexpr Point Gate{8,15}, Square{8,9}, Cave{14,5}, DepthsCave{18,6}, GuildBoard{10,9},
                FrontierGround{17,22};
constexpr std::array<Point,4> Directions{{{0,-1},{1,0},{0,1},{-1,0}}};
constexpr bool inside(Point p) { return p.x>=0 && p.x<Width && p.y>=0 && p.y<Height; }
constexpr int cell(Point p) { return p.y*Width+p.x; }
constexpr Point point(int i) { return {i%Width,i/Width}; }

enum Ground : uint8_t { Grass, Forest, River, Stone, Street, Bridge, Stair, Plot };
struct Tile { Ground ground; uint8_t height; uint8_t ward; };
Tile landscape(Point p);
unsigned texture(Point p);

enum Kind : uint8_t { Guild, Inn, Forge, Herb, House, Market, CaveMouth, Shrine, Road, StoneSteps, RaiseGround, LowerGround };
struct BuildingType { const char* name; const char* description; uint16_t cost; uint8_t rank; };
inline constexpr BuildingType Types[]{
    {"风石公会","委托与旅人的集散地",0,1},
    {"栖风旅店","恢复生命与体力",100,1},
    {"铜砧铁匠铺","来客购买武器，提升战力",140,1},
    {"月露药房","出售药剂，治疗伤势",150,2},
    {"灯窗民居","为一位旅人安排住处",125,1},
    {"山风集市","来客消费，增加满意度",85,1},
    {"回声矿洞","六层遗迹等待探索",0,1},
    {"风纹祭坛","炼成护符，提升队伍战力",225,3},
    {"碎石小路","把铺面接入镇内路网",12,1},
    {"石阶","连接相邻的两层台地",24,1},
    {"抬高地块","将空地抬高一层",20,1},
    {"降低地块","将空地降低一层",10,1}
};
inline constexpr Kind ShopList[]{Inn,Forge,Market,Herb,House,Shrine};
inline constexpr Kind GroundList[]{Road,StoneSteps,RaiseGround,LowerGround};
constexpr bool earthwork(Kind type) {return type==RaiseGround || type==LowerGround;}
constexpr bool paid_service(int kind) {return kind==Inn || kind==Forge || kind==Herb || kind==Market;}
constexpr int MaxBuildings=32, HeroCount=8, QuestCount=6;
enum class ItemGroup : uint8_t { Weapon, Armor, Charm, Consumable };
enum class Item : uint8_t {
    None, IronSword, BriarBow, RidgeHammer, StarStaff, ClothVest, PineVest, StoneMail, StarPlate,
    RiverPendant, EmberRing, FeatherBrooch, WindAmulet, LeafPotion, BlueTonic, HoneyBun, StudyBook, End
};
constexpr int ItemCount=int(Item::End)-1, GearSlots=3, StockLimit=99, LevelLimit=25, TalentLimit=8;
constexpr bool valid_item(Item item) {return item>Item::None && item<Item::End;}
constexpr int item_index(Item item) {return int(item)-1;}
struct ItemType {
    const char* name;
    ItemGroup group;
    Kind vendor;
    uint8_t rank;
    uint16_t price;
    uint8_t relics, attack, armor, stamina, amount;
};
inline constexpr ItemType Items[ItemCount]{
    {"铁木短剑",ItemGroup::Weapon,Forge,1,48,0,5,0,0,0},
    {"风枝弓",ItemGroup::Weapon,Forge,2,92,0,9,0,2,0},
    {"岩脊锤",ItemGroup::Weapon,Forge,3,155,0,15,2,0,0},
    {"晨星杖",ItemGroup::Weapon,Forge,4,245,0,22,0,2,0},
    {"旅布衣",ItemGroup::Armor,Forge,1,42,0,0,2,1,0},
    {"松皮甲",ItemGroup::Armor,Forge,2,86,0,0,4,1,0},
    {"石纹甲",ItemGroup::Armor,Forge,3,145,0,0,7,0,0},
    {"星纹重铠",ItemGroup::Armor,Forge,4,232,0,0,11,0,0},
    {"溪石坠",ItemGroup::Charm,Market,1,38,0,0,0,2,0},
    {"火纹戒",ItemGroup::Charm,Market,2,92,0,4,1,0,0},
    {"月羽扣",ItemGroup::Charm,Shrine,3,108,3,0,3,3,0},
    {"风眼符",ItemGroup::Charm,Shrine,4,168,6,8,4,4,0},
    {"森叶药剂",ItemGroup::Consumable,Herb,2,22,0,0,0,0,35},
    {"碧泉补剂",ItemGroup::Consumable,Herb,2,21,0,0,0,0,34},
    {"蜂蜜面包",ItemGroup::Consumable,Market,1,16,0,0,0,0,18},
    {"兵法笔记",ItemGroup::Consumable,Market,3,52,0,0,0,0,20},
};
constexpr const ItemType& item_type(Item item) {return Items[item_index(item)];}
constexpr int stock_level(Item item) {return item_type(item).rank>=3?item_type(item).rank-1:1;}
struct Recipe { Item input, output; Kind workshop; int rank, level, gold, relics; };
inline constexpr Recipe Recipes[]{
    {Item::ClothVest,Item::PineVest,Forge,2,1,18,1},
    {Item::IronSword,Item::BriarBow,Forge,2,1,28,2},
    {Item::PineVest,Item::StoneMail,Forge,3,2,42,3},
    {Item::StoneMail,Item::StarPlate,Forge,4,2,54,4},
    {Item::RiverPendant,Item::FeatherBrooch,Shrine,3,1,42,3},
    {Item::EmberRing,Item::WindAmulet,Shrine,4,2,50,5},
};
enum class Talent : uint8_t { Arms, Grit, Travel };
constexpr int TalentCount=3;
struct HeroStats {
    int attack=0, armor=0, stamina=0;
    int rating() const {return attack+2*armor+2*stamina;}
};
enum class Discipline : uint8_t { None, Assault, Guard, Trail };
struct DisciplineInfo { const char* name; const char* benefit; HeroStats bonus, mastery; };
inline constexpr DisciplineInfo Disciplines[]{
    {"突击","攻击 +4",{4,0,0},{1,0,0}},
    {"护卫","防护 +2",{0,2,0},{0,1,0}},
    {"探路","减耗 +3",{0,0,3},{0,0,1}},
};
constexpr int MasteryLimit=12, DisciplineLevel=2, DisciplineCost=32;
constexpr bool valid_discipline(Discipline choice) {
    return choice!=Discipline::None && unsigned(choice)<=std::size(Disciplines);
}
struct HeroType { const char* name; const char* role; uint8_t power, fame, fee; };
inline constexpr HeroType Heroes[HeroCount]{
    {"米菈","剑士",8,0,0},{"栩恩","斥候",7,0,0},{"蕨铃","药师",9,12,38},
    {"砾牙","守卫",12,35,58},{"丹鸢","游侠",13,60,75},{"青棘","咒术师",16,100,95},
    {"铁杉","骑士",19,145,120},{"清露","星术师",22,200,150}
};
struct QuestType { const char* name; const char* monster; uint8_t rank, hp, power; uint16_t reward; uint8_t fame, loot; };
inline constexpr QuestType Quests[QuestCount]{
    {"苔灯巷","苔壳团",1,32,7,90,12,2},
    {"铜铃矿井","穴居獾",1,54,9,130,18,3},
    {"沉水回廊","水纹灵",2,78,13,180,24,4},
    {"旧风塔","岩衣卫",3,110,17,245,32,5},
    {"霜脊深窟","霜角兽",4,146,22,330,40,6},
    {"风核圣所","眠风龙",4,188,27,450,55,8}
};
enum class Route : uint8_t { Standard, Depths, Frontier };
inline constexpr QuestType FrontierHunt{"南岭讨伐","岭角王",2,82,13,155,14,3};
constexpr int FrontierSpecies=4, FrontierRestDays=3, FrontierHeavyEvery=3, FrontierHeavyBonus=5;
constexpr const QuestType& expedition_target(int floor,Route route) {
    return route==Route::Frontier?FrontierHunt:Quests[floor];
}
constexpr Point expedition_entrance(Route route) {
    return route==Route::Frontier?FrontierGround:route==Route::Depths?DepthsCave:Cave;
}
constexpr Point frontier_party_tile(unsigned team,int id) {
    const int behind=std::popcount(team>>(id+1));
    const int distance=behind==2?5:behind==1?4:2;
    return FrontierGround-Point{distance,0};
}
struct DepthRules {
    uint8_t armor, heavy_every, heavy_bonus;
    Item first_loot, repeat_loot;
};
inline constexpr DepthRules Depths[QuestCount]{
    {3,3,4,Item::ClothVest,Item::BlueTonic},
    {2,2,5,Item::BriarBow,Item::LeafPotion},
    {5,3,6,Item::StoneMail,Item::StudyBook},
    {7,3,8,Item::FeatherBrooch,Item::StudyBook},
    {6,2,9,Item::StarStaff,Item::StudyBook},
    {9,2,11,Item::WindAmulet,Item::WindAmulet},
};
constexpr int guardian_health(int floor,Route route) {
    const int hp=expedition_target(floor,route).hp;
    return route==Route::Depths?hp+(hp+2)/3:hp;
}
constexpr bool heavy_round(int floor,Route route,unsigned round) {
    if (route==Route::Frontier) return round && round%FrontierHeavyEvery==0;
    return route==Route::Depths && round && round%Depths[floor].heavy_every==0;
}
struct RankGoal { uint16_t fame; uint8_t people, floors; const char* name; };
inline constexpr RankGoal Ranks[]{
    {0,0,0,"驿站"},{20,2,1,"溪谷村"},{65,3,2,"风石镇"},
    {130,4,4,"山脊繁镇"},{225,5,6,"风石名城"}
};

enum class ServicePolicy : uint8_t { Standard, Value, Premium };
struct ServiceTerms { const char* name; int price_delta, appeal, mood, recovery, forge; };
inline constexpr ServiceTerms ServicePlans[]{
    {"常价经营",0,0,0,0,0},
    {"惠民经营",-3,4,2,0,0},
    {"优质服务",5,-5,0,6,1},
};
constexpr bool valid_policy(ServicePolicy policy) {return unsigned(policy)<std::size(ServicePlans);}
struct BusinessDay { uint16_t visits=0, revenue=0; };
struct ShopAccount {
    static constexpr int DailyLimit=9999;
    ServicePolicy policy=ServicePolicy::Standard;
    BusinessDay today{}, yesterday{};
};
struct Building {
    uint8_t x,y,kind,level;
    ShopAccount account{};
    Point at() const {return {x,y};}
};
constexpr int service_price(const Building& building,ServicePolicy policy) {
    if (!paid_service(building.kind)) return 0;
    return (building.kind==Forge?14:8)+3*building.level+ServicePlans[unsigned(policy)].price_delta;
}
struct Hero {
    uint8_t level,xp,hp,energy,mood,gear,wallet,home;
    std::array<Item,GearSlots> equipment{};
    std::array<uint8_t,TalentCount> talents{};
    Discipline discipline=Discipline::None;
    std::array<uint8_t,std::size(Disciplines)> practice{};
    bool practicing() const {
        return valid_discipline(discipline) && practice[unsigned(discipline)-1]<MasteryLimit;
    }
};
enum Phase : uint8_t { Idle, Outbound, Fighting, Returning, Exploring };
enum class Tactic : uint8_t { Balanced, Assault, Guard };
struct TacticInfo { const char* name; const char* hint; int energy; };
inline constexpr TacticInfo Tactics[]{
    {"均衡","攻守均衡",21},{"强袭","伤害高，受伤重",30},{"戒备","降低双方伤害",25}
};
constexpr bool valid_tactic(Tactic tactic) {return unsigned(tactic)<std::size(Tactics);}
constexpr TacticInfo tactic_info(Tactic tactic) {return Tactics[unsigned(tactic)];}
constexpr int tactic_energy(Tactic tactic,Route route) {
    return tactic_info(tactic).energy+(route==Route::Depths?5:0);
}
constexpr int experience_goal(int level) {return 12+3*level;}
constexpr bool shields_party(int id) {return id==3 || id==6;}
constexpr int room_rent(int level) {return 3+level;}
enum class CavePath : uint8_t { Unchosen, Main, Side };
struct Expedition {
    uint8_t floor, team, phase, hp, result, round, supplied;
    Tactic tactic=Tactic::Balanced;
    Item loot=Item::None;
    Route route=Route::Standard;
    uint16_t reward_gold=0;
    uint8_t reward_fame=0, reward_relics=0, reward_xp=0;
    uint8_t distance=0, patrol_hp=0, patrol_timer=0, cache_found=0;
    CavePath cave_path=CavePath::Unchosen;
    uint8_t side_cache_found=0;
};
// The trail advances in beats and stops while the party is fighting a patrol.
constexpr int cave_length(int floor,Route route) {return 88+8*floor+(route==Route::Depths?24:0);}
constexpr bool second_patrol(int floor,Route route) {return floor>=2 || route==Route::Depths;}
constexpr int patrol_health(int floor,Route route) {return 12+4*floor+(route==Route::Depths?5:0);}
constexpr int cave_fork(const Expedition& trip) {return cave_length(trip.floor,trip.route)/2+14;}
constexpr bool waiting_at_fork(const Expedition& trip) {
    return trip.phase==Exploring && trip.distance==cave_fork(trip) && trip.cave_path==CavePath::Unchosen;
}
constexpr int cave_distance(const Expedition& trip) {
    return cave_length(trip.floor,trip.route)+(trip.cave_path==CavePath::Side?44:0);
}
constexpr int side_patrol(const Expedition& trip) {return cave_length(trip.floor,trip.route)+9;}
constexpr int side_cache(const Expedition& trip) {return cave_length(trip.floor,trip.route)+27;}
constexpr int cave_cache_relics(Route route,bool side) {return (side?3:1)+(route==Route::Depths?1:0);}
constexpr bool in_side_cave(const Expedition& trip) {
    return trip.cave_path==CavePath::Side && trip.distance>=cave_length(trip.floor,trip.route);
}
constexpr int current_patrol_health(const Expedition& trip) {
    return patrol_health(trip.floor,trip.route)+(in_side_cave(trip)?12:0);
}
constexpr int patrol_species(const Expedition& trip) {
    if (in_side_cave(trip)) return (trip.floor+3)%5;
    return (trip.floor+(trip.distance>cave_length(trip.floor,trip.route)/2?2:1))%5;
}
constexpr bool showing_cache(const Expedition& trip) {
    const int cache=in_side_cave(trip)?side_cache(trip):cave_length(trip.floor,trip.route)/2;
    return trip.distance>=cache && trip.distance<cache+14;
}
struct QuestReward {int gold, fame, relics, xp; Item loot;};
enum class CommissionKind : uint8_t { None, Survey, Trade, Build, Hunt };
struct Commission {
    CommissionKind kind=CommissionKind::None;
    uint8_t target=0, progress=0;
    constexpr int goal() const {return kind==CommissionKind::Survey?1:target;}
    constexpr bool complete() const {return goal()>0 && progress==goal();}
};
struct CommissionPrize { int gold, fame; };
constexpr CommissionPrize commission_prize(Commission order) {
    switch (order.kind) {
    case CommissionKind::Survey: return {45+10*order.target,5+order.target};
    case CommissionKind::Trade: return {16+4*order.target,4+order.target/4};
    case CommissionKind::Build: return {20+30*order.target,4+2*order.target};
    case CommissionKind::Hunt: return {24+10*order.target,4+order.target};
    default: return {0,0};
    }
}
enum class TownEvent : uint8_t { None, Lanterns, Drill, Feast };
struct TownEventInfo { const char* name; uint8_t rank, gold; };
inline constexpr TownEventInfo TownEvents[]{
    {"广场灯会",1,65},{"公会集训",1,48},{"备战宴",2,78}
};
constexpr bool valid_event(TownEvent event) {
    return event>TownEvent::None && unsigned(event)<=std::size(TownEvents);
}
constexpr const TownEventInfo& event_info(TownEvent event) {return TownEvents[unsigned(event)-1];}
constexpr int LanternFame=9, LanternMood=13, DrillExperience=12, DrillPractice=2, DrillEnergy=12,
              FeastEnergy=16, FeastMood=10, PreparedDiscount=8;
struct State {
    uint32_t seed=0;
    uint16_t gold=0, fame=0, relics=0, day=0, income=0, yesterday=0, visits=0;
    uint8_t rank=0, cleared=0, festival=0, charms=0, clock=0, count=0, victory=0;
    std::array<uint8_t,CellFlags> roads{}, steps{};
    // Zero follows the original map. One through four select an explicit elevation.
    std::array<uint8_t,Cells> elevations{};
    std::array<Building,MaxBuildings> buildings{};
    std::array<Hero,HeroCount> heroes{};
    Expedition trip{};
    Commission commission{};
    uint16_t commission_reward_day=0;
    uint8_t trained_today=0;
    std::array<uint8_t,ItemCount> stock{};
    uint8_t depth_cleared=0;
    std::array<uint8_t,2> cave_caches{};
    std::array<uint8_t,2> side_caches{};
    uint16_t frontier_next_day=0;
    TownEvent event=TownEvent::None;
    uint8_t event_attended=0, prepared=0;
};
struct Walker {
    Point from=Gate, at=Gate;
    uint8_t walk_beats=0;
    uint8_t walk_tile_beats=WalkTileBeats;
    int8_t shop=-1;
    uint8_t dwell=0;
    bool at_home=false;
    bool festival_guest=false;
    uint16_t festival_day=0;
    uint16_t length=0, next=0;
    std::array<uint16_t,Cells> steps{};
};
enum class Activity {
    Stroll, Rest, ToShop, Shop, ToHome, Home, ToHunt, Hunt, ToFestival, Festival,
    ToCave, Expedition, Returning, CaveTrail, ToFrontier, FrontierBattle, ToTownEvent, Drill, Feast
};
struct WildSite { Point at; int clock, rank, health, power, species; };
inline constexpr std::array<WildSite,5> WildSites{{
    {{11,12},54,1,24,8,0},
    {{15,8},96,2,34,11,1},
    {{8,18},88,1,24,8,0},
    {{16,19},113,2,32,10,2},
    {{18,22},125,3,38,12,3},
}};
constexpr int wild_spawn_clock(const WildSite& site,int day) {
    return site.clock+(day-1)*(site.species+1)%9;
}
struct WildMonster {
    uint8_t hp=0, age=0;
    int8_t hunter=-1;
};
constexpr int WildRound=12, WildImpact=6;
struct Notice { Text message; int value=0; };
enum class Placement { Good, Occupied, Locked, NoRoad, Funds, Limit, ElevationLimit, NoStairLink };

class Game {
public:
    static constexpr int DayBeats=200, EveningBeat=DayBeats*3/4;
    static constexpr int FestivalCost=event_info(TownEvent::Lanterns).gold;
    static constexpr int SupplyCost=24;
    static constexpr int SupplyHealth=28;
    static constexpr int SideCaveCost=8;
    static constexpr uint8_t NoHome=255;
    static constexpr int SettleMood=45;
    static constexpr int TrainingCost=26, TrainingEnergy=28, TrainingHealth=45;
    State s{};
    std::array<Walker,HeroCount> walkers{};
    std::array<WildMonster,WildSites.size()> wild{};
    std::array<uint8_t,Cells> traffic{};
    std::array<Notice,5> journal{};
    int revision=0;
    unsigned beat=0;

    void reset();
    void restore(bool previous_map=false);
    int hero_count() const;
    int power(int hero) const;
    HeroStats stats(int hero) const;
    int growth_points(int hero) const;
    bool can_invest(int hero,Talent talent) const;
    bool invest(int hero,Talent talent);
    bool can_choose_discipline(int hero,Discipline choice) const;
    bool choose_discipline(int hero,Discipline choice);
    bool guards(int hero) const;
    int owned(Item item) const;
    bool sells(int shop,Item item) const;
    bool can_buy(int shop,Item item) const;
    bool buy(int shop,Item item);
    bool sell(Item item);
    bool offers_recipe(int shop,int recipe) const;
    bool can_refine(int shop,int recipe) const;
    bool refine(int shop,int recipe);
    bool can_equip(int hero,Item item) const;
    bool equip(int hero,Item item);
    bool unequip(int hero,ItemGroup group);
    bool can_use(int hero,Item item) const;
    bool use(int hero,Item item);
    int building(Point p) const;
    Tile terrain(Point p) const;
    bool road(Point p) const;
    bool usable(Point a, Point b) const;
    Placement placement(Point p, Kind type) const;
    bool build(Point p, Kind type);
    int upgrade_cost(int id) const;
    bool upgrade(int id);
    bool set_service_policy(int id,ServicePolicy policy);
    bool recruit(int id);
    bool gift(int id);
    bool trained(int id) const;
    bool can_train(int id) const;
    bool train(int id);
    int training_experience() const;
    bool can_assign_home(int hero,int house) const;
    bool assign_home(int hero,int house);
    int hunt_target(int hero) const;
    Activity activity(int hero) const;
    int occupants(int house) const;
    int daily_rent() const;
    bool festival() {return host_event(TownEvent::Lanterns);}
    bool can_host_event(TownEvent event) const;
    bool host_event(TownEvent event);
    bool prepared(int hero) const;
    bool craft();
    bool floor_open(int floor,Route route=Route::Standard) const;
    bool frontier_visible() const;
    bool ready_for_trip(int id,Tactic tactic,Route route=Route::Standard) const;
    int travel_cost(int id,Tactic tactic,Route route=Route::Standard) const;
    int required_energy(int id,Tactic tactic,Route route=Route::Standard) const;
    unsigned suggest_team(int floor,Tactic tactic,Route route=Route::Standard) const;
    bool can_launch(int floor,unsigned team,Tactic tactic,Route route=Route::Standard) const;
    bool launch(int floor,unsigned team,Tactic tactic=Tactic::Balanced,Route route=Route::Standard);
    int opening_strike(int floor,unsigned team,Tactic tactic,int roll,Route route=Route::Standard) const;
    QuestReward preview_reward(int floor,Route route) const;
    bool can_supply() const;
    bool supply();
    bool retreat();
    std::array<Commission,2> commission_offers() const;
    bool accept_commission(unsigned offer);
    bool abandon_commission();
    bool claim_commission();
    bool eligible(int hero) const;
    bool can_promote() const;
    bool promote();
    bool can_choose_cave_path(CavePath path) const;
    bool choose_cave_path(CavePath path);
    void step(bool wait_for_fork=false);
    void note(Text text, int value=0);

private:
    void roads_changed();
    void migrate_map();
    Point entrance(Point at) const;
    bool path(int hero, Point target);
    bool send(int hero, int shop);
    bool start_hunt(int hero);
    void release_hunt(int site);
    void fight_wild(int site);
    bool visit(int hero, int shop);
    bool can_attend_event(int hero) const;
    bool attend_event(int hero);
    void choose(int hero);
    void advance_trip(bool wait_for_fork);
    void explore_cave(bool wait_for_fork);
    void open_cave_cache(bool side);
    void end_fight(bool win);
    void record_commission(CommissionKind kind,int floor=-1);
    bool store(Item item);
    unsigned random(unsigned range);
};

constexpr size_t LegacySaveSize=274, MapSaveSize=LegacySaveSize+OriginalCellFlags+OriginalCells,
                 CommissionSaveSize=MapSaveSize+6, TacticsSaveSize=CommissionSaveSize+2,
                 InventorySaveSize=TacticsSaveSize+ItemCount+HeroCount*(GearSlots+TalentCount)+1,
                 ExpeditionSaveSize=InventorySaveSize+7,
                 DisciplineSaveSize=ExpeditionSaveSize+HeroCount*(1+std::size(Disciplines)),
                 ShopSaveSize=DisciplineSaveSize+MaxBuildings*9,
                 SouthernSaveSize=ShopSaveSize+2*(CellFlags-OriginalCellFlags)+(Cells-OriginalCells)+6,
                 ForkSaveSize=SouthernSaveSize+4, FrontierSaveSize=ForkSaveSize+2, SaveSize=FrontierSaveSize+3;
using Save=std::array<std::byte,SaveSize>;
Save encode(const State& state);
bool decode(State& state,std::span<const std::byte> bytes,bool* previous_map=nullptr);
}
