/* SPDX-License-Identifier: MIT. Private cold native replay, never linked in Guest. */
#include "backend.h"
#include <cstring>
extern "C" void qa_boot(uint32_t seed){arduboy_pxa_boot(seed);}
extern "C" void qa_new(){arduboy_pxa_new();}
extern "C" void qa_release(){arduboy_pxa_release();}
extern "C" void qa_pointer(uint8_t m,int p){arduboy_pxa_pointer(m,p);}
extern "C" void qa_controller(uint8_t m){arduboy_pxa_controller(m);}
extern "C" void qa_advance(uint32_t ms){arduboy_pxa_advance(ms);}
extern "C" size_t qa_pack(uint8_t*out,size_t n){return arduboy_pxa_pack(out,n);}
extern "C" size_t qa_game(uint8_t*out,size_t n){return arduboy_game_pack(out,n);}
extern "C" void qa_pixels(uint8_t*out){std::memcpy(out,arduboy_pxa_state.display,1024);}
extern "C" void qa_eeprom(uint8_t*out){std::memcpy(out,arduboy_pxa_state.eeprom,1024);}
