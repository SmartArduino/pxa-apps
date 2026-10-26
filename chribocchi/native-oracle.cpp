/* SPDX-License-Identifier: MIT. Private observation only, no state loader. */
#include "backend.h"
#include "owner_compat.h"
#include <string.h>
extern "C" void qa_boot(uint32_t seed){arduboy_pxa_boot(seed);}
extern "C" void qa_new(){arduboy_pxa_new();}
extern "C" void qa_release(){arduboy_pxa_release();}
extern "C" void qa_pointer(uint8_t mask,int preserve){arduboy_pxa_pointer(mask,preserve);}
extern "C" void qa_controller(uint8_t mask){arduboy_pxa_controller(mask);}
extern "C" void qa_advance(uint32_t elapsed){arduboy_pxa_advance(elapsed);}
extern "C" size_t qa_pack(uint8_t*out,size_t cap){return arduboy_pxa_pack(out,cap);}
extern "C" size_t qa_game(uint8_t*out,size_t cap){return arduboy_game_pack(out,cap);}
extern "C" void qa_pixels(uint8_t*out){memcpy(out,arduboy_pxa_state.display,1024);}
extern "C" void qa_eeprom(uint8_t*out){memcpy(out,arduboy_pxa_state.eeprom,1024);}
extern "C" size_t qa_owner_bytes(){ObonoWriter w(nullptr);obono_owner_pack(w);obono_audio_pack(w);return w.size();}
