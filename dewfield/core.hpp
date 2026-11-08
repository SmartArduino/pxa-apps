// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
namespace farm {
constexpr int MapW = 32, MapH = 24, Tile = 16, PlotX = 10, PlotY = 9,
              PlotW = 10, PlotH = 8;
constexpr int PlotCount = PlotW * PlotH, SaveBytes = 1280;
// Device firmware may cap individual Storage values below the SDK maximum.
constexpr int StorageSaveBytes = 960, SaveDataBytes = 1117;
inline int walk_frame(uint32_t distance) {return 1+int((distance/6)%8);}
constexpr int InventorySize = 56, HotbarSize = 7, ItemCount = 45;
enum class Tool : uint8_t { Hoe, Seeds, Water, Hand, Axe, Pick, Rod, Sword, Count };
enum class Ground : uint8_t { Grass, Path, Water, Wall, Tree, Rock, Weed };
enum class Result : uint8_t {
  None,
  Tilled,
  Planted,
  Watered,
  Harvested,
  Cleared,
  Far,
  Blocked,
  NoEnergy,
  NoSeeds,
  NoWater,
  Refilled,
  NotReady,
  Shop,
  Sleep,
  Ship,
  Talk,
  Fish,
  Sprinkled,
  Struck,
  Travel,
  Locked,
  Dialogue,
  Foraged,
  Orders,
  Kitchen, Calendar, Forecast, Closed, WrongSeason, EmptyShipping, FullFish, Ranch, Animal, Attacked, MineForaged, MineStairs, MineLift, MineChest, NeedSword, Cooldown
};
struct Crop {
  const char *cn, *en;
  uint8_t days, price, sell, regrow;
};
inline constexpr Crop Crops[] = {{"萝卜", "Radish", 2, 5, 16, 0},
                                 {"土豆", "Potato", 3, 9, 30, 0},
                                 {"番茄", "Tomato", 4, 15, 34, 2},
                                 {"南瓜", "Pumpkin", 5, 22, 75, 0}};
struct Plot {
  uint8_t tilled = 0, crop = 0, growth = 0, wet = 0;
};
constexpr int AnimalMax=12, IncubatorMax=3, IntroScenes=5;
enum class Species : uint8_t { None, Chicken, Duck, Cow, Sheep, Pig };
struct Animal {
  uint8_t kind=0, male=0, age=0, heart=50, pregnancy=0, cooldown=0, products=0;
  uint16_t pet_day=0, fed_day=0;
};
struct Incubator { uint8_t kind=0, days=0; };
constexpr int WildTreeCount=25, MineFloors=30, MineRocks=24, MineMobs=6;
struct MineMob {
  uint16_t x=0,y=0,cooldown=0,windup=0,move_acc=0;
  uint8_t hp=0,dir=2;
};
struct State {
  Plot plots[PlotCount]{};
  uint8_t cleared[24]{}, sprinklers[10]{};
  uint16_t seeds[4]{12, 4, 0, 0}, produce[4]{}, fish = 0, wood = 0, stone = 0;
  uint32_t gold = 100, shipping = 0, earned = 0, harvested = 0, rng = 0x1248bad;
  uint16_t day = 1, minute = 360, energy = 100, water = 30, capacity = 30;
  uint16_t px = 14 * Tile + 8, py = 14 * Tile + 8;
  uint8_t selected = 0, tool = 0, quests = 0, friendship = 0;
  uint8_t tool_grade[7]{}, damage[24]{};
  uint8_t slots[InventorySize]{1, 2, 3, 4, 5, 6, 7, 8, 9};
  uint8_t active_slot = 0;
  uint8_t tree_age[24]{};
  bool show_joystick = true, show_action = true, music = true;
  bool auto_run = true, sound_effects = true;
  uint16_t last_shipped_count=0;
  uint8_t friend_chapter[4]{};
  bool rain = false;
  uint16_t visited=1;
  uint8_t region = 0, story = 0, met = 0, bonds[4]{}, personal = 0,
          order_claimed = 0;
  uint16_t forage = 0, gathered[3]{}, meals[2]{}, order_day = 0, orders_done = 0;
  uint16_t seasonal[5]{};
  uint8_t weather = 0, overwork = 0, last_shipped = 0;
  bool exhausted = false;
  uint16_t chat_day[4]{}, gift_day[4]{};
  uint32_t discoveries = 0;
  uint8_t fish_records[8]{}, collection_rewards = 0;
  uint16_t fish_caught = 0, perfect_catches = 0;
  Animal animals[AnimalMax]{};
  Incubator incubators[IncubatorMax]{};
  uint16_t animal_goods[8]{}, hay=0, trough=0, hatched=0, born=0, animal_collected=0;
  uint8_t intro=0, ranch=0, ranch_chapter=0;
  bool breeding=true;
  uint8_t wild_age[WildTreeCount]{},wild_damage[WildTreeCount]{};
  uint16_t minerals[8]{};
  uint32_t mine_cleared[MineFloors]{};
  uint8_t mine_defeated[MineFloors]{},ore_damage[MineRocks]{};
  MineMob mobs[MineMobs]{};
  uint32_t mine_seed=0x63a5be19;
  uint16_t mine_day=0,monsters_slain=0,ore_mined=0,cave_gathered=0,invulnerable=0,attack_cooldown=0;
  uint8_t mine_depth=0,mine_best=0,mine_loaded=0,health=100,mine_rewards=0,sword_grade=0;
  bool sword_owned=false;
};
struct Obstacle {
  uint8_t x, y, kind;
};
inline constexpr Obstacle Obstacles[] = {
    {10, 9, 0},  {12, 9, 1},  {14, 9, 2},  {16, 10, 0}, {18, 10, 1},
    {11, 11, 2}, {16, 13, 1}, {19, 14, 0}, {10, 16, 1}, {18, 16, 2},
    {7, 14, 0},  {6, 18, 1},  {23, 9, 0},  {25, 11, 1}, {21, 18, 2},
    {8, 20, 0},  {3, 12, 0},  {28, 6, 0},  {26, 20, 0}, {4, 20, 0},
    {17, 21, 1}, {22, 6, 2},  {8, 7, 2},   {21, 14, 2}};
int plot_index(int x, int y);
int obstacle_index(int x, int y);
constexpr int scenery_variant(int x, int y) {
  return (x * 17 + y * 23 + 3) & 3;
}
int obstacle_hp(int obstacle);
int tree_stage(const State &s, int obstacle);
int strikes_left(const State &s, int obstacle, Tool tool);
unsigned item_count(const State &s, int item);
bool change_item(State &s, int item, int amount);
void sync_inventory(State &s);
bool select_slot(State &s, int slot);
bool swap_slots(State &s, int from, int to);
bool equip_item(State &s, int item);
struct ToolCost {
  uint16_t gold, wood, stone;
};
ToolCost tool_cost(const State &s, Tool tool);
bool upgrade_tool(State &s, Tool tool);
Ground ground(const State &s, int x, int y);
bool walkable(const State &s, int x, int y);
bool near(const State &s, int x, int y);
void fresh(State &s);
Result act(State &s, int x, int y);
uint32_t sleep(State &s, bool forced = false);
constexpr int SeasonDays = 28;
inline int season_of(uint16_t day) { return ((day - 1) / SeasonDays) % 4; }
inline int season_day(uint16_t day) { return (day - 1) % SeasonDays + 1; }
inline bool indoors(uint8_t region) { return region == 4 || region == 6 || region == 7 || region == 8; }
inline bool carrying_item(int item){return item==2||(item>=8&&item!=37);}
inline int tool_grade(const State&s,int tool){return tool==7?s.sword_grade:s.tool_grade[tool<7?tool:3];}
bool buy_seed(State &s, int kind, int count = 1);
bool upgrade(State &s);
bool craft_sprinkler(State &s, int plot);
uint32_t ship(State &s);
uint32_t ship_items(State &s,int item,unsigned count);
bool unship(State &s);
uint32_t quest(State &s);
uint32_t random(State &s);
size_t encode(const State &s, uint8_t *out, size_t cap);
size_t encode_storage(const State &s, uint8_t *out, size_t cap);
bool decode(State &s, const uint8_t *in, size_t n);
// Bounded BFS; returns the first reachable step toward a tile or its neighbor.
bool next_step(const State &s, int tx, int ty, int &nx, int &ny, bool adjacent);
} // namespace farm
