#include "backend.h"
#include "owner_compat.h"
#include "vendor/platform/ardubullets.ino"
extern "C" void arduboy_game_boot(){arduboy.pxaReset();obono_owner_reset();obono_reset_common();obono_reset_menu();obono_reset_game();obono_reset_title();obono_register_game();mode=MODE_LOGO;setup();}
extern "C" void arduboy_game_step(){obono_audio_advance(millis());loop();}
static size_t packet(uint8_t*out){ObonoWriter w(out);w.u8(1);w.u8(11);w.u8(mode);arduboy.pxaPack(w);obono_owner_pack(w);obono_audio_pack(w);obono_pack_common(w);obono_pack_menu(w);obono_pack_title(w);obono_pack_game(w);return w.size();}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t capacity){size_t n=packet(nullptr);if(!out)return n;if(capacity<n)return 0;return packet(out);}
extern "C" const char*arduboy_game_name(){return "ardubullets";}
