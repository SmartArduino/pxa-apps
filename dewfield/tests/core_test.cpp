// SPDX-License-Identifier: MIT
#include "../living.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace farm;
static void stand(State &s, int x, int y) {
  s.px = x * 16 + 8;
  s.py = y * 16 + 8;
}
int main() {
  State s;
  fresh(s);
  for (int i = 0; i < PlotCount; ++i)
    if (s.plots[i].crop)
      assert(walkable(s, PlotX + i % PlotW, PlotY + i / PlotW));
  uint8_t bytes[SaveBytes];
  assert(encode(s, bytes, sizeof(bytes)) == SaveBytes);
  State restored;
  assert(decode(restored, bytes, sizeof(bytes)));
  assert(restored.day == 1 && restored.gold == 100 &&
         restored.plots[30].growth == 2);
  // A complete harvest, ship and overnight payout loop.
  stand(s, 10, 13);
  s.tool = uint8_t(Tool::Hand);
  assert(act(s, 10, 12) == Result::Harvested);
  assert(s.produce[0] == 1 && s.harvested == 1);
  stand(s,ShippingX,ShippingY+1);equip_item(s,12);assert(ship(s) == 16);
  assert(s.produce[0] == 0 && s.gold == 100 && s.shipping == 16);
  assert(sleep(s) == 16);
  assert(s.gold == 116 && s.shipping == 0 && s.day == 2);
  // Dry soil does not grow. A failed action never consumes seeds or energy.
  stand(s, 13, 15);
  s.tool = uint8_t(Tool::Hoe);
  int i = plot_index(13, 14);
  assert(act(s, 13, 14) == Result::Tilled);
  s.tool = uint8_t(Tool::Seeds);
  assert(act(s, 13, 14) == Result::Planted);
  int energy = s.energy, seed = s.seeds[0];
  assert(act(s, 13, 14) == Result::Blocked);
  assert(s.energy == energy && s.seeds[0] == seed);
  sleep(s);
  assert(s.plots[i].growth == 0);
  s.tool = uint8_t(Tool::Water);s.plots[i].wet=0;s.rain=false;
  assert(act(s, 13, 14) == Result::Watered);
  assert(s.water == 29);
  assert(act(s, 13, 14) == Result::None);
  assert(s.water == 29);
  sleep(s);
  assert(s.plots[i].growth == 1 && s.plots[i].wet==s.rain);
  while(!wet_weather(weather_on(s.day+1)))sleep(s);
  sleep(s);assert(s.rain);
  sleep(s);
  assert(s.plots[i].growth >= 2);
  // Rain and sprinklers establish moisture for the next day's growth.
  s.wood = 8;
  s.stone = 6;
  assert(craft_sprinkler(s, i));
  assert(!craft_sprinkler(s, i));
  sleep(s);
  assert(s.plots[i - 1].wet && s.plots[i + 1].wet && s.plots[i - PlotW].wet &&
         s.plots[i + PlotW].wet);
  assert(!s.plots[i + 2].wet);
  // Clearing is tool- and range-checked, and material is granted only once.
  stand(s, 10, 10);
  s.tool = uint8_t(Tool::Pick);
  assert(act(s, 10, 9) == Result::Blocked);
  s.tool = uint8_t(Tool::Axe);
  for (int hit=1;hit<obstacle_hp(0);++hit) assert(act(s,10,9)==Result::Struck);
  assert(act(s, 10, 9) == Result::Cleared);
  assert(s.wood == 4);
  assert(act(s, 10, 9) == Result::Cleared);
  assert(s.wood == 5);
  assert(act(s, 10, 9) != Result::Cleared);
  assert(act(s, 25, 7) == Result::Far);
  s.energy = 0;s.overwork=10;
  s.tool = uint8_t(Tool::Hoe);
  assert(act(s, 11, 10) == Result::NoEnergy);
  sleep(s);
  assert(s.energy == 100);
  // Price checks, upgrade idempotence, quest reward idempotence.
  s.gold = 200;
  s.region=unsigned(Region::SeedShop);s.visited|=1u<<s.region;s.px=15*16+8;s.py=11*16+8;s.day=85-28;s.minute=600;
  assert(buy_seed(s, 3));
  assert(s.gold == 178);
  assert(!upgrade(s));
  s.gold = 180;
  assert(upgrade(s) && s.capacity == 60);
  assert(!upgrade(s));
  s.region=0;
  s.harvested = 3;
  s.earned = 120;
  s.fish = 3;
  assert(quest(s) == 60);
  assert(quest(s) == 100);
  assert(quest(s) == 140);
  assert(quest(s) == 0 && s.friendship == 3 && s.fish == 0);
  // Navigate around obstacles to every important facility using actual BFS.
  for (auto target :
       {Obstacle{6, 7, 0}, Obstacle{25, 7, 0}, Obstacle{ShippingX, ShippingY, 0},
        Obstacle{WellX, WellY, 0}, Obstacle{24, 17, 0}, Obstacle{21, 8, 0}}) {
    stand(s, 14, 17);
    int steps = 0;
    while (!near(s, target.x, target.y)) {
      int x, y;
      assert(next_step(s, target.x, target.y, x, y, true));
      assert(walkable(s, x, y));
      stand(s, x, y);
      assert(++steps < MapW * MapH);
    }
  }
  // Capacity limits preserve a harvest and keep every future save readable.
  fresh(s);
  stand(s, 10, 13);
  s.produce[0] = 999;
  int before_energy = s.energy;
  assert(act(s, 10, 12) == Result::Blocked && s.energy == before_energy &&
         s.plots[30].crop == 1);
  s.produce[0] = 998;
  assert(act(s, 10, 12) == Result::Harvested && s.produce[0] == 999);
  s.shipping = 999999;
  ship(s);
  assert(s.shipping == 999999);
  // Every serialized bit is checked. Failed decode must preserve live state.
  assert(encode(s, bytes, sizeof(bytes)) == SaveBytes);
  assert(decode(restored, bytes, sizeof(bytes)));
  for (size_t b = 0; b < sizeof(bytes); ++b) {
    bytes[b] ^= 0x80;
    State before = restored;
    assert(!decode(restored, bytes, sizeof(bytes)));
    assert(std::memcmp(&before, &restored, sizeof(before)) == 0);
    bytes[b] ^= 0x80;
  }
  assert(!decode(restored, bytes, sizeof(bytes) - 1));
  puts("PASS: farming economy, dry/wet growth, rain, sprinklers, tools, "
       "energy, quests, paths and save integrity");
}
