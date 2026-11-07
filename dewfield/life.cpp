// SPDX-License-Identifier: MIT
#include "living.hpp"
#include <algorithm>
namespace farm {
Order daily_order(const State &s, int npc) {
  if (npc < 0 || npc >= NpcCount) return {};
  constexpr uint8_t goods[] = {12, 13, 14, 15, 16, 17, 18, 20};
  int item = goods[(unsigned(s.day) + npc * 3u) % 8];
  if ((s.visited & (1u << unsigned(Region::Orchard))) &&
      (unsigned(s.day) + unsigned(npc)) % 3 == 0)
    item = season_of(s.day) == 3 ? 20 : 19;
  // Mushroom requests appear only after the orchard has been discovered.
  if (item == 20 && !(s.visited & (1u << unsigned(Region::Orchard)))) item = 12;
  if(item>=12&&item<=15&&!crop_in_season(s.day,item-12)) {
    constexpr int seasonal[]={12,14,15,17};item=seasonal[season_of(s.day)];
  }
  if(item==19&&season_of(s.day)==0)item=21;
  int count = item == 16 || item >= 19 ? 2 : 2 + ((s.day + npc) % 3);
  int value = item >= 19 ? Foods[item - 19].value
              : item >= 12 && item <= 15 ? Crops[item - 12].sell
              : item == 16 ? 28 : 8;
  return {uint8_t(npc), uint8_t(item), uint8_t(count),
          uint16_t(value * count * 3 / 2 + 20),
          s.order_day == s.day && bool(s.order_claimed & (1u << npc))};
}
bool deliver_order(State &s, int npc) {
  if (npc < 0 || npc >= NpcCount || s.region != unsigned(Region::Town) ||
      !near(s, 18, 10)) return false;
  auto order = daily_order(s, npc);
  if (order.done || item_count(s, order.item) < order.count) return false;
  if (!change_item(s, order.item, -int(order.count))) return false;
  if (s.order_day != s.day) { s.order_day = s.day; s.order_claimed = 0; }
  s.order_claimed |= 1u << npc;
  s.orders_done = std::min(65535, int(s.orders_done) + 1);
  s.gold = std::min(999999u, s.gold + order.reward);
  s.bonds[npc] = std::min(40, int(s.bonds[npc]) + 3);
  sync_inventory(s);
  return true;
}
bool cook(State &s, int recipe) {
  if (recipe < 0 || recipe >= 2 || s.region != unsigned(Region::Home) ||
      !near(s, 21, 10)) return false;
  const auto &r = Recipes[recipe];
  if (item_count(s, r.first) < r.first_count ||
      item_count(s, r.second) < r.second_count || item_count(s, r.item) >= 999)
    return false;
  change_item(s, r.first, -int(r.first_count));
  change_item(s, r.second, -int(r.second_count));
  change_item(s, r.item, 1);
  sync_inventory(s);
  return true;
}
bool eat(State &s, int item) {
  if (!food_energy(item) || s.energy >= 100 || !item_count(s, item)) return false;
  change_item(s, item, -1);
  s.energy = std::min(100, int(s.energy) + food_energy(item));
  if(s.energy>=20)s.overwork=0;
  sync_inventory(s);
  select_slot(s,s.active_slot==255?-1:s.active_slot);
  return true;
}
} // namespace farm
