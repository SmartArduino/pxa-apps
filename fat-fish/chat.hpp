// SPDX-License-Identifier: MIT
#pragma once
#include "pxa.h"
#include <stdint.h>

namespace fish::chat {
void init();
void open();
void close();
bool is_open();
void toggle_voice();
void stop_voice();
bool voice_mode();
unsigned voice_stage(); // 0 off, 1 listening, 2 thinking, 3 speaking
const char *voice_status();
const char *spoken_reply();
bool event(const pxa_event_t &event, bool foreground, uint64_t now_us);
void update(bool foreground, uint64_t now_us);
// Each successful reply/animation is delivered once, including after leaving chat.
const char *take_reply();
int take_animation();
} // namespace fish::chat
