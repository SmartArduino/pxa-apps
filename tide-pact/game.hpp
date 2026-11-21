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
     "航路每日多行二十海里。掌舵更快，触礁少伤船。",
     "小时候我在塔霍河替渡船测深。把铅锤给我，我能听出船底离沙洲有多远。",{Portuguese,French}},
    {madeira,140,"贝阿特丽丝","司务",
     "每航行或考察一日少用一份淡水，至少仍用一份。",
     "糖仓漏过一桶水，此后我给每个桶盖都刻了记号。船上也可以照办。",{Portuguese,Castilian}},
    {cadiz,160,"杜阿尔特","甲板护卫",
     "敌炮每次少伤三点船体。接舷决斗体力增加六点。",
     "两船靠得再近，也等水手把缆钩扣牢了再跳。这条命是我在直布罗陀学回来的。",{Castilian,Arabic}}
};
static_assert(std::size(Officers)==OfficerLimit);

struct Chapter {
    int place, portrait;
    Text title, speaker, message, first, second;
};
inline constexpr Chapter Story[] {
    {lisbon, 0, "墨迹未干", "伊内丝",
     "你兄长失踪前把海图交给了我。海图上有一盏灯，明明该在海里。去大加那利岛找莱昂，记住：潮汐不会说谎，人会。",
     "独自出资", "收资助 +220金"},
    {canaries, 1, "逆风的灯", "莱昂",
     "码头有人买走了旧灯塔的图纸。你兄长留下半张货单，让我交给会救人的船长。昨夜有条渔船没回来。",
     "搜寻渔船 +2日", "买下线索 -70金"},
    {elmina, 3, "两本账簿", "阿杜瓦",
     "一批货被贴上了两个港口的印记。你兄长发现，有人用假灯引船触礁，再低价收购残货。证据在好望角外的蜡封木筒里。",
     "替她捎信", "签下雇船约"},
    {cape, 0, "风暴之角", "蜡封信",
     "木筒里没有金银，只有一张极细的星图。背面的字迹说：我仍活着。去基尔瓦，别把星图交给只认印章的人。",
     "记录灯标", "先封存星图"},
    {kilwa, 4, "星图上的约定", "萨利姆",
     "星图的墨迹标着几座港口公用的潮时。你兄长留下两份副本，人已乘商船离开。我们可以在本地印行，让每艘船都能认出假灯。",
     "答应公开", "保留专营权"},
    {lisbon, 5, "潮汐之约", "埃斯特旺",
     "我等这张图等了两年。你可以拿走这袋钱，也可以把各港商人的签名钉在码头，让每一间商栈都看得到。",
     "公布航图", "签独家合约"},
};
inline constexpr int Chapters = std::size(Story);
inline constexpr Text Endings[] {
    "各港抄录潮时、纠正灯标。失踪的船变少了。莱昂在来信里说，你的兄长终于搭上一艘认得新灯的归船。",
    "你的商号获得专营费。潮图锁进仓库，但萨利姆保住了另一份副本。次年，一艘无旗小船把它带回里斯本。"
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
};

inline constexpr std::size_t LegacySaveBytes=320, CombatSaveBytes=336, HelmSaveBytes=352;
inline constexpr std::size_t OfficerSaveBytes=376, CoastSaveBytes=960, WakeSaveBytes=1216, SaveBytes=1232;
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
    bool charted(int place) const;
    bool chart_cell(int column, int row) const;
    bool heard_rumor(int id) const;
    bool hear_rumor(int id);
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
    int market_price(int port, int good, bool sell, int supply) const;

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
