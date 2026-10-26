/* MIT. Independent comparison against the complete pinned original Tones.cpp.
 * Native fake Timer3 registers and exact 2MHz CTC clock; no AVR execution claim.
 */
#include <Arduino.h>
#include <stdio.h>
#include <assert.h>
static uint8_t TIMSK3,TCCR3A,TCCR3B,PORTC,DDRC;
static uint16_t OCR3A,TCNT3;
enum{OCIE3A=1,WGM32=3,CS30=0,CS31=1,PORTC6=6,PORTC7=7};
#define ISR(name) void name()
#define ArduboyTones OriginalTones
#include "vendor/original/ArduboyTones/src/ArduboyTones.cpp"
#undef ArduboyTones
#include "vendor/platform/ArduboyTones.cpp"
static uint32_t now;
extern "C" uint32_t arduboy_pxa_millis(){return now;}
static bool enabled=true;
static bool output(){return enabled;}
static unsigned comparisons;
static void equal(){
 const auto&s=randocity_tone_state;
 assert(s.last_ms==now&&s.counter==TCNT3&&s.compare==OCR3A);
 assert(s.playing==tonesPlaying&&s.silent==toneSilent&&s.high_volume==toneHighVol);
 assert(s.force_normal==forceNormVol&&s.force_high==forceHighVol&&s.in_progmem==inProgmem);
 assert(s.toggle_count==durationToggleCount&&s.index==(unsigned)(tonesIndex-toneSequence));
 assert(s.running==(TCCR3B!=0)&&s.interrupt_enabled==bool(bitRead(TIMSK3,OCIE3A)));
 for(unsigned i=0;i<7;++i)assert(s.sequence[i]==toneSequence[i]);++comparisons;
}
static void ms(unsigned n){while(n--){
 for(unsigned tick=0;tick<2000;++tick)if(TCCR3B){
  if(TCNT3==OCR3A){TCNT3=0;if(bitRead(TIMSK3,OCIE3A))TIMER3_COMPA_vect();}
  else ++TCNT3;
 }
 ++now;ArduboyTones::update();equal();
}}
int main(){
 OriginalTones original(output);ArduboyTones port(output);
 OriginalTones::tone(0,10);ArduboyTones::tone(0,10);equal();ms(20);
 OriginalTones::tone(65,40);ArduboyTones::tone(65,40);equal();ms(53);
 OriginalTones::tone(2800,20,2500,20,2300,20);ArduboyTones::tone(2800,20,2500,20,2300,20);equal();ms(100);
 enabled=false;OriginalTones::tone(880|TONE_HIGH_VOLUME,10,0,20);ArduboyTones::tone(880|TONE_HIGH_VOLUME,10,0,20);equal();ms(50);enabled=true;
 OriginalTones::volumeMode(VOLUME_ALWAYS_HIGH);ArduboyTones::volumeMode(VOLUME_ALWAYS_HIGH);
 OriginalTones::tone(100,0);ArduboyTones::tone(100,0);equal();ms(17);
 OriginalTones::tone(3000,20);ArduboyTones::tone(3000,20);equal();ms(80);
 OriginalTones::volumeMode(VOLUME_ALWAYS_NORMAL);ArduboyTones::volumeMode(VOLUME_ALWAYS_NORMAL);
 OriginalTones::tone(2000|TONE_HIGH_VOLUME,40);ArduboyTones::tone(2000|TONE_HIGH_VOLUME,40);equal();ms(7);
 OriginalTones::noTone();ArduboyTones::noTone();equal();ms(11);
 OriginalTones::tone(250,2);ArduboyTones::tone(250,2);equal();ms(20);
 printf("{\"passed\":true,\"comparisons\":%u,\"software_timer3_only\":true,\"full_original_source_included\":true,\"prescaler\":8,\"clock_hz\":2000000,\"avr_hardware_executed\":false}\n",comparisons);
}
