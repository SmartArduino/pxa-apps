/* SPDX-License-Identifier: MIT. Owner-local OBONO boundary. */
#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "backend.h"
#include <stdio.h>
#define pgm_read_dword(p) (*(const uint32_t *)(p))
#define memcpy_P memcpy
#define strcpy_P strcpy
class ObonoWriter {
 uint8_t*out;size_t n;
public:
 explicit ObonoWriter(uint8_t*p):out(p),n(0){}
 void u8(uint8_t v){if(out)out[n]=v;++n;}
 void u16(uint16_t v){u8(v);u8(v>>8);}
 void u32(uint32_t v){u16(v);u16(v>>16);}
 size_t size()const{return n;}
};
int obono_rand();
uint8_t obono_audio_channel_count();
#define rand obono_rand
uint8_t eeprom_read_byte(const uint8_t*);
uint16_t eeprom_read_word(const uint16_t*);
uint32_t eeprom_read_dword(const uint32_t*);
void eeprom_read_block(void*,const void*,size_t);
void eeprom_write_byte(uint8_t*,uint8_t);
void eeprom_write_word(uint16_t*,uint16_t);
void eeprom_write_dword(uint32_t*,uint32_t);
void eeprom_write_block(const void*,void*,size_t);
inline void eeprom_busy_wait(){}
void obono_owner_reset();void obono_owner_pack(ObonoWriter&);
uint8_t eva_func_id(void(*)());uint8_t eva_title_func_id(void(*)());uint8_t eva_game_func_id(void(*)());
void eva_reset_common();void eva_reset_menu();void eva_reset_title();void eva_reset_game();
void eva_pack_common(ObonoWriter&);void eva_pack_menu(ObonoWriter&);void eva_pack_title(ObonoWriter&);void eva_pack_game(ObonoWriter&);
void obono_audio_advance(uint32_t);void obono_audio_reset();void obono_audio_pack(ObonoWriter&);
inline size_t strnlen_P(const char*s,size_t n){return strnlen(s,n);}
#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769236907684886f
#endif

#undef pgm_read_ptr
#define pgm_read_ptr(p) (*(p))

#define constrain(x,a,b) ((x)<(a)?(a):((x)>(b)?(b):(x)))
void obono_audio_register(uint8_t,const uint8_t*,size_t);
void eva_register_game();

#ifndef PI
#define PI 3.1415926535897932384626433832795f
#endif
