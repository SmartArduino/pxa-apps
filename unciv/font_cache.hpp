// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_font.h"
namespace font_cache {
const pxa_font_glyph_t* get(uint32_t codepoint,uint16_t pixels);
bool event(const pxa_event_t&);
void update(bool foreground);
}
