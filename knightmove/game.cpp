#include "backend.h"
#include "owner_compat.h"
#include "vendor/platform/knightmove.ino"
extern "C" void arduboy_game_boot(){obono_owner_reset();arduboy.pxaEnsureConstructed();obono_reset_common();obono_reset_menu();obono_reset_title();obono_reset_game();obono_reset_logo();arduboy.pxaReset();obono_register_common();obono_register_game();setup();}
extern "C" void arduboy_game_step(){obono_audio_advance(millis());loop();}
static size_t packet(uint8_t*out){ObonoWriter w(out);w.u8(1);w.u8(14);w.u8(mode);arduboy.pxaPack(w);obono_owner_pack(w);obono_audio_pack(w);obono_pack_logo(w);obono_pack_common(w);obono_pack_menu(w);obono_pack_title(w);obono_pack_game(w);return w.size();}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t cap){size_t n=packet(nullptr);if(!out)return n;if(cap<n)return 0;return packet(out);}
extern "C" const char*arduboy_game_name(){return "knightmove";}
