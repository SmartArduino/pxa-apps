#include "backend.h"
#include "owner_compat.h"
#include <new>
#include <type_traits>
#include "vendor/platform/evasion.ino"
extern "C" void arduboy_game_boot(){new(&ab)MyArduboy2();ab.pxaResetButtons();obono_owner_reset();eva_reset_common();eva_reset_menu();eva_reset_title();eva_reset_game();eva_register_game();mode=MODE_LOGO;setup();}
extern "C" void arduboy_game_step(){obono_audio_advance(millis());loop();}
static size_t packet(uint8_t*out){ObonoWriter w(out);w.u8(1);w.u8(13);w.u8(mode);ab.pxaPack(w);obono_owner_pack(w);eva_pack_common(w);eva_pack_menu(w);eva_pack_title(w);eva_pack_game(w);obono_audio_pack(w);return w.size();}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t cap){size_t n=packet(nullptr);if(!out)return n;if(cap<n)return 0;return packet(out);}
extern "C" const char*arduboy_game_name(){return "evasion";}
