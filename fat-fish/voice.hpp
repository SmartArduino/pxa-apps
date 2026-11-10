// SPDX-License-Identifier: MIT
#pragma once
#include "pxa.h"
#include <stddef.h>

namespace fish::voice {
void press();
void listen();
void release(bool send);
void cancel();
bool recording();
bool playing();
bool busy();
bool take_failure();
bool event(const pxa_event_t &event, bool foreground, uint64_t now_us);
void update(bool foreground, uint64_t now_us);
const char *notice();
// The returned recording stays valid until press/cancel. take_recording is one-shot.
bool take_recording(const uint8_t *&bytes, size_t &size);
// Finish an independently decodable chunk while keeping the microphone open.
bool take_chunk(const uint8_t *&bytes, size_t &size);
void play(const uint8_t *mp3, size_t size);
} // namespace fish::voice
