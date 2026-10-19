#pragma once
#include "pxa.h"
#include <stdint.h>
void catalog_start(uint64_t root,uint32_t installed);
bool catalog_busy();
bool catalog_event(const pxa_event_t &event);
uint32_t catalog_mask();
const char *catalog_level_path(unsigned id);
