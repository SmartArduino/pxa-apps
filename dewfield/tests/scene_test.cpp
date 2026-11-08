// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace farm_scene;
constexpr int W = 320, H = 320;
using Pixels = std::array<uint16_t, W * H>;
static Pixels subject, actor, combined, reference;
static void overlap(Kind kind, int x, int y, int depth, int id = 0) {
  farm::State state;
  farm::fresh(state);
  View view{};
  Entry object{kind, int16_t(x),  int16_t(y), int16_t(depth),
               0,    uint8_t(id), false};
  state.plots[30] = {1, 1, 2, 1};
  for (int delta : {-3, 3}) {
    state.px = kind == Kind::House ? x + 40 : x;
    state.py = depth + delta;
    Queue q;
    q.viewport = {0, 0, W, H};
    q.add(kind, x, y, depth, id);
    subject.fill(0);
    art::Raster r{subject.data(), W, H};
    q.objects(r, state, view);
    Queue p;
    p.viewport = q.viewport;
    p.add(Kind::Player, state.px, state.py, state.py);
    actor.fill(0);
    r.p = actor.data();
    p.objects(r, state, view);
    q.add(Kind::Player, state.px, state.py, state.py);
    q.sort();
    combined.fill(0);
    r.p = combined.data();
    q.objects(r, state, view);
    int shared = 0;
    for (int i = 0; i < W * H; ++i)
      if (subject[i] && actor[i]) {
        assert(combined[i] == (delta < 0 ? subject[i] : actor[i]));
        ++shared;
      }
    assert(shared >= 5);
    if (delta > 0) {
      combined.fill(0);
      q.shadows(r, state, view);
      q.objects(r, state, view);
      for (int i = 0; i < W * H; ++i)
        if (actor[i])
          assert(combined[i] == actor[i]);
    }
  }
  (void)object;
}
int main() {
  overlap(Kind::Chest, 136, 156, 156); // Both sides remain inside map row 9.
  overlap(Kind::Well, 120, 140, 140);
  overlap(Kind::Board, 168, 140, 140);
  overlap(Kind::Television,168,140,140);overlap(Kind::Calendar,168,140,140);
  overlap(Kind::Counter,64,160,160);overlap(Kind::WorldNpc,168,140,140);
  overlap(Kind::Cabinet,168,140,140);overlap(Kind::Fireplace,168,140,140);
  overlap(Kind::Chair,168,140,140);overlap(Kind::Shelf,168,140,140);
  overlap(Kind::Signpost,168,140,140);
  overlap(Kind::House, 64, 48, 109);
  overlap(Kind::Fence, 160, 128, 143);
  overlap(Kind::Rock, 168, 220, 220);
  overlap(Kind::Crop, 168, 204, 204, 30);
  overlap(Kind::Sprinkler, 168, 159, 159);
  farm::State s;
  farm::fresh(s);farm::begin_ranch(s);
  for(auto &a:s.animals)a={1,0,4,50};
  for (auto &p : s.plots)
    p = {1, 1, 2, 1};
  std::memset(s.sprinklers, 255, sizeof(s.sprinklers));
  View v{};s.minute=480;
  Queue all;
  all.build(s, v, {0, 0, 512, 384});
  assert(all.count < Queue::Capacity && all.count > 250);
  bool found[unsigned(Kind::RanchShed)+1]{};
  for (unsigned i = 0; i < all.count; ++i) {
    found[int(all.entries[i].kind)] = true;
    if (i)
      assert(all.entries[i - 1].depth <= all.entries[i].depth);
  }
  for(unsigned k=0;k<=unsigned(Kind::Player);++k)if(k!=unsigned(Kind::Villager)&&k!=unsigned(Kind::Chicken))assert(found[k]);
  assert(found[unsigned(Kind::Animal)]&&found[unsigned(Kind::RanchShed)]);
  assert(found[unsigned(Kind::WorldNpc)]);
  // Sorting is deterministic even when the caller's submission order changes.
  Queue q;
  q.build(s, v, {0, 0, W, H});
  reference.fill(0);
  art::Raster r{reference.data(), W, H};
  q.shadows(r, s, v);
  q.objects(r, s, v);
  std::reverse(q.entries, q.entries + q.count);
  q.sort();
  combined.fill(0);
  r.p = combined.data();
  q.shadows(r, s, v);
  q.objects(r, s, v);
  assert(reference == combined);
  // A transparent canopy must not turn the trunk transparent.
  s.px = 168;
  s.py = 154;
  Entry tree{Kind::Tree, 168, 157, 157, 0, 0, true};
  assert(faded(tree, s));
  subject.fill(0x8c51);
  actor = subject;
  r.p = subject.data();
  art::tree(r, 168, 157, 0, 0, 8, false);
  r.p = actor.data();
  art::tree(r, 168, 157, 0, 0, 4, false);
  int changed = 0;
  for (int y = 93; y < 157; ++y)
    for (int x = 141; x < 195; ++x) {
      if (y >= 139)
        assert(subject[y * W + x] == actor[y * W + x]);
      else
        changed += subject[y * W + x] != actor[y * W + x];
    }
  assert(changed > 100);
  Queue picking;
  picking.viewport = {0, 0, W, H};
  picking.add(Kind::Tree, 168, 157, 157, 0, true);
  int tx = -1, ty = -1;
  for (unsigned i = 0; i < sprites::oak.count; ++i) {
    auto &span = sprites::oak.spans[i];
    if (span.y == 30)
      assert(!picking.pick(141 + span.x, 93 + span.y, s, tx, ty));
    if (span.y == 55) {
      assert(picking.pick(141 + span.x, 93 + span.y, s, tx, ty));
      assert(tx == 10 && ty == 9);
    }
  }
  picking.count = 0;
  picking.add(Kind::Chest, 136, 156, 156, 0, true);
  picking.add(Kind::Well, 136, 154, 154, 0, true);
  picking.sort();
  int shared = 0;
  for (int y = 135; y < 154; ++y)
    for (int x = 124; x < 148; ++x)
      if (art::hit(sprites::chest, x - 124, y - 135) &&
          art::hit(sprites::well, x - 120, y - 121)) {
        assert(picking.pick(x, y, s, tx, ty) && tx == 8 && ty == 9);
        ++shared;
      }
  assert(shared > 10);
  puts("PASS: exact-pixel foreground/background overlaps, all world object "
       "categories, shadow ordering, deterministic sorting, canopy/trunk "
       "separation and shared picking order");
}
