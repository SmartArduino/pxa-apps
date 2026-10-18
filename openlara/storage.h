#pragma once
#include "pxa.h"
// Two independent, checksummed private files. Cache changes become visible only
// after the Host has acknowledged atomic replacement of the previous file.
void store_start();
void store_tick();
bool store_event(const pxa_event_t &event);
bool store_ready();
bool store_busy();
bool store_read(unsigned kind, void *dest, unsigned capacity, unsigned *size);
const uint8_t *store_view(unsigned kind,unsigned *size);
bool store_write(unsigned kind, const void *data, unsigned size);
const char *store_message();
void store_stop();
