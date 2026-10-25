// SPDX-License-Identifier: CC0-1.0
// Event-triggered sound effects for the three Pocket Game applications.
#pragma once
#include "runtime.hpp"

namespace pocket_game_sfx {
enum class Cue : uint8_t { Move, Merge, Undo, Blocked, Select, Capture, Check, Goal, Reset, Push };
namespace detail {
inline float frequency(uint8_t midi) {
    constexpr float octave[] = {261.6256f,277.1826f,293.6648f,311.1270f,
        329.6276f,349.2282f,369.9944f,391.9954f,415.3047f,440.0f,466.1638f,493.8833f};
    float f = octave[midi % 12];
    int shift = int(midi / 12) - 5;
    while (shift < 0) { f *= .5f; ++shift; }
    while (shift > 0) { f *= 2; --shift; }
    return f;
}
inline void note(uint8_t midi, float seconds, float delay = 0) {
    pocket::tone(frequency(midi), seconds,
        delay > 0 ? float(pocket::millis()) / 1000.0f + delay : 0);
}
}
inline void cue(Cue event) {
    using detail::note;
    switch (event) {
    case Cue::Move: note(67,.035f); break;
    case Cue::Merge: note(76,.06f); note(84,.09f,.06f); break;
    case Cue::Undo: note(72,.055f); note(67,.075f,.06f); break;
    case Cue::Blocked: note(48,.045f); break;
    case Cue::Select: note(79,.035f); break;
    case Cue::Capture: note(64,.055f); note(76,.09f,.06f); break;
    case Cue::Check: note(74,.07f); note(81,.11f,.08f); break;
    case Cue::Goal: note(72,.09f); note(76,.09f,.1f); note(79,.18f,.2f); break;
    case Cue::Reset: note(67,.07f); note(72,.1f,.08f); break;
    case Cue::Push: note(55,.05f); note(62,.07f,.05f); break;
    }
}
} // namespace pocket_game_sfx
