// SPDX-License-Identifier: MIT
#pragma once
#include "../core.hpp"
namespace unciv::test {
// Used only by native tests and builds explicitly defining UNCIV_TEST_TACTICS.
inline int tactics_fixture(State &s) {
  s = State{};
  for (auto &t : s.tiles)
    t = {Grass, 0, 0, 7, 7};
  int home = 10 * W + 10, enemy = home + 8;
  for (int o = 0; o < 2; ++o) {
    int u = add_unit(s, o, Settler, o ? enemy : home);
    found(s, u);
    s.cities[o].job = Warrior;
  }
  int tile = enemy - 2, raider = add_unit(s, 0, Warrior, tile);
  s.units[raider].hp = 50;
  s.tiles[tile].improvement = 1;
  s.wars = 1;
  vision(s);
  return raider;
}
} // namespace unciv::test
