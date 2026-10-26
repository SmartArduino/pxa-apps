/* SPDX-License-Identifier: MIT. Native-only oracle ABI, absent from Guest build. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "backend.h"
extern "C" {
void qa_boot(uint32_t seed){arduboy_pxa_boot(seed);}
void qa_new(void){arduboy_pxa_new();}
void qa_advance(uint32_t ms){arduboy_pxa_advance(ms);}
void qa_pointer(uint8_t mask,int preserve){arduboy_pxa_pointer(mask,preserve);}
void qa_controller(uint8_t mask){arduboy_pxa_controller(mask);}
void qa_release(void){arduboy_pxa_release();}
size_t qa_pack(uint8_t*out,size_t capacity){return arduboy_pxa_pack(out,capacity);}
size_t qa_game(uint8_t*out,size_t capacity){return arduboy_game_pack(out,capacity);}
void qa_pixels(uint8_t*out){memcpy(out,arduboy_pxa_state.display,1024);}
void qa_eeprom(uint8_t*out){memcpy(out,arduboy_pxa_state.eeprom,1024);}
}
