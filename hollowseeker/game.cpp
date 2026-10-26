#include "backend.h"
#include "owner_compat.h"
#include "vendor/platform/hollow.ino"
extern "C" void arduboy_game_boot(){arduboy.pxaEnsureConstructed();obono_owner_reset();obono_reset_game();obono_reset_title();obono_reset_logo();arduboy.pxaReset();obono_register_game();obono_register_title();mode=LOGO_MODE;setup();}
extern "C" void arduboy_game_step(){obono_audio_advance(millis());loop();}
static size_t packet(uint8_t*out){ObonoWriter w(out);w.u8(1);w.u8(7);w.u8(mode);arduboy.pxaPack(w);obono_owner_pack(w);obono_pack_logo(w);obono_pack_title(w);obono_pack_game(w);return w.size();}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t capacity){size_t n=packet(nullptr);if(!out)return n;if(capacity<n)return 0;return packet(out);}
extern "C" const char*arduboy_game_name(){return"hollowseeker";}
