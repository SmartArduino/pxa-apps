// SPDX-License-Identifier: MIT
#include "../world.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <queue>
using namespace farm;
static void stand(State &s, int x, int y) {
  s.px = x * 16 + 8;
  s.py = y * 16 + 8;
}
static void reach(State &s, int x, int y) {
  int steps = 0;
  while (!near(s, x, y)) {
    int nx, ny;
    assert(next_step(s, x, y, nx, ny, true));
    stand(s, nx, ny);
    assert(++steps < 768);
  }
}
static void legacy(uint8_t *p, int version) {
  p[4] = version;
  p[5] = 0;
  unsigned h = 2166136261u;
  for (int i = 0; i < 508; ++i)
    h = (h ^ p[i]) * 16777619u;
  for (int i = 0; i < 4; ++i)
    p[508 + i] = h >> (i * 8);
}
int main() {
  State s;
  fresh(s);
  uint8_t bytes[SaveBytes];
  State loaded;
  // Real saved v1/v2 layouts migrate without losing farms, inventory or money.
  s.gold = 449;
  s.wood = 12;
  s.stone = 12;
  encode(s, bytes, sizeof(bytes));
  legacy(bytes, 2);
  assert(decode(loaded, bytes, 512) && loaded.gold == 449 &&
         loaded.region == 0 && loaded.visited == 1);
  legacy(bytes, 1);
  assert(decode(loaded, bytes, 512) && loaded.tree_age[0] == 5);
  // Every portal has a safe arrival and every region is reachable after unlock.
  s.story = 3;
  for (auto &p : Portals) {
    s.region = uint8_t(p.from);
    stand(s, p.x, p.y);
    assert(travel(s, p));
    assert(walkable(s, s.px / 16, s.py / 16));
    assert(s.visited & (1u << s.region));
  }
  for (int region = 0; region < RegionCount; ++region) {
    s.region = region;
    for (int target = 0; target < RegionCount; ++target)
      if (target != region)
        assert(next_portal(s, Region(target)) >= 0);
  }
  // Simulate walking from each arrival to every exit in its region.
  for (auto &arrival : Portals) {
    s.region = uint8_t(arrival.to);
    s.story = 3;
    stand(s, arrival.spawn_x, arrival.spawn_y);
    for (auto &exit : Portals)
      if (exit.from == arrival.to) {
        State walk = s;
        reach(walk, exit.x, exit.y);
      }
  }
  // A locked bridge cannot teleport, consume materials or accidentally unlock.
  s.region = 2;
  s.story = 2;
  stand(s, 6, 12);
  auto p = portal_at(s, 5, 12);
  assert(p && p->bridge);
  auto before = s;
  assert(!travel(s, *p) && s.region == before.region && s.wood == before.wood);
  // Stations and short walks never land villagers in a wall or water.
  for (int rain = 0; rain < 2; ++rain)
    for (int story : {0, 3, 5})
      for (int time = 360; time < 1320; ++time)
        for (int id = 0; id < 4; ++id) {
          s.rain = rain;
          s.story = story;
          s.minute = time;
          auto pose = npc_pose(s, id);
          State check = s;
          check.region = uint8_t(pose.region);
          auto g = world_ground(check, pose.x / 16, pose.y / 16);
          assert(g == Ground::Grass || g == Ground::Path);
        }
  // Friendship cannot be farmed by reopening dialogue; gifting consumes once.
  fresh(s);
  assert(chat(s, 0) && !chat(s, 0) && s.bonds[0] == 2);
  s.produce[0] = 2;
  sync_inventory(s);
  assert(gift(s, 0, 12) && s.produce[0] == 1 && s.bonds[0] == 8);
  assert(!gift(s, 0, 12) && s.produce[0] == 1);
  sleep(s);
  assert(chat(s, 0) && s.bonds[0] == 10);
  assert(personal_event(s, 0) && !personal_event(s, 0));
  // Full original story, costs, unlocks and non-repeatable chapter rewards.
  fresh(s);
  assert(!advance_story(s, 3));
  for (int i = 0; i < 4; ++i)
    chat(s, i);
  assert(advance_story(s, 3) && s.story == 1);
  unsigned gold = s.gold;
  assert(!advance_story(s, 3) && s.gold == gold);
  s.harvested = 3;
  assert(advance_story(s, 0) && s.story == 2);
  s.wood = 12;
  s.stone = 9;
  assert(advance_story(s, 1) && s.story == 3 && s.wood == 0 && s.stone == 0);
  s.fish = 3;
  assert(!advance_story(s, 2));
  s.visited |= 1u << 3;
  assert(advance_story(s, 2) && s.story == 4 && !s.fish);
  s.produce[0] = 4;
  assert(!advance_story(s, 3));
  for (auto &bond : s.bonds)
    bond = 10;
  assert(advance_story(s, 3) && s.story == 5 && s.produce[0] == 0);
  gold = s.gold;
  assert(!advance_story(s, 3) && s.gold == gold);
  // Off-map growth and daily forage reset, save/reload of all extension state.
  s.region = 2;
  s.visited |= 4;
  stand(s, 7, 8);
  assert(collect_forage(s, 0) && s.wood == 3 && !collect_forage(s, 0));
  auto day = s.day;
  sleep(s);
  assert(s.day == day + 1 && s.forage == 0);
  assert(collect_forage(s, 0));
  encode(s, bytes, sizeof(bytes));
  assert(decode(loaded, bytes, sizeof(bytes)));
  assert(loaded.story == 5 && loaded.region == 2 && loaded.bonds[0] == 10 &&
         loaded.wood == 6 && loaded.met == 15);
  puts(
      "PASS: five-region graph, safe spawn/paths, schedules, bridge gate, "
      "daily friendship/gifts, all story chapters, forage and v1/v2 migration");
}
