// SPDX-License-Identifier: MIT
#include "../pinch_tracker.hpp"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main() {
  using namespace pinch_tracker;
  Tracker t;
  assert(t.event(Phase::Down, 4, 100, 100, true).action == Action::Pass);
  auto r = t.event(Phase::Down, 9, 200, 100, true);
  assert(r.action == Action::Begin && r.x == 150 && r.scale == 65536);
  r = t.event(Phase::Move, 9, 250, 100, true);
  assert(r.action == Action::Update && r.x == 175 && r.scale == 98304);
  assert(t.event(Phase::Up, 4, 100, 100, true).action == Action::End);
  assert(t.event(Phase::Move, 9, 200, 100, true).action == Action::Consume);
  assert(t.event(Phase::Up, 9, 200, 100, true).action == Action::Consume);
  assert(t.event(Phase::Down, 4, 100, 100, true).action == Action::Pass);
  assert(t.event(Phase::Up, 4, 100, 100, true).action == Action::Pass);
  t.reset();
  t.event(Phase::Down, 0, 100, 10, false);
  assert(t.event(Phase::Down, 1, 200, 120, true).action == Action::Pass);
  assert(!t.active());
  t.reset();
  t.event(Phase::Down, 0, 100, 100, true);
  t.event(Phase::Down, 1, 100, 100, true);
  assert(t.active());
  assert(t.event(Phase::Cancel, 7, 0, 0, false).action == Action::End);
  assert(!t.active());
  for (uint32_t scale : {1u, 32768u, 65536u, 87381u, 98304u, UINT32_MAX}) {
    auto c = zoom(48, 20, -15, 170, 130, 144, 124, 185, 142, scale);
    assert(c.size >= 32 && c.size <= 64 && !(c.size & 1));
    assert(abs((170 - 144 - 20) * c.size - (185 - 144 - c.pan_x) * 48) <= 48);
    assert(abs((130 - 124 + 15) * c.size - (142 - 124 - c.pan_y) * 48) <= 48);
  }
  puts("Pinch: contact IDs, midpoint/scale, cancellation, toolbar exclusion "
       "and anchored zoom passed");
}
