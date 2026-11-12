// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <algorithm>
namespace unciv {
// Session history is intentionally separate from the portable game save.
struct TurnLog {
  enum Type : uint8_t {
    Research,
    Produced,
    BuildingDone,
    Growth,
    Starvation,
    Improved,
    UnitLost,
    CityLost,
    CityAttacked,
    PromotionReady,
    Pillaged
  };
  struct Entry {
    uint16_t turn;
    int16_t tile;
    uint8_t type, value;
  };
  Entry entries[32]{};
  int count = 0, unread = 0;
  void clear() { count = unread = 0; }
  void add(int turn, int tile, Type type, int value) {
    int last = std::min(count, 31);
    for (int i = last; i > 0; --i)
      entries[i] = entries[i - 1];
    entries[0] = {uint16_t(turn), int16_t(tile), uint8_t(type), uint8_t(value)};
    count = std::min(32, count + 1);
    unread = std::min(32, unread + 1);
  }
  void collect(const State &before, const State &after) {
    for (int t = 0; t < TECHS; ++t)
      if ((after.civ[0].tech & (1 << t)) && !(before.civ[0].tech & (1 << t)))
        add(after.turn, -1, Research, t);
    for (int i = 0; i < CITIES; ++i) {
      const auto &a = before.cities[i], &b = after.cities[i];
      if (a.tile < 0 || a.owner != 0)
        continue;
      if (b.owner != 0) {
        add(after.turn, a.tile, CityLost, i);
        continue;
      }
      if (b.pop != a.pop)
        add(after.turn, b.tile, b.pop > a.pop ? Growth : Starvation, b.pop);
      for (int n = 0; n < 4; ++n)
        if ((b.buildings & (1 << n)) && !(a.buildings & (1 << n)))
          add(after.turn, b.tile, BuildingDone, KINDS + n);
      if (b.hp < a.hp)
        add(after.turn, b.tile, CityAttacked, b.hp);
    }
    for (int t = 0; t < N; ++t)
      if (before.tiles[t].improvement != 3 && after.tiles[t].improvement == 3) {
        int c = working_city(before, t);
        if (c >= 0 && before.cities[c].owner == 0)
          add(after.turn, t, Pillaged, before.tiles[t].improvement);
      }
    for (int i = 0; i < UNITS; ++i) {
      const auto &a = before.units[i], &b = after.units[i];
      if (a.tile >= 0 && a.owner == 0 &&
          (b.tile < 0 || b.owner != 0 || a.kind != b.kind))
        add(after.turn, a.tile, UnitLost, a.kind);
      if (b.tile >= 0 && b.owner == 0 &&
          (a.tile < 0 || a.owner != 0 || a.kind != b.kind))
        add(after.turn, b.tile, Produced, b.kind);
      if (b.tile >= 0 && b.owner == 0 &&
          earned_promotions(b) > promotion_count(b) &&
          (a.tile < 0 || a.owner != 0 ||
           earned_promotions(b) - promotion_count(b) >
               earned_promotions(a) - promotion_count(a)))
        add(after.turn, b.tile, PromotionReady, b.kind);
      if (a.tile >= 0 && a.owner == 0 && a.work_left && !b.work_left &&
          after.tiles[a.tile].improvement == a.project)
        add(after.turn, a.tile, Improved, a.project);
    }
  }
};
} // namespace unciv
