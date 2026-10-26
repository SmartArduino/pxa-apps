/* SPDX-License-Identifier: MIT. PXA owner entry around the original BSD-3-Clause sketch.
 * Original rules, enemy AI, assets and map bytes are in vendor/platform.
 * Changes are documented in README.md and platform-source.diff. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <type_traits>
#include <Arduboy2.h>
#include <ArduboyTones.h>
#include "../common/retro-games/arduboy-batch-20261010/backend.h"
#include "vendor/platform/src/utils/Utils.h"
#define LODE_PXA_ALL_LEVELS 1
#include "vendor/platform/src/levels/Levels.h"
#include "vendor/platform/src/levels/Level.h"
#include "vendor/platform/src/utils/Queue.h"
#include "vendor/platform/src/utils/EEPROM_Utils.h"

/* Explicit Arduino forward declarations, before the original sketch body. */
void Intro();
void GameSelect();
void LevelPlay();
void playerDies();
void NextGame();
void CompleteGame();
void CompleteSeries();
void clearEnemyMovementPositions(Enemy *enemies);
void enemyMovements(Enemy *enemy);
boolean isOccupiedByAnotherEnemy(Enemy *enemies, Enemy *enemy, int8_t xDelta, int8_t yDelta);
void setDirectionAfterHoleEscape(Enemy *enemy);
void move(Enemy *enemy, int8_t x, int8_t y, bool randomMoves);
void updateEnemyStance(Enemy *enemy, PlayerStance lowerRange, PlayerStance upperRange);
void stopPlayer();
void playerMovements(uint8_t nearestX, uint8_t nearestY, LevelElement nearest);
void movePlayerLeft();
void movePlayerRight();
void movePlayerDown();
void movePlayerUp();
void updatePlayerStance(PlayerStance lowerRange, PlayerStance upperRange);
void renderScreen();
void renderLevelElements();
void renderEnemies();
void renderArrows();
void renderArrows(bool smallArrows);
void renderEntryRectangle();
void renderScoreboard();
void splashScreen_Init();
void splashScreen();
boolean inCellX();
boolean inCellX(uint8_t margin);
boolean inCellY();
boolean inCellY(uint8_t margin);
PlayerStance getNextStance(PlayerStance stance);
Direction getDirection_4Directions(Direction direction);
Direction getDirection_16Directions(int16_t xDiff, int16_t yDiff);
boolean attemptToMove(Enemy *enemy, uint8_t enemyX, uint8_t enemyY, 
                      Direction direction, 
                      LevelElement current, LevelElement up,  
                      LevelElement right, LevelElement rightDown, LevelElement down,
                      LevelElement leftDown, LevelElement left, bool randomMoves);
#define setup lode_original_setup
#define loop lode_original_loop
#include "vendor/platform/LodeRunner.ino"
#include "vendor/platform/LodeRunner_EnemyMovements.ino"
#include "vendor/platform/LodeRunner_PlayerMovements.ino"
#include "vendor/platform/LodeRunner_Render.ino"
#include "vendor/platform/LodeRunner_SplashScreen.ino"
#include "vendor/platform/LodeRunner_Utils.ino"
#undef setup
#undef loop

static uint8_t logged_state=255,logged_buttons=255;
extern "C" void arduboy_game_boot(void) {
    logged_state=logged_buttons=255;
    static_assert(std::is_trivially_copyable<Player>::value,"Player reset storage");
    static_assert(std::is_trivially_copyable<Level>::value,"Level reset storage");
    static_assert(std::is_trivially_copyable<Enemy>::value,"Enemy reset storage");
    // Original global objects start with zeroed static storage before empty constructors.
    memset(&player,0,sizeof player);
    memset(&level,0,sizeof level);
    memset(enemies,0,sizeof enemies);
    holes=Queue<Hole,20>();
    flashPlayer=false;gameState=GameState::SplashScreen_Init;
    bannerStripe=-30;introRect=0;suicide=0;levelCount=0;menuSelect=0;
#ifdef INC_LEVEL_SELECTOR
    menuLevelSelect=1;
#endif
    sound.noTone();
    // NEW always creates a new run, including lives/score and the original save markers.
    // Session EEPROM is retained by the provider; only this game's address range is reset.
    for(int i=175;i<=186;++i)EEPROM.update(i,0);
    EEPROM_Utils::initEEPROM(true);
    lode_original_setup();
    arduboy.clear();arduboy.display(CLEAR_BUFFER); // NEW clears the prior run immediately.
}
#ifdef __wasm__
#include "pxa_port_log.h"
#endif
extern "C" void arduboy_game_step(void) {
    lode_original_loop();
    uint8_t current=static_cast<uint8_t>(gameState),buttons=arduboy_pxa_state.applied;
    if(current!=logged_state || buttons!=logged_buttons) {
#ifdef __wasm__
        pxlog_t log;pxlog_init(&log);pxlog_text(&log,"CLASSIC lode");
        pxlog_field(&log,"state",current);pxlog_field(&log,"level",level.getLevelNumber());
        pxlog_field(&log,"gold",level.getGoldLeft());pxlog_field(&log,"lives",player.getMen());
        pxlog_field(&log,"x",player.getX()-level.getXOffset());pxlog_field(&log,"y",player.getY()-level.getYOffset());
        pxlog_field(&log,"score",player.getScore());pxlog_field(&log,"holes",holes.getCount());
        pxlog_field(&log,"buttons",buttons);pxlog_send(&log);
#endif
        logged_state=current;logged_buttons=buttons;
    }
}
extern "C" const char *arduboy_game_name(void) { return "Lode Runner"; }

// Stable little-endian diagnostic state; no native padding, pointers or object memcpy.
// v1: state, level, gold, lives, worldX/Y, score, screenX/Y, deltas, stance, nextState,
// offsets, offsetDeltas, holeCount, suicide/levelCount/menu/flash/intro/banner;
// six enemy records, active hole records, then the 28x16 tile map in row-major order.
extern "C" size_t arduboy_game_pack(uint8_t *out,size_t capacity) {
    const size_t required=31+NUMBER_OF_ENEMIES*17+holes.getCount()*4+28*16;
    if(!out)return required;
    if(capacity<required)return 0;
    uint8_t *p=out;
    auto u8=[&](uint8_t v){*p++=v;};
    auto u16=[&](uint16_t v){u8(static_cast<uint8_t>(v));u8(static_cast<uint8_t>(v>>8));};
    u8(1);u8(static_cast<uint8_t>(gameState));u8(level.getLevelNumber());
    u8(level.getGoldLeft());u8(player.getMen());
    u16(static_cast<uint16_t>(player.getX()-level.getXOffset()));
    u16(static_cast<uint16_t>(player.getY()-level.getYOffset()));u16(player.getScore());
    u8(player.getX());u8(player.getY());u8(player.getXDelta());u8(player.getYDelta());
    u8(static_cast<uint8_t>(player.getStance()));u8(static_cast<uint8_t>(player.getNextState()));
    u16(static_cast<uint16_t>(level.getXOffset()));u16(static_cast<uint16_t>(level.getYOffset()));
    u8(level.getXOffsetDelta());u8(level.getYOffsetDelta());u8(holes.getCount());
    u8(suicide);u8(levelCount);u8(menuSelect);u8(flashPlayer);u8(introRect);u8(bannerStripe);
    u8(level.getLevelLadderElementCount());
    for(const Enemy &e:enemies){
        u8(e.getId());u8(e.getEnabled());u16(e.getX());u16(e.getY());
        u8(e.getXDelta());u8(e.getYDelta());u8(e.getXFuturePosition());u8(e.getYFuturePosition());
        u8(e.getGoldCountdown());u8(e.getDirectionCountdown());u8(static_cast<uint8_t>(e.getStance()));
        u8(static_cast<uint8_t>(e.getEscapeHole()));u8(static_cast<uint8_t>(e.getDirection()));
        u8(static_cast<uint8_t>(e.getPreferredDirection()));u8(0);
    }
    for(uint8_t i=0;i<holes.getCount();++i){const Hole &h=holes[i];u8(h.x);u8(h.y);u16(h.countDown);}
    for(uint8_t y=0;y<16;++y)for(uint8_t x=0;x<28;++x)u8(static_cast<uint8_t>(level.getLevelData(x,y)));
    return static_cast<size_t>(p-out);
}
