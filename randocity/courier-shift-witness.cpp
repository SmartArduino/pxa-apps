/* MIT. Explicit boundary fixture, separate from ordinary-input acceptance.
 * Includes the entire pinned raw sketch for the failing original witness.
 */
#include <Arduboy2.h>
#include "vendor/platform/ArduboyTones.h"
#ifdef WITNESS_FIXED
#include "vendor/platform/Records.h"
#else
#include "vendor/original/Records.h"
#endif
#include "original-prototypes.h"
#include <stdio.h>
#include <assert.h>
struct SerialNoop {void begin(unsigned){}};static SerialNoop Serial;
static void records_load_avr(uint16_t,Records&){}
static void records_save_avr(uint16_t,const Records&){}
#ifdef WITNESS_FIXED
#include "vendor/platform/Randocity.ino"
#else
#include "vendor/original/Randocity.ino"
#endif
extern "C" void arduboy_game_boot(){}
extern "C" void arduboy_game_step(){}
extern "C" const char*arduboy_game_name(){return "boundary-witness";}
extern "C" size_t arduboy_game_pack(uint8_t*,size_t){return 0;}
int main(){
 world.init(806737);playerX=0;playerY=0;randomSeed(2);randomizeCourier();
#ifdef WITNESS_FIXED
 unsigned cases=0;
 for(int block=-5;block<=260;++block)for(int offset=-5;offset<5;++offset){
  int value=block+offset;int32_t scaled=(int32_t)(value*2048l);
  assert(scaled==(int64_t)value*2048);
  if(value>=0)assert(scaled==(int32_t)((uint32_t)value<<11));++cases;
 }
 printf("{\"passed\":true,\"explicit_fixture\":true,\"defined_positive_shift_and_bounded_negative_scaling_cases\":%u,\"ordinary_game_acceptance\":false}\n",cases);
#else
 printf("{\"passed\":true,\"explicit_fixture\":true,\"player_x\":0,\"player_y\":0,\"seed\":2,\"courier_x\":%d,\"courier_y\":%d,\"ordinary_game_acceptance\":false}\n",courierX,courierY);
#endif
}
