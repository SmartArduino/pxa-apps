#pragma once

#include <array>
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
inline constexpr int PlaceCount = std::size(Places);

} // namespace tide

#include "expeditions.hpp"
#include "languages.hpp"
#include "port_stories.hpp"
#include "rumors.hpp"
#include "stories.hpp"

namespace tide {

enum Goods { Wine, Sugar, Cloth, Wool, Timber, Salt, Pepper, Porcelain, GoodsCount };
inline constexpr std::array<Text, GoodsCount> GoodsNames{
    "葡萄酒", "蔗糖", "织锦", "羊毛", "木料", "海盐", "胡椒", "瓷器"
};

enum OfficerId { NoOfficer, Pilot, Quartermaster, Boatswain, OfficerLimit };
struct Officer {
    int port, fee;
    Text name, specialty, effect, greeting;
    std::array<Language,2> languages;
};
inline constexpr Officer Officers[]{
    {},
    {lisbon,95,"安娜","领航",
     "走航线时每天多行二十海里。自由掌舵时更快，触礁也少受些伤。",
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

enum SeaWeather { Fair, Heat, Rain, Squall };
struct SeaConditions {
    int direction, strength;
    SeaWeather weather;
};
inline constexpr Point Directions[]{{0,-16},{11,-11},{16,0},{11,11},{0,16},{-11,11},{-16,0},{-11,-11}};
inline constexpr Text Bearings[]{"北","东北","东","东南","南","西南","西","西北"};
inline constexpr Text SeaWeatherNames[]{"晴","炎热","降雨","骤风"};
enum DriftStage : std::uint8_t { DriftUnheard, DriftSearching, DriftRecovered, DriftReturned, DriftFiled };
enum DriftMethod { LaunchBoat, RigHook };
inline constexpr Point DriftGround{174,335};
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

struct PassageForecast {
    std::array<int, PlaceCount> arrival_days{};
    int days=0;
    int provisions=0, water=0;
};

struct TradeQuote {
    int units=0;
    int gold=0;
    int stock_change=0;
    int saved=0;
};
inline constexpr int TradeLot=5;
inline constexpr int BargainUsed=GoodsCount+1;

Passage passage(int from, int to);
Point vessel_at(const Passage& route, int leg, float fraction);

inline constexpr std::array MerchantPorts{lisbon,madeira,canaries};
inline constexpr std::array<Text,2> MerchantNames{"晨雾商行","白鹭商号"};
inline constexpr int NoMerchantPort=MerchantPorts.size(), MerchantLot=5, MerchantReserve=6000;

struct MerchantVoyage {
    int progress=0, gold=650;
    std::uint8_t port=0, destination=NoMerchantPort, good=Wine, units=0, held=0;
};

struct State {
    int gold = 520, day = 0, hull = 100, stores = 74, spirits = 78;
    int ship = 0, guns = 0, crew = 12, charts = 0, renown = 0, trust = 0;
    int origin = lisbon, destination = -1, leg = 0, progress = 0, reefed = 0;
    int chapter = 0, ending = 0, patron = 0, allies = 0;
    int contract = -1, contract_good = 0, deadline = 0;
    int weather = 0, enemy = 0, range = 0, aim = 0, stop_next = 0;
    std::uint32_t visited = 1, surveyed = 0, rng = 0x57980218, event_mask = 0;
    std::array<int, GoodsCount> cargo{0, 0, 0, 0, 0, 2, 0, 0};
    std::array<std::int8_t, PortCount * GoodsCount> stock_delta{};
    std::array<std::uint8_t, TaleCount> tales{};
    std::uint8_t beacon = BeaconUnseen;
    std::array<std::uint32_t, 3> sea_log{};
    std::uint8_t naval_turn=0, needs_reload=0, duel_turn=0, vigor=0, rival=0;
    std::uint8_t escort=EscortOpen, cadiz_victory=0;
    std::uint32_t guild_stakes=0;
    int helm_x=0, helm_y=0;
    std::uint8_t water=74, helm=0, heading=0, helm_pulse=0;
    std::array<ExpeditionProgress, ExpeditionCount> expeditions{};
    std::uint8_t officer_roster=0, first_mate=NoOfficer;
    std::uint32_t chart_marks=StartingChart, heard_rumors=0;
    std::array<std::uint8_t, ChartBytes> coast{};
    std::array<Point,WakeCapacity> wake{};
    std::uint8_t wake_head=0, wake_count=0;
    std::uint32_t language_skills=0;
    int interpreter_port=-1;
    std::uint8_t tide_ledger=LedgerUnseen;
    std::uint8_t bargain_good=0, bargain_rate=0;
    std::uint8_t drift_stage=DriftUnheard;
    std::uint8_t port_stories=0;
    std::array<MerchantVoyage,MerchantNames.size()> merchants{{{.port=0},{.port=1}}};
};

inline constexpr std::size_t LegacySaveBytes=320, CombatSaveBytes=336, HelmSaveBytes=352;
inline constexpr std::size_t OfficerSaveBytes=376, CoastSaveBytes=960, WakeSaveBytes=1216;
inline constexpr std::size_t LanguageSaveBytes=1232, SaveBytes=1264;
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
    int quote(int good, bool sell) const;
    int price_at(int port, int good, bool sell) const;
    Point merchant_position(int company) const;
    TradeQuote trade_quote(int good, bool sell, int quantity) const;
    int negotiation_rate() const;
    bool can_negotiate(int good) const;
    bool negotiate(int good);
    int stock(int good) const;
    int wind(int leg) const;
    PassageForecast forecast(const Passage& path) const;
    int daily_rations() const;
    int daily_water(SeaWeather weather) const;
    bool tow_needed() const;
    bool request_tow();
    int resupply_cost(int food, int water) const;
    bool resupply(int food, int water);
    bool unload_provisions(bool water);
    SeaConditions conditions() const { return conditions_at(position(),s.day); }
    bool start_helm(int destination);
    void helm_step(WaterTest water_at);
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
    bool port() const { return !sea() && !encounter() && s.origin<PortCount; }
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
    void survey();
    void chapter_choice(int choice);
    void navigator();
    void encounter_action(int action);
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
    unsigned roll(unsigned range);
    void arrive();
    void rescue();
    void finish_encounter();
    bool on_leg(int from, int to) const;
    void start_encounter(int kind);
    void start_duel(int kind);
    void naval_action(int action);
    void duel_action(int action);
    void win_at_sea(bool boarded);
    void lose_duel(bool yielded);
    void log_encounter(int choice);
    Point provisions_order(int food, int water) const;
    int stock_at(int port, int good, int stock_change) const;
    int market_price(int port, int good, bool sell, int supply, bool player=true) const;
    void merchant_day(int company);
    void plan_merchant(int company);

    void spend_day(int n);
    void earn(int gold);
    void check_ship();
    bool consume_day(SeaConditions weather);
    void clear_helm();
    bool observation_day();
    void reveal_chart(Point center, int radius);
    int lookout_radius() const { return s.first_mate==Pilot?19:14; }
    int sighting(Point center);
    void chart_passage(const Leg& leg, int progress);
    void record_wake();
    int anchorage_at(Point position, WaterTest water_at) const;
};

} // namespace tide
