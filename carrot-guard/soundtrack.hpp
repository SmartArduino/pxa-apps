// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace carrot::music {
// Generated exclusively for Carrot Guard with the workflows in art/audio/.
enum class Cue {
  Tap,
  Begin,
  Build,
  Upgrade,
  Sell,
  Coin,
  Alert,
  Freeze,
  Win,
  Lose,
  Fire,
  Ice,
  Firework,
  Clear,
  Count
};
constexpr unsigned Count = unsigned(Cue::Count);
constexpr const char *Theme = "assets/patrol.ogg";
constexpr const char *Sounds[Count] = {
    "assets/sfx-tap.s16",      "assets/sfx-begin.s16",  "assets/sfx-build.s16",
    "assets/sfx-upgrade.s16",  "assets/sfx-sell.s16",   "assets/sfx-coin.s16",
    "assets/sfx-alert.s16",    "assets/sfx-freeze.s16", "assets/sfx-win.s16",
    "assets/sfx-lose.s16",     "assets/sfx-fire.s16",   "assets/sfx-ice.s16",
    "assets/sfx-firework.s16", "assets/sfx-clear.s16"};
inline uint8_t track(Cue cue) {
  return cue == Cue::Win || cue == Cue::Lose    ? 5
         : cue == Cue::Coin || cue >= Cue::Fire ? 4
                                                : 3;
}
} // namespace carrot::music
