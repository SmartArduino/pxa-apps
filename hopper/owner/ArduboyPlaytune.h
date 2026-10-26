/* SPDX-License-Identifier: MIT. Silent Playtune1.0.4 software facade;
 * original MIT API/commands retained; AVR pitch/ISR quantization omitted. */
#pragma once
#include "owner_compat.h"
class ArduboyPlaytune{
public:
 explicit ArduboyPlaytune(boolean(*)());
 static void initChannel(byte);
 void playScore(const byte*);void stopScore();boolean playing();
};
