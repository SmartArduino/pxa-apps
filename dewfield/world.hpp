// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
namespace farm {
enum class Region : uint8_t { Farm, Town, Forest, Lake, Home, Orchard, SeedShop, Lodge, Count };
constexpr int RegionCount = int(Region::Count), NpcCount = 4;
struct Point {
  int x, y;
};
struct Portal {
  Region from, to;
  int x, y, spawn_x, spawn_y;
  bool bridge = false;
};
inline constexpr Portal Portals[] = {
    {Region::Farm, Region::Town, 29, 7, 2, 12},
    {Region::Town, Region::Farm, 1, 12, 28, 7},
    {Region::Farm, Region::Forest, 2, 18, 28, 12},
    {Region::Forest, Region::Farm, 29, 12, 3, 18},
    {Region::Town, Region::Forest, 15, 21, 15, 3},
    {Region::Forest, Region::Town, 15, 2, 15, 20},
    {Region::Forest, Region::Lake, 2, 12, 28, 12, true},
    {Region::Lake, Region::Forest, 29, 12, 3, 12},
    {Region::Farm, Region::Home, 6, 7, 16, 18},
    {Region::Home, Region::Farm, 16, 20, 6, 8},
    {Region::Forest, Region::Orchard, 22, 21, 16, 3},
    {Region::Orchard, Region::Forest, 16, 2, 22, 20},
    {Region::Town, Region::SeedShop, 7, 7, 16, 18},
    {Region::SeedShop, Region::Town, 16, 20, 7, 8},
    {Region::Town, Region::Lodge, 24, 7, 16, 18},
    {Region::Lodge, Region::Town, 16, 20, 24, 8}};
struct NpcPose {
  Region region;
  int x, y, dir;
  bool walking;
  int frame=0;
};
struct Forage {
  Region region;
  uint8_t x, y, item, seasons = 15;
};
inline constexpr Forage Foraging[] = {
    {Region::Forest, 7, 7, 17},  {Region::Forest, 24, 17, 17},
    {Region::Forest, 8, 18, 18}, {Region::Lake, 23, 14, 28, 6},
    {Region::Lake, 24, 17, 18},  {Region::Town, 25, 17, 8},
    {Region::Orchard, 7, 8, 19, 6}, {Region::Orchard, 13, 17, 19, 6},
    {Region::Orchard, 23, 16, 20}, {Region::Forest, 25, 7, 20},
    {Region::Orchard, 22, 8, 21, 3}, {Region::Orchard, 8, 17, 21, 3},
    {Region::Forest, 8, 8, 24, 1}, {Region::Lake, 23, 9, 25, 2},
    {Region::Orchard, 13, 8, 26, 4}, {Region::Forest, 24, 18, 27, 8}};
bool forage_available(const State &s, int id);
const char *region_name(Region region, bool en = false);
const char *npc_name(int id, bool en = false);
const char *npc_job(int id, bool en = false);
Ground world_ground(const State &s, int x, int y);
bool road_tile(uint8_t region,int x,int y);
bool exit_clearance(uint8_t region,int x,int y);
const Portal *portal_at(const State &s, int x, int y);
int portal_heading(const Portal &portal);
Point portal_approach(const Portal &portal);
bool crossed_portal(const Portal &portal, int from_x, int from_y,
                    int to_x, int to_y, int heading);
bool travel(State &s, const Portal &portal);
int next_portal(const State &s, Region destination);
NpcPose npc_pose(const State &s, int id, int subminute_ms=0);
int npc_at(const State &s, int tx, int ty);
bool chat(State &s, int npc); // once/day friendship
bool gift(State &s, int npc, int item);
bool personal_event(State &s, int npc);
const char *npc_line(const State &s, int npc, bool personal, bool en = false);
const char *story_title(const State &s, bool en = false);
const char *story_goal(const State &s, bool en = false);
const char *story_line(const State &s, bool en = false);
const char *story_completion(int chapter, bool en = false);
bool advance_story(State &s, int npc);
int forage_at(const State &s, int x, int y);
bool collect_forage(State &s, int id);
} // namespace farm
