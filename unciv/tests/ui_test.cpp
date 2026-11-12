// SPDX-License-Identifier: MIT
// Drive public game hooks and check durable state after touch and controller
// journeys.
#include "../../common/pocket-play/runtime.hpp"
#include "../../common/pocket-studio/studio.hpp"
#include "../audio.hpp"
#include "../core.hpp"
#include "../map_gestures.hpp"
#include "../map_raster.hpp"
#include "tactics_fixture.hpp"
#include <assert.h>
#include <initializer_list>
#include <stdio.h>
#include <string.h>
namespace pocket {
void clear(uint16_t) {}
void rect(int, int, int, int, uint16_t) {}
void outline(int, int, int, int, uint16_t) {}
void line(int, int, int, int, uint16_t) {}
void pixel(int, int, uint16_t) {}
void redraw() {}
void save() {}
void log(const char *) {}
uint32_t millis() { return 1000; }
uint32_t random() { return 12345; }
} // namespace pocket
namespace studio {
void label(int, int, int, const char *, uint16_t, int, unsigned) {}
void label_box(int, int, int, int, const char *, uint16_t) {}
void round(int, int, int, int, int, uint16_t) {}
void disc(int, int, int, uint16_t) {}
} // namespace studio
namespace sound {
void play(Cue) {}
void toggle() {}
bool enabled() { return true; }
void toggle_music() {}
bool music_enabled() { return true; }
} // namespace sound
namespace map_raster {
bool minimap_restore(int, int, int, int, uint32_t) { return false; }
void minimap_capture() {}
unsigned fills = 0, routes = 0, targets = 0;
int first_sprite_width = 0;
void begin() {}
void flush() {}
void end() {}
void icon(int, int, int, const uint16_t *, const uint8_t *, int, int,
          uint16_t) {}
void overlay(int, int, int, int, const uint8_t *, int, int, uint16_t,
             int opacity, int, int) {
  fills += opacity == 180;
  targets += opacity == 256;
}
void stroke(int, int, int, int, int width, uint16_t, int, int) {
  routes += width == 4;
}
bool text(int, int, int, const char *, uint16_t, int, unsigned) {
  return false;
}
void sprite(int, int, int w, int, const uint16_t *, const uint8_t *, int, int,
            int, int, int) {
  if (!first_sprite_width)
    first_sprite_width = w;
}
void record_draw(uint64_t) {}
uint64_t now() { return 1000000; }
unsigned frames() { return 0; }
uint64_t render_us() { return 0; }
} // namespace map_raster
unciv::State saved() {
  uint8_t bytes[2048];
  auto n = game::serialize(bytes, sizeof(bytes));
  unciv::State s;
  assert(n && unciv::restore(s, bytes, n));
  return s;
}
void tap(int x, int y) {
  game::pointer(pocket::Phase::DOWN, x, y);
  game::pointer(pocket::Phase::UP, x, y);
}
void movement_ui() {
  using namespace unciv;
  using pocket::Phase;
  pocket::WIDTH = pocket::HEIGHT = 288;
  State fixture;
  for (auto &t : fixture.tiles) {
    t.terrain = Grass;
    t.seen = 1;
  }
  int origin = 10 * W + 10, destination = neighbor(origin, 0);
  int unit = add_unit(fixture, 0, Warrior, origin);
  fixture.tiles[neighbor(origin, 1)].terrain = Mountain;
  auto load = [&] {
    uint8_t data[2048];
    auto n = serialize(fixture, data, sizeof(data));
    assert(n);
    game::load(data, n);
    game::key(pocket::Key::A);
  };
  auto draw = [] {
    map_raster::fills = map_raster::routes = map_raster::targets = 0;
    game::draw();
  };
  load();
  draw();
  unsigned circles = map_raster::fills, base_strokes = map_raster::routes;
  assert(circles > 0);
  // A held destination previews a legal path without spending movement.
  game::pointer(Phase::DOWN, 180, 145);
  draw();
  assert(map_raster::routes > base_strokes);
  assert(saved().units[unit].tile == origin && saved().units[unit].moves == 2);
  game::pointer(Phase::CANCEL, 180, 145);
  game::pointer(Phase::UP, 180, 145);
  draw();
  assert(map_raster::routes == base_strokes && map_raster::fills == circles);
  assert(saved().units[unit].tile == origin);
  // Unreachable terrain must preserve the selected unit and its range.
  tap(144, 166);
  draw();
  assert(map_raster::fills == circles && saved().units[unit].tile == origin);
  // Starting a pan cancels the route; releasing it must not move the unit.
  game::pointer(Phase::DOWN, 180, 145);
  game::pointer(Phase::MOVE, 194, 145);
  game::pointer(Phase::UP, 194, 145);
  assert(saved().units[unit].tile == origin);
  load();
  tap(180, 145);
  auto moved = saved();
  assert(moved.units[unit].tile == destination && moved.units[unit].moves == 1);
  // Loading exhausted or diplomatic states must invalidate the cached overlays.
  fixture.units[unit].moves = 0;
  load();
  draw();
  assert(map_raster::fills == 0 && map_raster::targets == 0);
  fixture.units[unit].moves = 2;
  add_unit(fixture, 1, Warrior, neighbor(origin, 4));
  fixture.wars = 1;
  load();
  draw();
  assert(map_raster::targets == 1);
  fixture.wars = 0;
  load();
  draw();
  assert(map_raster::targets == 0);
  // Two fingers cancel the pending single-finger movement. A remaining
  // release cannot move a unit, and a zoom must leave durable game state alone.
  assert(game::pinch_area(144, 124) && !game::pinch_area(144, 20));
  auto before_pinch = saved();
  game::pointer(Phase::DOWN, 180, 145);
  assert(game::pinch_begin(160, 124));
  game::pinch_update(170, 124, 87381);
  map_raster::first_sprite_width = 0;
  game::draw();
  assert(map_raster::first_sprite_width == 64);
  game::pointer(Phase::UP, 180, 145);
  game::pinch_end();
  assert(saved().units[unit].tile == before_pinch.units[unit].tile);
  assert(saved().units[unit].moves == before_pinch.units[unit].moves);
  assert(game::pinch_begin(144, 124));
  game::pinch_update(144, 124, 49152);
  game::pinch_end();
}
void management_ui() {
  using namespace unciv;
  using pocket::Key;
  pocket::WIDTH = 288;
  pocket::HEIGHT = 224;
  State fixture;
  for (auto &t : fixture.tiles) {
    t.terrain = Grass;
    t.seen = 1;
  }
  for (int n = 0; n < 6; ++n) {
    fixture.cities[n].tile = (5 + n) * W + 5;
    fixture.cities[n].job = Warrior;
    fixture.cities[n].production = 7;
    fixture.cities[n].pop = 2;
  }
  for (int n = 0; n < 10; ++n)
    add_unit(fixture, 0, Warrior, 5 * W + 10 + n);
  uint8_t data[2048];
  auto size = serialize(fixture, data, sizeof(data));
  assert(size);
  game::load(data, size);
  game::key(Key::A);
  game::key(Key::R);
  tap(40, 50); // Cities tab.
  game::draw();
  tap(230, 182); // Page 2.
  tap(70, 80);   // Fourth city.
  game::draw();
  tap(70, 118); // Inspect Settler; this must not discard the city's production.
  game::draw();
  assert(saved().cities[3].job == Warrior && saved().cities[3].production == 7);
  game::key(Key::B);
  assert(saved().cities[3].production == 7);
  tap(70, 118);
  game::key(Key::A);
  auto after = saved();
  assert(after.cities[3].job == Settler && after.cities[3].production == 0);
  assert(after.cities[0].job == Warrior && after.cities[0].production == 7);
  // Locked project inspection and confirmation cannot change production.
  tap(220, 170); // Swordsman at the minimum-height grid.
  game::draw();
  game::key(Key::A);
  assert(saved().cities[3].job == Settler);
  game::key(Key::B);
  game::key(Key::B);
  game::key(Key::R);
  tap(140, 50);  // Units tab.
  tap(230, 182); // Page 2.
  tap(70, 80);   // Fourth unit, then fortify it.
  game::key(Key::X);
  assert(saved().units[3].fortified && !saved().units[0].fortified);
  game::key(Key::X); // Wake must not grant movement points.
  assert(!saved().units[3].fortified && saved().units[3].moves == 0);
}
void strategy_ui() {
  using namespace unciv;
  using pocket::Key;
  pocket::WIDTH = pocket::HEIGHT = 288;
  State fixture;
  for (auto &t : fixture.tiles)
    t = {Grass, 0, 0, 7, 7};
  int origin = 10 * W + 10, target = neighbor(origin, 0);
  int a = add_unit(fixture, 0, Warrior, origin),
      b = add_unit(fixture, 1, Warrior, target);
  fixture.wars = 1;
  auto load = [&] {
    uint8_t bytes[2048];
    size_t n = serialize(fixture, bytes, sizeof(bytes));
    assert(n);
    game::load(bytes, n);
    game::key(Key::A);
  };
  load();
  auto p = battle_preview(fixture, a, target);
  tap(180, 145);
  game::draw(); // Forecast is read-only; cancel is safe.
  assert(saved().units[a].hp == 100 && saved().units[b].hp == 100);
  game::key(Key::B);
  tap(180, 145);
  game::key(Key::A);
  assert(saved().units[a].hp == p.attacker_after &&
         saved().units[b].hp == p.defender_after);
  fixture = State{};
  for (auto &t : fixture.tiles)
    t = {Grass, 0, 0, 7, 7};
  int settler = add_unit(fixture, 0, Settler, origin);
  assert(found(fixture, settler));
  for (auto &c : fixture.cities)
    if (c.tile >= 0)
      c.job = Warrior;
  int worker = add_unit(fixture, 0, Worker, target);
  load();
  game::key(Key::X);
  assert(saved().units[worker].work_left == 3);
  game::key(Key::Y);
  assert(saved().units[worker].work_left == 2);
  game::key(Key::SELECT); // Working unit is skipped without cancelling work.
  assert(saved().units[worker].work_left == 2);
  game::key(Key::R);
  tap(40, 50);
  tap(70, 80);
  game::key(Key::SELECT);
  game::draw();
  tap(180, 62);
  assert(saved().cities[0].focus == ProductionFocus);
  game::key(Key::B);
  game::key(Key::B);
  game::key(Key::Y);
  game::key(Key::Y);
  tap(218, 30);
  game::draw(); // Completion notifications are reachable.
  game::key(Key::B);
  game::key(Key::R);
  game::key(Key::SELECT);
  game::draw();
  for (int h : {224, 288, 416}) {
    pocket::HEIGHT = h;
    game::draw();
  }
}
void citizen_purchase_ui() {
  using namespace unciv;
  using pocket::Key;
  pocket::WIDTH = pocket::HEIGHT = 288;
  State fixture;
  for (auto &t : fixture.tiles)
    t = {Grass, 0, 0, 7, 7};
  int origin = 10 * W + 10, settler = add_unit(fixture, 0, Settler, origin);
  assert(found(fixture, settler));
  for (auto &c : fixture.cities)
    if (c.tile >= 0)
      c.job = Warrior;
  fixture.civ[0].gold = 500;
  uint8_t bytes[2048];
  size_t n = serialize(fixture, bytes, sizeof(bytes));
  assert(n);
  game::load(bytes, n);
  game::key(Key::A);
  game::key(Key::R);
  tap(40, 50);
  tap(70, 80);
  tap(220, 270);
  game::draw();
  assert(saved().civ[0].gold == 500);
  game::key(Key::B);
  tap(220, 270);
  game::key(Key::A);
  assert(saved().civ[0].gold < 500);
  game::key(Key::SELECT);
  game::key(Key::SELECT);
  game::draw();
  game::key(Key::RIGHT);
  game::key(Key::X);
  assert(tile_locked(saved(), 0, neighbor(origin, 0)));
  game::key(Key::B);
  game::key(Key::B);
  game::key(Key::SELECT);
  game::key(Key::SELECT);
  assert(tile_locked(saved(), 0, neighbor(origin, 0)));
  for (int h : {224, 288, 416}) {
    pocket::HEIGHT = h;
    game::draw();
  }
  game::key(Key::Y);
  assert(!saved().cities[0].locked);
  pocket::HEIGHT = 288;
  game::pointer(pocket::Phase::DOWN, 172, 140);
  game::pointer(pocket::Phase::CANCEL, 172, 140);
  game::pointer(pocket::Phase::UP, 172, 140);
  assert(!saved().cities[0].locked);
}
void progression_ui() {
  using namespace unciv;
  using pocket::Key;
  pocket::WIDTH = pocket::HEIGHT = 288;
  State fixture;
  for (auto &t : fixture.tiles)
    t = {Grass, 0, 0, 7, 7};
  int home = 10 * W + 10, settler = add_unit(fixture, 0, Settler, home);
  assert(found(fixture, settler));
  for (auto &c : fixture.cities)
    if (c.tile >= 0)
      c.job = Warrior;
  int a = add_unit(fixture, 0, Warrior, neighbor(home, 0));
  int scout = add_unit(fixture, 0, Scout, neighbor(home, 1));
  fixture.units[a].xp = 30;
  fixture.units[a].hp = 60;
  fixture.civ[0].gold = 100;
  fixture.civ[0].tech |= 1 << Iron;
  uint8_t bytes[2048];
  size_t n = serialize(fixture, bytes, sizeof(bytes));
  assert(n);
  game::load(bytes, n);
  game::key(Key::A);
  tap(263, 220);
  game::draw();
  tap(220, 132);
  game::draw();
  tap(220, 76);
  assert(saved().units[a].promotions == 0); // Choosing a card is read-only.
  tap(144, 246);
  assert(saved().units[a].promotions == 4 && saved().units[a].xp == 30);
  tap(220, 132);
  tap(220, 132);
  game::key(Key::B);
  assert(saved().units[a].hp == 60);
  tap(70, 166);
  game::draw();
  game::key(Key::B);
  assert(saved().civ[0].gold == 100);
  tap(70, 166);
  tap(144, 246);
  auto s = saved();
  assert(s.civ[0].gold == 48 && s.units[a].kind == Swordsman &&
         s.units[a].xp == 30 && s.units[a].promotions == 4 &&
         s.units[a].hp == 60 && !s.units[a].moves);
  game::key(Key::B);
  game::key(Key::Y);
  tap(263, 220);
  tap(220, 101);
  assert(saved().units[a].order == Healing);
  game::key(Key::SELECT);
  game::key(Key::X);
  s = saved();
  assert(s.units[scout].fortified && s.units[a].order == Healing);
  game::key(Key::L);
  tap(40, 145);
  tap(144, 246);
  assert(saved().civ[0].research == Mining);
  game::key(Key::B);
  game::key(Key::R);
  tap(140, 50);
  game::key(Key::X);
  game::draw();
  for (int h : {224, 288, 416}) {
    pocket::HEIGHT = h;
    game::draw();
  }
  pocket::HEIGHT = 288;
}
void production_ui() {
  using namespace unciv;
  using pocket::Key;
  pocket::WIDTH = pocket::HEIGHT = 288;
  State fixture;
  for (auto &t : fixture.tiles)
    t = {Grass, 0, 0, 7, 7};
  int settler = add_unit(fixture, 0, Settler, 10 * W + 10);
  assert(found(fixture, settler));
  uint8_t bytes[2048];
  auto n = serialize(fixture, bytes, sizeof(bytes));
  assert(n);
  game::load(bytes, n);
  game::key(Key::A);
  game::key(Key::Y);
  assert(saved().turn == 1 && saved().cities[0].job == NoJob);
  game::draw(); // Routes to required production.
  game::key(Key::A);
  game::key(Key::A);
  assert(saved().cities[0].job == Warrior);
  game::key(Key::B);
  game::key(Key::Y);
  auto bank = saved().cities[0].production;
  assert(saved().turn == 2 && bank > 0);
  game::key(Key::X);
  game::key(Key::DOWN);
  game::key(Key::A);
  game::key(Key::X);
  assert(saved().cities[0].queued == 1 &&
         saved().cities[0].queue[0] == Worker &&
         saved().cities[0].production == bank);
  game::key(Key::B);
  game::key(Key::LEFT);
  game::key(Key::DOWN);
  game::key(Key::A);
  game::key(Key::X);
  assert(saved().cities[0].queued == 2 && saved().cities[0].queue[1] == Scout);
  game::key(Key::DOWN);
  game::key(Key::L);
  assert(saved().cities[0].queue[0] == Scout);
  game::key(Key::X);
  assert(saved().cities[0].queued == 1 && saved().cities[0].queue[0] == Worker);
  game::key(Key::X);
  game::key(Key::Y);
  assert(saved().cities[0].repeat);
  for (int h : {224, 288, 416}) {
    pocket::HEIGHT = h;
    game::draw();
  }
  pocket::HEIGHT = 288;
  game::key(Key::B);
  game::key(Key::LEFT);
  game::key(Key::DOWN);
  game::key(Key::A);
  game::key(Key::A);
  assert(saved().cities[0].job == Scout &&
         saved().cities[0].invested[Warrior] == bank &&
         !saved().cities[0].repeat);
}
void tactics_ui() {
  using namespace unciv;
  using pocket::Key;
  pocket::WIDTH = pocket::HEIGHT = 288;
  State fixture;
  int a = unciv::test::tactics_fixture(fixture), tile = fixture.units[a].tile;
  uint8_t bytes[2048];
  auto n = serialize(fixture, bytes, sizeof(bytes));
  assert(n);
  game::load(bytes, n);
  game::key(Key::A);
  tap(263, 220);
  game::draw();
  tap(220, 166); // Open pillage confirmation.
  game::draw();
  assert(saved().civ[0].gold == 30 && saved().units[a].hp == 50);
  game::key(Key::B);
  assert(saved().tiles[tile].improvement == 1);
  tap(220, 166);
  tap(144, 246);
  auto result = saved();
  assert(result.civ[0].gold == 55 && result.units[a].hp == 75 &&
         result.units[a].moves == 1 && result.tiles[tile].improvement == 3);
  tap(263, 220);
  tap(220, 166);
  tap(144, 246); // A second attempt must not award gold again.
  assert(saved().civ[0].gold == 55);
}
int main() {
  tactics_ui();
  using pocket::Key;
  pocket::WIDTH = 288;
  pocket::HEIGHT = 224;
  game::init();
  uint8_t bytes[2048];
  assert(!game::serialize(bytes, sizeof(bytes)));
  // Touch on Start, then the bottom Found control.
  tap(144, 124);
  tap(216, 162);
  auto s = saved();
  assert(s.turn == 1 && s.capitals[0] < 0);
  tap(100, 208);
  s = saved();
  assert(s.capitals[0] >= 0);
  game::key(Key::X);
  game::key(Key::LEFT);
  game::key(Key::DOWN);
  game::key(Key::A);
  assert(saved().cities[unciv::city_at(saved(), saved().capitals[0])].job ==
         unciv::NoJob);
  game::key(Key::A);
  s = saved();
  assert(s.cities[unciv::city_at(s, s.capitals[0])].job == unciv::Scout);
  game::key(Key::B);
  game::key(Key::L);
  game::key(Key::DOWN);
  game::key(Key::A);
  s = saved();
  assert(s.civ[0].research == unciv::Mining);
  game::key(Key::B);
  game::key(Key::R);
  game::key(Key::A);
  s = saved();
  assert(unciv::war(s, 0, 1));
  game::key(Key::B);
  auto n = game::serialize(bytes, sizeof(bytes));
  game::pointer(pocket::Phase::DOWN, 250, 208);
  game::pointer(pocket::Phase::CANCEL, 250, 208);
  game::pointer(pocket::Phase::UP, 250, 208);
  auto after = saved();
  assert(after.turn == 1);
  game::key(Key::Y);
  s = saved();
  assert(s.turn == 2);
  game::load(bytes, n);
  s = saved();
  assert(s.turn == 1 && s.civ[0].research == unciv::Mining);
  // Reset only opens confirmation; cancel must preserve the campaign.
  game::reset();
  game::key(Key::A);
  s = saved();
  assert(s.capitals[0] >= 0 && s.turn == 1);
  // Render every screen at minimum and tall aspect ratios; under sanitizers
  // this checks clipping loops.
  game::key(Key::A);
  for (int h : {224, 416}) {
    pocket::HEIGHT = h;
    game::draw();
    game::key(Key::L);
    game::draw();
    game::key(Key::B);
    game::key(Key::R);
    game::draw();
    game::key(Key::B);
  }
  movement_ui();
  management_ui();
  strategy_ui();
  citizen_purchase_ui();
  progression_ui();
  production_ui();
  puts("Touch/controller, cancellation, load and reset journeys passed");
}
