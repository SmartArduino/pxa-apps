/* SPDX-License-Identifier: MIT. Expected failure on BYTE-EXACT upstream
 * game.h, ordinary buttons only. Keep ASan/UBSan stderr as evidence.
 * This source is never compiled into the Guest.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <Arduboy2.h>
#include "backend.h"
using u32 = uint32_t;
#define private public
#include "vendor/original/guess_dots/game.h"
#undef private
static Game original;
extern "C" void arduboy_game_boot(void){original.init();}
extern "C" void arduboy_game_step(void){original.run();}
extern "C" const char* arduboy_game_name(void){return "original OOB witness";}
extern "C" size_t arduboy_game_pack(uint8_t*,size_t){return 0;}
static void advance(unsigned n){while(n){unsigned m=n>16?16:n;arduboy_pxa_advance(m);n-=m;}}
static void tap(uint8_t mask){arduboy_pxa_pointer(mask,0);advance(100);arduboy_pxa_pointer(0,1);advance(100);}
int main(){
    arduboy_pxa_boot(17);advance(100);
    tap(A_BUTTON);tap(DOWN_BUTTON);tap(DOWN_BUTTON);
    if(original.chosen_difficulty!=lvl::Difficulty::HARD)return 2;
    tap(UP_BUTTON|DOWN_BUTTON);
    if(original.chosen_difficulty!=lvl::Difficulty::COUNT)return 3;
    fprintf(stderr,"ORDINARY UP+DOWN produced COUNT3 on untouched upstream; next A indexes lm[3]\n");fflush(stderr);
    tap(A_BUTTON);
    return 4; /* The sanitizer should terminate before this point. */
}
