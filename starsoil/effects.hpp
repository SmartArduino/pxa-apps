// SPDX-License-Identifier: MIT
#pragma once
#include "core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
namespace starsoil {
// Bounded cosmetic state. It never changes the world RNG, loot or save format.
struct Effects {
  struct Particle {
    float x = 0, y = 0, vx = 0, vy = 0;
    uint16_t life = 0;
    uint32_t color = 0;
  };
  struct Pop {
    float x = 0, y = 0;
    uint16_t life = 0;
    uint8_t item = 0;
    int amount = 0;
    uint32_t color = 0;
  };
  std::array<Particle, 40> particles{};
  std::array<Pop, 8> pops{};
  std::array<int, ItemCount> inventory{};
  std::array<int, MaxMobs> health{};
  uint32_t mined = 0, seed = 0x27d4eb2d;
  uint16_t hp = 100, dust = 0;
  uint8_t target = Stone;
  unsigned next_particle = 0, next_pop = 0;
  bool initialized = false;
  uint32_t random() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    return seed ^= seed << 5;
  }
  void burst(float x, float y, uint32_t color, int amount = 8) {
    for (int i = 0; i < amount; ++i) {
      auto &p = particles[next_particle++ % particles.size()];
      p = {x,
           y,
           float(int(random() % 81) - 40),
           -float(20 + random() % 55),
           uint16_t(230 + random() % 260),
           color};
    }
  }
  void pop(float x, float y, int amount, uint32_t color, uint8_t item = 0) {
    // Repeated pickups of one material combine instead of filling the screen.
    if (item)
      for (auto &p : pops)
        if (p.life > 350 && p.item == item) {
          p.amount += amount;
          p.life = 1100;
          return;
        }
    pops[next_pop++ % pops.size()] = {x, y, 1100, item, amount, color};
  }
  void sync(const State &s) {
    inventory.fill(0);
    for (auto b : s.bag)
      inventory[b.item] += b.count;
    for (int i = 0; i < MaxMobs; ++i)
      health[i] = s.mobs[i].hp;
    mined = s.mined;
    hp = s.hp;
    target = tile(s, s.target_x, s.target_y);
    initialized = true;
  }
  void step(const State &s, uint32_t ms, bool active) {
    float dt = std::min(ms, 100u) * .001f;
    for (auto &p : particles)
      if (p.life) {
        p.life = p.life > ms ? p.life - ms : 0;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy += 130 * dt;
      }
    for (auto &p : pops)
      if (p.life) {
        p.life = p.life > ms ? p.life - ms : 0;
        p.y -= 13 * dt;
      }
    if (!initialized || !active) {
      sync(s);
      return;
    }
    if (s.mined > mined)
      burst(s.target_x * 16 + 8, s.target_y * 16 + 8,
            items[tile_drop(target ? target : Stone)].color, 12);
    if (s.hp < hp)
      pop(s.player.x, s.player.y - 35, -int(hp - s.hp), 0xe99389);
    std::array<int, ItemCount> now{};
    for (auto b : s.bag)
      now[b.item] += b.count;
    for (int id = 1; id < ItemCount; ++id)
      if (now[id] > inventory[id])
        pop(s.player.x, s.player.y - 39, now[id] - inventory[id],
            items[id].color, uint8_t(id));
    for (int i = 0; i < MaxMobs; ++i)
      if (health[i] > 0 && s.mobs[i].hp < health[i]) {
        const auto &m = s.mobs[i];
        pop(m.x, m.y - 29, -(health[i] - std::max(0, int(m.hp))), 0xffd797);
        burst(m.x, m.y - 10, 0xe4b98c, 5);
      }
    dust = uint16_t(dust + ms);
    if (s.player.grounded && std::abs(s.player.vx) > 25 && dust >= 120) {
      dust = 0;
      burst(s.player.x, s.player.y - 1, 0x9e947a, 1);
    }
    sync(s);
  }
};
} // namespace starsoil
