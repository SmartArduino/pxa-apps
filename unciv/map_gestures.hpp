// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace game {
bool pinch_area(int x, int y);
bool pinch_begin(int x, int y);
void pinch_update(int x, int y, uint32_t scale);
void pinch_end();
} // namespace game
