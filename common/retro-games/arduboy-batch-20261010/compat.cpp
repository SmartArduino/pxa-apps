/* SPDX-License-Identifier: MIT. Original library hardware boundary and cooperative clock. */
#include "backend.h"
#include "Arduboy2.h"
#include <string.h>
#include <limits.h>
extern "C" { arduboy_pxa_state_t arduboy_pxa_state; }
EEPROMClass EEPROM;
Arduboy2Audio Arduboy2Base::audio;
static bool eeprom_initialized;
__attribute__((constructor(101))) static void record_constructor(){++arduboy_pxa_state.constructors;}
#ifdef __wasm__
extern "C" void __wasm_call_ctors(void);
#endif
class Arduboy2Ex {
public:
 static void reset(){
  memset(Arduboy2Base::sBuffer,0,1024);Arduboy2Base::frameCount=0;
  Arduboy2Base::currentButtonState=Arduboy2Base::previousButtonState=0;
  Arduboy2Base::eachFrameMillis=16;Arduboy2Base::thisFrameStart=(uint8_t)arduboy_pxa_state.clock_ms;
  Arduboy2Base::lastFrameDurationMs=0;Arduboy2Base::justRendered=false;
  Arduboy2::cursor_x=Arduboy2::cursor_y=0;Arduboy2::textColor=1;Arduboy2::textBackground=0;
  Arduboy2::textSize=1;Arduboy2::textWrap=Arduboy2::textRaw=false;
 }
 static const uint8_t *font(){return Arduboy2::font5x7;}
 static size_t pack(uint8_t*out){uint8_t*p=out;
  auto u8=[&](uint8_t v){*p++=v;};auto u16=[&](uint16_t v){u8((uint8_t)v);u8((uint8_t)(v>>8));};
  u16(Arduboy2Base::frameCount);u8(Arduboy2Base::eachFrameMillis);u8(Arduboy2Base::thisFrameStart);u8(Arduboy2Base::lastFrameDurationMs);u8(Arduboy2Base::justRendered);
  u8(Arduboy2Base::currentButtonState);u8(Arduboy2Base::previousButtonState);
  u16((uint16_t)Arduboy2::cursor_x);u16((uint16_t)Arduboy2::cursor_y);u8(Arduboy2::textColor);u8(Arduboy2::textBackground);u8(Arduboy2::textSize);u8(Arduboy2::textWrap);u8(Arduboy2::textRaw);
  u8(Arduboy2Audio::enabled());u8(BeepPin1::duration);u8(BeepPin2::duration);return (size_t)(p-out);
 }
 static void release(){Arduboy2Base::currentButtonState=Arduboy2Base::previousButtonState=0;}
};
extern "C" const uint8_t *arduboy_pxa_font(void){return Arduboy2Ex::font();}
extern "C" uint32_t arduboy_pxa_hash(const uint8_t*p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
extern "C" uint32_t arduboy_pxa_millis(void){return arduboy_pxa_state.clock_ms;}
unsigned long millis(){return arduboy_pxa_millis();}
unsigned long micros(){return (uint32_t)(arduboy_pxa_millis()*1000u);}
void delay(unsigned long ms){arduboy_pxa_wait((uint32_t)ms);}
extern "C" void arduboy_pxa_wait(uint32_t ms){if(ms>INT32_MAX){arduboy_pxa_state.fault=3;return;}arduboy_pxa_state.next_due_ms=arduboy_pxa_state.clock_ms+(ms?ms:1);}
void Arduboy2Core::delayShort(uint16_t ms){arduboy_pxa_wait(ms);}
void Arduboy2Core::boot(){}
void Arduboy2Core::idle(){}
void Arduboy2Core::sendLCDCommand(uint8_t){}
void Arduboy2Core::digitalWriteRGB(uint8_t,uint8_t,uint8_t){}
void Arduboy2Core::digitalWriteRGB(uint8_t,uint8_t){}
unsigned long Arduboy2Core::generateRandomSeed(){return arduboy_pxa_state.seed^arduboy_pxa_state.clock_ms;}
uint8_t Arduboy2Core::buttonsState(){arduboy_pxa_state.sampled=1;arduboy_pxa_state.applied=arduboy_pxa_state.pointer|arduboy_pxa_state.controller|arduboy_pxa_state.pending_pointer|arduboy_pxa_state.pending_controller;return arduboy_pxa_state.applied;}
void Arduboy2Core::paintScreen(const uint8_t*image){memcpy(arduboy_pxa_state.display,image,1024);++arduboy_pxa_state.display_count;arduboy_pxa_state.display_step=arduboy_pxa_state.steps;}
void Arduboy2Core::paintScreen(uint8_t*image,bool clear){paintScreen((const uint8_t*)image);if(clear)memset(image,0,1024);}
/* Park-Miller software sequence, AVR-width signed conversion and Arduino range semantics.
   Algorithm reference: official avr-libc libc/stdlib/random.c; no host libc RNG. */
long random(void){int32_t x;memcpy(&x,&arduboy_pxa_state.rng,4);if(!x)x=123459876;int64_t value=16807LL*(x%127773)-2836LL*(x/127773);if(value<0)value+=2147483647LL;arduboy_pxa_state.rng=(uint32_t)value;return (long)(arduboy_pxa_state.rng&0x7fffffffu);}
void randomSeed(unsigned long seed){if((uint32_t)seed)arduboy_pxa_state.rng=(uint32_t)seed;}
long random(long maximum){int32_t n=(int32_t)maximum;return n?(long)((int32_t)random()%n):0;}
long random(long minimum,long maximum){int32_t a=(int32_t)minimum,b=(int32_t)maximum;if(a>=b)return a;return random((long)((uint32_t)b-(uint32_t)a))+a;}
extern "C" void arduboy_pxa_release(void){arduboy_pxa_state.pointer=arduboy_pxa_state.controller=arduboy_pxa_state.pending_pointer=arduboy_pxa_state.pending_controller=arduboy_pxa_state.applied=arduboy_pxa_state.sampled=0;Arduboy2Ex::release();}
extern "C" void arduboy_pxa_pointer(uint8_t mask,int preserve){mask&=252;uint8_t prev=arduboy_pxa_state.pointer;arduboy_pxa_state.pending_pointer|=mask&~prev;if(mask||!preserve)arduboy_pxa_state.pending_pointer&=mask;arduboy_pxa_state.pointer=mask;}
extern "C" void arduboy_pxa_controller(uint8_t mask){mask&=252;arduboy_pxa_state.pending_controller|=mask&~arduboy_pxa_state.controller;arduboy_pxa_state.controller=mask;}
extern "C" void arduboy_pxa_new(void){arduboy_pxa_release();arduboy_pxa_state.fault=0;arduboy_pxa_state.next_due_ms=arduboy_pxa_state.clock_ms+1;Arduboy2Ex::reset();BeepPin1::noTone();BeepPin2::noTone();arduboy_game_boot();}
extern "C" void arduboy_pxa_boot(uint32_t clock_seed){
#ifdef __wasm__
 if(!arduboy_pxa_state.constructors)__wasm_call_ctors();
#endif
 if(arduboy_pxa_state.constructors!=1){arduboy_pxa_state.fault=4;return;}
 arduboy_pxa_state.seed=clock_seed;arduboy_pxa_state.rng=1;
 if(!eeprom_initialized){memset(arduboy_pxa_state.eeprom,255,1024);eeprom_initialized=true;}
 arduboy_pxa_new();
}
extern "C" void arduboy_pxa_advance(uint32_t elapsed){
 if(elapsed>64){uint32_t skipped=elapsed-64;arduboy_pxa_state.clock_ms+=skipped;arduboy_pxa_state.dropped_steps+=skipped;elapsed=64;}
 while(elapsed--&&!arduboy_pxa_state.fault){++arduboy_pxa_state.clock_ms;if((int32_t)(arduboy_pxa_state.clock_ms-arduboy_pxa_state.next_due_ms)<0)continue;
  arduboy_pxa_state.next_due_ms=arduboy_pxa_state.clock_ms+1;arduboy_pxa_state.sampled=0;++arduboy_pxa_state.steps;arduboy_game_step();
  if(arduboy_pxa_state.sampled)arduboy_pxa_state.pending_pointer=arduboy_pxa_state.pending_controller=0;
 }
}
extern "C" size_t arduboy_pxa_pack(uint8_t*out,size_t capacity){const size_t bytes=3136;if(!out)return bytes;if(capacity<bytes){arduboy_pxa_state.fault=5;return 0;}uint8_t*p=out;auto u8=[&](uint8_t v){*p++=v;};auto u16=[&](uint16_t v){u8((uint8_t)v);u8((uint8_t)(v>>8));};auto u32=[&](uint32_t v){u16((uint16_t)v);u16((uint16_t)(v>>16));};
 u32(arduboy_pxa_state.clock_ms);u32(arduboy_pxa_state.next_due_ms);u32(arduboy_pxa_state.steps);u32(arduboy_pxa_state.seed);u32(arduboy_pxa_state.rng);u32(arduboy_pxa_state.display_count);u32(arduboy_pxa_state.display_step);u32(arduboy_pxa_state.dropped_steps);
 u8(arduboy_pxa_state.pointer);u8(arduboy_pxa_state.controller);u8(arduboy_pxa_state.pending_pointer);u8(arduboy_pxa_state.pending_controller);u8(arduboy_pxa_state.applied);u8(arduboy_pxa_state.sampled);u8(arduboy_pxa_state.fault);u8(arduboy_pxa_state.constructors);u16(arduboy_pxa_state.tone1);u16(arduboy_pxa_state.tone2);
 p+=Arduboy2Ex::pack(p);memcpy(p,Arduboy2Base::getBuffer(),1024);p+=1024;memcpy(p,arduboy_pxa_state.display,1024);p+=1024;memcpy(p,arduboy_pxa_state.eeprom,1024);p+=1024;return (size_t)(p-out);
}
