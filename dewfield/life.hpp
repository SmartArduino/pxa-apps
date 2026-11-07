// SPDX-License-Identifier: MIT
#pragma once
#include "world.hpp"
namespace farm {
struct Food { const char *cn, *en; uint8_t value, energy; };
inline constexpr Food Foods[] = {{"苹果", "Apple", 14, 8},
    {"山菌", "Mushroom", 18, 6}, {"浆果", "Berries", 12, 10},
    {"山菌浓汤", "Mushroom stew", 86, 45}, {"果园果酱", "Orchard jam", 80, 25}};
struct Recipe { uint8_t item, first, first_count, second, second_count; };
inline constexpr Recipe Recipes[] = {{22, 20, 2, 13, 1}, {23, 19, 3, 21, 1}};
struct Order { uint8_t npc, item, count; uint16_t reward; bool done; };
Order daily_order(const State &s, int npc);
bool deliver_order(State &s, int npc);
bool cook(State &s, int recipe);
bool eat(State &s, int item);
} // namespace farm
