/* SPDX-License-Identifier: MIT.
 * Hardware hooks for upstream's existing no-_SDL_SOUND profile.
 * The original sound.c fallback remains the implementation of music/SFX requests.
 */
#include <stdint.h>
#include <stdbool.h>
int16_t sound_device=0;
extern void inittimer(void);
/* The desktop sound backend normally initializes this shared timer. */
void initsound(void){inittimer();}
void setsoundt2(void){}
void soundint(void){}
void soundinitglob(uint16_t b,uint16_t r){(void)b;(void)r;}
void s1setupsound(void){}
void s1killsound(void){}
void s1soundoff(void){}
void s1setspkrt2(void){}
void s1settimer0(uint16_t v){(void)v;}
void s1timer0(uint16_t v){(void)v;}
void s1timer2(uint16_t v,bool m){(void)v;(void)m;}
void s1settimer2(uint16_t v,bool m){(void)v;(void)m;}
void s0soundinitglob(void){}

