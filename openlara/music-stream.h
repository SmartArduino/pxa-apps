#pragma once
#include "pxa.h"

// A single AD4 bank stays on media; only its directory and a bounded FIFO
// occupy guest memory. Reads and seeks return to Host between callbacks.
void music_stream_config(uint64_t media_root, uint32_t bank_bytes);
void music_stream_begin(uint32_t offset, uint32_t bytes);
void music_stream_stop();
void music_stream_tick();
bool music_stream_event(const pxa_event_t &event);
unsigned music_stream_read(uint8_t *destination, unsigned count);
bool music_stream_failed();
