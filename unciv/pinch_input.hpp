// SPDX-License-Identifier: MIT
#pragma once
#include "pxa.h"
namespace pinch_input {
bool event(const pxa_event_t &, bool foreground);
void reset();
} // namespace pinch_input
