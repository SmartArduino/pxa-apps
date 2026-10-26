/* SPDX-License-Identifier: MIT. Private ordinary-input native oracle.
 * Never linked into Guest; has no state loader, target-state setter or seed
 * injection API beyond one observed cold bootstrap.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "backend.h"
extern "C" void qa_boot(uint32_t observed_seed){arduboy_pxa_boot(observed_seed);}
extern "C" void qa_new(void){arduboy_pxa_new();}
extern "C" void qa_release(void){arduboy_pxa_release();}
extern "C" void qa_pointer(uint8_t mask,int preserve){arduboy_pxa_pointer(mask,preserve);}
extern "C" void qa_controller(uint8_t mask){arduboy_pxa_controller(mask);}
extern "C" void qa_advance(uint32_t elapsed){arduboy_pxa_advance(elapsed);}
extern "C" size_t qa_pack(uint8_t *out,size_t cap){return arduboy_pxa_pack(out,cap);}
extern "C" size_t qa_game(uint8_t *out,size_t cap){return arduboy_game_pack(out,cap);}
extern "C" void qa_pixels(uint8_t out[1024]){memcpy(out,arduboy_pxa_state.display,1024);}
extern "C" void qa_eeprom(uint8_t out[1024]){memcpy(out,arduboy_pxa_state.eeprom,1024);}
