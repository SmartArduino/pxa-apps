/* SPDX-License-Identifier: MIT. Ordinary original-game native acceptance. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <Arduboy2.h>
#include "../common/retro-games/arduboy-batch-20261010/backend.h"
static uint8_t wire[571];
static unsigned checks;
static void read_game(void) { assert(arduboy_game_pack(wire,sizeof wire)==sizeof wire);assert(!arduboy_pxa_state.fault); }
static unsigned u16(unsigned p) { return (unsigned)wire[p]|((unsigned)wire[p+1]<<8); }
static void check(int condition) {
    if(!condition) fprintf(stderr,"check=%u clock=%u state=%u distance=%u best=%u jump=%u grounded=%u dead=%u pause=%u continuation=%u\n",checks+1,arduboy_pxa_state.clock_ms,wire[137],u16(126),u16(128),wire[107],wire[110],wire[111],wire[136],wire[570]);
    assert(condition);++checks;
}
static void advance(unsigned ms) {
    /* Ordinary delivered ticks, not a large-gap catch-up/drop simulation. */
    while(ms) { unsigned n=ms>16?16:ms;arduboy_pxa_advance(n);ms-=n; }
    read_game();
}
static void press(uint8_t mask,unsigned ms=50) { arduboy_pxa_pointer(mask,0);advance(ms); }
static void release(unsigned ms=50) { arduboy_pxa_pointer(0,1);advance(ms); }
static unsigned eeprom_score(void) { return arduboy_pxa_state.eeprom[19]|((unsigned)arduboy_pxa_state.eeprom[20]<<8); }
int main(void) {
    arduboy_pxa_boot(17);advance(1);
    check(wire[0]==24 && wire[137]==0 && wire[570]==0 && u16(128)==0);
    check(arduboy_pxa_state.eeprom[16]==255 && arduboy_pxa_state.display_count>0);
    unsigned title_pixels=0;for(unsigned i=0;i<1024;i++)title_pixels+=arduboy_pxa_state.display[i]!=0;
    check(title_pixels>0);
    press(8);check(wire[137]==1 && u16(126)>0 && wire[109]);release();
    /* Original DOWN justPressed toggles pause, without replacing Game.run. */
    press(16);check(wire[136]==1);unsigned paused_distance=u16(126);release(100);
    check(u16(126)==paused_distance);
    press(16);check(wire[136]==0);release();
    /* No jump/controller assistance: the real first saw eventually kills. */
    unsigned waited=0;
    while(!wire[111] && waited<4000) { advance(1);++waited; }
    check(wire[111] && wire[137]==1);
    while(!wire[570] && waited<4500) { advance(1);++waited; }
    check(wire[570] && u16(128)>0 && wire[108]>=5);
    check(arduboy_pxa_state.eeprom[16]=='G' && arduboy_pxa_state.eeprom[17]=='E' && arduboy_pxa_state.eeprom[18]=='M');
    check(eeprom_score()==u16(128));
    uint8_t delay_state[571];memcpy(delay_state,wire,571);
    uint32_t at_delay=arduboy_pxa_state.clock_ms;
    check(arduboy_pxa_state.next_due_ms-at_delay==250);
    advance(249);check(!memcmp(delay_state,wire,571));
    advance(1);check(!wire[570] && !wire[111] && wire[137]==1 && u16(126)==0);
    unsigned best=u16(128);uint32_t before_new=arduboy_pxa_state.clock_ms;
    arduboy_pxa_new();advance(1);
    check(wire[137]==0 && u16(128)==best && arduboy_pxa_state.clock_ms>=before_new && eeprom_score()==best);
    press(144);check(wire[137]==0 && u16(128)==0 && eeprom_score()==0);release();
    /* Three genuine original jump-button paths, no direct Player writes. */
    const uint8_t jump_buttons[]={128,4,8};
    for(uint8_t button: jump_buttons) {
        arduboy_pxa_new();advance(1);press(8);release(700);
        unsigned leave_wait=0;
        while((wire[570]||wire[111]) && leave_wait++<1500)advance(1);
        check(!wire[570]&&!wire[111]);
        check(wire[110]);
        press(button);check(wire[109] && !wire[110] && wire[107]>0);release();
    }
    /* A short pulse lasts until the original gameplay poll consumes it. */
    arduboy_pxa_new();advance(1);press(8);release(700);
    arduboy_pxa_pointer(16,0);arduboy_pxa_pointer(0,1);advance(50);check(wire[136]==1);
    /* Repeated natural attempts exercise original reset/pool reuse. */
    arduboy_pxa_new();advance(1);press(8);release(10000);
    check(wire[137]==1 && wire[125]<=48 && u16(128)>0);
    printf("{\"passed\":true,\"checks\":%u,\"game_wire_bytes\":571,\"original_requested_fps\":24,\"clock_ms\":%u,\"raw_steps\":%u,\"displays\":%u,\"EEPROM_bytes\":1024,\"ordinary_input_only\":true}\n",checks,arduboy_pxa_state.clock_ms,arduboy_pxa_state.steps,arduboy_pxa_state.display_count);
}
