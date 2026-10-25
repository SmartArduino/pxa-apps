#pragma once
#include "port.h"
static inline int64_t esp_timer_get_time(void) {
  return (int64_t)port_time_us();
}
