/* MIT: Arduboy 1.1 software API bridge; no hardware or blocking music player. */
#ifndef PXA_CLASSIC_ARDUMAN_ARDUBOY_H
#define PXA_CLASSIC_ARDUMAN_ARDUBOY_H
#include "Arduboy2.h"
#include "backend.h"
class PxaLegacyTunes {
public:
    void initChannel(byte) {}
    void playScore(const byte *) {} // Original music is retained, playback not provided.
    void tone(unsigned int frequency, unsigned long) {
        arduboy_pxa_state.tone1 = (uint16_t)frequency;
    }
};
class Arduboy : public Arduboy2 {
public:
    PxaLegacyTunes tunes;
    uint8_t getInput() { return buttonsState(); }
    uint8_t getPixel(int16_t x, int16_t y) {
        return x>=0 && x<WIDTH && y>=0 && y<HEIGHT ? Arduboy2::getPixel((uint8_t)x,(uint8_t)y) : 0;
    }
};
#endif
