// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "pxa.h"
// Opt-in asynchronous services. Disabled for all existing callback
// applications.
namespace game {
bool service_event(const pxa_event_t &event, bool foreground, uint64_t now_us);
void service_update(bool foreground, uint64_t now_us);
} // namespace game
