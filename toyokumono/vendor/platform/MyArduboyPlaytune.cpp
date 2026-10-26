/**
 * MyArduboyPlaytune
 *
 * Derived from ArduboyPlaytune
 * https://github.com/Arduboy/ArduboyPlaytune
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 OBONO
 * Based on work (c) Copyright 2016, Chris J. Martinez, Kevin Bates, Josh Goebel, Scott Allen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "MyArduboy2.h"
#define AVAILABLE_CHANNELS 2
#define TUNE_OP_PLAYNOTE 0x90
#define TUNE_OP_STOPNOTE 0x80
#define TUNE_OP_MARK 0xD0
#define TUNE_OP_REPEAT 0xE0
#define TUNE_OP_STOP 0xF0
#define FREQUENCY_MAX (0xFFFF/DUTY_CYCLE_MAX)
#define FREQUENCY_IDLE 500
#define DUTY_CYCLE_MIN 2
#define DUTY_CYCLE_MAX 5
#define NOTE_MAX 127
#define NOTE_MIDDLE sizeof(midiByteNoteFrequencies)
#define F_CPU 16000000UL
#define getTimerNum(chan) ((chan==0)?3:1)
#define setLowTimerPin(t) ((void)0)
#define toggleTimerPin(t) ((void)0)
static void stepScore();static void playNote(uint8_t,uint8_t,uint8_t);static void stopNote(uint8_t);static void setupTimer(uint8_t,uint16_t,uint8_t);
struct OwnerTimer{uint32_t period,remaining;bool active;};static OwnerTimer timers[2];
static void owner_stop_timer(uint8_t t){timers[t==3?0:1].active=false;}
#define stopTimer(t) owner_stop_timer(t)
struct OwnerSong{const byte*data;size_t size;};static OwnerSong songs[32];static uint8_t audioFault=0;static uint32_t audioClock=0,audioInterrupts=0,audioStarts=0,audioStops=0;
/*  Local Variables  */

static uint8_t numChans = 0;
static uint8_t audioPriority;

static volatile byte *timer1PinPort;
static volatile byte timer1PinMask;
static volatile byte *timer3PinPort;
static volatile byte timer3PinMask;

static volatile uint8_t timer1DutyCycle;
static volatile uint32_t timer1ToggleCount;
static volatile uint8_t timer3DutyCycle;
static volatile uint32_t timer3ToggleCount;
static volatile uint16_t timer3Frequency;

static volatile bool isAllMuted = false;         // indicates all sound is muted
static volatile bool isMuteScore = false;        // indicates tone playing so mute other channels
static volatile bool isScorePlaying = false;     // is the score still playing?
static volatile bool isWaitTimerPlaying = false; // is it currently playing a note?
static volatile bool isTonePlaying = false;      // is the tone still playing?

// pointer to a function that indicates if sound is enabled
static boolean (*outputEnabled)();

// pointers to your musical score and your position in said score
static volatile const byte *scoreStart = 0;
static volatile const byte *scoreCursor = 0;

// score tuning parameters
static volatile int8_t scorePitchDefault, scorePitch;
static volatile uint8_t scoreRepeat;

// Table of midi note frequencies
// The lowest notes might not work, depending on the Arduino clock frequency
// Ref: http://www.phy.mtu.edu/~suits/notefreqs.html
PROGMEM static const uint8_t midiByteNoteFrequencies[] = {
    8,8,9,9,10,11,11,12,13,14,14,15,16,17,18,19,20,22,23,24,26,27,29,31,32,34,
    36,39,41,43,46,49,52,55,58,61,65,69,73,78,82,87,92,98,104,110,116,123,131,
    138,147,155,165,174,185,196,207,220,233,247
};
PROGMEM static const uint16_t midiWordNoteFrequencies[(NOTE_MAX + 1) - NOTE_MIDDLE] = {
    261,277,293,311,329,349,370,392,415,440,466,494,523,554,587,622,659,698,
    740,784,830,880,932,988,1046,1108,1174,1244,1318,1397,1480,1568,1661,
    1760,1864,1975,2093,2217,2349,2489,2637,2794,2960,3136,3322,3520,3729,
    3951,4186,4435,4698,4978,5274,5587,5920,6272,6645,7040,7458,7902,8372,
    8870,9397,9956,10548,11175,11840,12544
};
PROGMEM static const int8_t tuneNoteTable[] = {
    0, 1, 2, 3, 4, 6, 8, 12, 0, -12, -8, -6, -4, -3, -2, -1
};

/*---------------------------------------------------------------------------*/
/*                            Initialize & Finalize                          */
/*---------------------------------------------------------------------------*/

void MyArduboy2::initAudio(uint8_t chans){outputEnabled=audio.enabled;if(chans==0||chans>AVAILABLE_CHANNELS)return;if(numChans>0)closeAudio();numChans=chans;stopNote(0);}
void MyArduboy2::closeAudio(){for(uint8_t chan=0;chan<numChans;chan++)owner_stop_timer(getTimerNum(chan));numChans=0;isScorePlaying=isTonePlaying=isMuteScore=false;}

/*---------------------------------------------------------------------------*/
/*                             Control parameters                            */
/*---------------------------------------------------------------------------*/

bool MyArduboy2::isAudioEnabled(void)
{
    return audio.enabled();
}

void MyArduboy2::setAudioEnabled(bool on)
{
    (on) ? audio.on() : audio.off();
}

void MyArduboy2::toggleAudioEnabled(void)
{
    audio.toggle();
}

void MyArduboy2::saveAudioOnOff(void)
{
    audio.saveOnOff();
}

/*---------------------------------------------------------------------------*/
/*                                 Play sounds                               */
/*---------------------------------------------------------------------------*/

void MyArduboy2::playTone(uint16_t frequency, uint16_t duration,
        uint8_t priority, uint8_t dutyCycle)
{
    if (numChans == 0) return;
    if (numChans == 1 && isScorePlaying) {
        if (priority > audioPriority) return;
        stopScore();
    }

    isAllMuted = !outputEnabled();
    frequency = min(frequency, FREQUENCY_MAX);
    dutyCycle = constrain(dutyCycle, DUTY_CYCLE_MIN, DUTY_CYCLE_MAX);
    uint32_t toggle_count = ((uint32_t)duration * frequency * dutyCycle + 500) / 1000;
    if (toggle_count == 0) toggle_count = 1;

    uint8_t chan = numChans - 1;
    if (chan == 0) {
        isWaitTimerPlaying = true;
        timer3ToggleCount = toggle_count;
    } else {
        isTonePlaying = true;
        timer1ToggleCount = toggle_count;
        if (isScorePlaying) {
            isMuteScore = (priority <= audioPriority);
        } else {
            audioPriority = priority;
        }
    }
    setupTimer(getTimerNum(chan), frequency, dutyCycle);
}

void MyArduboy2::playScore(const byte *score, uint8_t priority, int8_t pitch)
{
    if (isScorePlaying) {
        if (priority > audioPriority) return;
        stopScore();
    } else if (isTonePlaying) {
        isMuteScore = (priority > audioPriority);
    }
    audioPriority = priority;
    ++audioStarts;
    scoreStart = score;
    scoreCursor = scoreStart;
    scorePitchDefault = scorePitch = pitch;
    scoreRepeat = 0;
    isScorePlaying = true;
    stepScore(); // execute initial commands
}

void MyArduboy2::stopScore(void)
{
    ++audioStops;
    for (uint8_t chan = 0; chan < numChans; chan++) {
        stopNote(chan);
    }
    isScorePlaying = false;
}

/*---------------------------------------------------------------------------*/

/*
Do score commands until a "wait" is found, or the score is stopped.
This is called initially from playScore(), but then is called
from the interrupt routine when waits expire.

If CMD < 0x80, then the other 7 bits and the next byte are a
15-bit big-endian number of msec to wait
*/
static uint8_t owner_score_read(){uintptr_t p=(uintptr_t)scoreCursor;for(uint8_t i=0;i<32;i++)if(songs[i].data&&p>=(uintptr_t)songs[i].data&&p<(uintptr_t)songs[i].data+songs[i].size)return *scoreCursor++;audioFault=1;isScorePlaying=false;return 0xf0;}
static void stepScore(void)
{
    while (true) {
        byte command = owner_score_read();
        uint8_t opcode = command & 0xF0;
        uint8_t opvalue = command & 0x0F;
        uint8_t chan = opvalue & 0x03;
        if (opcode == TUNE_OP_STOPNOTE) { // stop note
            stopNote(chan);
        } else if (opcode == TUNE_OP_PLAYNOTE) { // play note
            uint8_t dutyCycle = ((opvalue & 0x0C) >> 2) + 2;
            isAllMuted = !outputEnabled();
            playNote(chan, owner_score_read() + scorePitch, dutyCycle);
        } else if (opcode < 0x80) { // wait count in msec.
            uint16_t duration = (uint16_t)command << 8 | owner_score_read();
            timer3ToggleCount = ((uint32_t)duration * timer3Frequency + 500) / 1000;
            if (timer3ToggleCount == 0) timer3ToggleCount = 1;
            break;
        } else if (opcode == TUNE_OP_MARK) { // set mark to repeat
            scoreStart = scoreCursor - 1;
            if (scoreRepeat > 0) scorePitch += (int8_t)pgm_read_byte(tuneNoteTable + opvalue);
        } else if (opcode == TUNE_OP_REPEAT) { // repeat score
            if (opvalue == 0 || ++scoreRepeat < (1 << opvalue)) {
                scoreCursor = scoreStart;
            } else {
                scoreStart = scoreCursor;
                scorePitch = scorePitchDefault;
                scoreRepeat = 0;
            }
        } else if (opcode == TUNE_OP_STOP) { // stop score
            isScorePlaying = false;
            break;
        }
    }
}

static void playNote(uint8_t chan, uint8_t note, uint8_t dutyCycle)
{
    if (chan >= numChans) return;
    if (chan == 1 && isTonePlaying) return;
    if (note > NOTE_MAX) return;

    uint16_t frequency = (note < NOTE_MIDDLE) ?
            pgm_read_byte(midiByteNoteFrequencies + note) :
            pgm_read_word(midiWordNoteFrequencies + note - NOTE_MIDDLE);
    if (chan == 0) isWaitTimerPlaying = true;
    setupTimer(getTimerNum(chan), frequency, dutyCycle);
}

static void stopNote(uint8_t chan)
{
    switch (getTimerNum(chan)) {
    case 1:
        if (!isTonePlaying) stopTimer(1);
        break;
    case 3:
        isWaitTimerPlaying = false;
        if (!isMuteScore) setLowTimerPin(3);
        setupTimer(3, FREQUENCY_IDLE, DUTY_CYCLE_MIN);
        break;
    }
}

static void setupTimer(uint8_t timerNum,uint16_t frequency,uint8_t dutyCycle){
 frequency=(uint16_t)(frequency*dutyCycle);if(!frequency){audioFault=2;return;}uint32_t ocr=F_CPU/frequency-1;uint32_t prescale=1;if(ocr>65535){ocr=F_CPU/64/frequency-1;prescale=64;}
 OwnerTimer&t=timers[timerNum==3?0:1];t.period=(ocr+1)*prescale;t.remaining=t.period;t.active=true;
 if(timerNum==1)timer1DutyCycle=dutyCycle;else{timer3DutyCycle=dutyCycle;timer3Frequency=frequency;}
}

/*---------------------------------------------------------------------------*/
/*                        Interrupt service routines                         */
/*---------------------------------------------------------------------------*/

static void owner_timer1_interrupt() // TIMER 1
{
    if (isTonePlaying) {
        if (timer1ToggleCount > 0) {
            if (!isAllMuted && timer1ToggleCount % timer1DutyCycle < 2) toggleTimerPin(1);
            if (timer1ToggleCount > 0) timer1ToggleCount--;
        } else {
            isTonePlaying = isMuteScore = false;
            stopTimer(1);
        }
    } else {
        if (!isAllMuted && timer1ToggleCount % timer1DutyCycle < 2) toggleTimerPin(1);
    }
}

static void owner_timer3_interrupt() // TIMER 3
{
    // Timer 3 is the one assigned first, so we keep it running always
    // and use it to time score waits, whether or not it is playing a note.
    if (isWaitTimerPlaying && !isMuteScore && !isAllMuted) {
        if (timer3ToggleCount % timer3DutyCycle < 2) toggleTimerPin(3);
    }
    if (timer3ToggleCount > 0 && --timer3ToggleCount == 0) {
        (isScorePlaying) ? stepScore() : stopNote(0);
    }
}

void obono_audio_register(uint8_t id,const uint8_t*p,size_t n){if(id>=32){audioFault=3;return;}songs[id]={p,n};}
void obono_audio_advance(uint32_t now){while(audioClock!=now){++audioClock;uint32_t cycles=16000;for(unsigned guard=0;cycles&&guard<1024;guard++){uint32_t dt=cycles;for(auto&t:timers)if(t.active&&t.remaining<dt)dt=t.remaining;if(!dt){audioFault=4;break;}for(auto&t:timers)if(t.active)t.remaining-=dt;cycles-=dt;for(uint8_t i=0;i<2;i++)if(timers[i].active&&!timers[i].remaining){timers[i].remaining=timers[i].period;++audioInterrupts;if(i==0)owner_timer3_interrupt();else owner_timer1_interrupt();}}}}
void obono_audio_reset(){numChans=audioPriority=timer1DutyCycle=timer3DutyCycle=0;timer1ToggleCount=timer3ToggleCount=timer3Frequency=0;isAllMuted=isMuteScore=isScorePlaying=isWaitTimerPlaying=isTonePlaying=false;outputEnabled=nullptr;scoreStart=scoreCursor=nullptr;scorePitchDefault=scorePitch=0;scoreRepeat=0;memset(timers,0,sizeof(timers));memset(songs,0,sizeof(songs));audioFault=0;audioClock=millis();audioInterrupts=audioStarts=audioStops=0;}
static uint8_t owner_song_id(const byte*p){if(!p)return 255;for(uint8_t i=0;i<32;i++)if(songs[i].data==p)return i;uintptr_t a=(uintptr_t)p;for(uint8_t i=0;i<32;i++)if(songs[i].data&&a>(uintptr_t)songs[i].data&&a<=(uintptr_t)songs[i].data+songs[i].size)return i;return 254;}
// Cursor and MARK pointers belong to the same original playScore array.
// Prefer that array even when its one-past pointer equals the next score start.
static uint8_t owner_cursor_song_id(const byte*p,uint8_t id){if(!p)return 255;if(id<32){uintptr_t a=(uintptr_t)p,b=(uintptr_t)songs[id].data;if(a>=b&&a<=b+songs[id].size)return id;}return 254;}
static uint16_t owner_song_offset(const byte*p,uint8_t id){return id<32?(uint16_t)((uintptr_t)p-(uintptr_t)songs[id].data):0;}
void obono_audio_pack(ObonoWriter&w){w.u8(numChans);w.u8(audioPriority);w.u8(timer1DutyCycle);w.u8(timer3DutyCycle);w.u32(timer1ToggleCount);w.u32(timer3ToggleCount);w.u16(timer3Frequency);w.u8(isAllMuted);w.u8(isMuteScore);w.u8(isScorePlaying);w.u8(isWaitTimerPlaying);w.u8(isTonePlaying);w.u8(outputEnabled?1:0);uint8_t a=owner_song_id((const byte*)scoreStart),b=owner_cursor_song_id((const byte*)scoreCursor,a);w.u8(a);w.u16(owner_song_offset((const byte*)scoreStart,a));w.u8(b);w.u16(owner_song_offset((const byte*)scoreCursor,b));w.u8(scorePitchDefault);w.u8(scorePitch);w.u8(scoreRepeat);for(auto&t:timers){w.u32(t.period);w.u32(t.remaining);w.u8(t.active);}w.u8(audioFault);w.u32(audioClock);w.u32(audioInterrupts);w.u32(audioStarts);w.u32(audioStops);}
