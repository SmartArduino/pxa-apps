/* Native-only CC0 original SNEK tests. No Guest state/seed injection. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SAF_PLATFORM_NATIVE_TEST
#include "minigames.ino"
static uint8_t rawButtons[7],pixels[4096],saves[32];static unsigned pixel_calls,sound_calls,save_calls;
static inline void SAF_FE_drawPixel(uint8_t x,uint8_t y,uint8_t c){assert(x<64&&y<64);pixels[y*64+x]=c;pixel_calls++;}
static inline void SAF_FE_playSound(uint8_t s){assert(s<SAF_SOUNDS);sound_calls++;}
static inline void SAF_FE_save(uint8_t i,uint8_t d){assert(i<32);saves[i]=d;save_calls++;}
static inline uint8_t SAF_FE_load(uint8_t i){assert(i<32);return saves[i];}
static inline uint8_t SAF_FE_buttonPressed(uint8_t b){assert(b<7);return rawButtons[b];}
static inline const char* SAF_FE_extension(const char*s){(void)s;return "";}
static void reset(void){memset(rawButtons,0,sizeof(rawButtons));memset(pixels,0,sizeof(pixels));memset(saves,0,sizeof(saves));memset(_SAF_buttonStates,0,sizeof(_SAF_buttonStates));memset(_SAF_saveMemory,0,sizeof(_SAF_saveMemory));_SAF_saveMemoryLoaded=0;_SAF_currentRandom=0;_SAF_frame=0;firstClick=menuItem=0;pixel_calls=sound_calls=save_calls=0;SAF_FE_init();snakeInit();stepFunction=snakeStep;}
static void frame(unsigned mask){for(int i=0;i<7;i++)rawButtons[i]=!!(mask&(1U<<i));assert(SAF_FE_loop()==1);}
static int length(void){int n=0;for(int i=0;i<928;i++)n+=getMemoryHalfByte(i)!=0;return n;}
static void chain(void){int i=SNAKE_TAIL_SQUARE,count=0;uint8_t visited[928]={0};while(1){assert(i>=0&&i<928&&!visited[i]);visited[i]=1;count++;if(i==SNAKE_HEAD_SQUARE)break;i=snakeNextSquare(i);}assert(count==length());}
static void full_last_food(void){reset();clearMemory();for(int y=0;y<29;y++)for(int x=0;x<32;x++){int index=y*32+x;if(index==927)continue;int dir=(y%2)?SNAKE_SQUARE_SL:SNAKE_SQUARE_SR;if((!(y%2)&&x==31)||((y%2)&&x==0))dir=SNAKE_SQUARE_SD;setMemoryHalfByte(index,dir);}SNAKE_TAIL_SQUARE=0;SNAKE_HEAD_SQUARE=926;SNAKE_FOOD_SQUARE=927;SNAKE_DIRECTION=SNAKE_SQUARE_SR;SNAKE_MOVE_COUNT=6;SNAKE_SCORE=SNAKE_FOOD_STEPS=SNAKE_GAME_STATE=0;chain();frame(0);printf("Entering original final-food move; valid length=%d\n",length());fflush(stdout);frame(0);puts("Returned from final-food move");}
int main(int argc,char**argv){
 if(argc==2&&!strcmp(argv[1],"--full-food")){reset();for(int i=0;i<928;i++)setMemoryHalfByte(i,2);puts("Entering original food scan on 928 occupied cells");fflush(stdout);snakeSpawnFood();puts("Returned");return 0;}
 if(argc==2&&!strcmp(argv[1],"--last-food")){full_last_food();return 0;}
 reset();assert(length()==6&&SNAKE_HEAD_SQUARE-SNAKE_TAIL_SQUARE==5&&SNAKE_DIRECTION==2&&SNAKE_SCORE==0&&SNAKE_MOVE_COUNT==6&&!SNAKE_GAME_STATE&&getMemoryHalfByte(SNAKE_FOOD_SQUARE)==0);chain();snakeDraw();assert(pixel_calls>4096);puts("PASS original Init/Draw: six segments, valid empty food, 4096 bounded RGB332 callback pixels");
 int head=SNAKE_HEAD_SQUARE;frame(0);assert(SNAKE_HEAD_SQUARE==head&&SNAKE_MOVE_COUNT==5&&SAF_frame()==1&&SAF_time()==40);frame(0);assert(SNAKE_HEAD_SQUARE==head+1&&SNAKE_MOVE_COUNT==6&&SNAKE_FOOD_STEPS==1);chain();puts("PASS original 40ms FE frame and two-frame normal movement");
 reset();head=SNAKE_HEAD_SQUARE;for(int i=0;i<5;i++)frame(1U<<SAF_BUTTON_A);assert(SNAKE_HEAD_SQUARE==head);frame(1U<<SAF_BUTTON_A);assert(SNAKE_HEAD_SQUARE==head+1&&SNAKE_FOOD_STEPS==1);reset();head=SNAKE_HEAD_SQUARE;for(int i=0;i<6;i++)frame(0);assert(SNAKE_HEAD_SQUARE==head+3);puts("PASS original A slows movement to six frames; it is not acceleration");
 reset();head=SNAKE_HEAD_SQUARE;frame(1U<<SAF_BUTTON_LEFT);frame(1U<<SAF_BUTTON_LEFT);assert(SNAKE_HEAD_SQUARE==head+1&&getMemoryHalfByte(SNAKE_HEAD_SQUARE)==SNAKE_SQUARE_SR);chain();puts("PASS original 180-degree turn request cannot reverse the head");
 reset();for(int i=0;i<4;i++)frame(1U<<SAF_BUTTON_UP);head=SNAKE_HEAD_SQUARE;frame((1U<<SAF_BUTTON_UP)|(1U<<SAF_BUTTON_RIGHT));assert(SAF_buttonPressed(SAF_BUTTON_UP)==5&&SAF_buttonPressed(SAF_BUTTON_RIGHT)==1&&SNAKE_DIRECTION==SNAKE_SQUARE_SR);frame((1U<<SAF_BUTTON_UP)|(1U<<SAF_BUTTON_RIGHT));assert(SNAKE_HEAD_SQUARE==head+1);reset();frame((1U<<SAF_BUTTON_UP)|(1U<<SAF_BUTTON_RIGHT));assert(SNAKE_DIRECTION==SNAKE_SQUARE_SU);puts("PASS original shortest-held direction and deterministic equal-duration Up priority");
 reset();for(int i=0;i<260;i++)frame(1U<<SAF_BUTTON_C);assert(SAF_buttonPressed(SAF_BUTTON_C)==255&&SAF_time()==10400);frame(0);assert(SAF_buttonPressed(SAF_BUTTON_C)==0);uint32_t old_frame=SAF_frame();SAF_FE_init();assert(SAF_frame()==old_frame);puts("PASS original held 1..255 saturation, release 0 and FE_init does not reset time");
 reset();frame(1U<<SAF_BUTTON_UP);frame(1U<<SAF_BUTTON_UP);frame(1U<<SAF_BUTTON_LEFT);frame(1U<<SAF_BUTTON_LEFT);frame(1U<<SAF_BUTTON_DOWN);frame(1U<<SAF_BUTTON_DOWN);assert(SNAKE_GAME_STATE==2&&length()==6);puts("PASS original self collision, equal zero high score end-state 2");
 reset();for(int i=0;i<28;i++)frame(0);assert(SNAKE_GAME_STATE==2&&SNAKE_HEAD_SQUARE%32==31);puts("PASS original row-wrap border collision");
 reset();saves[0]=100;for(int i=0;i<28;i++)frame(0);assert(SNAKE_GAME_STATE==1&&getHiScore(0)==100);frame(1U<<SAF_BUTTON_A);assert(!SNAKE_GAME_STATE&&length()==6&&SNAKE_SCORE==0&&getHiScore(0)==100);puts("PASS original lower-score end-state 1 and A JustPressed restart preserves RAM high score");
 reset();int tail=SNAKE_TAIL_SQUARE;SNAKE_FOOD_SQUARE=snakeNextSquare(SNAKE_HEAD_SQUARE);frame(0);frame(0);assert(SNAKE_SCORE==20&&SNAKE_FOOD_STEPS==0&&length()==7&&SNAKE_TAIL_SQUARE==tail&&getMemoryHalfByte(SNAKE_FOOD_SQUARE)==0);chain();reset();SNAKE_FOOD_SQUARE=snakeNextSquare(SNAKE_HEAD_SQUARE);SNAKE_FOOD_STEPS=250;frame(0);frame(0);assert(SNAKE_SCORE==10&&length()==7);puts("PASS original eat grows one segment and scores 20-age/25; age is moves, not frames");
 reset();SNAKE_SCORE=512;for(int i=0;i<28;i++)frame(0);assert(SNAKE_GAME_STATE==2&&getHiScore(0)==512&&saves[0]==0&&saves[1]==2&&save_calls==1);puts("PASS original two-byte score and save-only-changed callback");
 reset();for(int i=0;i<17;i++)frame(1U<<SAF_BUTTON_B);assert(stepFunction==snakeStep);frame(1U<<SAF_BUTTON_B);assert(stepFunction==menuStep);frame(0);frame(1U<<SAF_BUTTON_A);assert(stepFunction==snakeStep&&!SNAKE_GAME_STATE&&length()==6&&firstClick==1);puts("PASS original B held18 returns menu and original A selection reinitializes SNEK");
 int sole_empty[]={0,463,926,927};for(int p=0;p<4;p++){int only=sole_empty[p];reset();for(int i=0;i<928;i++)setMemoryHalfByte(i,2);setMemoryHalfByte(only,0);snakeSpawnFood();assert(SNAKE_FOOD_SQUARE==only);}puts("PASS original backward food scan wraps to the sole empty cell, including indices 0 and 927");return 0;
}
