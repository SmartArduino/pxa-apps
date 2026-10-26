#include "backend.h"
#include "owner_compat.h"
#include <new>
#include <type_traits>
#include "vendor/platform/toyokumono.ino"
extern "C" void arduboy_game_boot(){static_assert(std::is_trivially_destructible<MyArduboy2>::value,"ordinary restart needs no destruction");new(&ab)MyArduboy2();ab.pxaResetButtons();obono_owner_reset();toy_reset_common();toy_reset_menu();toy_reset_title();toy_reset_game();toy_register_game();mode=MODE_LOGO;setup();}
extern "C" void arduboy_game_step(){obono_audio_advance(millis());loop();}
static size_t packet(uint8_t*out){ObonoWriter w(out);w.u8(1);w.u8(12);w.u8(mode);ab.pxaPack(w);obono_owner_pack(w);toy_pack_common(w);toy_pack_menu(w);toy_pack_title(w);toy_pack_game(w);obono_audio_pack(w);return w.size();}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t cap){size_t n=packet(nullptr);if(!out)return n;if(cap<n)return 0;return packet(out);}
extern "C" const char*arduboy_game_name(){return "toyokumono";}
