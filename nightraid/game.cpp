/* SPDX-License-Identifier: MIT. Original NightRaid with explicit read-only wire state. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <Arduboy2.h>
#include "../common/retro-games/arduboy-batch-20261010/backend.h"
void setup();
void loop();
void updateState();
void drawState();
void updateCursor();
void drawCursor();
void initBonusPlane();
void initIncomingMissile(byte index);
void missileSpawner();
byte liveIncomingCount();
void initNullMissile(byte index);
void updatePlane();
int nextScoreForModulo(int modulo);
void updateMissiles();
void drawMissiles();
void drawScore();
void drawPlane();
void drawCity(byte i);
void drawCities();
void randomRubble(byte targetPosition);
void fireMissile(bool turret);
byte activePlayerMissileCount();
void initRandomness();
void menuHandler();
void clearHighscore();
void updateDifficulty();
void updateLives();
void resetGame();
void drawMenu();
int calculateOutgoingExplosionRadiusFromFrame(byte frame);
int calculateIncomingExplosionRadiusFromFrame(byte frame);
int cityCount();
void drawGameover();
void drawPaused();
void gameoverHandler();
int activeIncomingCount();
void updateHighscore();
void checkForLoss();
void pauseHandler();
#include "vendor/platform/night-raid.ino"
static constexpr size_t GAME_WIRE_BYTES=394;
/* Original OLED command state: source sBuffer stays intact; physical display
 * is complemented after an actual paintScreen when the command is enabled. */
static bool lcd_inverted;
void Arduboy2Core::invert(bool enabled){lcd_inverted=enabled;}
static_assert(sizeof(float)==4,"original binary32 state");
extern "C" void arduboy_game_boot(void) {
 lcd_inverted=false;gameState=MENU;menuIndex=0;highscore=0;
 for(int i=0;i<8;++i){targets[i]=true;targetRubble[i]=targetRubbleSecondLayer[i]=0;targetXCoords[i]=8+16*i;}
 flashIndex[0]=flashIndex[1]=255;flashCounter[0]=flashCounter[1]=0;
 plane=Plane{};for(auto &m:incomingMissiles)m=Missile{};
 score=0;nextDifficultyUpdateScore=10;nextLivesUpdateScore=100;nextPlaneUpdateScore=25;
 aFired=bFired=true;pendingLoss=false;lossCounter=0;cursorX=63;cursorY=26;
 transitionLock=pauseLock=rightButtonMenuLock=leftButtonMenuLock=false;
 missileSpawnFrameDelay=MISSILE_FRAME_DELAY;setup();
}
extern "C" void arduboy_game_step(void){
 uint32_t previous_display=arduboy_pxa_state.display_count;loop();
 if(lcd_inverted && arduboy_pxa_state.display_count!=previous_display)
  for(auto &pixel:arduboy_pxa_state.display)pixel^=255;
}
extern "C" const char *arduboy_game_name(void){return "NightRaid";}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t capacity){
 if(!out)return GAME_WIRE_BYTES;if(capacity<GAME_WIRE_BYTES)return 0;
 uint8_t*p=out;auto u8=[&](uint8_t x){*p++=x;};
 auto u16=[&](uint16_t x){u8((uint8_t)x);u8((uint8_t)(x>>8));};
 auto f32=[&](float x){uint32_t v;memcpy(&v,&x,4);u16((uint16_t)v);u16((uint16_t)(v>>16));};
 u8(gameState);u8(menuIndex);u16((uint16_t)highscore);
 for(bool x:targets)u8(x?1:0);for(int16_t x:targetRubble)u16((uint16_t)x);
 for(int16_t x:targetRubbleSecondLayer)u16((uint16_t)x);
 for(byte x:flashIndex)u8(x);for(byte x:flashCounter)u8(x);for(byte x:targetXCoords)u8(x);
 u8(plane.startY);u8(plane.dir?1:0);f32(plane.currentX);u8(plane.explosionFrame);u8(plane.explosionRadius);u8(plane.valid?1:0);u8(plane.scored?1:0);
 for(const auto&m:incomingMissiles){u8(m.startX);u8(m.startY);u8(m.target);u8(m.stopX);u8(m.stopY);f32(m.currentY);f32(m.currentX);f32(m.speedY);u8(m.incoming?1:0);u8(m.valid?1:0);u8(m.explosionFrame);u8(m.explosionRadius);u8(m.scored?1:0);}
 u16((uint16_t)score);u16((uint16_t)nextDifficultyUpdateScore);u16((uint16_t)nextLivesUpdateScore);u16((uint16_t)nextPlaneUpdateScore);
 u8(aFired?1:0);u8(bFired?1:0);u8(pendingLoss?1:0);u8(lossCounter);u8(cursorX);u8(cursorY);
 u8(transitionLock?1:0);u8(missileSpawnFrameDelay);u8(pauseLock?1:0);u8(rightButtonMenuLock?1:0);u8(leftButtonMenuLock?1:0);u8(lcd_inverted?1:0);
 return size_t(p-out)==GAME_WIRE_BYTES?GAME_WIRE_BYTES:0;
}
