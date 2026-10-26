/* SPDX-License-Identifier: MIT. Bounded per-Guest session EEPROM. */
#ifndef ARDUBOY_PXA_EEPROM_H
#define ARDUBOY_PXA_EEPROM_H
#include "backend.h"
#include <string.h>
class EEPROMClass {
public:
 uint8_t read(int address)const {if(address<0||address>=ARDU_EEPROM_BYTES){arduboy_pxa_state.fault=2;return 255;}return arduboy_pxa_state.eeprom[address];}
 void update(int address,uint8_t value){if(address<0||address>=ARDU_EEPROM_BYTES){arduboy_pxa_state.fault=2;return;}arduboy_pxa_state.eeprom[address]=value;}
 void write(int address,uint8_t value){update(address,value);}
 int length()const{return ARDU_EEPROM_BYTES;}
 template<class T>T& get(int address,T&value)const {uint8_t *p=(uint8_t*)&value;for(size_t i=0;i<sizeof(T);i++)p[i]=read(address+(int)i);return value;}
 template<class T>const T& put(int address,const T&value){const uint8_t*p=(const uint8_t*)&value;for(size_t i=0;i<sizeof(T);i++)update(address+(int)i,p[i]);return value;}
};
extern EEPROMClass EEPROM;
#endif
