// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_core.h"
namespace dewfield_transport {
void init();
bool event(const pxa_event_t&);
void update(uint32_t now_ms);
void shutdown();
void browse();
}
