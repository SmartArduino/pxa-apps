// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace cafe {
constexpr uint32_t DAY_MS=150000;
constexpr size_t SAVE_BYTES=160;
constexpr uint8_t NONE=3;
enum class View : uint8_t { CAFE, SUPPLY, UPGRADES, HELP };
enum class Event : uint8_t { NONE, ARRIVED, ACCEPTED, COOKING, READY, SOLD, FAILED, CLOSED, NEXT_DAY, BOUGHT, UPGRADED, CREDIT, DISCARDED, BUSY, EMPTY, STOCK, POOR, WRONG, MAXED };
struct Customer { uint8_t stage=0, recipe=0, skin=0; uint32_t patience=0; };
struct State {
 uint32_t coins=30, day_ms=0, spawn_ms=11400, seed=0x12cafe51, cook_ms=0, revenue=0, cogs=0;
 uint16_t day=1, paid_day=0, total_served=0, total_failed=0, stars=0, debt=0;
 uint16_t stock[3]={8,6,6}, served=0, failed=0, bonus=0, fee=0;
 uint8_t level[3]={}, recipe=0, table=0, shop=0, sound=1, rescued=0, summary=0;
 uint8_t cook_recipe=NONE, ready_recipe=NONE;
 View view=View::CAFE;
 Customer customers[3];
};
extern const char *const RECIPES[3];
extern const uint8_t INGREDIENTS[3][3];
extern const uint16_t PRICES[3], UNIT_COST[3], PACK_COST[3];
void fresh(State &s);
void restart(State &s); // Current day only; cash, stock, debts and growth survive.
Event act(State &s);
Event step(State &s, uint32_t elapsed);
Event buy(State &s, unsigned item); // 0..2 four-unit packs; 3 daily recovery credit.
Event upgrade(State &s, unsigned line);
Event discard(State &s);
void next_day(State &s);
unsigned target(const State &s);
unsigned cost(unsigned recipe);
unsigned upgrade_cost(const State &s, unsigned line);
unsigned patience_limit(const State &s);
bool valid(const State &s);
size_t encode(const State &s, uint8_t *out, size_t capacity);
bool decode(State &s, const uint8_t *data, size_t size);
}
