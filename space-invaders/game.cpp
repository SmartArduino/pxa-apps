/* SPDX-License-Identifier: MIT. Original game by Teo Taylor; PXA continuation adapter. */
#include "backend.h"
#include "continuation.h"
#include <Arduboy2.h>
#include <ArduboyTones.h>
#include <EEPROM.h>
#include <string.h>
#include "port_support.h"
byte LightsOn{};
byte MoveNow = 0;
byte SetLevel = 1;
byte SetGameType = 1;
byte SetType1 = 3;
byte SetType2 = 2;
byte SetType3 = 1;
boolean AdaptedPowerUp[6]{};
unsigned char AutoBackForth = 1;
byte AutoInterval = 20;
byte Repeat1 = 0;
byte Repeat2 = 0;
unsigned char UFOHoverMovement = 1;
byte UFOHIT = 0;
byte UFODefeat = 0;
byte UFOShoot = 250;
byte UFOGlideTo = 20;
void ResetVariables();
port::Task setup();
port::Task loop();
port::Task SpawnEnemy(byte PlaceX, byte PlaceY, byte Type, boolean IsThereDelay);
void SpawnLaser(byte Xposition, byte Yposition, boolean BombOrNot);
void SlowControl();
void EnemysLaser();
void MoveEnemy(byte EnemyID);
port::Task RunEnemyCode();
void ClearEnemiesOnScreen();
port::Task NewLevel(boolean LoadFromSave);
byte RandomAlienType();
void Explosion(byte xpos);
void DrawBackground();
port::Task BuildBarriers();
byte WithinBarrier(byte Xpos);
void DestroyBarrier(byte Xposi);
void DrawBarriers();
port::Task RunBonusAlienCode();
void IOIndicator(byte pos, boolean OnOff);
port::Task PauseGame();
void DisplayValue(byte YPOS, byte inputValue);
port::Task PrepareNewGame();
port::Task DisplayInfo(boolean BlackWhite);
void RunParticleCode();
void SpawnParticle(byte tYpe, byte Xpos, byte Ypos, unsigned char MoveX);
void Blast(byte Xpos, byte Ypos);
void SpawnPlayerLasers(byte Xposit);
void DeletePlayerLaser(byte PlayerLaserID);
void EndGame();
port::Task SpawnLaserIfCommanded();
port::Task RunPlayerCode();
void InvulSparkle();
void LINE(byte LStartX, byte LStartY, byte LEndX, byte LEndY, byte COLOUR);
void RECT(byte RStartX, byte RStartY, byte REndX, byte REndY, byte BW);
void SOLIDRECT(int RStartX, int RStartY, int REndX, int REndY, byte BW);
void TEXT(String Text, byte TXpos, byte TYpos, byte Tsize, boolean BW);
void RenderPowerUp(int PXpos, byte PYpos, byte PBW);
void Render(byte Xpos, byte Ypos, byte type, byte BW);
void RenderEnemy(byte EnemyID, byte WB, boolean mode);
void SaveProgress();
void EraseSaveProgress();
boolean MELODY(String Original, byte Duration);
port::Task GameOverMelody();
unsigned int ReturnTone(char InputChar);
void SOUND(byte Type, unsigned int Frequency, byte Duration);
void PlayMusic(byte track);
port::Task UFOAppear();
port::Task RunUFOCode();
#include "vendor/platform/PlanetaryIntruders.ino"
#include "vendor/platform/Aliens.ino"
#include "vendor/platform/BackGround.ino"
#include "vendor/platform/Barriers.ino"
#include "vendor/platform/BonusAlien.ino"
#include "vendor/platform/MenuInfoDisplay.ino"
#include "vendor/platform/Particles.ino"
#include "vendor/platform/Player.ino"
#include "vendor/platform/Render.ino"
#include "vendor/platform/SaveData.ino"
#include "vendor/platform/Sound.ino"
#include "vendor/platform/UFO.ino"

static port::Task root_task;
static port::Task run_game(){co_await setup();for(;;)co_await loop();}
static int previous_menu=-1,previous_type=-1,previous_level=-1,previous_lives=-1;
static int32_t previous_score=-1;
static uint8_t previous_buttons=255;
static void game_log(const char* action){
 port_log(action, Menu, GameType, TitleScreenMove, Level, Lives, Score, PlayerX, EnemyCount, port::wait_line);
}
extern "C" void arduboy_game_boot(){
 root_task=port::Task{};port::leaf={};
 if(port::active){port::fail();return;}
 port::wait_line=0;port::transitions=0;
UFOPoints = 0;
UFOHP = 0;
UFOX = 0;
UFOY = 0;
UFOHasBombs = 0;
UFOAliensAppear = 0;
AudioOn = 1;
MusicOn = 1;
IsItCustomGame = 0;
CustomGameFinish = 0;
AnimationTick = 0;
memset(BarrierHP,0,sizeof(BarrierHP));
GameType = 10;
memset(EnemyX,0,sizeof(EnemyX));
memset(EnemyY,0,sizeof(EnemyY));
memset(EnemyDirection,0,sizeof(EnemyDirection));
memset(EnemyType,0,sizeof(EnemyType));
memset(EnemyMoveInterval,0,sizeof(EnemyMoveInterval));
memset(EnemyShootInterval,0,sizeof(EnemyShootInterval));
MoveSlow = 0;
AppearingEnemyType = 0;
AppearingEnemyX = 0;
AppearingEnemyY = 0;
IncomingLeftOrRight = 1;
IncomingY = 20;
memset(LaserX,0,sizeof(LaserX));
memset(LaserY,0,sizeof(LaserY));
memset(LaserBomb,0,sizeof(LaserBomb));
Death = 0;
Invulnerable = 0;
MusicPlace = 0;
MusicInterval = 5;
Flash = 0;
MegaShot = 0;
MegaShotCharge = 0;
Freeze = 0;
BonusAlienX = 0;
PowerUpX = 0;
PowerUpY = 0;
PowerUpType = 1;
BonusAlienInterval = 120;
InvulBonus = 1;
BonusTwoHits = 0;
StartMove = 0;
EnemyCount = 0;
Level = 1;
CurrentEnemies = 0;
EnemiesToSpawn = 0;
Lives = 3;
Score = 0;
memset(PlayerLaserX,0,sizeof(PlayerLaserX));
memset(PlayerLaserY,0,sizeof(PlayerLaserY));
PlayerX = 60;
ShootDelay = 0;
MainScreenMove = 0;
TitleScreenMove = 1;
BackgroundOffset = 0;
Menu = 1;
Select = 0;
ButtonHold = 0;
ShowBackground = 1;
memset(NewType,0,sizeof(NewType));
ExplosionSound = 0;
memset(ParticleX,0,sizeof(ParticleX));
memset(ParticleY,0,sizeof(ParticleY));
memset(ParticleMoveX,0,sizeof(ParticleMoveX));
memset(ParticleMoveY,0,sizeof(ParticleMoveY));
memset(ParticleType,0,sizeof(ParticleType));
LightsOn = 0;
MoveNow = 0;
SetLevel = 1;
SetGameType = 1;
SetType1 = 3;
SetType2 = 2;
SetType3 = 1;
memset(AdaptedPowerUp,0,sizeof(AdaptedPowerUp));
AutoBackForth = 1;
AutoInterval = 20;
Repeat1 = 0;
Repeat2 = 0;
UFOHoverMovement = 1;
UFOHIT = 0;
UFODefeat = 0;
UFOShoot = 250;
UFOGlideTo = 20;
 memset(ShowScore,0,sizeof(ShowScore));musicAndSound.noTone();
 previous_menu=previous_type=previous_level=previous_lives=-1;previous_score=-1;previous_buttons=255;
 root_task=run_game();port::leaf=root_task.handle;
 game_log("boot");
}
extern "C" void arduboy_game_step(){
 port::transitions=0;
 if(port::leaf&&!arduboy_pxa_state.fault)port::leaf.resume();
 if(port::transitions>port::max_transitions)port::max_transitions=port::transitions;
 uint8_t buttons=arduboy_pxa_state.applied;
 if(Menu!=previous_menu||GameType!=previous_type||Level!=previous_level||Lives!=previous_lives||Score!=previous_score||buttons!=previous_buttons){
  game_log("step");previous_menu=Menu;previous_type=GameType;previous_level=Level;previous_lives=Lives;previous_score=Score;previous_buttons=buttons;
 }
}
extern "C" const char* arduboy_game_name(){return "Planetary Intruders";}
extern "C" size_t arduboy_game_pack(uint8_t*out,size_t capacity){
 PortWriter writer(out,capacity);
writer.u8(UFOPoints);
writer.u16(UFOHP);
writer.u8(UFOX);
writer.u8(UFOY);
writer.u8(UFOHasBombs);
writer.u8(UFOAliensAppear);
writer.u8(AudioOn);
writer.u8(MusicOn);
writer.u8(IsItCustomGame);
writer.u8(CustomGameFinish);
writer.u8(AnimationTick);
for(auto value:BarrierHP) writer.u8(value);
writer.u8(GameType);
for(auto value:EnemyX) writer.u8(value);
for(auto value:EnemyY) writer.u8(value);
for(auto value:EnemyDirection) writer.u8(value);
for(auto value:EnemyType) writer.u8(value);
for(auto value:EnemyMoveInterval) writer.u8(value);
for(auto value:EnemyShootInterval) writer.u8(value);
writer.u8(MoveSlow);
writer.u8(AppearingEnemyType);
writer.u16(AppearingEnemyX);
writer.u16(AppearingEnemyY);
writer.u8(IncomingLeftOrRight);
writer.u8(IncomingY);
for(auto value:LaserX) writer.u8(value);
for(auto value:LaserY) writer.u8(value);
for(auto value:LaserBomb) writer.u8(value);
writer.u8(Death);
writer.u8(Invulnerable);
writer.u8(MusicPlace);
writer.u8(MusicInterval);
writer.u8(Flash);
writer.u8(MegaShot);
writer.u8(MegaShotCharge);
writer.u8(Freeze);
writer.u8(BonusAlienX);
writer.u8(PowerUpX);
writer.u8(PowerUpY);
writer.u8(PowerUpType);
writer.u16(BonusAlienInterval);
writer.u8(InvulBonus);
writer.u8(BonusTwoHits);
writer.u8(StartMove);
writer.u8(EnemyCount);
writer.u8(Level);
writer.u8(CurrentEnemies);
writer.u8(EnemiesToSpawn);
writer.u8(Lives);
writer.u32(Score);
for(auto value:PlayerLaserX) writer.u8(value);
for(auto value:PlayerLaserY) writer.u8(value);
writer.u8(PlayerX);
writer.u8(ShootDelay);
writer.u8(MainScreenMove);
writer.u8(TitleScreenMove);
writer.u8(BackgroundOffset);
writer.u8(Menu);
writer.u8(Select);
writer.u8(ButtonHold);
writer.u8(ShowBackground);
for(auto value:NewType) writer.u8(value);
writer.u8(ExplosionSound);
for(auto value:ParticleX) writer.u8(value);
for(auto value:ParticleY) writer.u8(value);
for(auto value:ParticleMoveX) writer.u8(value);
for(auto value:ParticleMoveY) writer.u8(value);
for(auto value:ParticleType) writer.u8(value);
writer.u8(LightsOn);
writer.u8(MoveNow);
writer.u8(SetLevel);
writer.u8(SetGameType);
writer.u8(SetType1);
writer.u8(SetType2);
writer.u8(SetType3);
for(auto value:AdaptedPowerUp) writer.u8(value);
writer.u8(AutoBackForth);
writer.u8(AutoInterval);
writer.u8(Repeat1);
writer.u8(Repeat2);
writer.u8(UFOHoverMovement);
writer.u8(UFOHIT);
writer.u8(UFODefeat);
writer.u8(UFOShoot);
writer.u8(UFOGlideTo);
 writer.u16(port::wait_line);writer.u8(port::active);writer.u8(port::high_water);
 return writer.finish();
}
