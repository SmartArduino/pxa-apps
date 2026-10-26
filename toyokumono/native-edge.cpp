#include "backend.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks=0;
static void need(bool b){++checks;if(!b)abort();}
static void advance(unsigned n,uint8_t m=0){arduboy_pxa_controller(m);while(n--){arduboy_pxa_advance(16);need(!arduboy_pxa_state.fault&&!arduboy_pxa_state.dropped_steps);}}
static void tap(uint8_t m){advance(2,m);advance(2);}
int main(){
 arduboy_pxa_boot(0x12345678);advance(185);tap(16);tap(4);advance(125);
 for(unsigned f=0;f<1300;f++)advance(1,32|(f%4<2?4:0));
 printf("{\"passed\":true,\"checks\":%u,\"ordinary_frames\":1618,\"scope\":\"ordinary LEFT wrapping and B clouds\"}\n",checks);
}
