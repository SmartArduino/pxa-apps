// SPDX-License-Identifier: MIT
#include "core.hpp"
#include "world.hpp"
#include "living.hpp"
#include "collections.hpp"
#include "livestock.hpp"
#include "wilderness.hpp"
#include "mines.hpp"
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
  if(y==1&&(x%2)&&!exit_clearance(s.region,x,y))return Ground::Wall;
  if(y==21&&(x==24||x==26||x==28))return Ground::Wall;
  if ((x >= 4 && x <= 8 && y >= 3 && y <= 6))
    return Ground::Wall;
  if (x >= 24 && x <= 29 && y >= 14 && y <= 20 &&
      !(x == 24 && (y == 14 || y == 20)) && !(x == 29 && (y == 14 || y == 20)))
    return Ground::Water;
  if ((x == 21 && y == 8) || (x == 8 && y == 9) || (x == 7 && y == 8))
    return Ground::Wall;
  if ((y == 8 || y == 17) && ((x >= 10 && x <= 13) || (x >= 16 && x <= 19)))
    return Ground::Wall;
  if(x>=RanchX-1&&x<=RanchX+1&&y>=RanchY-2&&y<=RanchY)return Ground::Wall;
  int o = obstacle_index(x, y);
  if (o >= 0 && !s.cleared[o] && (Obstacles[o].kind != 0 || s.tree_age[o] >= 2))
    return Ground(int(Ground::Tree) + Obstacles[o].kind);
  if (road_tile(s.region,x,y))
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
  if(item==37)return s.sword_owned;
  if (item >= 1 && item <= 7)
    return item != 2;
  if (item >= 8 && item <= 11)
    return s.seeds[item - 8];
  if (item >= 12 && item <= 15)
    return s.produce[item - 12];
  return item == 16 ? s.fish : item == 17 ? s.wood : item == 18 ? s.stone
      : item >= 19 && item <= 21 ? s.gathered[item - 19]
      : item >= 22 && item <= 23 ? s.meals[item - 22]
      : item >= 24 && item <= 28 ? s.seasonal[item - 24]
      : item >= 29 && item <= 36 ? s.animal_goods[item-29]
      : item>=38&&item<=45?s.minerals[item-38]:0;
}
bool change_item(State &s, int item, int amount) {
  uint16_t *stack = item >= 8 && item <= 11 ? &s.seeds[item - 8]
      : item >= 12 && item <= 15 ? &s.produce[item - 12]
      : item == 16 ? &s.fish : item == 17 ? &s.wood : item == 18 ? &s.stone
      : item >= 19 && item <= 21 ? &s.gathered[item - 19]
      : item >= 22 && item <= 23 ? &s.meals[item - 22]
      : item >= 24 && item <= 28 ? &s.seasonal[item - 24]
      : item >= 29 && item <= 36 ? &s.animal_goods[item-29]
      : item>=38&&item<=45?&s.minerals[item-38]:nullptr;
  if (!stack || amount < -int(*stack) || amount > 999 - int(*stack)) return false;
  if(amount>0 && !*stack) {
    int types=0;for(int i=1;i<=ItemCount;++i)types+=item_count(s,i)!=0;
    if(types>=InventorySize)return false;
  }
  *stack = uint16_t(int(*stack) + amount);
  if(amount>0 && item>=12 && item<=28)s.discoveries|=1u<<(item-12);
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
  if(s.active_slot<InventorySize && !s.slots[s.active_slot]) { s.active_slot=255;s.tool=uint8_t(Tool::Hand); }
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
  if (slot < 0 || slot >= InventorySize)
    return false;
  s.active_slot = uint8_t(slot);
  int item = s.slots[slot];
  s.tool = item==37?uint8_t(Tool::Sword):item >= 1 && item <= 7 ? item - 1 : uint8_t(Tool::Hand);
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
  if(s.active_slot==from)s.active_slot=uint8_t(to);
  else if(s.active_slot==to)s.active_slot=uint8_t(from);
  select_slot(s, s.active_slot == 255 ? -1 : s.active_slot);
  return true;
}
bool equip_item(State &s, int item) {
  if (item < 1 || item > ItemCount || !item_count(s, item))
    return false;
  sync_inventory(s);
  for (int i = 0; i < InventorySize; ++i)
    if (s.slots[i] == item) {
      return select_slot(s, i);
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
  std::fill(s.wild_age,s.wild_age+WildTreeCount,uint8_t(5));
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
  if(in_mine(s))return mine_act(s,x,y);
  int wild=wild_tree_at(s,x,y);if(wild>=0)return chop_wild(s,wild);
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
        return s.fish>=999?Result::FullFish:s.energy && spend_energy(s,4) ? Result::Fish : Result::NoEnergy;
      s.water = s.capacity;
      return Result::Refilled;
    }
    return Result::Blocked;
  }
  if(x==RanchX&&y==RanchY)return Result::Ranch;
  if(animal_at(s,x,y)>=0)return Result::Animal;
  if (x == 8 && y == 9) {
    return ship(s)?Result::Ship:Result::EmptyShipping;
  }
  if (x == 21 && y == 8) {
    quest(s);
    return Result::Talk;
  }
  if ((x == 7 && y == 8) || ground(s, x, y) == Ground::Water) {
    if (Tool(s.tool) == Tool::Rod && ground(s, x, y) == Ground::Water)
      return s.fish>=999?Result::FullFish:s.energy && spend_energy(s,4) ? Result::Fish : Result::NoEnergy;
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
    if(!change_item(s,12+c,1))return Result::Blocked;
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
  int item=s.active_slot<InventorySize?s.slots[s.active_slot]:0,price=item_price(item);
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
  livestock_night(s);wild_night(s);
  s.day = s.day == 65535 ? 1 : s.day + 1;
  mine_new_day(s);
  s.health=forced?60:100;
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
    for(auto &a:s.animals){a.pet_day=0;a.fed_day=0;}
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
  if(!change_item(s,8+kind,count))return false;
  s.gold -= Crops[kind].price * count;
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
  put(10, 2);
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
  for(int i=0;i<28;++i)put(s.slots[i],1);
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
  put(s.auto_run, 1);
  put(s.discoveries,4);
  for(auto length:s.fish_records)put(length,1);
  put(s.fish_caught,2);put(s.perfect_catches,2);put(s.collection_rewards,1);
  for(auto v:s.animal_goods)put(v,2);
  for(auto &a:s.animals) {
    for(auto v:{a.kind,a.male,a.age,a.heart,a.pregnancy,a.cooldown,a.products})put(v,1);
    put(a.pet_day,2);put(a.fed_day,2);
  }
  for(auto &e:s.incubators){put(e.kind,1);put(e.days,1);}
  for(auto v:{s.hay,s.trough,s.hatched,s.born,s.animal_collected})put(v,2);
  put(s.intro,1);put(s.ranch,1);put(s.ranch_chapter,1);put(s.breeding,1);
  for(int i=28;i<42;++i)put(s.slots[i],1);
  for(int i=42;i<InventorySize;++i)put(s.slots[i],1);
  for(auto v:s.wild_age)put(v,1);for(auto v:s.wild_damage)put(v,1);
  for(auto v:s.minerals)put(v,2);
  put(s.visited>>8,1);put(s.sword_owned,1);put(s.sword_grade,1);put(s.health,1);
  put(s.mine_depth,1);put(s.mine_best,1);put(s.mine_loaded,1);put(s.mine_seed,4);
  for(auto v:{s.mine_day,s.monsters_slain,s.ore_mined,s.cave_gathered,s.invulnerable,s.attack_cooldown})put(v,2);
  put(s.mine_rewards,1);
  for(auto v:s.mine_cleared)put(v,4);for(auto v:s.mine_defeated)put(v,1);for(auto v:s.ore_damage)put(v,1);
  for(auto &m:s.mobs){for(auto v:{m.x,m.y,m.cooldown,m.windup,m.move_acc})put(v,2);put(m.hp,1);put(m.dir,1);}
  if(n!=SaveDataBytes)return 0;
  n = SaveBytes - 4;
  put(checksum(out, n), 4);
  return SaveBytes;
}
// v11 packs each farm plot's four bounded fields into one byte. The remaining
// v10 fields keep their order; unused tail bytes are omitted before padding.
// This is fixed-cost packing, never content-dependent compression.
size_t encode_storage(const State &s,uint8_t *out,size_t cap) {
  if(cap<StorageSaveBytes)return 0;
  uint8_t raw[SaveBytes];if(encode(s,raw,sizeof(raw))!=SaveBytes)return 0;
  std::memset(out,0,StorageSaveBytes);std::memcpy(out,raw,98);out[4]=11;
  for(int i=0;i<PlotCount;++i){auto &p=s.plots[i];out[98+i]=p.tilled|(p.crop<<1)|(p.growth<<4)|(p.wet<<7);}
  std::memcpy(out+98+PlotCount,raw+418,SaveDataBytes-418);
  uint32_t sum=checksum(out,StorageSaveBytes-4);
  for(int i=0;i<4;++i)out[StorageSaveBytes-4+i]=sum>>(i*8);
  return StorageSaveBytes;
}
bool decode(State &s, const uint8_t *in, size_t n) {
  if(n==StorageSaveBytes) {
    if(std::memcmp(in,"DWF1",4)||in[4]!=11||in[5])return false;
    uint32_t sum=0;for(int i=0;i<4;++i)sum|=uint32_t(in[n-4+i])<<(i*8);
    if(checksum(in,n-4)!=sum)return false;
    uint8_t raw[SaveBytes]{};std::memcpy(raw,in,98);raw[4]=10;
    for(int i=0;i<PlotCount;++i){auto v=in[98+i];raw[98+i*4]=v&1;raw[99+i*4]=(v>>1)&7;raw[100+i*4]=(v>>4)&7;raw[101+i*4]=v>>7;}
    std::memcpy(raw+418,in+98+PlotCount,SaveDataBytes-418);
    sum=checksum(raw,SaveBytes-4);for(int i=0;i<4;++i)raw[SaveBytes-4+i]=sum>>(i*8);
    return decode(s,raw,sizeof(raw));
  }

  if (n != SaveBytes && n != 768 && n != 512)
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
  if (version < 1 || version > 10)
    return false;
  if ((version==10&&n!=SaveBytes)||(version>=3&&version<=9&&n!=768)||(version<3&&n!=512))
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
    for(int slot=0;slot<28;++slot) {
      auto &item=t.slots[slot];item = get(1);
      if (item > ItemCount || (item && (present[item] ||
          (item < 19 && !(version<5&&item==2) && !item_count(t, item)))))
        return false;
      if (item)
        present[item] = true;
    }
    for (int item = 1; item <= ItemCount; ++item)
      if (version<9 && item_count(t, item) && !present[item])
        return false;
    t.active_slot = get(1);
    if (t.active_slot >= (version>=10?InventorySize:version>=9?42:version>=7?28:HotbarSize) && t.active_slot != 255)
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
    if (t.region >= (version>=10?RegionCount:version>=5?8:version==4?6:5) || t.story > 5 || t.met > 15 ||
        t.visited >= (1u << (version>=10?RegionCount:version>=5?8:version==4?6:5)) || !(t.visited & 1) ||
        (version<10&&!(t.visited & (1u << t.region))) || t.personal > 15 ||
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
  if (version >= 6) {
    auto auto_run = get(1);
    if (auto_run > 1) return false;
    t.auto_run = auto_run;
  }
  if(version>=8) {
    t.discoveries=get(4);
    int species=0;
    for(int i=0;i<FishSpeciesCount;++i) {
      auto &length=t.fish_records[i];length=get(1);
      const auto &f=FishSpeciesList[i];
      if(length && (length<f.min_cm||length>f.max_cm))return false;
      species+=length!=0;
    }
    t.fish_caught=get(2);t.perfect_catches=get(2);t.collection_rewards=get(1);
    if(t.discoveries>=(1u<<17)||t.perfect_catches>t.fish_caught||
       t.fish_caught<species||t.collection_rewards>=(1u<<MilestoneCount))return false;
    for(int i=0;i<MilestoneCount;++i) {
      auto m=milestone(t,i);
      if((t.collection_rewards&(1u<<i)) && m.progress<m.goal)return false;
    }
  } else {
    // Old saves can prove owned goods, but cannot prove a particular fish species.
    for(int item=12;item<=28;++item)
      if(item_count(t,item))t.discoveries|=1u<<(item-12);
  }
  if(version>=9) {
    for(auto &v:t.animal_goods){v=get(2);if(v>999)return false;}
    for(auto &a:t.animals) {
      a.kind=get(1);a.male=get(1);a.age=get(1);a.heart=get(1);a.pregnancy=get(1);a.cooldown=get(1);a.products=get(1);a.pet_day=get(2);a.fed_day=get(2);
      if(a.kind>5||a.male>1||a.age>250||a.heart>100||a.pregnancy>4||a.cooldown>4||a.products>7)return false;
      if(!a.kind&&(a.age||a.pregnancy||a.products||a.pet_day||a.fed_day))return false;
      if(a.kind&&(a.products||a.pregnancy)&&animal_stage(a)<2)return false;
      if(a.pregnancy&&(a.kind<3||a.male||a.pregnancy>Breeds[a.kind-1].gestation))return false;
      if(a.products && ((a.products&4)&&(!(a.products&1)||a.kind>2)||a.kind==5||((a.products&1)&&a.male)||((a.products&2)&&a.kind!=4)))return false;
    }
    for(auto &e:t.incubators) {e.kind=get(1);e.days=get(1);if(e.kind>2||(!e.kind&&e.days)||(e.kind&&(!e.days||e.days>(e.kind==1?3:4))))return false;}
    t.hay=get(2);t.trough=get(2);t.hatched=get(2);t.born=get(2);t.animal_collected=get(2);
    t.intro=get(1);t.ranch=get(1);t.ranch_chapter=get(1);auto breeding=get(1);t.breeding=breeding;
    for(int i=28;i<42;++i){t.slots[i]=get(1);if(t.slots[i]>ItemCount)return false;}
    if(t.hay>999||t.trough>24||t.intro>IntroScenes||t.ranch>1||t.ranch_chapter>4||breeding>1||animal_reserved(t)>AnimalMax)return false;
    if(!t.ranch&&(animal_count(t)||t.ranch_chapter||t.hay||t.trough||t.hatched||t.born||t.animal_collected))return false;
    if(t.ranch_chapter<1)for(auto &e:t.incubators)if(e.kind)return false;
  } else t.intro=IntroScenes; // Existing players continue their farm immediately.
  if(version>=10) {
    for(int i=42;i<InventorySize;++i){t.slots[i]=get(1);if(t.slots[i]>ItemCount)return false;}
    for(auto &v:t.wild_age){v=get(1);if(v>7)return false;}
    for(int i=0;i<WildTreeCount;++i){t.wild_damage[i]=get(1);if((wild_stage(t,i)==3&&t.wild_damage[i])||(wild_stage(t,i)!=3&&t.wild_damage[i]>=wild_hp(t,i)))return false;}
    for(auto &v:t.minerals){v=get(2);if(v>999)return false;}
    t.visited|=get(1)<<8;auto sword=get(1);t.sword_owned=sword;t.sword_grade=get(1);t.health=get(1);
    t.mine_depth=get(1);t.mine_best=get(1);t.mine_loaded=get(1);t.mine_seed=get(4);
    t.mine_day=get(2);t.monsters_slain=get(2);t.ore_mined=get(2);t.cave_gathered=get(2);t.invulnerable=get(2);t.attack_cooldown=get(2);t.mine_rewards=get(1);
    if(sword>1||t.sword_grade>2||(!sword&&t.sword_grade)||!t.health||t.health>100||t.mine_depth>MineFloors||t.mine_best>MineFloors||t.mine_depth>t.mine_best||t.mine_loaded>MineFloors||t.invulnerable>1200||t.attack_cooldown>220||t.mine_rewards>1)return false;
    for(auto &v:t.mine_cleared){v=get(4);if(v&~0x2fffffffu)return false;}
    for(auto &v:t.mine_defeated){v=get(1);if(v>=64)return false;}
    for(int i=0;i<MineRocks;++i){auto &v=t.ore_damage[i];v=get(1);if(v>6)return false;if(in_mine(t)&&t.mine_depth&&(v>=ore_hp(t,i)||((t.mine_cleared[t.mine_depth-1]&(1u<<i))&&v)))return false;}
    for(int i=0;i<MineMobs;++i){auto &m=t.mobs[i];m.x=get(2);m.y=get(2);m.cooldown=get(2);m.windup=get(2);m.move_acc=get(2);m.hp=get(1);m.dir=get(1);
      if(m.x>=MapW*16||m.y>=MapH*16||m.cooldown>5000||m.windup>400||m.move_acc>=1000||m.hp>90||m.dir>3)return false;
      if(in_mine(t)&&t.mine_depth&&(m.hp>mob_max_hp(t,i)||(bool(m.hp)==bool(t.mine_defeated[t.mine_depth-1]&(1u<<i)))||(m.hp&&!mine_walkable(t,m.x,m.y))))return false;
    }
    if(t.visited>=(1u<<RegionCount)||!(t.visited&(1u<<t.region)))return false;
    if(in_mine(t)&&(!sword||!t.mine_depth||t.mine_loaded!=t.mine_depth))return false;
    if(!in_mine(t)&&(t.mine_depth||t.mine_loaded))return false;
  } else std::fill(t.wild_age,t.wild_age+WildTreeCount,uint8_t(5));
  if(version<10&&t.region) {
    int wi=wild_tree_at(t,t.px/16,t.py/16);
    if(wi>=0&&!walkable(t,t.px/16,t.py/16)) {
      bool found=false;int x=t.px/16,y=t.py/16;
      for(int radius=1;radius<=4&&!found;++radius)for(int dy=-radius;dy<=radius&&!found;++dy)for(int dx=-radius;dx<=radius&&!found;++dx)
       if(std::abs(dx)+std::abs(dy)==radius&&walkable(t,x+dx,y+dy)){t.px=(x+dx)*16+8;t.py=(y+dy)*16+8;found=true;}
      if(!found)return false;
    }
  }
  for (int item = version>=9?1:19; item <= ItemCount; ++item) {
    int copies = 0;
    for (auto slot : t.slots) copies += slot == item;
    if (copies != int(item_count(t, item) != 0)) return false;
  }
  if(!t.region&&t.px/16>=RanchX-1&&t.px/16<=RanchX+1&&t.py/16>=RanchY-2&&t.py/16<=RanchY) {
    t.px=RanchX*16+8;t.py=(RanchY+1)*16+8;
  }
  if (!t.day || t.minute < 360 || t.minute >= (version>=5?1560:1320) || t.energy > 100 ||
      (t.capacity != 30 && t.capacity != 60) || t.water > t.capacity ||
      t.selected >= 4 || t.tool >= (version>=10?uint8_t(Tool::Count):7) || (t.tool==7&&!t.sword_owned) || t.quests > 3 ||
      t.friendship != t.quests || rain > 1 || t.gold > 999999 ||
      t.shipping > 999999 || t.px >= MapW * Tile || t.py >= MapH * Tile ||
      !walkable(t, t.px / Tile, t.py / Tile))
    return false;
  s = t;
  return true;
}
} // namespace farm
