// SPDX-License-Identifier: MIT
#pragma once
namespace sound {
enum Cue { Click, Move, City, Paper, Battle, Arrow };
void play(Cue);
void toggle();
bool enabled();
void toggle_music();
bool music_enabled();
} // namespace sound
