#include "backend.h"
#include "owner_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#undef rand
static unsigned checks=0,frames=0;
static void need(bool b,const char*why){++checks;if(!b){fprintf(stderr,"failed %s frame=%u check=%u\n",why,frames,checks);abort();}}
static std::vector<uint8_t> game(){size_t n=arduboy_game_pack(nullptr,0);need(n==337,"typed full bytes");std::vector<uint8_t>b(n);need(arduboy_game_pack(b.data(),n)==n,"complete pack");need(b[0]==1&&b[1]==13,"schema identity");need(b[13]==0&&b[320]==0,"owner/audio fault");need(!arduboy_pxa_state.fault&&!arduboy_pxa_state.dropped_steps,"core fault/dropped");return b;}
static unsigned u16(const std::vector<uint8_t>&b,unsigned p){return b[p]|unsigned(b[p+1])<<8;}
static void advance(unsigned n,uint8_t mask=0){arduboy_pxa_controller(mask);while(n--){arduboy_pxa_advance(16);++frames;game();}}
static void tap(uint8_t mask){advance(2,mask);advance(2,0);}
static void select(unsigned pos){for(unsigned i=0;game()[43]!=pos&&i<8;i++)tap(game()[43]<pos?16:128);need(game()[43]==pos,"ordinary menu selection");}
int main(){arduboy_pxa_boot(0x12345678);need(game()[2]==0&&game()[28]==120,"cold original logo");for(unsigned i=0;i<1024;i++)need(arduboy_pxa_state.eeprom[i]==255,"fresh FF EEPROM");advance(185);need(game()[2]==1&&game()[77]==1,"original title");
 select(1);tap(4);need(game()[77]==2,"settings");unsigned speed=game()[14];tap(64);need(game()[14]!=speed,"RIGHT setting");tap(32);need(game()[14]==speed,"LEFT setting");tap(128);tap(16);need(game()[43]==1,"UP/DOWN setting selection");select(5);tap(4);need(game()[77]==1,"setting exit");select(2);tap(4);need(game()[77]==3,"MIT credit");tap(4);select(0);tap(4);need(game()[2]==2&&game()[82]==1,"original ready");advance(65);need(game()[82]==2,"original play");advance(8,32);need(game()[268]==255,"LEFT velocity");advance(8,64);need(game()[268]==1,"RIGHT velocity");advance(2);need(u16(game(),259)>0,"natural score");
 tap(8);need(game()[82]==5,"A original pause");unsigned score=u16(game(),259);advance(10);need(u16(game(),259)==score,"original pause freezes score");tap(4);need(game()[82]==2,"B original continue");tap(8);select(1);tap(4);need(game()[42]==2&&game()[43]==1,"retry CANCEL default");tap(4);need(game()[82]==2,"cancel returns same game");tap(8);select(1);tap(4);tap(128);tap(4);need(game()[82]==1&&u16(game(),259)==0,"confirmed original restart");advance(65);
 unsigned deathAt=0;for(unsigned f=0;f<6000;f++){advance(1);if(game()[82]==3){deathAt=f;break;}}need(game()[82]==3,"natural collision death");need(u16(game(),259)>0&&u16(game(),20)>0,"natural hiscore");advance(65);need(game()[82]==4,"original gameover");need(arduboy_pxa_state.eeprom[640]=='O'&&arduboy_pxa_state.eeprom[641]=='B'&&arduboy_pxa_state.eeprom[642]=='N'&&arduboy_pxa_state.eeprom[643]==0x14,"original EEPROM signature");tap(4);need(game()[82]==1,"original B gameover retry");advance(65);tap(8);select(2);tap(4);tap(128);tap(4);need(game()[2]==1,"original confirmed back to title");
 uint8_t eep[1024];memcpy(eep,arduboy_pxa_state.eeprom,1024);uint32_t clock=arduboy_pxa_state.clock_ms;arduboy_pxa_new();need(arduboy_pxa_state.clock_ms==clock&&!memcmp(eep,arduboy_pxa_state.eeprom,1024),"PXA new preserves clockEEPROM");need(game()[2]==0&&game()[28]==120,"PXA new original logo");advance(185);need(u16(game(),20)>0&&u16(game(),26)>=2,"original saved record reload");
 printf("{\"passed\":true,\"checks\":%u,\"ordinary_frames\":%u,\"natural_gameover_input_frame\":%u,\"game_wire_bytes\":337,\"ordinary_input_only\":true,\"state_restores\":0,\"executed_AVR\":false}\n",checks,frames,deathAt);}
