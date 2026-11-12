// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
namespace md {
constexpr int W = 48, H = 48, MaxBuildings = 160, MaxEnemies = 40,
              MaxShots = 64, MaxUnits = 8, MaxPlans = 48;
constexpr float BuildRange = 220.f / 8.f;
constexpr float MineRange = 70.f / 8.f, ItemTransferRange = 220.f / 8.f;
constexpr float PlayerSpeed = 3.f * 3.f / 8.f, ViewAttractRange = 15.f / 8.f;
constexpr uint8_t CarryCapacity = 30;
constexpr uint16_t BuildComplete = 60000;
constexpr uint16_t WorkStep = 48; // Three original 60 Hz frames, in Q4.
constexpr uint16_t FactoryTime = 900 * 16; // 15 seconds in original frame Q4.
constexpr uint16_t FullPower = 1024,
                   BatteryCapacity = 40000; // 0.1 energy units.
enum Item : uint8_t {
  Copper,
  Lead,
  Coal,
  Sand,
  Graphite,
  Silicon,
  Metaglass,
  Titanium,
  Items,
  None = 255
};
enum Ground : uint8_t { Stone, Sandy, Water, Rock };
// Append content: identities 0..14 are shared with the 0.1 save format.
enum Block : uint8_t {
  Empty,
  Core,
  Drill,
  Conveyor,
  Router,
  Junction,
  Duo,
  Wall,
  Press,
  Smelter,
  Generator,
  Node,
  Battery,
  Mender,
  Arc,
  Pneumatic,
  Kiln,
  Pump,
  Conduit,
  LiquidRouter,
  Tank,
  GroundFactory,
  AirFactory,
  Blocks
};
constexpr float blockRange(Block b) {
  return b == Duo      ? 20.f
         : b == Arc    ? 11.25f
         : b == Node   ? 6.f
         : b == Mender ? 5.f
                       : 0.f;
}
// Identities match the app's original-v154 recording table.
enum class Sound : uint8_t {
  Click, Wave, DuoShot, Explosion, Place, Break, UnitCreate, Unlock,
  AlphaShot, ArcShot, UnitShot, Heal, BuildLoop, MineLoop, DrillLoop,
  SmeltLoop, FactoryLoop, Count
};
struct SoundEvent { Sound kind; float x, y; };
constexpr unsigned MaxSoundEvents = 16;
enum class Command : uint8_t { Defend, Rally, Attack };
struct Def {
  const char *en, *cn;
  uint8_t size, category;
  uint16_t hp;
  uint16_t cost[Items];
};
extern const Def defs[Blocks];
struct Tile {
  uint8_t ground, ore;
};
struct DrillOre {
  Item item = None;
  uint8_t count = 0;
  uint16_t delay = 0; // Original 60 Hz frames, including item hardness.
};
struct ConveyorTiling {
  uint8_t shape = 0, connections = 0;
  bool flip = false;
};
struct Building {
  uint8_t type = 0, x = 0, y = 0, dir = 0;
  uint16_t hp = 0;
  uint8_t inv[Items]{};
  uint16_t timer = 0;
  uint8_t cursor = 0, ammoType = 0, ammoLeft = 0;
  bool powered = false;
  uint16_t powerEfficiency = 0; // Q10, derived from the local power network.
  uint16_t water = 0, progress = 0; // water is in tenths of a liquid unit.
  // Drill timer is warmup / 1000; progress is original frames in Q5.
  // Ore selection is rebuilt only when terrain/occupancy changes.
  DrillOre ore{};
  float drillRotation = 0; // Cosmetic; never part of the save payload.
  float drillRate = 0;
  uint8_t conveyorClog = 0; // Derived half-second animation stop, not saved.
  // 0 operational, 1 queued ghost, 2 construction, 3 deconstruction.
  // Sites reuse timer/progress/inv for initial work/current work/paid
  // materials.
  uint8_t buildState = 0;
};
struct Enemy {
  float x = 0, y = 0;
  uint16_t hp = 0;
  uint8_t kind = 0;
  uint16_t timer = 0;
};
struct Unit {
  float x = 0, y = 0;
  uint16_t hp = 0;
  uint8_t kind = 0, cooldown = 0, burst = 0, burstTimer = 0;
};
struct Shot {
  float x = 0, y = 0, tx = 0, ty = 0;
  uint8_t life = 0;
  bool hostile = false;
};
struct State {
  uint32_t seed = 1, ticks = 0, researched = 0;
  uint16_t wave = 0, countdown = 1200, stock[Items]{};
  uint16_t kills = 0;
  uint8_t sector = 0, terrainVersion = 2, captured = 0;
  bool sandbox = false, won = false, lost = false, muted = false;
  uint8_t musicMode = 0; // 0 original intermittent, 1 continuous, 2 off.
  float playerX = 24.5f, playerY = 38.5f, targetX = 24.5f, targetY = 38.5f;
  uint16_t playerHP = 150;
  Item carriedItem = None;
  uint8_t carriedCount = 0;
  int8_t mineX = -1, mineY = -1;
  uint16_t mineProgress = 0; // Twice the original 60 Hz mineTimer.
  bool mining = false;       // Derived visual state, never serialized.
  bool followingView = false; // Mobile camera attraction, not serialized.
  Command command = Command::Defend;
  float rallyX = 24.5f, rallyY = 31.5f;
  Building buildings[MaxBuildings]{};
  Enemy enemies[MaxEnemies]{};
  Unit units[MaxUnits]{};
  Shot shots[MaxShots]{};
  Tile tiles[W * H]{};
  int16_t occupancy[W * H]{};
  uint16_t flow[W * H]{};
  uint16_t unitFlow[W * H]{};
  int16_t flowGoal = -1, unitRequestedGoal = -1, unitPathGoal = -1;
  bool flowDirty = true, unitNavDirty = true;
  uint32_t flowRebuilds = 0, unitNavRebuilds = 0;
  uint16_t powerProduced = 0, powerUsed = 0;
  SoundEvent soundEvents[MaxSoundEvents]{};
  uint8_t soundCount = 0;
  void emit(Sound kind, float x, float y);
  uint8_t buildQueue[MaxPlans]{}, buildCount = 0;
  int16_t builder = -1; // Current beam target; derived each simulation step.
  bool powerDirty = true;
  int16_t powerGroup[MaxBuildings]{};
  uint8_t powerIds[MaxBuildings]{}, powerCount = 0;
  uint32_t powerRebuilds = 0; // Derived diagnostics, not part of the save.
  void generate(uint32_t value, uint8_t map = 0, bool free = false);
  void terrain();
  void rebuild();
  int at(int x, int y) const;
  bool canBuild(Block b, int x, int y) const;
  DrillOre drillOre(Block b, int x, int y) const;
  int build(Block b, int x, int y, int dir, bool free = false);
  int buildLine(Block b, int x0, int y0, int x1, int y1, int direction = 0);
  int queueBuild(Block b, int x, int y, int direction = 0);
  int queueLine(Block b, int x0, int y0, int x1, int y1, int direction = 0);
  bool queueRemove(int id);
  void constructionStep();
  void powerStep();
  void productionStep(int id);
  bool remove(int id);
  void step();
  bool startWave();
  bool research(Block b);
  bool unlocked(Block b) const;
  int alive() const;
  int unitCount() const;
  int core() const;
  bool accept(int id, Item item, int dir);
  void supply(int id);
  bool mine(int x, int y);
  bool canMine(int x, int y) const;
  void stopMining();
  void movePlayer(float x, float y);
  void followView(float x, float y);
  ConveyorTiling conveyorTiling(int x, int y, int direction,
                                bool includePlans = false) const;
  void miningStep();
  bool walkable(int x, int y) const;
  void liquidStep();
  void unitStep();
  bool spawnUnit(int factory);
  size_t save(uint8_t *data, size_t cap) const;
  bool load(const uint8_t *data, size_t size);
};
uint32_t noise(uint32_t s, int x, int y);
uint16_t liquidCapacity(Block b);
Block prerequisite(Block b);
uint16_t researchCost(Block b, Item item);
float buildTime(Block b);
struct Recipe {
  Item first, second, output;
  uint8_t firstCount, secondCount;
  uint16_t duration;
};
const Recipe *recipe(Block b);
uint16_t processTime(Block b);
bool canProcess(const Building &b);
uint16_t powerDemand(const Building &b);
bool powerConnects(const Building &a, const Building &b);
} // namespace md
