/* SPDX-License-Identifier: MIT. Native-only safety fixtures; no Guest injection. */
#include <assert.h>
#include <stdio.h>
#include "../common/retro-games/saf/core.h"
static unsigned length(void){unsigned n=0;for(int i=0;i<928;i++)n+=getMemoryHalfByte(i)!=0;return n;}
static void chain(void){unsigned n=0;int i=SNAKE_TAIL_SQUARE;uint8_t seen[928]={0};while(1){assert(i>=0&&i<928&&!seen[i]);seen[i]=1;n++;if(i==SNAKE_HEAD_SQUARE)break;i=snakeNextSquare(i);}assert(n==length());}
int main(void){
 saf_boot();clearMemory();
 for(int y=0;y<29;y++)for(int x=0;x<32;x++){int i=y*32+x;if(i==927)continue;int d=y%2?SNAKE_SQUARE_SL:SNAKE_SQUARE_SR;if((!(y%2)&&x==31)||((y%2)&&x==0))d=SNAKE_SQUARE_SD;setMemoryHalfByte(i,d);}
 SNAKE_TAIL_SQUARE=0;SNAKE_HEAD_SQUARE=926;SNAKE_FOOD_SQUARE=927;SNAKE_DIRECTION=SNAKE_SQUARE_SR;SNAKE_MOVE_COUNT=6;SNAKE_SCORE=SNAKE_FOOD_STEPS=SNAKE_GAME_STATE=0;chain();assert(length()==927);
 assert(saf_frame()&&length()==927);assert(saf_frame());chain();
 assert(length()==928&&SNAKE_HEAD_SQUARE==927&&SNAKE_TAIL_SQUARE==0&&SNAKE_FOOD_SQUARE==-1&&SNAKE_SCORE==20&&SNAKE_GAME_STATE==2&&getHiScore(0)==20&&!saf_io.fault);
 puts("PASS genuine valid927 final-food consumption returns bounded; grows928, score20, FOOD=-1, highscore20");
 int empty[]={0,463,926,927};
 for(unsigned p=0;p<4;p++){saf_boot();for(int i=0;i<928;i++)setMemoryHalfByte(i,2);setMemoryHalfByte(empty[p],0);snakeSpawnFood();assert(SNAKE_FOOD_SQUARE==empty[p]&&!saf_io.fault);}
 puts("PASS bounded scan preserves original sole-empty selection at0/463/926/927");
 saf_boot();SNAKE_TAIL_SQUARE=-1;snakeDraw();assert(saf_io.fault==3);
 puts("PASS invalid draw-chain tail terminates with explicit safety fault");
 saf_boot();clearMemory();setMemoryHalfByte(0,SNAKE_SQUARE_SR);setMemoryHalfByte(1,SNAKE_SQUARE_SL);SNAKE_TAIL_SQUARE=0;SNAKE_HEAD_SQUARE=10;snakeDraw();assert(saf_io.fault==3);
 puts("PASS cyclic draw-chain terminates after bounded traversal with explicit safety fault");
 return 0;
}
