// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "pxa_core.h"
#include "render.hpp"
namespace gfx {
bool attach(uint64_t handle, uint32_t capabilities);
void detach();
bool begin(uint64_t frame, int width, int height);
int32_t finish();
bool font_event(const pxa_event_t &);
void service_fonts(unsigned max_requests = 8);
bool fonts_pending();
void profile(uint64_t build_us, uint64_t submit_us);
void clear(uint16_t);
void rect(int, int, int, int, uint16_t);
void line(int, int, int, int, uint16_t);
} // namespace gfx
