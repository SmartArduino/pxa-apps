#pragma once
#include "port.h"
static inline void delay(unsigned ms){port_sleep(ms);}
