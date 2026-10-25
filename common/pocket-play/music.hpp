// SPDX-License-Identifier: CC0-1.0
// Original Pocket Play event sound cues, dedicated to the public domain.
// No background score, gameplay RNG, or saved state.
#pragma once
#include "runtime.hpp"

namespace pocket_music {
enum class Cue {
    Select, Confirm, Collect, Upgrade, Goal, Error, Order, Cook, Ready,
    Serve, Restock, Day, Paint, Clear, Reaction
};
namespace detail {
inline float frequency(uint8_t midi) {
    static constexpr float octave[12] = {
        261.625565f,277.182631f,293.664768f,311.126984f,
        329.627557f,349.228231f,369.994423f,391.995436f,
        415.304698f,440.0f,466.163762f,493.883301f
    };
    float result = octave[midi % 12];
    const int shift = int(midi / 12) - 5;
    for (int i = 0; i < shift; ++i) result *= 2.0f;
    for (int i = 0; i > shift; --i) result *= 0.5f;
    return result;
}
struct Effect {
    uint8_t notes[4];
    uint16_t spacing_ms;
    uint16_t duration_ms;
    uint8_t count;
};
inline Effect effect(Cue cue) {
    switch (cue) {
    case Cue::Select:   return {{79, 0, 0, 0},  0,35,1};
    case Cue::Confirm:  return {{72,76, 0, 0}, 55,45,2};
    case Cue::Collect:  return {{79,83,86, 0}, 40,35,3};
    case Cue::Upgrade:  return {{74,81,86,90}, 55,50,4};
    case Cue::Goal:     return {{72,76,79,84}, 65,60,4};
    case Cue::Error:    return {{52,51, 0, 0}, 60,45,2};
    case Cue::Order:    return {{74,79, 0, 0}, 55,40,2};
    case Cue::Cook:     return {{60,62,64, 0}, 40,30,3};
    case Cue::Ready:    return {{76,79,84, 0}, 40,35,3};
    case Cue::Serve:    return {{67,72,76, 0}, 40,35,3};
    case Cue::Restock:  return {{55,62, 0, 0}, 60,45,2};
    case Cue::Day:      return {{72,79,81, 0}, 40,35,3};
    case Cue::Paint:    return {{81, 0, 0, 0},  0,25,1};
    case Cue::Clear:    return {{67,60, 0, 0}, 60,40,2};
    case Cue::Reaction: return {{69,76,81, 0}, 40,35,3};
    }
    return {{0,0,0,0},0,0,0};
}
} // namespace detail

// Event-only audio. Synchronize set_enabled() before input cues. There is no
// periodic audio tick: an idle game never schedules tones. The runtime owns
// pause/background/exit silence and the bounded synthesized-tone queue.
class Effects {
public:
    void reset(bool enabled = true) {
        pocket::silence();
        enabled_ = enabled;
        clear_cues();
    }
    void set_enabled(bool enabled) {
        if (enabled_ == enabled) return;
        enabled_ = enabled;
        if (!enabled) pocket::silence();
        clear_cues();
    }
    void cue(Cue cue) {
        if (!enabled_) return;
        const detail::Effect effect = detail::effect(cue);
        if (!effect.count) return;
        const uint32_t now = pocket::millis();
        const bool important = cue == Cue::Goal || cue == Cue::Upgrade;
        // Rewards have an independent limiter, so Select/Paint cannot hide them.
        if (important) {
            if (important_seen_ && now - important_at_ < 350) return;
        } else {
            if (normal_seen_ && now - normal_at_ < 40) return;
            if (cue == Cue::Paint && paint_seen_ && now - paint_at_ < 150) return;
        }
        // Serialize effects into a short lane, reserving room for rewards.
        // A 60 ms lead covers the runtime's already-rendered PCM blocks.
        const uint32_t remaining = cue_end_ - now;
        const uint32_t delay = remaining < 1000 && remaining > 60 ? remaining : 60;
        const uint32_t length = uint32_t(effect.count - 1) * effect.spacing_ms + effect.duration_ms;
        if (!important && delay + length > 190) return;
        for (uint8_t i = 0; i < effect.count; ++i)
            tone(effect.notes[i], effect.duration_ms, now,
                 uint16_t(delay + uint32_t(i) * effect.spacing_ms));
        cue_end_ = now + delay + length;
        if (important) { important_seen_ = true; important_at_ = now; }
        else {
            normal_seen_ = true;
            normal_at_ = now;
            if (cue == Cue::Paint) { paint_seen_ = true; paint_at_ = now; }
        }
    }
private:
    bool enabled_ = false;
    bool normal_seen_ = false, important_seen_ = false, paint_seen_ = false;
    uint32_t cue_end_ = 0, normal_at_ = 0, important_at_ = 0, paint_at_ = 0;
    void clear_cues() {
        normal_seen_ = important_seen_ = paint_seen_ = false;
        cue_end_ = pocket::millis();
    }
    static void tone(uint8_t midi, uint16_t duration_ms, uint32_t now, uint16_t ahead_ms) {
        // Sum before converting so foreground-clock wrap cannot wrap an onset.
        pocket::tone(detail::frequency(midi), float(duration_ms) / 1000.0f,
                     float((double(now) + ahead_ms) / 1000.0));
    }
};
} // namespace pocket_music
