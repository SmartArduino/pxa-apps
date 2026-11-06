// SPDX-License-Identifier: MIT
#include "../scene.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <queue>
using namespace farm_scene;
constexpr int W = 144, H = 144, X = 72, Y = 104;
constexpr uint16_t Floor = art::rgb(178, 149, 111);
using Pixels = std::array<uint16_t, W * H>;
static Pixels body, shadow, composite, reference;

static void compiled_contact(const sprites::Sprite &s) {
  assert(s.shadow && s.shadow->count);
  int bottom = 0, contacts = 0;
  for (unsigned i = 0; i < s.count; ++i)
    bottom = std::max(bottom, int(s.spans[i].y));
  assert(bottom + 1 == s.anchor_y);
  // Dark contact pixels must sit immediately below real opaque source pixels.
  // A gap, a detached ellipse, or contact beneath a lifted boot fails here.
  bool visited[128 * 128]{};
  for (unsigned i = 0; i < s.shadow->count; ++i) {
    const auto &span = s.shadow->spans[i];
    assert(span.count && span.alpha >= 1 && span.alpha <= 3);
    for (int x = span.x; x < span.x + span.count; ++x) {
      int at = (span.y + 4) * 128 + x + 4;
      assert(at >= 0 && at < 128 * 128 && !visited[at]);
      visited[at] = true; // Each ground pixel is blended only once.
      assert(!art::hit(s, x, span.y));
      if (span.alpha > 1) {
        assert(art::hit(s, x, span.y - 1));
        ++contacts;
      }
    }
  }
  assert(contacts > 0);
}

static void actor_contacts() {
  farm::State state;
  farm::fresh(state);
  state.px = X;
  state.py = Y;
  for (int dir = 0; dir < 4; ++dir)
    for (int frame = 0; frame < 9; ++frame) {
      const auto &s = art::person_sprite(dir, frame);
      compiled_contact(s);
      body.fill(0);
      art::Raster r{body.data(), W, H};
      art::person(r, X, Y, dir, frame, false, 0, 0, false);
      shadow.fill(Floor);
      r.p = shadow.data();
      art::person_shadow(r, X, Y, dir, frame);
      int supported = 0;
      for (int yy = Y - 2; yy <= Y + 1; ++yy)
        for (int xx = X - 12; xx <= X + 12; ++xx)
          if (shadow[yy * W + xx] == art::blend(Floor, art::ShadowTint, 3)) {
            assert(body[(yy - 1) * W + xx]);
            ++supported;
          }
      assert(supported >= 2);
      if (frame) {
        auto p = art::gait::pose(frame, dir);
        auto opposite = art::gait::pose((frame + 3) % 8 + 1, dir);
        for (int i = 0; i < 2; ++i) {
          // One phase drives all four limbs: same-side arm counters the leg.
          assert(p.arms[i].swing == -p.legs[i].swing);
          assert(p.arms[i].swing == p.legs[1 - i].swing);
          assert(p.legs[i].swing == opposite.legs[1 - i].swing);
          assert(p.legs[i].lift == opposite.legs[1 - i].lift);
          assert(!p.legs[i].lift || !p.legs[1 - i].lift);
        }
      }
      bool visited[W * H]{};
      int first = -1;
      for (int i = 0; i < W * H; ++i)
        if (body[i]) {
          first = i;
          break;
        }
      assert(first >= 0);
      std::queue<int> pending;
      pending.push(first);
      visited[first] = true;
      while (!pending.empty()) {
        int at = pending.front();
        pending.pop();
        for (int d : {-W, W, -1, 1}) {
          int n = at + d;
          if (n < 0 || n >= W * H || visited[n] || !body[n] ||
              std::abs(n % W - at % W) + std::abs(n / W - at / W) != 1)
            continue;
          visited[n] = true;
          pending.push(n);
        }
      }
      for (int yy = Y - 2; yy <= Y + 1; ++yy)
        for (int xx = X - 12; xx <= X + 12; ++xx)
          if (shadow[yy * W + xx] == art::blend(Floor, art::ShadowTint, 3))
            assert(visited[(yy - 1) * W + xx]); // Planted boot joins the hip.
      for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
          if (shadow[y * W + x] != Floor)
            assert(y >= Y - 2 && y <= Y + 1 && std::abs(x - X) <= 12);
      // Standalone and queued painting must agree, including the gait-specific
      // planted foot. This catches stale standing shadows during walking.
      View view{dir, frame};
      Queue q;
      q.viewport = {0, 0, W, H};
      q.add(Kind::Player, X, Y, Y);
      composite.fill(Floor);
      r.p = composite.data();
      q.shadows(r, state, view);
      q.objects(r, state, view);
      reference.fill(Floor);
      r.p = reference.data();
      art::person(r, X, Y, dir, frame);
      assert(composite == reference);
    }
}

// Compare actual rasterized limbs at opposing contacts. This also catches a
// renderer that applies the correct phase to the wrong source arm or leg.
static int limb_centre(int dir, int frame, int index, bool arm) {
  body.fill(0);
  art::Raster r{body.data(), W, H};
  const auto &rig = art::gait::rig(dir);
  auto p = art::gait::pose(frame, dir);
  if (arm)
    art::gait::arm(r, rig, p, dir, index, X - 12, Y - 30);
  else
    art::gait::leg(r, rig, p, dir, index, X - 12, Y - 30);
  int count = 0, sum = 0;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
      if (body[y * W + x]) {
        ++count;
        sum += dir & 1 ? x : y;
      }
  assert(count);
  return sum * 1024 / count;
}
static void counter_swing() {
  for (int frame = 1; frame <= 8; ++frame) {
    auto canonical = art::gait::pose(frame);
    for (int dir = 0; dir < 4; ++dir) {
      auto p = art::gait::pose(frame, dir);
      int left = dir == 1 || dir == 2 ? 1 : 0;
      assert(p.legs[left].swing == canonical.legs[0].swing);
      assert(p.legs[left].lift == canonical.legs[0].lift);
    }
  }
  for (int dir = 0; dir < 4; ++dir)
    for (int limb = 0; limb < 2; ++limb) {
      int hand = limb_centre(dir, 1, limb, true) -
                 limb_centre(dir, 5, limb, true);
      int boot = limb_centre(dir, 1, limb, false) -
                 limb_centre(dir, 5, limb, false);
      assert(hand && boot && hand * boot < 0);
    }
}

static void walking_actions() {
  for (int dir = 0; dir < 4; ++dir)
    for (int frame = 1; frame <= 8; ++frame) {
      reference.fill(0);
      art::Raster r{reference.data(), W, H};
      auto pose = art::gait::pose(frame, dir);
      pose.bob = 0;
      const auto &rig = art::gait::rig(dir);
      art::gait::leg(r, rig, pose, dir, 1, X - 12, Y - 30);
      art::gait::leg(r, rig, pose, dir, 0, X - 12, Y - 30);
      for (int tool = 0; tool < 7; ++tool)
        for (int swing : {360, 170, 70}) {
          body.fill(0);
          r.p = body.data();
          art::person(r, X, Y, dir, frame, false, swing, tool, false);
          for (int y = Y - 5; y <= Y; ++y)
            for (int x = X - 12; x <= X + 12; ++x)
              if (reference[y * W + x])
                assert(body[y * W + x]); // Working never replaces moving boots.
        }
    }
}

static void object_contacts() {
  farm::State state;
  farm::fresh(state);
  for (const auto *s :
       {&sprites::oak, &sprites::oak_summer, &sprites::oak_fall,
        &sprites::oak_winter, &sprites::rocks, &sprites::grass, &sprites::chest,
        &sprites::well, &sprites::board, &sprites::sprinkler, &sprites::cottage,
        &sprites::villager})
    compiled_contact(*s);
  for (Kind kind :
       {Kind::Tree, Kind::Rock, Kind::Grass, Kind::Chest, Kind::Well,
        Kind::Board, Kind::Sprinkler, Kind::House, Kind::Villager}) {
    Queue q;
    q.viewport = {0, 0, W, H};
    q.add(kind, kind == Kind::House ? X - 42 : X,
          kind == Kind::House ? Y - 61 : Y, Y);
    body.fill(0);
    art::Raster r{body.data(), W, H};
    q.objects(r, state, {});
    int bottom = -1;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        if (body[y * W + x])
          bottom = y;
    assert(bottom == Y - 1);
    shadow.fill(Floor);
    r.p = shadow.data();
    q.shadows(r, state);
    int supported = 0;
    for (int x = 0; x < W; ++x)
      if (body[(Y - 1) * W + x]) {
        assert(shadow[Y * W + x] != Floor);
        ++supported;
      }
    assert(supported);
    if (kind == Kind::Board)
      for (int x = X - 4; x <= X + 3; ++x)
        assert(shadow[Y * W + x] == Floor); // Air between the two posts.
  }
}

static void procedural_contacts() {
  for (int type = 0; type < 4; ++type)
    for (int stage = 0; stage < 4; ++stage) {
      body.fill(0);
      art::Raster r{body.data(), W, H};
      art::crop(r, X, Y, type, stage, stage == 3);
      bool planted = false;
      for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
          if (body[y * W + x]) {
            assert(y < Y); // No roots, fruit or stake below the ground plane.
            planted |= y == Y - 1;
          }
      assert(planted);
      shadow.fill(Floor);
      r.p = shadow.data();
      art::crop_shadow(r, X, Y, type, stage, stage == 3);
      for (int x = 0; x < W; ++x)
        if (shadow[Y * W + x] == art::blend(Floor, art::ShadowTint, 2)) {
          bool attached = false;
          for (int dx = -1; dx <= 1; ++dx)
            attached |= body[(Y - 1) * W + x + dx] != 0;
          assert(attached);
        }
    }
  for (int frame = 0; frame < 2; ++frame) {
    body.fill(0);
    art::Raster r{body.data(), W, H};
    art::chicken(r, X, Y, frame);
    assert(body[(Y - 1) * W + X - 2]);
    for (int y = Y; y < H; ++y)
      for (int x = 0; x < W; ++x)
        assert(!body[y * W + x]);
  }
  // Scan clipped profiles at every edge, including non-zero camera origins.
  for (auto size : {std::pair<int, int>{1, 1}, {7, 9}, {33, 27}})
    for (int x : {-80, -1, 0, 20}) {
      std::array<uint16_t, 33 * 27 + 32> pixels;
      pixels.fill(0xabcd);
      art::Raster r{pixels.data() + 16, size.first, size.second, 3, 5};
      for (const auto *s :
           {&sprites::cottage, &sprites::oak, &sprites::west2, &sprites::board})
        art::contact_shadow(r, *s, x, 12);
      art::fence_shadow(r, x, -1, 48);
      for (int i = 0; i < 16; ++i) {
        assert(pixels[i] == 0xabcd);
        assert(pixels[16 + size.first * size.second + i] == 0xabcd);
      }
    }
}

int main() {
  actor_contacts();
  counter_swing();
  walking_actions();
  object_contacts();
  procedural_contacts();
  puts("PASS: all gait/prop contacts, registered ground planes, separate post "
       "shadows, planted crops/chickens, single-pass profiles and clipping");
}
