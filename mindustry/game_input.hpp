// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../common/pocket-game/runtime.hpp"
namespace game {
// App-local multi-touch adapter; shared runtimes retain their single-pointer
// API.
void touch(pocket::Phase phase, uint8_t id, int x, int y);
void cancel_touches();
bool touch_active();
} // namespace game
