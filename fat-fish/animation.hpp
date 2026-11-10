// SPDX-License-Identifier: MIT
#pragma once
#include "animation_data.hpp"
#include "pxa.h"

namespace fish::animation {
void play(art::Clip clip);
void update(bool foreground);
bool event(const pxa_event_t &event);
void draw(int x, int y, int width, int height);
bool failed();
} // namespace fish::animation
