// SPDX-License-Identifier: MIT
#include "core.hpp"
#include "world.hpp"
#include "living.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
namespace farm {
int plot_index(int x, int y) {
  return x >= PlotX && x < PlotX + PlotW && y >= PlotY && y < PlotY + PlotH
             ? (y - PlotY) * PlotW + x - PlotX
             : -1;
}
int obstacle_index(int x, int y) {
  for (int i = 0; i < 24; ++i)
    if (Obstacles[i].x == x && Obstacles[i].y == y)
      return i;
  return -1;
}
Ground ground(const State &s, int x, int y) {
  if (s.region)
    return world_ground(s, x, y);
  if (x < 1 || y < 1 || x >= MapW - 1 || y >= MapH - 1)
    return Ground::Wall;
  if ((x >= 4 && x <= 8 && y >= 3 && y <= 6))
    return Ground::Wall;
  if (x >= 24 && x <= 29 && y >= 14 && y <= 20 &&
      !(x == 24 && (y == 14 || y == 20)) && !(x == 29 && (y == 14 || y == 20)))
    return Ground::Water;
  if ((x == 21 && y == 8) || (x == 8 && y == 9) || (x == 7 && y == 8))
    return Ground::Wall;
  if ((y == 8 || y == 17) && ((x >= 10 && x <= 13) || (x >= 16 && x <= 19)))
    return Ground::Wall;
  int o = obstacle_index(x, y);
  if (o >= 0 && !s.cleared[o] && (Obstacles[o].kind != 0 || s.tree_age[o] >= 2))
    return Ground(int(Ground::Tree) + Obstacles[o].kind);
  if (y == 7 || x == 9 || x == 20 || (y == 18 && x >= 9 && x <= 24))
    return Ground::Path;
  return Ground::Grass;
}
bool walkable(const State &s, int x, int y) {
  auto g = ground(s, x, y);
  return g == Ground::Grass || g == Ground::Path;
}
bool near(const State &s, int x, int y) {
  return std::abs(int(s.px) / Tile - x) + std::abs(int(s.py) / Tile - y) <= 1;
}
int obstacle_hp(int index) {
  if (index < 0 || index >= 24)
    return 0;
  const auto &o = Obstacles[index];
  constexpr int tree[] = {5, 5, 7, 6}, rock[] = {5, 4, 7, 3};
  int variant = scenery_variant(o.x, o.y);
  return o.kind == 0 ? tree[variant] : o.kind == 1 ? rock[variant] : 1;
}
int tree_stage(const State &s, int index) {
  return s.cleared[index]        ? 3
         : s.tree_age[index] < 2 ? 0
         : s.tree_age[index] < 5 ? 1
                                 : 2;
}
int strikes_left(const State &s, int index, Tool tool) {
  if (index < 0 || index >= 24 || s.cleared[index] || unsigned(tool) >= 7)
    return 0;
  int power = 1 + s.tool_grade[unsigned(tool)];
  int hp = Obstacles[index].kind == 0 && tree_stage(s, index) < 2
               ? (tree_stage(s, index) == 0 ? 1 : 3)
               : obstacle_hp(index);
  int left = std::max(0, hp - s.damage[index]);
  return (left + power - 1) / power;
}
unsigned item_count(const State &s, int item) {
  if (item >= 1 && item <= 7)
    return item != 2;
  if (item >= 8 && item <= 11)
    return s.seeds[item - 8];
  if (item >= 12 && item <= 15)
    return s.produce[item - 12];
  return item == 16 ? s.fish : item == 17 ? s.wood : item == 18 ? s.stone
      : item >= 19 && item <= 21 ? s.gathered[item - 19]
      : item >= 22 && item <= 23 ? s.meals[item - 22]
      : item >= 24 && item <= 28 ? s.seasonal[item - 24] : 0;
}
bool change_item(State &s, int item, int amount) {
  uint16_t *stack = item >= 8 && item <= 11 ? &s.seeds[item - 8]
      : item >= 12 && item <= 15 ? &s.produce[item - 12]
      : item == 16 ? &s.fish : item == 17 ? &s.wood : item == 18 ? &s.stone
      : item >= 19 && item <= 21 ? &s.gathered[item - 19]
      : item >= 22 && item <= 23 ? &s.meals[item - 22]
      : item >= 24 && item <= 28 ? &s.seasonal[item - 24] : nullptr;
  if (!stack || amount < -int(*stack) || amount > 999 - int(*stack)) return false;
  *stack = uint16_t(int(*stack) + amount);
  return true;
}
void sync_inventory(State &s) {
  for(int i=0;i<InventorySize;++i)if(s.slots[i]==2) {
    int seed=s.seeds[s.selected]?8+s.selected:0;
    for(int j=0;j<InventorySize;++j)if(j!=i&&s.slots[j]==seed&&seed)s.slots[j]=0;
    s.slots[i]=seed;
  }
  bool present[ItemCount + 1]{};
  for (auto &item : s.slots) {
    if (item >= 1 && item <= ItemCount) {
      if (!item_count(s, item))
        item = 0;
      else
        present[item] = true;
    }
  }
  if(s.active_slot<HotbarSize && !s.slots[s.active_slot]) { s.active_slot=255;s.tool=uint8_t(Tool::Hand); }
  for (int item = 1; item <= ItemCount; ++item)
    if (!present[item] && item_count(s, item))
      for (auto &slot : s.slots)
        if (!slot) {
          slot = uint8_t(item);
          break;
        }
}
bool select_slot(State &s, int slot) {
  if (slot == -1) {
    s.active_slot = 255;
    s.tool = uint8_t(Tool::Hand);
    return true;
  }
  if (slot < 0 || slot >= HotbarSize)
    return false;
  s.active_slot = uint8_t(slot);
  int item = s.slots[slot];
  s.tool = item >= 1 && item <= 7 ? item - 1 : uint8_t(Tool::Hand);
  if (item >= 8 && item <= 11) {
    s.tool = uint8_t(Tool::Seeds);
    s.selected = item - 8;
  }
  return true;
}
bool swap_slots(State &s, int from, int to) {
  if (from < 0 || to < 0 || from >= InventorySize || to >= InventorySize ||
      from == to)
    return false;
  std::swap(s.slots[from], s.slots[to]);
  select_slot(s, s.active_slot == 255 ? -1 : s.active_slot);
  return true;
}
bool equip_item(State &s, int item) {
  if (item < 1 || item > ItemCount || !item_count(s, item))
    return false;
  sync_inventory(s);
  for (int i = 0; i < InventorySize; ++i)
    if (s.slots[i] == item) {
      int slot = i < HotbarSize               ? i
                 : s.active_slot < HotbarSize ? s.active_slot
                                              : 0;
      if (i >= HotbarSize)
        std::swap(s.slots[i], s.slots[slot]);
      return select_slot(s, slot);
    }
  return false;
}
ToolCost tool_cost(const State &s, Tool tool) {
  if ((tool != Tool::Axe && tool != Tool::Pick) ||
      s.tool_grade[unsigned(tool)] >= 2)
    return {};
  return s.tool_grade[unsigned(tool)] == 0 ? ToolCost{80, 8, 6}
                                           : ToolCost{220, 16, 12};
}
bool upgrade_tool(State &s, Tool tool) {
  auto cost = tool_cost(s, tool);
  if (!cost.gold || s.gold < cost.gold || s.wood < cost.wood ||
      s.stone < cost.stone)
    return false;
  s.gold -= cost.gold;
  s.wood -= cost.wood;
  s.stone -= cost.stone;
  ++s.tool_grade[unsigned(tool)];
  sync_inventory(s);
  return true;
}
void fresh(State &s) {
  s = State{};
  for (int i = 0; i < 24; ++i)
    s.tree_age[i] = Obstacles[i].kind == 0 ? 5 : 0;
  sync_inventory(s);
  // The farm starts with harvestable trees; distant woodland also has saplings.
  s.tree_age[16] = 0;
  s.tree_age[17] = 2;
  s.tree_age[18] = 3;
  for (int x = 0; x < 5; ++x)
    s.plots[3 * PlotW + x] = {1, 1, uint8_t(x < 2 ? 2 : 1), 0};
  for (int i = 0; i < 3; ++i)
    s.plots[3 * PlotW + 5 + i] = {1, 2, 2, 0};
}
uint32_t random(State &s) {
  s.rng ^= s.rng << 13;
  s.rng ^= s.rng >> 17;
  s.rng ^= s.rng << 5;
  return s.rng;
}
Result act(State &s, int x, int y) {
  if (!near(s, x, y))
    return Result::Far;
  if (const auto *portal = portal_at(s, x, y))
    return !building_open(s,portal->to)?Result::Closed:
      travel(s, *portal) ? Result::Travel : Result::Locked;
  if (npc_at(s, x, y) >= 0)
    return Result::Dialogue;
  if (s.region) {
    if (s.region == uint8_t(Region::Home) && x >= 9 && x <= 11 && y >= 7 &&
        y <= 9)
      return Result::Sleep;
    if (s.region == uint8_t(Region::SeedShop) && x>=12 && x<=19 && y==10)
      return shop_open(s)?Result::Shop:Result::Closed;
    if (s.region == uint8_t(Region::Home) && x==15 && y==7) return Result::Forecast;
    if (s.region == uint8_t(Region::Home) && x==13 && y==5 ||
        s.region == uint8_t(Region::Town) && x==5 && y==7) return Result::Calendar;
    if (s.region == uint8_t(Region::Town) && x == 16 && y == 10) {
      s.water=s.capacity;return Result::Refilled;
    }
    if (s.region == uint8_t(Region::Town) && x == 18 && y == 10)
      return Result::Orders;
    if (s.region == uint8_t(Region::Home) && x == 21 && y == 10)
      return Result::Kitchen;
    int forage = forage_at(s, x, y);
    if (forage >= 0)
      return collect_forage(s, forage) ? Result::Foraged : Result::Blocked;
    if (ground(s, x, y) == Ground::Water) {
      if (Tool(s.tool) == Tool::Rod)
        return s.energy && spend_energy(s,4) ? Result::Fish : Result::NoEnergy;
      s.water = s.capacity;
      return Result::Refilled;
    }
    return Result::Blocked;
  }
  if (x == 8 && y == 9) {
    return ship(s)?Result::Ship:Result::EmptyShipping;
  }
  if (x == 21 && y == 8) {
    quest(s);
    return Result::Talk;
  }
  if ((x == 7 && y == 8) || ground(s, x, y) == Ground::Water) {
    if (Tool(s.tool) == Tool::Rod && ground(s, x, y) == Ground::Water)
      return s.energy && spend_energy(s,4) ? Result::Fish : Result::NoEnergy;
    s.water = s.capacity;
    return Result::Refilled;
  }
  auto t = Tool(s.tool);
  int o = obstacle_index(x, y);
  if (o >= 0 && !s.cleared[o]) {
    bool young = Obstacles[o].kind == 0 && s.tree_age[o] < 2;
    if ((Obstacles[o].kind == 0 && t != Tool::Axe &&
         !(young && (t == Tool::Hoe || t == Tool::Hand))) ||
        (Obstacles[o].kind == 1 && t != Tool::Pick) ||
        (Obstacles[o].kind == 2 && t != Tool::Hoe && t != Tool::Hand))
      return Result::Blocked;
    int grade = s.tool_grade[unsigned(t)];
    int energy = grade ? 2 : 3;
    if (!spend_energy(s, energy))
      return Result::NoEnergy;
    s.damage[o] += uint8_t(1 + grade);
    int hp = Obstacles[o].kind == 0 && tree_stage(s, o) < 2
                 ? (tree_stage(s, o) == 0 ? 1 : 3)
                 : obstacle_hp(o);
    if (s.damage[o] < hp)
      return Result::Struck;
    s.damage[o] = 0;
    s.cleared[o] = 1;
    if (Obstacles[o].kind == 0) {
      s.wood = std::min(999, int(s.wood) + (s.tree_age[o] >= 5   ? 4
                                            : s.tree_age[o] >= 2 ? 1
                                                                 : 0));
      s.tree_age[o] = young ? 5 : 0;
    } else if (Obstacles[o].kind == 1)
      s.stone = std::min(999, int(s.stone) + 3);
    else
      s.seeds[0] = std::min(999, int(s.seeds[0]) + 1);
    return Result::Cleared;
  }
  int i = plot_index(x, y);
  if (i < 0)
    return Result::Blocked;
  auto &p = s.plots[i];
  if (p.crop && p.growth >= Crops[p.crop - 1].days) {
    int c = p.crop - 1;
    if (s.produce[c] >= 999)
      return Result::Blocked;
    s.produce[c]++;
    s.harvested++;
    if (Crops[c].regrow)
      p.growth = Crops[c].days - Crops[c].regrow;
    else {
      p.crop = 0;
      p.growth = 0;
    }
    return Result::Harvested;
  }
  if (t == Tool::Hand)
    return Result::NotReady;
  if (t == Tool::Hoe) {
    if (p.tilled)
      return Result::None;
    if (!spend_energy(s, 2))
      return Result::NoEnergy;
    p.tilled = 1;
    return Result::Tilled;
  }
  if (t == Tool::Seeds) {
    if (!p.tilled || p.crop)
      return Result::Blocked;
    if (!crop_in_season(s.day,s.selected)) return Result::WrongSeason;
    if (!s.seeds[s.selected])
      return Result::NoSeeds;
    s.seeds[s.selected]--;
    sync_inventory(s);
    p.crop = s.selected + 1;
    p.growth = 0;
    return Result::Planted;
  }
  if (t == Tool::Water) {
    if (!p.tilled || p.wet)
      return Result::None;
    if (!s.water)
      return Result::NoWater;
    if (!spend_energy(s, 1))
      return Result::NoEnergy;
    s.water--;
    p.wet = 1;
    return Result::Watered;
  }
  return Result::None;
}
uint32_t ship(State &s) {
  int item=s.active_slot<HotbarSize?s.slots[s.active_slot]:0,price=item_price(item);
  if(s.region || !near(s,8,9) || !price || !item_count(s,item) || s.shipping>999999u-unsigned(price))return 0;
  if(!change_item(s,item,-1))return 0;
  s.shipping+=price;s.last_shipped=item;sync_inventory(s);
  return price;
}
bool unship(State&s) {
  int item=s.last_shipped,price=item_price(item);
  if(s.region||!near(s,8,9)||!price||s.shipping<unsigned(price)||!change_item(s,item,1))return false;
  s.shipping-=price;s.last_shipped=0;sync_inventory(s);return true;
}
uint32_t sleep(State &s, bool forced) {
  int restoration=s.minute<=1440?100:std::max(50,100-(int(s.minute)-1440)*50/120);
  if(forced||s.exhausted)restoration=std::min(restoration,50);
  if(forced&&s.region!=unsigned(Region::Home))s.gold-=std::min(1000u,s.gold/10);
  uint32_t income = s.shipping;
  s.gold = std::min(999999u, s.gold + income);
  s.earned += income;
  s.shipping = 0;
  for (int i = 0; i < 24; ++i) {
    if (Obstacles[i].kind != 0)
      continue;
    const auto &o = Obstacles[i];
    int plot = plot_index(o.x, o.y);
    if ((plot >= 0 && s.plots[plot].tilled) ||
        (!s.region && s.px / Tile == o.x && s.py / Tile == o.y))
      continue;
    if (s.cleared[i]) {
      if (s.tree_age[i] >= 5)
        continue;
      if (++s.tree_age[i] >= 2) {
        s.cleared[i] = 0;
        s.tree_age[i] = 0;
      }
    } else if (s.tree_age[i] < 5) {
      ++s.tree_age[i];
    }
  }
  for (int i = 0; i < PlotCount; ++i) {
    auto &p = s.plots[i];
    if (p.crop && (p.wet || s.rain) && p.growth < Crops[p.crop - 1].days)
      ++p.growth;
    p.wet = 0;
  }
  s.day = s.day == 65535 ? 1 : s.day + 1;
  s.minute = 360;
  s.energy = restoration;
  s.exhausted=false;s.overwork=0;s.last_shipped=0;
  s.water = s.capacity;
  s.weather=unsigned(weather_on(s.day));
  s.rain=wet_weather(Weather(s.weather));
  for(auto &p:s.plots) if(p.crop&&!crop_in_season(s.day,p.crop-1)) {p.crop=0;p.growth=0;}
  for (int i = 0; i < PlotCount; ++i) {
    if (s.rain)
      s.plots[i].wet = 1;
    if (!(s.sprinklers[i / 8] & (1u << (i % 8))))
      continue;
    for (int d : {-PlotW, -1, 0, 1, PlotW}) {
      int j = i + d;
      if (j >= 0 && j < PlotCount && std::abs(j % PlotW - i % PlotW) <= 1)
        s.plots[j].wet = 1;
    }
  }
  s.forage = 0;
  s.order_day = s.day;
  s.order_claimed = 0;
  if (s.day == 1) {
    std::memset(s.chat_day, 0, sizeof(s.chat_day));
    std::memset(s.gift_day, 0, sizeof(s.gift_day));
  }
  return income;
}
bool buy_seed(State &s, int kind, int count) {
  if (!crop_in_season(s.day,kind) || !shop_open(s) || s.region!=unsigned(Region::SeedShop) || kind < 0 || kind >= 4 || count < 1 || count > 99 ||
      s.seeds[kind] + count > 999 ||
      s.gold < uint32_t(Crops[kind].price * count))
    return false;
  s.gold -= Crops[kind].price * count;
  s.seeds[kind] += count;
  return true;
}
bool upgrade(State &s) {
  if (s.capacity >= 60 || s.gold < 180)
    return false;
  s.gold -= 180;
  s.capacity = 60;
  s.water = 60;
  return true;
}
bool craft_sprinkler(State &s, int i) {
  if (i < 0 || i >= PlotCount || s.wood < 8 || s.stone < 6 ||
      (s.sprinklers[i / 8] & (1u << (i % 8))))
    return false;
  s.wood -= 8;
  s.stone -= 6;
  s.sprinklers[i / 8] |= 1u << (i % 8);
  return true;
}
uint32_t quest(State &s) {
  bool ready = s.quests == 0   ? s.harvested >= 3
               : s.quests == 1 ? s.earned >= 120
               : s.quests == 2 ? s.fish >= 3
                               : false;
  if (!ready)
    return 0;
  if (s.quests == 2)
    s.fish -= 3;
  uint32_t reward = 60 + 40 * s.quests;
  s.gold = std::min(999999u, s.gold + reward);
  ++s.quests;
  ++s.friendship;
  return reward;
}
bool next_step(const State &s, int tx, int ty, int &nx, int &ny,
               bool adjacent) {
  if (tx < 0 || tx >= MapW || ty < 0 || ty >= MapH)
    return false;
  int start = (s.py / Tile) * MapW + s.px / Tile;
  int16_t parent[MapW * MapH], queue[MapW * MapH];
  std::fill(parent, parent + MapW * MapH, int16_t(-1));
  int head = 0, tail = 0, goal = -1;
  queue[tail++] = start;
  parent[start] = start;
  while (head < tail) {
    int at = queue[head++], x = at % MapW, y = at / MapW;
    if (std::abs(x - tx) + std::abs(y - ty) <= (adjacent ? 1 : 0)) {
      goal = at;
      break;
    }
    for (int d : {-MapW, 1, MapW, -1}) {
      int n = at + d;
      if (n < 0 || n >= MapW * MapH ||
          std::abs(n % MapW - x) + std::abs(n / MapW - y) != 1 ||
          parent[n] != -1 || !walkable(s, n % MapW, n / MapW))
        continue;
      parent[n] = at;
      queue[tail++] = n;
    }
  }
  if (goal < 0 || goal == start)
    return false;
  while (parent[goal] != start)
    goal = parent[goal];
  nx = goal % MapW;
  ny = goal / MapW;
  return true;
}
static uint32_t checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; ++i)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
size_t encode(const State &original, uint8_t *out, size_t cap) {
  State s = original;
  sync_inventory(s);
  if (cap < SaveBytes)
    return 0;
  std::memset(out, 0, SaveBytes);
  size_t n = 0;
  auto put = [&](uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) {
      out[n++] = uint8_t(v);
      v >>= 8;
    }
  };
  put(0x31465744, 4);
  put(5, 2);
  put(s.day, 2);
  put(s.minute, 2);
  put(s.energy, 1);
  put(s.water, 1);
  put(s.capacity, 1);
  put(s.selected, 1);
  put(s.tool, 1);
  put(s.quests, 1);
  put(s.friendship, 1);
  put(s.rain, 1);
  put(s.px, 2);
  put(s.py, 2);
  for (auto v : {s.gold, s.shipping, s.earned, s.harvested, s.rng})
    put(v, 4);
  for (int i = 0; i < 4; ++i) {
    put(s.seeds[i], 2);
    put(s.produce[i], 2);
  }
  put(s.fish, 2);
  put(s.wood, 2);
  put(s.stone, 2);
  for (auto v : s.cleared)
    put(v, 1);
  for (auto v : s.sprinklers)
    put(v, 1);
  for (auto &p : s.plots) {
    put(p.tilled, 1);
    put(p.crop, 1);
    put(p.growth, 1);
    put(p.wet, 1);
  }
  for (auto grade : s.tool_grade)
    put(grade, 1);
  for (auto damage : s.damage)
    put(damage, 1);
  for (auto item : s.slots)
    put(item, 1);
  put(s.active_slot, 1);
  for (auto age : s.tree_age)
    put(age, 1);
  put(s.show_joystick | (s.show_action << 1) | (s.music << 2), 1);
  put(s.region, 1);
  put(s.story, 1);
  put(s.met, 1);
  put(s.visited, 1);
  for (auto v : s.bonds)
    put(v, 1);
  for (auto v : s.chat_day)
    put(v, 2);
  for (auto v : s.gift_day)
    put(v, 2);
  put(s.personal, 1);
  put(s.forage, 1);
  put(s.forage >> 8, 1);
  for (auto v : s.gathered) put(v, 2);
  for (auto v : s.meals) put(v, 2);
  put(s.order_day, 2);
  put(s.order_claimed, 1);
  put(s.orders_done, 2);
  for(auto v:s.seasonal)put(v,2);
  put(s.weather,1);put(s.overwork,1);put(s.exhausted,1);put(s.last_shipped,1);
  n = SaveBytes - 4;
  put(checksum(out, n), 4);
  return SaveBytes;
}
bool decode(State &s, const uint8_t *in, size_t n) {
  if (n != SaveBytes && n != 512)
    return false;
  size_t at = 0;
  auto get = [&](int bytes) {
    uint32_t v = 0;
    for (int i = 0; i < bytes; ++i)
      v |= uint32_t(in[at++]) << (8 * i);
    return v;
  };
  if (get(4) != 0x31465744)
    return false;
  uint32_t version = get(2);
  if (version < 1 || version > 5)
    return false;
  if ((version >= 3 && n != SaveBytes) || (version < 3 && n != 512))
    return false;
  at = n - 4;
  uint32_t sum = get(4);
  if (checksum(in, n - 4) != sum)
    return false;
  at = 6;
  State t;
  t.day = get(2);
  t.minute = get(2);
  t.energy = get(1);
  t.water = get(1);
  t.capacity = get(1);
  t.selected = get(1);
  t.tool = get(1);
  t.quests = get(1);
  t.friendship = get(1);
  uint32_t rain = get(1);
  t.rain = rain;
  t.px = get(2);
  t.py = get(2);
  t.gold = get(4);
  t.shipping = get(4);
  t.earned = get(4);
  t.harvested = get(4);
  t.rng = get(4);
  // New inventory quantities are serialized after the compatible v3 prefix,
  // but slot validation below must see them before checking occupied stacks.
  // Defer those quantity-dependent checks until the extension has been read.
  for (int i = 0; i < 4; ++i) {
    t.seeds[i] = get(2);
    t.produce[i] = get(2);
    if (t.seeds[i] > 999 || t.produce[i] > 999)
      return false;
  }
  t.fish = get(2);
  t.wood = get(2);
  t.stone = get(2);
  for (auto &v : t.cleared) {
    v = get(1);
    if (v > 1)
      return false;
  }
  for (auto &v : t.sprinklers)
    v = get(1);
  for (auto &p : t.plots) {
    p.tilled = get(1);
    p.crop = get(1);
    p.growth = get(1);
    p.wet = get(1);
    if (p.tilled > 1 || p.crop > 4 || p.wet > 1 || (!p.tilled && p.crop) ||
        (!p.crop && p.growth) || (p.crop && p.growth > Crops[p.crop - 1].days))
      return false;
  }
  if (version >= 2) {
    for (unsigned i = 0; i < 7; ++i) {
      t.tool_grade[i] = get(1);
      if (t.tool_grade[i] > 2 || (i != unsigned(Tool::Axe) &&
                                  i != unsigned(Tool::Pick) && t.tool_grade[i]))
        return false;
    }
    for (int i = 0; i < 24; ++i) {
      t.damage[i] = get(1);
      if (t.damage[i] >= obstacle_hp(i) || (t.cleared[i] && t.damage[i]))
        return false;
    }
    bool present[ItemCount + 1]{};
    for (auto &item : t.slots) {
      item = get(1);
      if (item > ItemCount || (item && (present[item] ||
          (item < 19 && !(version<5&&item==2) && !item_count(t, item)))))
        return false;
      if (item)
        present[item] = true;
    }
    for (int item = 1; item <= ItemCount; ++item)
      if (item_count(t, item) && !present[item])
        return false;
    t.active_slot = get(1);
    if (t.active_slot >= HotbarSize && t.active_slot != 255)
      return false;
    for (int i = 0; i < 24; ++i) {
      t.tree_age[i] = get(1);
      if (t.tree_age[i] > 5 || (Obstacles[i].kind != 0 && t.tree_age[i]))
        return false;
      if (Obstacles[i].kind == 0 && !t.cleared[i] && t.tree_age[i] < 5 &&
          t.damage[i] >= (t.tree_age[i] < 2 ? 1 : 3))
        return false;
    }
    unsigned settings = get(1);
    if (settings & ~7u)
      return false;
    t.show_joystick = settings & 1;
    t.show_action = settings & 2;
    t.music = settings & 4;
  } else {
    t.active_slot = t.tool;
    for (int i = 0; i < 24; ++i)
      t.tree_age[i] = Obstacles[i].kind == 0 && !t.cleared[i] ? 5 : 0;
    sync_inventory(t);
  }
  if (version >= 3) {
    t.region = get(1);
    t.story = get(1);
    t.met = get(1);
    t.visited = get(1);
    for (auto &v : t.bonds) {
      v = get(1);
      if (v > 40)
        return false;
    }
    for (auto &v : t.chat_day)
      v = get(2);
    for (auto &v : t.gift_day)
      v = get(2);
    t.personal = get(1);
    t.forage = get(1);
    if (t.region >= (version >= 5 ? RegionCount : version==4?6:5) || t.story > 5 || t.met > 15 ||
        t.visited >= (1u << (version >= 5 ? RegionCount : version==4?6:5)) || !(t.visited & 1) ||
        !(t.visited & (1u << t.region)) || t.personal > 15 ||
        (version == 3 && t.forage >= 64))
      return false;
    if (t.region == uint8_t(Region::Lake) && t.story < 3)
      return false;
  }
  if (version >= 4) {
    t.forage |= get(1) << 8;
    for (auto &v : t.gathered) { v = get(2); if (v > 999) return false; }
    for (auto &v : t.meals) { v = get(2); if (v > 999) return false; }
    t.order_day = get(2);
    t.order_claimed = get(1);
    t.orders_done = get(2);
    if (t.forage >= (1u << (version>=5?sizeof(Foraging)/sizeof(*Foraging):12)) ||
        t.order_claimed > 15 || (t.order_claimed && t.order_day != t.day)) return false;
  }
  if(version>=5) {
    for(auto &v:t.seasonal) {v=get(2);if(v>999)return false;}
    t.weather=get(1);t.overwork=get(1);auto exhausted=get(1);t.exhausted=exhausted;t.last_shipped=get(1);
    if(t.weather>=unsigned(Weather::Count)||t.overwork>10||exhausted>1||
      (t.last_shipped&&(!item_price(t.last_shipped)||t.shipping<unsigned(item_price(t.last_shipped)))))return false;
  } else {
    auto migrate=[](uint16_t day)->uint16_t {return day?uint16_t(std::min(65535,((int(day)-1)/7)*28+(day-1)%7+1)):0;};
    t.day=migrate(t.day);t.order_day=migrate(t.order_day);
    for(auto &d:t.chat_day)d=migrate(d);for(auto &d:t.gift_day)d=migrate(d);
    t.weather=t.rain?unsigned(Weather::Rain):unsigned(Weather::Sun);
    // Existing saves may stand on a tile now occupied by a wall appliance.
    // Move only those new furniture collisions to the adjacent open tile.
    if(t.region==unsigned(Region::Home)&&((t.px/16==15&&t.py/16==7)||(t.px/16==13&&t.py/16==5)))t.py+=16;
    if(t.region==unsigned(Region::Town)&&t.px/16==5&&t.py/16==7)t.py+=16;
    sync_inventory(t);select_slot(t,t.active_slot==255?-1:t.active_slot);
  }
  for (int item = 19; item <= ItemCount; ++item) {
    int copies = 0;
    for (auto slot : t.slots) copies += slot == item;
    if (copies != int(item_count(t, item) != 0)) return false;
  }
  if (!t.day || t.minute < 360 || t.minute >= (version>=5?1560:1320) || t.energy > 100 ||
      (t.capacity != 30 && t.capacity != 60) || t.water > t.capacity ||
      t.selected >= 4 || t.tool >= uint8_t(Tool::Count) || t.quests > 3 ||
      t.friendship != t.quests || rain > 1 || t.gold > 999999 ||
      t.shipping > 999999 || t.px >= MapW * Tile || t.py >= MapH * Tile ||
      !walkable(t, t.px / Tile, t.py / Tile))
    return false;
  s = t;
  return true;
}
} // namespace farm
