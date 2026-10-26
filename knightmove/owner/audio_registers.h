/* SPDX-License-Identifier: MIT. Owner-local16MHz AVR timer register observation, silent GPIO. */
#pragma once
#include <Arduino.h>
struct AudioCounter{uint16_t value=0;uint32_t fraction=0;AudioCounter&operator=(uint16_t v){value=v;fraction=0;return *this;}};
static uint8_t TCCR1A,TCCR1B,TCCR3A,TCCR3B,TIMSK1,TIMSK3;static uint16_t OCR1A,OCR3A;static AudioCounter TCNT1,TCNT3;static byte silentPin,silentMode;
#define WGM12 3
#define WGM32 3
#define CS10 0
#define CS30 0
#define OCIE1A 1
#define OCIE3A 1
#define ISR(name) void name()
inline void power_timer1_enable(){}inline void power_timer3_enable(){}
inline byte digitalPinToPort(byte){return 0;}inline byte digitalPinToBitMask(byte){return 1;}
inline byte*portOutputRegister(byte){return &silentPin;}inline byte*portModeRegister(byte){return &silentMode;}
