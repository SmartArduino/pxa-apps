/* SPDX-License-Identifier: MIT. Portable software API; no AVR devices/registers. */
#ifndef ARDUBOY_PXA_ARDUINO_H
#define ARDUBOY_PXA_ARDUINO_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define PROGMEM
#define _BV(b) (1u << (b))
#define bit(b) _BV(b)
#define bitRead(v,b) (((v)>>(b))&1u)
#define bitSet(v,b) ((v)|=_BV(b))
#define bitClear(v,b) ((v)&=~_BV(b))
#define bitWrite(v,b,x) ((x)?bitSet(v,b):bitClear(v,b))
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define pgm_read_ptr(p) (*(const void * const *)(p))
#define HIGH 1
#define LOW 0
#define F_CPU 16000000UL
#define TXLED1 do{}while(0)
#define TXLED0 do{}while(0)
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
using byte=uint8_t;using boolean=bool;using word=uint16_t;
template<class T,class U> constexpr auto min(T a,U b)->decltype(a+b){return a<b?a:b;}
template<class T,class U> constexpr auto max(T a,U b)->decltype(a+b){return a>b?a:b;}
inline uint8_t lowByte(uint16_t v){return (uint8_t)v;}
inline uint8_t highByte(uint16_t v){return (uint8_t)(v>>8);}
unsigned long millis();
unsigned long micros();
void delay(unsigned long milliseconds);
void randomSeed(unsigned long seed);
long random(long maximum);
long random(long minimum,long maximum);
long random(void);
#endif
