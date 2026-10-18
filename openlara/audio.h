#pragma once
#include "pxa.h"
void audio_start();
void audio_tick(uint64_t now);
bool audio_event(const pxa_event_t &event);
void audio_flush();
void audio_stop();
