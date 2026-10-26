#pragma once
/* Keep the original simulation order while allowing the PXA event port to
 * service input and the watchdog during long rule/map initialization. */
#ifdef ESP_PLATFORM_CNC
#include "port.h"
static inline void Cnc_Cooperate(void) {
    if (port_cooperate_due(port_time_us(), PORT_COOPERATE_BUDGET_US))
        port_sleep(0);
}
#else
static inline void Cnc_Cooperate(void) {}
#endif
