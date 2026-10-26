#include "backend.h"
#include "owner_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#undef rand
static unsigned checks=0,frames=0;
static void need(bool b,const char*why){++checks;if(!b){fprintf(stderr,"failed %s frame=%u check=%u\n",why,frames,checks);abort();}}
static std::vector<uint8_t> game(){size_t n=arduboy_game_pack(nullptr,0);need(n==772,"typed source bytes");std::vector<uint8_t>b(n);need(arduboy_game_pack(b.data(),n)==n,"complete pack");need(b[0]==1&&b[1]==12,"schema identity");need(b[13]==0&&b[755]==0,"owner and audio faults");need(arduboy_pxa_state.fault==0&&arduboy_pxa_state.dropped_steps==0,"core faults dropped");return b;}
static unsigned u16(const std::vector<uint8_t>&b,unsigned p){return b[p]|unsigned(b[p+1])<<8;}
static void advance(unsigned n,uint8_t mask=0){arduboy_pxa_controller(mask);while(n--){arduboy_pxa_advance(16);++frames;game();}}
static void tap(uint8_t mask){advance(2,mask);advance(2,0);}
static void select(unsigned pos){for(unsigned i=0;game()[251]!=pos&&i<8;i++)tap(16);need(game()[251]==pos,"ordinary menu selection");}
static void boot(){arduboy_pxa_boot(0x12345678);auto b=game();need(b[2]==0&&b[234]==120,"cold authenticLogo");for(unsigned i=0;i<1024;i++)need(arduboy_pxa_state.eeprom[i]==255,"fresh FF EEPROM");advance(185);need(game()[2]==1&&game()[285]==1,"authentic title");}
int main(){boot();
 select(0);tap(4);need(game()[2]==2&&game()[289]==5&&game()[705]==0,"instruction first page");tap(64);need(game()[705]==1,"instruction RIGHT");tap(4);need(game()[705]==2,"instruction B");tap(32);need(game()[705]==1,"instruction LEFT");tap(4);tap(4);need(game()[705]==3,"instruction fourthpage");tap(4);need(game()[2]==1,"instruction completion title");
 select(2);tap(4);need(game()[285]==2,"original record");tap(4);select(3);tap(4);need(game()[285]==3,"original MITcredit");tap(4);select(1);auto audio=arduboy_pxa_state.eeprom[2];tap(8);tap(8);need(arduboy_pxa_state.eeprom[2]==audio,"original sound toggles no prematurewrite");tap(4);need(game()[2]==2&&game()[289]==1,"authentic READY");advance(125);need(game()[289]==2,"original120frameREADY");
 auto p=game();advance(24,32);need(u16(game(),678)!=u16(p,678),"LEFT actualmovement");p=game();advance(24,64);need(u16(game(),678)!=u16(p,678),"RIGHT actualmovement");p=game();advance(24,128);need(u16(game(),680)<u16(p,680),"UP actualmovement");p=game();advance(24,16);need(u16(game(),680)>u16(p,680),"DOWN actualmovement");advance(3);
 unsigned scoreAt=0;for(unsigned f=0;f<6000;f++){auto b=game();int x=u16(b,678);unsigned wet=u16(b,354+7*8)+u16(b,354+8*8);int target=wet<4600?4096:6144;int delta=(target-x+4096)%8192-4096;advance(1,delta>64?64:delta<-64?32:0);if(u16(game(),688)>0){scoreAt=f;break;}}need(u16(game(),688)>0,"natural plantedflower score");advance(2);tap(8);need(game()[289]==4,"original A pause menu");unsigned pausedFrames=u16(game(),692);advance(10);need(u16(game(),692)==pausedFrames,"originalmenu freezesplaycounter");tap(4);need(game()[289]==2,"originalCONTINUE");
 tap(8);select(1);tap(4);need(game()[250]==2&&game()[251]==1,"restart confirmation CANCELdefault");tap(4);need(game()[289]==2,"originalcancel retainsgame");tap(8);select(1);tap(4);tap(128);tap(4);need(game()[289]==1&&u16(game(),688)==0&&u16(game(),690)==100,"confirmedoriginalrestart");advance(125);
 unsigned deathAt=0;for(unsigned f=0;f<4000;f++){advance(1,f%4<2?4:0);if(game()[289]==3){deathAt=f;break;}}need(game()[289]==3&&u16(game(),690)==0,"naturalcloudresourceexhaustionGAMEOVER");need(arduboy_pxa_state.eeprom[568]=='O'&&arduboy_pxa_state.eeprom[569]=='B'&&arduboy_pxa_state.eeprom[570]=='N'&&arduboy_pxa_state.eeprom[571]==0x18,"originalEE signature");advance(65);tap(4);need(game()[289]==1,"ordinary Bgameoverretry");advance(125);tap(8);select(2);tap(4);tap(128);tap(4);need(game()[2]==1,"originalconfirmedBACKTITLE");
 uint8_t eep[1024];memcpy(eep,arduboy_pxa_state.eeprom,1024);uint32_t clock=arduboy_pxa_state.clock_ms;arduboy_pxa_new();need(arduboy_pxa_state.clock_ms==clock&&!memcmp(eep,arduboy_pxa_state.eeprom,1024),"wrapperNEW preservesclockEEP");need(game()[2]==0&&game()[234]==120,"wrapperNEW originalcoldlogo");advance(185);need(u16(game(),38)>=2,"originalrecordreloadplaycount");
 printf("{\"passed\":true,\"checks\":%u,\"ordinary_frames\":%u,\"natural_score_input_frame\":%u,\"natural_gameover_input_frame\":%u,\"game_wire_bytes\":772,\"ordinary_input_only\":true,\"state_restores\":0,\"executed_AVR\":false}\n",checks,frames,scoreAt,deathAt);}
