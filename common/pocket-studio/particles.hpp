// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "studio.hpp"
namespace studio {
// Cosmetic randomness is deliberately separate from each game's random stream.
template <unsigned Count = 32> class Particles {
    static_assert(Count > 0, "A particle pool needs at least one slot");
    struct Dot {
        float x = 0, y = 0, vx = 0, vy = 0;
        uint16_t life = 0, total = 1, color = 0;
    } dots[Count];
    unsigned next = 0;
    uint32_t random = 0x7f4a7c15u;
    uint32_t sample() {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return random;
    }

  public:
    void clear() {
        for (auto &p : dots)
            p.life = 0;
        next = 0;
    }
    void burst(int x, int y, uint16_t color, unsigned count = 12) {
        if (count > Count)
            count = Count;
        for (unsigned i = 0; i < count; ++i) {
            auto &p = dots[next++ % Count];
            p.x = float(x);
            p.y = float(y);
            p.vx = float(int(sample() % 91) - 45);
            p.vy = -float(18 + sample() % 48);
            p.total = p.life = uint16_t(400 + sample() % 380);
            p.color = color;
        }
    }
    bool tick(uint32_t ms) {
        const uint32_t physics_ms = ms > 100 ? 100 : ms;
        bool changed = false;
        float dt = physics_ms * .001f;
        for (auto &p : dots)
            if (p.life) {
                changed = true;
                p.life = ms >= p.life ? 0 : uint16_t(p.life - ms);
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                p.vy += 85 * dt;
            }
        return changed;
    }
    unsigned active() const {
        unsigned n = 0;
        for (const auto &p : dots)
            n += p.life != 0;
        return n;
    }
    void draw() const {
        for (const auto &p : dots)
            if (p.life) {
                int x = int(p.x), y = int(p.y);
                glow(x, y, 3, p.color, 100 * int(p.life) / p.total);
                glow(x, y, 1, p.color, 256);
            }
    }
};
} // namespace studio
