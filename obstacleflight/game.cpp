/* SPDX-License-Identifier: MIT. Original Unlicense game with explicit wire observer. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>
#include <type_traits>
#include <cmath>
#include <Arduboy2.h>
#include "backend.h"
#include "vendor/platform/tinyfont/Tinyfont.h"
#ifndef sq
#define sq(value) ((value)*(value)) // Original Arduino integer square macro.
#endif
void spawnObstacle();void drawRoom();void drawObstacle();void moveShip();void collideWall();
void destroyObstacle();void drawLives();void killPlayer();void resetGame();
void drawVector(int,int,int[],int[],byte,int);
void drawVectorRot(int,int,int[],int[],byte,int,int,int);
byte getDistance(int,int,int,int);int getAngle(int,int,int,int);
int lendirX(int,int);int lendirY(int,int);void drawLineInv(int16_t,int16_t,int16_t,int16_t);
/* Original sketch uses the older public swap method. Delegate it to the
   frozen library's same int16 reference swap; no extra state or rule. */
class ObstacleArduboy:public Arduboy2{
 public:static void swap(int16_t&a,int16_t&b){swapInt16(a,b);}
};
#define Arduboy2 ObstacleArduboy
#ifdef OBSTACLE_ORIGINAL_UNFIXED
#include "vendor/original/obstacleflight/ObstacleFlight.ino"
#else
#include "vendor/platform/ObstacleFlight.ino"
#endif
#undef Arduboy2
extern "C" void arduboy_game_boot(void){
 screen=2;
 const int initialX[13]={0,2,4,6,8,4,0,-4,-8,-6,-4,-2,0};
 const int initialY[13]={-11,-6,-8,0,4,7,5,7,4,0,-8,-6,-11};
 for(unsigned i=0;i<13;i++){shipX[i]=initialX[i];shipY[i]=initialY[i];}
 obstacleDepth=0;obstacleType=0;roto=0;pitch=76;xo=64;yo=48;
 const float initialDepth[5]={1,13.8f,26.6f,39.4f,52.2f};
 for(unsigned i=0;i<5;i++)leftDepth[i]=initialDepth[i];
 lives=3;score=0;temp=0;
 /* Reconstruct the original Tinyfont using its original arguments; the frozen
    Host can skip non-folded constructors while its common counter is folded. */
 static_assert(std::is_trivially_destructible<Tinyfont>::value,"No destructor side effects");
 new(static_cast<void*>(&tinyfont)) Tinyfont(arduboy.sBuffer,Arduboy2::width(),Arduboy2::height());
 setup();
}
extern "C" void arduboy_game_step(void){loop();}
extern "C" const char*arduboy_game_name(void){return "ObstacleFlight";}
static constexpr size_t GAME_WIRE_BYTES=101;
static void put8(uint8_t**p,uint8_t x){*(*p)++=x;}
static void put16(uint8_t**p,uint16_t x){put8(p,(uint8_t)x);put8(p,(uint8_t)(x>>8));}
static void putf(uint8_t**p,float x){static_assert(sizeof(float)==4,"binary32");uint32_t bits;memcpy(&bits,&x,4);put16(p,(uint16_t)bits);put16(p,(uint16_t)(bits>>16));}
struct TinyfontPxaState{
 static void pack(uint8_t**p,const Tinyfont&f){
  put16(p,(uint16_t)f.sWidth);put16(p,(uint16_t)f.sHeight);put16(p,(uint16_t)f.cursorX);put16(p,(uint16_t)f.baseX);put16(p,(uint16_t)f.cursorY);
  put8(p,f.textColor);put8(p,f.letterSpacing);put8(p,f.lineHeight);put8(p,f.maskText?1:0);
 }
};
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t capacity){
 if(!out)return GAME_WIRE_BYTES;if(capacity<GAME_WIRE_BYTES)return 0;uint8_t*p=out;
 put8(&p,screen);for(int x:shipX)put16(&p,(uint16_t)x);for(int y:shipY)put16(&p,(uint16_t)y);
 putf(&p,obstacleDepth);put8(&p,obstacleType);put16(&p,(uint16_t)roto);put16(&p,(uint16_t)pitch);put8(&p,xo);put8(&p,yo);
 for(float x:leftDepth)putf(&p,x);put8(&p,lives);put8(&p,score);put8(&p,temp);TinyfontPxaState::pack(&p,tinyfont);
 return (size_t)(p-out)==GAME_WIRE_BYTES?GAME_WIRE_BYTES:0;
}
