#include "game.cpp"
#include <assert.h>
#include <stdio.h>
static void advance(unsigned ms){while(ms){unsigned n=ms>64?64:ms;arduboy_pxa_advance(n);ms-=n;}assert(!arduboy_pxa_state.fault);}
static void key(uint8_t mask,unsigned held=80,unsigned released=80){arduboy_pxa_controller(mask);advance(held);arduboy_pxa_controller(0);advance(released);}
static unsigned lasers(){unsigned count=0;for(auto y:PlayerLaserY)if(y)++count;return count;}
static void stop(){root_task=port::Task{};port::leaf={};assert(port::active==0);}
static port::Task fill(){
 for(unsigned i=0;i<MaxEnemies;i++)co_await SpawnEnemy(1,1,1,false);
 co_await SpawnEnemy(1,1,1,false);
}
int main(){
 arduboy_pxa_boot(1234);advance(2200);
 assert(Menu==1&&TitleScreenMove==1&&GameType==10);
 key(A_BUTTON);assert(Menu==2);key(A_BUTTON,80,4000);
 assert(Menu==0&&TitleScreenMove==0&&GameType==1&&Lives==3);assert(EnemyCount==27);
 for(auto hp:BarrierHP)assert(hp==8);
 const unsigned x=PlayerX;key(RIGHT_BUTTON,160);assert(PlayerX>x);
 key(B_BUTTON,80,40);assert(lasers()>0); // original swapped AButton fires on physical B
 // Original pause must freeze game state but still accept a released/second A press.
 key(A_BUTTON,80,500);const unsigned paused_x=PlayerX;
 key(RIGHT_BUTTON,200,200);assert(PlayerX==paused_x);
 key(A_BUTTON,80,500);key(LEFT_BUTTON,80,80);assert(PlayerX<paused_x);
 // Save data must use the AVR/WASM32 4-byte score without overwriting address 22.
 Score=123456;Invulnerable=17;SaveProgress();Score=0;EEPROM.get(18,Score);assert(Score==123456&&EEPROM.read(22)==17);
 const size_t packed=arduboy_game_pack(nullptr,0);assert(packed<ARDU_GAME_MAX_BYTES);uint8_t data[2048];assert(arduboy_game_pack(data,sizeof data)==packed);
 // Fill exactly 50 slots then reject the 51st without changing count or corrupting memory.
 stop();ResetVariables();root_task=fill();port::leaf=root_task.handle;advance(1);assert(EnemyCount==50);
 for(auto type:EnemyType)assert(type==1);stop();
 // Full boss entrance retains all 2500 one-millisecond updates and the 600ms wait.
 arduboy_pxa_new();stop();TitleScreenMove=0;GameType=3;Level=4;
 root_task=UFOAppear();port::leaf=root_task.handle;unsigned begin=arduboy_pxa_state.clock_ms;
 advance(3099);assert(port::leaf);advance(3);assert(!port::leaf);assert(UFOHP==32&&UFOX==56&&UFOY==16);
 assert(arduboy_pxa_state.clock_ms-begin>=3100);stop();
 // Exercise the deepest real chain: run_game/loop/RunEnemyCode/NewLevel/
 // UFOAppear/DisplayInfo/PauseGame, then cancel it from NEW.
 arduboy_pxa_new();advance(2200);key(A_BUTTON);key(A_BUTTON,80,4000);
 memset(EnemyType,0,sizeof EnemyType);memset(LaserY,0,sizeof LaserY);
 EnemyCount=0;StartMove=0;Death=0;MegaShot=0;Invulnerable=0;GameType=1;
 randomSeed(2);advance(40);assert(GameType==3);
 key(A_BUTTON,100,500);assert(port::high_water>=7);
 stop();
 // Reset while startup, wave-building and paused continuations are suspended.
 for(unsigned i=0;i<100;i++){
  arduboy_pxa_new();assert(Menu==1&&Lives==3&&Score==0);
  if(i%3==0)advance((i*37)%2050+1);
  else {
   advance(2200);key(A_BUTTON);
   key(A_BUTTON,80,i%3==1?140:4000);
   if(i%3==1)assert(EnemyCount>0&&EnemyCount<27);
   else key(A_BUTTON,80,250);
  }
  assert(port::active<=16);
 }
 stop();assert(port::largest_frame<=2048);assert(port::max_transitions<=128);
 printf("PASS Invaders: menu->27 aliens+4 barriers, movement/fire/pause/save, full array rejection, timed boss, 100 pending NEW cancellations; pack=%zu pool_high=%u max_frame=%u max_transitions=%u\n",packed,port::high_water,port::largest_frame,port::max_transitions);
}
