// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
namespace farm {
enum class Region : uint8_t { Farm, Town, Forest, Lake, Home, Count };
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
    {Region::Forest, Region::Lake, 5, 12, 28, 12, true},
    {Region::Lake, Region::Forest, 29, 12, 6, 12},
    {Region::Farm, Region::Home, 6, 7, 16, 18},
    {Region::Home, Region::Farm, 16, 20, 6, 8}};
struct NpcPose {
  Region region;
  int x, y, dir;
  bool walking;
};
struct Forage {
  Region region;
  uint8_t x, y, item;
};
inline constexpr Forage Foraging[] = {
    {Region::Forest, 7, 7, 17},  {Region::Forest, 24, 17, 17},
    {Region::Forest, 8, 18, 18}, {Region::Lake, 9, 10, 16},
    {Region::Lake, 24, 17, 18},  {Region::Town, 25, 17, 8}};
const char *region_name(Region region, bool en = false);
const char *npc_name(int id, bool en = false);
const char *npc_job(int id, bool en = false);
Ground world_ground(const State &s, int x, int y);
const Portal *portal_at(const State &s, int x, int y);
bool travel(State &s, const Portal &portal);
int next_portal(const State &s, Region destination);
NpcPose npc_pose(const State &s, int id);
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
