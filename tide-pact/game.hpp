#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <span>

namespace tide {

using Text = std::string_view;
struct Point { int x, y; };
struct Location { Text name, region, note; Point map; int longitude, latitude; };
struct Leg { int from, to, offset, count, nautical_miles; };
#include "world_data.hpp"
inline constexpr int PlaceCount=std::size(Places), PortCount=std::size(PortPlaces);
static_assert(PlaceCount<128 && PortCount<64);
inline constexpr auto PortIndices=[] {
    std::array<int,PlaceCount> lookup{};
    lookup.fill(-1);
    for (int i=0;i<PortCount;++i) lookup[PortPlaces[i]]=i;
    return lookup;
}();
constexpr int port_index(int place) {
    return place>=0 && place<PlaceCount?PortIndices[place]:-1;
}
constexpr bool is_port(int place) { return port_index(place)>=0; }
constexpr int port_place(int index) { return PortPlaces[index]; }
constexpr bool is_village(int place) {
    return std::ranges::find(Villages,place)!=std::ranges::end(Villages);
}

struct PlaceFlags {
    std::uint64_t low=0, high=0;
    constexpr PlaceFlags(std::uint64_t low=0,std::uint64_t high=0) : low(low), high(high) {}
    constexpr explicit operator bool() const { return low || high; }
    constexpr bool operator==(const PlaceFlags&) const = default;
    constexpr PlaceFlags operator~() const { return {~low,~high}; }
    friend constexpr PlaceFlags operator&(PlaceFlags a,PlaceFlags b) { return {a.low&b.low,a.high&b.high}; }
    friend constexpr PlaceFlags operator|(PlaceFlags a,PlaceFlags b) { return {a.low|b.low,a.high|b.high}; }
    constexpr PlaceFlags& operator|=(PlaceFlags other) { return *this=*this|other; }
    constexpr PlaceFlags& operator&=(PlaceFlags other) { return *this=*this&other; }
    constexpr int count() const { return std::popcount(low)+std::popcount(high); }
};
constexpr PlaceFlags place_bit(int place) {
    if (place<0 || place>=128) return {};
    return place<64?PlaceFlags{std::uint64_t{1}<<place}:PlaceFlags{0,std::uint64_t{1}<<(place-64)};
}
constexpr PlaceFlags places_before(int count) {
    return count<64?PlaceFlags{(std::uint64_t{1}<<count)-1}:
                    PlaceFlags{~std::uint64_t{0},(std::uint64_t{1}<<(count-64))-1};
}
inline constexpr int LegacyPortCount=20, LegacyPlaceCount=29, LegacyChartWidth=728, LegacyChartHeight=912;
inline constexpr int LegacyMapX=(-45-ChartWest)*ChartLongitudeScale;
inline constexpr int LegacyMapY=(ChartNorth-72)*ChartLatitudeScale;
constexpr Point chart_point(float lon, float lat) {
    return {int((lon-ChartWest)*ChartLongitudeScale+.5f),int((ChartNorth-lat)*ChartLatitudeScale+.5f)};
}

} // namespace tide

#include "climate.hpp"
#include "expeditions.hpp"
#include "dispatch.hpp"
#include "languages.hpp"
#include "port_stories.hpp"
#include "rumors.hpp"
#include "stories.hpp"
#include "settlement.hpp"
#include "pilotage.hpp"
#include "land_battle.hpp"

namespace tide {

enum Goods { Wine, Sugar, Cloth, Wool, Timber, Salt, Pepper, Porcelain, GoodsCount };
enum SpecialtyGoods { MadeiraSugar=GoodsCount, DriedCod, MuranoGlass, GoldDust, MalabarGinger, MarketGoodsCount };
inline constexpr int SpecialtyCount=int(MarketGoodsCount)-int(GoodsCount);
inline constexpr int SpecialtyBuyerSlots=5, SpecialtyOrderLimit=14;
inline constexpr std::array<Text, MarketGoodsCount> GoodsNames{
    "葡萄酒", "蔗糖", "织锦", "羊毛", "木料", "海盐", "胡椒", "瓷器",
    "马德拉糖", "鳕鱼干", "穆拉诺玻璃", "黄金砂", "马拉巴尔姜"
};

enum class MarketAccess { Closed, Trading, Producer, Buyer };
MarketAccess market_access(int port,int good);
Text specialty_note(int good);

struct MarketNotice {
    int port, good, first_day, duration, price_percent, reserve_shift;
    Text headline;
    constexpr int phase(int day) const { return (day+84-first_day)%84; }
    constexpr int until(int day) const { return phase(day)<duration?0:84-phase(day); }
    constexpr int remaining(int day) const { return std::max(0,duration-phase(day)); }
};
inline constexpr MarketNotice MarketNotices[]{
    {lisbon,Timber,4,9,24,-8,"船坞收木料"},
    {madeira,Sugar,14,11,-17,12,"新糖进仓"},
    {cadiz,Salt,27,9,26,-9,"渔队备海盐"},
    {canaries,Wine,38,12,16,-7,"商船收葡萄酒"},
    {elmina,Cloth,55,10,21,-8,"集市收织锦"},
    {ragusa,Salt,18,10,22,-8,"盐船集齐，岸上收盐"},
    {candia,Wine,52,14,-18,13,"山村的新酒到港"},
    {diu,Cloth,34,11,-15,12,"织户的棉布入仓"},
    {gresik,Pepper,66,12,23,-9,"外海商船抢收胡椒"},
    {quanzhou,Porcelain,17,11,-15,11,"内河瓷货入仓"},
    {hakata,Timber,41,10,20,-7,"湾内商船换桅"},
    {kochi,Pepper,57,13,-18,12,"回水船带来新晒的椒"},
    {busanpo,Cloth,7,12,22,-8,"市棚收布备渡海"},
};
const MarketNotice* market_notice(int port, int good, int day);

enum OfficerId { NoOfficer, Pilot, Quartermaster, Boatswain, OfficerLimit };
struct Officer {
    int port, fee;
    Text name, specialty, effect, greeting;
    std::array<Language,2> languages;
};
inline constexpr Officer Officers[]{
    {},
    {lisbon,95,"安娜","领航",
     "领航顺利时每天约多走十二海里，触礁也少受些伤。",
     "小时候，我常替塔霍河的渡船探水深。给我一根测深绳，我来帮你避开沙洲。",{Portuguese,French}},
    {madeira,140,"贝阿特丽丝","司务",
     "航行或考察时每天省一份淡水，每天至少消耗一份。",
     "我管糖仓时漏过一桶水。打那以后，我在桶盖上都刻了记号。船上的水也交给我吧。",{Portuguese,Castilian}},
    {cadiz,160,"杜阿尔特","甲板护卫",
     "每次中炮少受三点伤害。跳上敌船决斗时，体力多六点。",
     "两船靠稳，钩索扣牢，再跳。我在直布罗陀吃过亏，这话你得记住。",{Castilian,Arabic}}
};
static_assert(std::size(Officers)==OfficerLimit);

struct Chapter {
    int place, portrait;
    Text title, speaker, message, first, second;
};
inline constexpr Chapter Story[] {
    {lisbon, 0, "墨迹未干", "伊内丝",
     "你哥哥失踪前，把海图托我保管。你看，这盏灯画在海上。去大加那利岛找莱昂，他见过你哥哥。",
     "自己出钱", "领220金资助"},
    {canaries, 1, "逆风的灯", "莱昂",
     "你哥哥留了半张货单。昨晚有条渔船没回来，你帮我找船，我就把货单给你。赶时间的话，七十金也行。",
     "找渔船 2天", "花70金买货单"},
    {elmina, 3, "两本账簿", "阿杜瓦",
     "同一批货，盖了两个港的印。有人点假灯引船撞礁，再低价收货。你哥哥把证据藏在好望角外的蜡封木筒里。",
     "送信换补给", "受雇拿140金"},
    {cape, 0, "风暴之角", "哥哥的信",
     "木筒里有张星图。背面是哥哥的字：「我没事。把图带去基尔瓦，交给萨利姆。别给商号。」",
     "记下灯标位置", "收起星图"},
    {kilwa, 4, "星图上的约定", "萨利姆",
     "你哥哥已搭商船走了。我手里也有一份图。上面记着各港涨潮的时辰，拿它对一对，就能认出假灯。",
     "答应公开", "收260金订金"},
    {lisbon, 5, "潮汐之约", "埃斯特旺",
     "商号出一千七百金，要独用这张图。你也可以把图贴到码头，让每条船出港前都能查潮时、认灯标。",
     "公开航图", "签独家合约"},
};
inline constexpr int Chapters = std::size(Story);
inline constexpr Text Endings[] {
    "各港照着潮图改了灯标，遇难的船也少了。后来莱昂来信，说你哥哥已经搭上归船。船长出港前，特地在码头抄了一张新图。",
    "商号付清了钱，把潮图锁进仓库。萨利姆还留着一份抄本。第二年，一条没挂旗的小船把它带回了里斯本。"
};

struct Passage {
    std::array<int, PlaceCount> edge{};
    std::array<bool, PlaceCount> reverse{};
    int count = 0;
    int miles = 0;
};

enum SeaWeather { Fair, Heat, Rain, Squall, Snow };
struct SeaConditions {
    int direction, strength;
    SeaWeather weather;
};
inline constexpr Point Directions[]{{0,-16},{11,-11},{16,0},{11,11},{0,16},{-11,11},{-16,0},{-11,-11}};
inline constexpr Text Bearings[]{"北","东北","东","东南","南","西南","西","西北"};
inline constexpr Text SeaWeatherNames[]{"晴","炎热","降雨","骤风","降雪"};
inline constexpr int HoursPerDay=24, SailingWatches=4, HoursPerWatch=HoursPerDay/SailingWatches;
inline constexpr int HelmPulsesPerWatch=6;
enum DriftStage : std::uint8_t { DriftUnheard, DriftSearching, DriftRecovered, DriftReturned, DriftFiled };
enum DriftMethod { LaunchBoat, RigHook };
inline constexpr Point DriftGround{174+LegacyMapX,335+LegacyMapY};
inline constexpr int DriftReach=7;
SeaConditions conditions_at(Point position, int day);
int direction_to(Point offset);
using WaterTest = bool (*)(Point);

inline constexpr int WakeCapacity=64;
struct HarborApproach {
    std::array<Point,48> points{};
    int count=0;
    bool direct=false;
};

inline constexpr int ForecastDays=3;
enum RationFlag : std::uint8_t { SaveFood=1, SaveWater=2 };
struct SailingRation {
    int food, water, morale_cost;
};

struct ForecastDay {
    int day=0;
    SeaConditions sea{};
    int speed_tenths=0, water=0, food=0;
};

struct PassageForecast {
    std::array<int, PlaceCount> arrival_days{};
    std::array<ForecastDay, ForecastDays> outlook{};
    int days=0;
    int provisions=0, water=0;
};

struct TradeQuote {
    int units=0;
    int gold=0;
    int stock_change=0;
    int saved=0;
};
enum class ArrivalMarket { Unknown, Closed, Held, Open };
struct TradePlan {
    int days=0, food=0, water=0, cargo=0, supply_gold=0;
    bool supplies_fit=false;
    ArrivalMarket market=ArrivalMarket::Unknown;
    TradeQuote purchase, sale;
};
inline constexpr int TradeLot=5;
inline constexpr int BargainUsed=GoodsCount+1;
inline constexpr int HagglePatience=3, MaxHaggleRate=18;
enum HaggleReply { InvalidOffer, DealAgreed, CounterOffer, TalksEnded };
struct HaggleResult {
    HaggleReply reply=InvalidOffer;
    int counter=0;
};
enum TimberTrail : std::uint8_t { TrailUnknown, TrailFound, TimberReturned, TimberPaid };
enum LandStage : std::uint8_t {
    LandUnvisited, LandRidge, LandInscription, LandLookout, LandNotes, LandFullNotes, LandFiled, LandFullFiled
};
enum LandAction { FollowStream, CrossPass, CopyMilestone, SurveyLookout, ReturnToCity };
struct LandLeg { int days, food, water; };
inline constexpr LandLeg LandLegs[]{{2,3,2},{1,2,3},{1,1,1},{1,2,2},{1,1,1}};

} // namespace tide

#include "inland.hpp"
#include "world_field.hpp"
#include "nature.hpp"
#include "charters.hpp"
#include "flags.hpp"
#include "shore_tasks.hpp"

namespace tide {

Passage passage(int from, int to);
Point vessel_at(const Passage& route, int leg, float fraction);

inline constexpr std::array MerchantPorts{lisbon,madeira,canaries};
inline constexpr std::array<Text,2> MerchantNames{"晨雾商行","白鹭商号"};
inline constexpr int NoMerchantPort=MerchantPorts.size(), MerchantLot=5, MerchantReserve=6000;

struct MerchantVoyage {
    int progress=0, gold=650;
    std::uint8_t port=0, destination=NoMerchantPort, good=Wine, units=0, held=0;
    std::uint8_t sold_port=NoMerchantPort, sold_good=Wine, sold_units=0;
    int sold_day=0;
};

struct SeaShot {
    Point position{};
    std::uint8_t heading=0, steps=0;
};

enum SeaOpponent : std::uint8_t { PrivateerOpponent, PatrolOpponent, FirstMerchantOpponent, SecondMerchantOpponent };

struct SeaSkirmish {
    Point rival{}, rope{};
    std::array<SeaShot,2> shots{};
    std::uint8_t heading=0, reload=0, enemy_reload=0, turn_delay=0, rope_ticks=0, snared=0;
    bool active=false;
    std::uint8_t opponent=PrivateerOpponent, court=0;
};

inline constexpr int SkirmishPlayerReach=88;
constexpr int skirmish_reload_ticks(int guns) { return 29-guns*3; }

enum SkirmishEffect : unsigned {
    SkirmishEnemyFire=1, SkirmishHit=2, SkirmishDamaged=4, SkirmishSplash=8,
    SkirmishRope=16, SkirmishEnded=32,
};

enum FisheryTarget : std::uint8_t { NoFishery, SardineFishery, WhaleFishery };
enum FisheryStage : std::uint8_t { TrackingFish, GearSet, GearReady };
struct SeaFishery {
    Point animal{}, gear{};
    std::uint8_t target=NoFishery, stage=TrackingFish, bearing=0, age=0, work=0;
};

struct State {
    PilotageState pilot;
    std::uint32_t pilot_visits=0;
    std::uint64_t coast_visits=0;
    std::array<PilotParcelStage,std::size(PilotParcels)> pilot_parcels{};
    int gold = 520, day = 0, hull = 100, stores = 74, spirits = 78;
    int ship = 0, guns = 0, crew = 12, charts = 0, renown = 0, trust = 0;
    int origin = lisbon, destination = -1, leg = 0, progress = 0, reefed = 0;
    int chapter = 0, ending = 0, patron = 0, allies = 0;
    int contract = -1, contract_good = 0, deadline = 0;
    int weather = 0, enemy = 0, range = 0, aim = 0, stop_next = 0;
    PlaceFlags visited=1, surveyed=0;
    std::uint32_t rng=0x57980218, event_mask=0;
    std::array<int, MarketGoodsCount> cargo{0, 0, 0, 0, 0, 2, 0, 0};
    std::array<std::int8_t, PortCount * GoodsCount> stock_delta{};
    std::array<std::int8_t,SpecialtyCount> specialty_stock{};
    std::array<std::array<std::uint8_t,SpecialtyBuyerSlots>,SpecialtyCount> specialty_demand{};
    LandFight land_fight;
    std::uint8_t shore_campaign=0;
    std::array<std::uint8_t, TaleCount> tales{};
    std::uint8_t beacon = BeaconUnseen;
    std::array<std::uint32_t, 3> sea_log{};
    std::uint8_t naval_turn=0, needs_reload=0, duel_turn=0, vigor=0, rival=0;
    std::uint8_t escort=EscortOpen, cadiz_victory=0;
    std::uint64_t guild_stakes=0;
    int helm_x=0, helm_y=0;
    std::uint8_t water=74, helm=0, heading=0, sail_watch=0, helm_pulse=0;
    std::uint8_t ration_flags=0;
    std::array<ExpeditionProgress, ExpeditionCount> expeditions{};
    std::uint8_t officer_roster=0, first_mate=NoOfficer;
    PlaceFlags chart_marks=StartingChart;
    std::uint32_t heard_rumors=0;
    std::array<std::uint8_t, ChartBytes> coast{};
    std::array<std::uint8_t, OuterChartBytes> outer_coast{};
    std::uint32_t world_notebook=0;
    std::array<int,2> trail_hunt_day{};
    std::array<std::array<std::uint32_t,PilotSurveyRows>,WorldPilotIndex> pilot_chart{};
    std::array<Point,WakeCapacity> wake{};
    std::uint8_t wake_head=0, wake_count=0;
    std::uint64_t language_skills=0;
    int interpreter_port=-1;
    std::uint8_t tide_ledger=LedgerUnseen;
    std::uint8_t bargain_good=0, bargain_rate=0;
    std::uint8_t bargain_warnings=0, timber_trail=TrailUnknown, land_stage=LandUnvisited;
    std::uint8_t drift_stage=DriftUnheard;
    std::uint8_t port_stories=0;
    bool road_delivery=false;
    std::array<std::uint8_t,std::size(RegionalSurveys)> inland_stages{};
    std::uint8_t nature_records=0;
    bool nature_hike=false;
    MerchantBerth berth;
    bool met_aden_ship=false;
    int calicut_hold_until=0;
    bool in_company=false;
    std::uint8_t colors=MerchantColors, royal_passes=0;
    std::array<int,CourtCount> court_hold_until{};
    std::array<int,CourtCount> sea_warrant{}, patrol_after{};
    std::array<std::uint8_t,ShoreTaskCount> shore_tasks{};
    SeaSkirmish skirmish;
    SeaFishery fishery;
    std::array<EnclaveHolding,EnclaveCount> enclaves{};
    std::uint8_t migrant_site=0;
    std::array<int,2> fishery_next{};
    std::uint8_t whale_oil=0, whale_blubber=0;
    DispatchRecord dispatch;
    std::array<MerchantVoyage,MerchantNames.size()> merchants{{{.port=0},{.port=1}}};
};

inline constexpr std::size_t LegacySaveBytes=320, CombatSaveBytes=336, HelmSaveBytes=352;
inline constexpr std::size_t OfficerSaveBytes=376, CoastSaveBytes=960, WakeSaveBytes=1216;
inline constexpr std::size_t LanguageSaveBytes=1232, MerchantSaveBytes=1264, NatureSaveBytes=1648;
inline constexpr std::size_t BerthSaveBytes=1664, HelmClockSaveBytes=1696, SkirmishSaveBytes=1760, FisherySaveBytes=1792, EnclaveSaveBytes=1840, SeaLawSaveBytes=1872, PilotSaveBytes=1896, WorldPilotSaveBytes=1904, CoastalSaveBytes=1912;
inline constexpr std::size_t ShoreSaveBytes=1960, LocalOrdersSaveBytes=1984, CargoSaveBytes=2016;
inline constexpr int LegacyPilotRegionCount=5;
inline constexpr std::size_t FirstSurveySaveBytes=CargoSaveBytes+LegacyPilotRegionCount*PilotSurveyRows*sizeof(std::uint32_t);
inline constexpr std::size_t PreviousWorldSaveBytes=CargoSaveBytes+WorldPilotIndex*PilotSurveyRows*sizeof(std::uint32_t);
inline constexpr std::size_t FieldSaveBytes=PreviousWorldSaveBytes+OuterChartBytes+3*sizeof(std::uint32_t);
inline constexpr std::size_t PortSaveBytes=FieldSaveBytes+(PreviousPortCount-BasePortCount)*GoodsCount;
inline constexpr std::size_t SaveBytes=PortSaveBytes+(PortCount-PreviousPortCount)*GoodsCount+
                                       3*sizeof(std::uint64_t)+2*sizeof(std::uint32_t);
std::array<std::byte, SaveBytes> encode(const State& state);
bool decode(State& state, std::span<const std::byte> bytes);

struct Game {
    Game();
    State s;
    Passage route;
    Text notice;
    bool paused = false;

    int capacity() const;
    int load() const;
    int max_stores() const;
    int food_capacity() const;
    int water_capacity() const;
    int crew_limit() const;
    int recruitable_sailors() const;
    int sailor_hire_cost() const { return recruitable_sailors()*12; }
    int quote(int good, bool sell) const;
    int price_at(int port, int good, bool sell) const;
    int specialty_orders(int port, int good) const;
    Point merchant_position(int company) const;
    TradeQuote trade_quote(int good, bool sell, int quantity) const;
    TradePlan trade_plan(int destination, int good) const;
    int trade_limit(int good, bool sell) const;
    int negotiation_rate() const;
    Text bargain_block(int good, bool sell) const;
    TradeQuote bargain_quote(int good, bool sell, int rate) const;
    HaggleResult bargain_offer(int good, bool sell, int rate);
    bool inspect_timber_trail();
    bool resolve_timber_trail(bool return_cargo);
    int road_timber_bonus() const;
    int land_stage() const;
    bool land_away() const { return s.nature_hike || (land_stage()>=LandRidge && land_stage()<=LandLookout); }
    LandLeg land_cost(LandAction action) const;
    ClimateSeason land_season() const;
    LandCondition land_weather() const;
    bool wait_land_weather();
    bool cadiz_low_tide() const;
    bool wait_land_tide();
    bool knows_land_pass() const;
    Text land_requirement(LandAction action) const;
    bool explore_land(LandAction action);
    bool file_land_report();
    NatureSite nature_site() const;
    bool nature_recorded(int entry) const {
        return entry>=0 && entry<NatureCount && (s.nature_records&(1U<<entry));
    }
    Text nature_requirement(int entry) const;
    Text nature_wait() const;
    bool wait_for_nature();
    bool record_nature(int entry);
    int stock(int good) const;
    int market_stock(int port, int good) const;
    int wind(int leg) const;
    int speed_in_tenths_of_a_knot() const;
    PassageForecast forecast(const Passage& path, int extra_load=0) const;
    PassageForecast remaining_forecast() const;
    std::array<ForecastDay, ForecastDays> local_forecast() const;
    int sailing_target() const;
    int daily_rations() const;
    int daily_water(SeaWeather weather) const;
    SailingRation sailing_ration(int day, SeaWeather weather) const;
    bool rationing(RationFlag flag) const { return s.ration_flags&flag; }
    void toggle_ration(RationFlag flag);
    bool tow_needed() const;
    bool request_tow();
    int resupply_cost(int food, int water) const;
    int voyage_supply_target(int need) const { return need+2*daily_rations(); }
    bool resupply(int food, int water);
    bool unload_provisions(bool water);
    SeaConditions conditions() const { return conditions_at(position(),s.day); }
    int pilot_region() const;
    int starting_pilot_patch() const;
    PilotRegion pilot_area() const;
    Point pilot_atlas_position() const;
    Point pilot_atlas_site(int site,bool berth) const;
    PilotSite pilot_site(int site) const;
    bool pilot_site_visible(int site) const;
    bool pilot_charted(int region,Point local) const;
    bool reveal_pilot_chart();
    bool pilot_site_visited(int site) const;
    int pilot_coastal_hint() const;
    bool record_pilot_coast(int site);
    bool begin_pilotage(int index,PilotWater map);
    bool pilot_exiting() const { return pilot_exit.x>=0; }
    Point pilot_exit_position() const { return pilot_exit; }
    bool connect_pilotage(PilotWater map);
    bool refine_pilotage(int region,PilotWater map);
    bool pilot_slow_water(PilotWater map) const;
    bool leave_pilotage(WaterTest ocean);
    void set_pilot_heading(int heading);
    int pilot_speed(PilotWater map,int near_berth=-1) const;
    PilotCurrent pilot_current() const;
    bool pilot_tick(PilotWater map);
    int pilot_berth(PilotWater map,int preferred=-1) const;
    PilotApproach pilot_approach(int site,PilotWater map) const;
    bool begin_pilot_docking(PilotWater map,int preferred);
    void cancel_pilot_docking() { pilot_dock_target=-1; }
    int pilot_docking() const { return pilot_dock_target; }
    void cast_off_pilotage();
    PilotSupplies pilot_supply_offer(bool water) const;
    bool pilot_supplies(bool water);
    int pilot_parcel_destination() const;
    bool pilot_parcel_at(int site) const;
    bool handle_pilot_parcel();
    bool start_helm(int destination);
    bool take_over_helm();
    void helm_step(WaterTest water_at);
    void helm_pulses(WaterTest water_at, int count);
    int visible_landmark() const;
    HarborApproach harbor_approach(int port, WaterTest water_at) const;
    int nearby_anchorage(WaterTest water_at=nullptr) const;
    bool dock_helm(WaterTest water_at=nullptr);
    Point wake_point(int index) const;
    int reward(int port) const;
    int ration_cost() const;
    int repair_cost() const;
    int water_price() const;
    int tale_stop(int tale) const;
    int tale_here() const;
    Text tale_requirement(int tale) const;
    Text encounter_requirement(int choice) const;
    bool choose_tale(int tale, bool share);
    int proposal() const;
    int proposal_good() const;
    bool port() const { return !sea() && !encounter() && !land_away() && !s.land_fight.kind && is_port(s.origin); }
    bool sea() const { return s.helm || s.destination>=0; }
    bool encounter() const { return s.weather != 0; }
    Point position(float frame = 0) const;
    bool depart(int to);
    void advance();
    void turn_back();
    void wait_at_port();
    void restore();
    TradeQuote trade(int good, bool sell, int quantity);
    void service(int action);
    void take_contract();
    void deliver();
    int dispatch_order() const { return int(s.dispatch.number)-1; }
    int dispatch_target() const;
    int dispatch_space() const;
    bool dispatch_board_here() const;
    bool dispatch_offer_here(int id) const;
    DispatchQuote dispatch_quote(int id) const;
    Text dispatch_requirement(int id) const;
    bool accept_dispatch(int id);
    bool deliver_dispatch();
    bool cancel_dispatch();
    void survey();
    void learn_ocean_routes();
    Text field_requirement(int study) const;
    bool sketch_field(int study,int type);
    bool hunt_field(int study);
    bool has_shore_supply() const;
    bool shore_resupply(bool water);
    int shore_provision_amount(bool water) const;
    void chapter_choice(int choice);
    void navigator();
    void encounter_action(int action);
    bool begin_skirmish(WaterTest water);
    int raid_target(WaterTest water) const;
    Text merchant_jurisdiction(int company) const;
    bool begin_merchant_raid(int company,WaterTest water);
    int piracy_docket_until() const;
    int port_settlement_cost() const;
    bool fire_skirmish(int side);
    bool drop_drift_rope();
    bool can_board_skirmish() const;
    bool board_skirmish();
    unsigned tick_skirmish(WaterTest water);
    int fishery_available() const;
    bool begin_fishery(WaterTest water);
    bool whale_at_surface() const;
    Text fishery_action_requirement() const;
    bool work_fishery();
    bool cancel_fishery();
    int whale_oil_quote() const;
    bool sell_whale_oil();
    int whale_rendering_cost() const;
    bool render_whale_oil();
    int enclave_here() const;
    int enclave_nearby(WaterTest water) const;
    Text enclave_lease_requirement(int site) const;
    bool lease_enclave(int site);
    Text enclave_build_requirement(int site,int building) const;
    bool build_enclave(int site,int building);
    int enclave_export_quote(int site) const;
    bool sell_enclave_exports(int site);
    void wait_enclave_day();
    bool supply_enclave(int site,int action);
    bool hire_enclave_people(int site,bool carpenter);
    bool take_enclave_migrants(int site);
    bool land_enclave_migrants(int site);
    Text enclave_siege_requirement(int site) const;
    bool siege_enclave(int site);
    Text land_battle_requirement(int kind,int site=0) const;
    bool begin_land_battle(int kind,int site=0);
    bool move_land_unit(int unit,int cell);
    bool strike_land_unit(int unit,int target);
    bool brace_land_unit(int unit);
    bool end_land_round();
    void withdraw_land_battle();
    bool decide_enclave_prisoners(int site,int choice);
    bool duel() const { return s.weather==Boarding || s.weather==DockDuel; }
    bool convoy_battle() const;
    int naval_intent() const;
    int duel_intent() const;
    int vigor_limit() const;
    int rival_limit() const;
    bool accept_escort();
    bool deliver_escort(bool publish);
    bool start_dock_duel();
    bool guild_stake(int port) const;
    int guild_cost() const;
    bool invest();
    int berth_port() const { return int(s.berth.port)-1; }
    bool at_berth() const { return port() && berth_port()==s.origin; }
    Text lease_requirement() const;
    bool open_berth();
    int reserve_value() const;
    bool purchase_reserve(int vessel);
    Text swap_requirement() const;
    bool swap_vessel();
    bool sell_reserve();
    Text company_requirement() const;
    bool change_company();
    bool has_royal_pass(RoyalCourt court) const {
        return court!=NoCourt && (s.royal_passes&(1U<<court));
    }
    bool display_colors(int colors);
    Text royal_pass_requirement() const;
    bool receive_royal_pass();
    static constexpr int RoyalPassCost=90;
    int shore_task_here() const;
    Text shore_requirement(int task) const;
    bool advance_shore_task(int task);
    int port_hold_until() const;
    bool market_held() const { return port() && port_hold_until()>s.day; }
    static constexpr int PortSettlement=60, AdenWaterPrice=18;
    bool settle_port_case();
    int archive_here() const;
    int expedition_here() const;
    bool read_expedition(int expedition);
    bool consult_witness(int expedition);
    bool launch_expedition(int expedition, bool sponsor);
    Text observation_block(int expedition) const;
    bool wait_to_observe(int expedition);
    bool observe(int expedition, int method);
    int expedition_purse(int expedition) const;
    bool file_expedition(int expedition);
    bool has_officer(int officer) const;
    int hiring_fee(int officer) const;
    bool hire_officer(int officer);
    bool assign_officer(int officer);
    Fluency fluency(Language language) const;
    int crew_interpreter(Language language) const;
    bool can_study(Language language) const;
    bool local_interpreter(Language language) const;
    bool understands(Language language) const;
    int lesson_fee(Language language) const;
    int lesson_days(Language language) const;
    static constexpr int InterpreterFee=24;
    bool hire_interpreter();
    bool take_lesson(Language language);
    bool tide_ledger_here() const;
    bool copy_tide_ledger();
    bool decide_tide_ledger(bool publish);
    bool take_drift_lead();
    Point drift_position() const;
    bool drift_in_range(int radius) const;
    bool sees_drift() const;
    bool near_drift() const;
    Text drift_block(DriftMethod method) const;
    bool recover_drift(DriftMethod method);
    bool wait_for_drift();
    bool finish_drift(bool return_to_crew);
    bool charted(int place) const;
    bool chart_cell(int column, int row) const;
    bool heard_rumor(int id) const;
    bool hear_rumor(int id);
    PortStoryStage port_story_stage(int id) const;
    bool meet_port_story(int id);
    Text port_story_requirement(int id, int choice) const;
    bool resolve_port_story(int id, int choice);
private:
    bool settle_land_battle(bool retreat=false, bool after_enemy_turn=false);
    void resolve_enclave_siege(int site,bool victory);
    Point pilot_exit{-1,-1};
    Point pilot_remainder{}, pilot_dock_from{};
    float pilot_dock_travel=0;
    int pilot_dock_target=-1;
    bool finish_pilot_docking(int site);
    bool reveal_pilot_area(int index,Point center,float radius);
    PassageForecast forecast_from(const Passage& path, int first_leg, int progress, int end, int cargo) const;
    int next_route_pace() const;
    unsigned roll(unsigned range);
    void arrive();
    void inspect_colors();
    void wear_company_hull(int loss);
    void rescue();
    void finish_encounter();
    void finish_dispatch(DispatchOutcome outcome);
    bool on_leg(int from, int to) const;
    int regional_encounter(Point here) const;
    void resolve_regional_encounter(int choice);
    void start_encounter(int kind);
    void start_duel(int kind);
    void naval_action(int action);
    void duel_action(int action);
    void win_at_sea(bool boarded);
    void lose_duel(bool yielded);
    void log_encounter(int choice);
    Point provisions_order(int food, int water) const;
    int port_hold_until(int port) const;
    int market_price(int port, int good, bool sell, int supply, int day, bool player=true) const;
    TradeQuote quote_order(int good, bool sell, int quantity, int rate, bool limit_gold) const;
    TradeQuote quote_market(int port, int good, bool sell, int quantity, int budget, int day,
                            int rate=0, bool player=false) const;
    void update_market(int port, int good, int stock_change);
    void apply_trade(int good, bool sell, TradeQuote offer);
    void merchant_day(int company);
    void plan_merchant(int company);
    bool sell_merchant_cargo(MerchantVoyage& ship);

    bool advance_watch();
    void spend_day(int n);
    void earn(int gold);
    void check_ship();
    bool consume_day(SeaConditions weather);
    void clear_helm();
    void helm_move(WaterTest water_at, int pulses);
    void tick_fishery(WaterTest water);
    void tick_enclaves();
    bool start_patrol_interception(Point position);
    void resolve_special_skirmish();
    bool observation_day();
    void reveal_chart(Point center, int radius);
    int lookout_radius() const { return s.first_mate==Pilot?19:14; }
    int sighting(Point center);
    void chart_passage(const Leg& leg, int progress);
    void record_wake();
    int anchorage_at(Point position, WaterTest water_at) const;
};

} // namespace tide
