/**
  @file utd.h

  microTD (uTD): SAF edition

  This is a SAF rewrite of a tiny tower defense game originally written for
  Arduboy. It adds some extra things such as color graphics and a new map.

  by drummyfish, 2021
  released under CC0 1.0, public domain

  small game manual:

  tower     $  range speed damage upgrades              notes
  -----------------------------------------------------------------------------
  guard     8  *      **    *     +range,  +speed       Shoots arrows.
  cannon    8  *      *     *     +range,  +damage      Does splash damage.
  ice      17  *      *           +speed,  +range       Slows down enemies.
  electro  30  **     *     **    +damage, shock        Shoots lightning.
  sniper   45  *****  **    **    +speed,  +range       Covers huge range.
  magic    60  *      *     *     +damage, speed aura   Support tower.
  water   100  ***    ***   ****  +range                Can knock enemies back.
  fire    100  ***    **    ****  +range                Does splash damage.

  creep      hp    speed $ notes            attacked by: G  C  I  E  S  M  W  F 
  -----------------------------------------------------------------------------
  spider     *      **   1                               .  .  .  .  .  .  .  .
  lizard     *      ***  1                               .  .  .  .  .  .  .  .
  snake      **     **   1                               .  .  .  .  .  .  .  .
  wolf       ***    **   1 Good against cold.            .  . 50% .  .  .  .  .
  bat        **     ***  1                               .  NO .  .  .  .  .  .    
  ent        ****   *    2                               .  .  .  .  .  .  .  .
  big spider ***    **   2 Spawns 2 small ones on death. .  .  .  .  .  .  .  .
  ghost      ***    **   2                               NO NO .  .  NO .  .  .
  ogre       *****  **   3                               .  .  .  .  .  .  .  .
  dino       *****  ***  3                               .  .  .  .  .  .  .  .
  demon      *****  ***  3 Supposed to make you lose.    NO NO NO NO NO NO .  .

  TODO: procedurally generated maps?
*/

#define SAF_PROGRAM_NAME "microTD"
#define SAF_SETTING_FASTER_1BIT 2
//#define SAF_SETTING_FORCE_1BIT 1
#define SAF_SETTING_POKITTO_SCALE 2

#define UTD_ALTERNATIVE_TILES 0

#include "saf.h"

//#define UTD_MAPS 5
#define UTD_MAX_MAP_SIZE 34 ///< maximum map description size in bytes
#define UTD_MAX_CREEPS 50   ///< maximum number of creeps present on map at once

#define UTD_MAP_WIDTH 16    ///< map width in squares
#define UTD_MAP_HEIGHT 8    ///< map height in squares

#define UTD_MAPS 6          ///< total number of maps

#define UTD_SPLASH_RANGE 12
#define UTD_WAVE_BASE_REWARD 5

// game states:
#define UTD_GAME_STATE_MENU 0
#define UTD_GAME_STATE_PLAYING 1
#define UTD_GAME_STATE_PLAYING_MENU 2
#define UTD_GAME_STATE_PLAYING_WAVE 3
#define UTD_GAME_STATE_CONFIRM_QUIT 4
#define UTD_GAME_STATE_LOST 5

uint8_t UTD_gameState;
uint8_t UTD_mapIndex;
uint8_t UTD_sound;

uint8_t UTD_backColor = SAF_COLOR_WHITE;

uint8_t UTD_menuItem = 0;   ///< currently selected menu item
uint8_t UTD_cameraPos = 0;  ///< horizontal camera position in squares

uint8_t UTD_ramImage[10] = {8,8,0,0,0,0,0,0,0,0}; ///< 8x8 binary image in RAM
uint8_t UTD_ramMask[10] =  {8,8,0,0,0,0,0,0,0,0}; ///< 8x8 binary image mask

uint8_t UTD_cursorPos = 0;  ///< sequential square position of the cursor

int8_t UTD_lives = 0;       ///< player's current lives
uint16_t UTD_money = 0;     ///< player's current money
uint16_t UTD_round = 0;     ///< current round
uint8_t UTD_updateCounter;

/// Instance of a creep on the map.
typedef struct
{
  uint8_t typeFreeze;   ///< lower 4 bits: type, upper 4 bits: freeze counter
  uint8_t healthLives;  ///< lower 6 bits: health, upper 2 bits: lives
  uint8_t pathStart;    ///< offset to path start within level data
  int16_t pathPosition; ///< position in pixels on path (can be negative)
} UTD_Creep;

UTD_Creep UTD_creeps[UTD_MAX_CREEPS]; ///< array of creeps currently on map
uint8_t UTD_creepCount;               ///< current number of creeps on the map

#if SAF_PLATFORM_HARWARD
  #define UTD_PROGMEM const PROGMEM
#else
  #define UTD_PROGMEM static const
#endif

/// array of 8x8 1bit images
UTD_PROGMEM uint8_t UTD_images[] =
{
  #define UTD_IMAGE_CREEPS 0   // creep images, each one followed by mask
  0xff,0xff,0x99,0xc3,0x81,0xc3,0x99,0xff,
  0x00,0x00,0x66,0x3c,0x7e,0x3c,0x66,0x00,
  0xff,0x7f,0x7f,0x31,0x80,0xc3,0x99,0xff,
  0x00,0x80,0x80,0xce,0x7f,0x3c,0x66,0x00,
  0xe3,0xc9,0x83,0x9f,0xc3,0xf9,0x83,0xff,
  0x1c,0x3e,0x7c,0x60,0x3c,0x06,0x7c,0x00,
  0xff,0x79,0x32,0x80,0xc3,0x9b,0xb9,0xff,
  0x00,0x86,0xcf,0x7f,0x3c,0x64,0x46,0x00,
  0xff,0xbd,0x18,0x81,0x81,0xc3,0xdb,0xff,
  0x00,0x42,0xe7,0x7e,0x7e,0x3c,0x24,0x00,
  0xc3,0xc3,0xe7,0x81,0x24,0xe7,0xc3,0x99,
  0x3c,0x3c,0x18,0x7e,0xdb,0x18,0x3c,0x66,
  0xff,0x24,0x81,0xc3,0x00,0xc3,0x81,0x3c,
  0x00,0xdb,0x7e,0x3c,0xff,0x3c,0x7e,0xc3,
  0xe7,0xc3,0x5a,0x00,0x81,0x81,0xa5,0xff,
  0x18,0x3c,0xbd,0xff,0x7e,0x7e,0x5a,0x00,
  0x67,0x67,0x01,0x80,0xc2,0xc3,0xdb,0x99,
  0x98,0x98,0xfe,0x7f,0x3d,0x3c,0x24,0x66,
  0x73,0x71,0x24,0x00,0x83,0xc7,0x93,0xc9,
  0x8c,0x8e,0xdf,0xff,0x7c,0x38,0x6c,0x36,
  0x42,0x00,0xdb,0x81,0x00,0x42,0xc3,0x99,
  0xbd,0xff,0x3c,0x7e,0xff,0xbd,0x3c,0x66,

  #define UTD_IMAGE_TILES 22

#if UTD_ALTERNATIVE_TILES
  0x7f,0xff,0x7f,0xff,0x7f,0xff,0x7f,0xaa,
  0xff,0xc3,0x81,0x81,0x81,0x99,0xbd,0xff,
  0x7f,0xfe,0x7f,0xfe,0x7f,0xfe,0x7f,0xfe,
  0x55,0xff,0x7f,0xff,0x7f,0xff,0x7f,0xfe,
  0x7f,0xff,0x7f,0xff,0x7f,0xff,0x7f,0xfe,
  0xff,0xc3,0x99,0xbd,0xbd,0xbd,0xbd,0xff,
  0x7f,0xfe,0xff,0xfe,0xff,0xfe,0xff,0xaa,
  0x55,0xff,0xff,0xff,0xff,0xff,0xff,0xaa,
  0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xaa,
  0x55,0xfe,0xff,0xfe,0xff,0xfe,0xff,0xfe,
  0x7f,0xfe,0xff,0xfe,0xff,0xfe,0xff,0xfe,
  0x55,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
  0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
#else
  0xef,0xf7,0xeb,0xf5,0xfa,0xff,0xff,0xff,
  0xff,0xc3,0x81,0x81,0x81,0x99,0xbd,0xff,
  0xef,0xf7,0xef,0xf7,0xef,0xf7,0xef,0xf7,
  0xff,0xff,0xff,0xfd,0xfa,0xf5,0xeb,0xf7,
  0xef,0xf7,0xef,0xf5,0xea,0xf7,0xef,0xf7,
  0xff,0xc3,0x99,0xbd,0xbd,0xbd,0xbd,0xff,
  0xef,0xd7,0xaf,0x5f,0xbf,0xff,0xff,0xff,
  0xff,0xff,0xff,0x55,0xaa,0xff,0xff,0xff,
  0xef,0xf7,0xef,0x55,0xaa,0xff,0xff,0xff,
  0xff,0xff,0xff,0x5f,0xaf,0xd7,0xef,0xf7,
  0xef,0xf7,0xef,0x57,0xaf,0xf7,0xef,0xf7,
  0xff,0xff,0xff,0x55,0xaa,0xf7,0xef,0xf7,
  0xef,0xf7,0xef,0x55,0xaa,0xf7,0xef,0xf7,
#endif

  #define UTD_IMAGE_TOWERS (UTD_IMAGE_TILES + 13)
  0xff,0xe7,0xdb,0xbd,0x99,0xdb,0xc3,0xff,
  0xff,0xe7,0xc3,0x81,0x81,0xc3,0xc3,0xff,
  0xff,0x81,0xbd,0xdb,0xdb,0xbd,0x81,0xff,
  0xff,0x81,0x81,0xc3,0xc3,0x81,0x81,0xff,
  0xff,0xe7,0xdb,0xdb,0xbd,0xbd,0x81,0xff,
  0xff,0xe7,0xc3,0xc3,0x81,0x81,0x81,0xff,
  0xff,0xdb,0xa5,0xbd,0xdb,0xbd,0x81,0xff,
  0xff,0xdb,0x81,0x81,0xc3,0x81,0x81,0xff,
  0xff,0x99,0xa5,0xbd,0xdb,0xdb,0xc3,0xff,
  0xff,0x99,0x81,0x81,0xc3,0xc3,0xc3,0xff,
  0xff,0xc3,0xbd,0xdb,0xbd,0xdb,0xc3,0xff,
  0xff,0xc3,0x81,0xc3,0x81,0xc3,0xc3,0xff,

  #define UTD_IMAGE_TOWERS_BIG (UTD_IMAGE_TOWERS + 12)
  0xff,0xfc,0xf9,0xf2,0xf6,0xf3,0xc1,0xcf,
  0xff,0x3f,0x9f,0x4f,0x6f,0xcf,0x83,0xf3,
  0xe1,0xfb,0xfb,0xf1,0xef,0xe5,0xe0,0xff,
  0x87,0xdf,0xdf,0x8f,0xf7,0xa7,0x07,0xff,
  0xff,0xfc,0xf8,0xf1,0xf1,0xf0,0xc0,0xdf,
  0xff,0x3f,0x1f,0x8f,0x8f,0x0f,0x03,0xfb,
  0xe0,0xf8,0xf8,0xf0,0xe0,0xea,0xe0,0xff,
  0x07,0x1f,0x1f,0x0f,0x07,0x57,0x07,0xff,
  0xff,0xf3,0xe5,0xec,0xe6,0xf3,0xfb,0xf9,
  0xff,0xcf,0xa7,0x37,0x67,0xcf,0xdf,0x9f,
  0xf3,0xe1,0xcf,0xe1,0xf7,0xed,0xe0,0xff,
  0xcf,0x87,0xf3,0x87,0xef,0xb7,0x07,0xff,
  0xff,0xf3,0xe1,0xec,0xe6,0xf2,0xf8,0xf8,
  0xff,0xcf,0x87,0x37,0x67,0x4f,0x1f,0x1f,
  0xf0,0xe1,0xdf,0xe1,0xf0,0xea,0xe0,0xff,
  0x0f,0x87,0xfb,0x87,0x0f,0x57,0x07,0xff,

  #define UTD_IMAGE_ICONS (UTD_IMAGE_TOWERS_BIG + 16)
  0xff,0xc3,0x99,0xa5,0xa5,0x99,0xdb,0xff,
  0xff,0x99,0x81,0xa5,0xbd,0x99,0xdb,0xff,
  0xff,0xef,0xc7,0xfd,0xf9,0xfd,0xfd,0xff,
  0xff,0xdf,0x8f,0xf9,0xfd,0xfb,0xf9,0xff,
  0xff,0xff,0x99,0xc3,0xe7,0xc3,0x99,0xff,
  0xff,0xef,0xe7,0xe3,0xe3,0xe7,0xef,0xff,
  0xff,0xc3,0x99,0xad,0xb5,0x99,0xc3,0xff,

  #define UTD_IMAGE_WAVE (UTD_IMAGE_ICONS + 7)
  0xff,0xff,0xc7,0x93,0x39,0x7c,0xff,0xff
};

UTD_PROGMEM uint8_t UTD_logo[59] = // logo image, not 8x8
{
  0x19,0x12,0xfd,0xd5,0xdf,0xfc,0x60,0xc7,0xec,0x10,0x41,0xe3,0x1c,0x71,0xdb,
  0x8e,0x38,0xc7,0x00,0x00,0x17,0x7f,0xff,0xf7,0x40,0x00,0x05,0x4f,0xcf,0xb9,
  0x0d,0xb5,0x56,0x96,0xd8,0x89,0x43,0x6c,0x44,0xa5,0x8e,0x22,0x50,0xdf,0x11,
  0x69,0x3f,0x08,0xe5,0x40,0x00,0x05,0xd5,0x55,0x55,0xf0,0x00,0x01,0xc0
};

// map tile constants:
#define UTD_TILE_NONE         0x0000
#define UTD_TILE_ROAD_U       0x0001 // road tiles can be combine with |
#define UTD_TILE_ROAD_R       0x0002
#define UTD_TILE_ROAD_D       0x0004
#define UTD_TILE_ROAD_L       0x0008
#define UTD_TILE_START        0x0004
#define UTD_TILE_FINISH       0x0008
#define UTD_TILE_ROAD_UD      (UTD_TILE_ROAD_U | UTD_TILE_ROAD_D)
#define UTD_TILE_ROAD_LR      (UTD_TILE_ROAD_L | UTD_TILE_ROAD_R)

/* Tower tile constants also server as tower IDs. The tower constant value
  format is following:

  MSB abcdefgh ijklmnop LSB 

  - a:       always 1
  - bcdefgh: targeted creep ID, only valid if attack progress != 0
  - ij:      upgrade info
  - klm:     attack progress, 0 or 1 = looking for target, 0 = no target
  - nop:     tower type */
#define UTD_TOWER_GUARD       0x8000
#define UTD_TOWER_CANNON      0x8001
#define UTD_TOWER_ICE         0x8002
#define UTD_TOWER_ELECTRO     0x8003
#define UTD_TOWER_SNIPER      0x8004
#define UTD_TOWER_MAGIC       0x8005
#define UTD_TOWER_WATER       0x8006
#define UTD_TOWER_FIRE        0x8007

#define UTD_TOWER_TYPE(v)     (v & 0x8007)
#define UTD_IS_TOWER(v)       (v & 0x8000)
#define UTD_TOWER_IMAGE(v)    (UTD_IMAGE_TOWERS + (v & 0x07) * 2)

#define UTD_TOWER_IS_SMALL(v)              (((v) & 0x8007) < UTD_TOWER_WATER)

#define UTD_TOWER_TARGET(v)                (((v) >> 8) & 0x7f)
#define UTD_TOWER_TARGET_SET(v,x)          (((v) & 0x80ff) | (x) << 8)

#define UTD_TOWER_ATTACK_PROGRESS(v)       (((v) >> 3) & 0x07)
#define UTD_TOWER_ATTACK_PROGRESS_SET(v,x) (((v) & 0xffc7) | ((x) << 3))

#define UTD_TOWERS_SMALL      6 ///< total number of small tower types
#define UTD_TOWERS            (UTD_TOWERS_SMALL + 2)  

// play menu items:
#define UTD_UPGRADE1          0x0080
#define UTD_UPGRADE2          0x0040
#define UTD_PLAY_MENU_SELL    0x000a
#define UTD_PLAY_MENU_GO      0x000b
#define UTD_PLAY_MENU_QUIT    0x000c

#define UTD_UPGRADE_NONE        0x00
#define UTD_UPGRADE_RANGE       0x01
#define UTD_UPGRADE_SPEED       0x02
#define UTD_UPGRADE_DAMAGE      0x03
#define UTD_UPGRADE_SHOCK       0x04
#define UTD_UPGRADE_SPEED_AURA  0x05

#define UTD_PLAY_MENU_ITEMS   13

#define UTD_CREEP_SPIDER      0x00
#define UTD_CREEP_LIZARD      0x01
#define UTD_CREEP_SNAKE       0x02
#define UTD_CREEP_WOLF        0x03 ///< freeze is less effective
#define UTD_CREEP_BAT         0x04 ///< immune to cannon
#define UTD_CREEP_ENT         0x05
#define UTD_CREEP_SPIDER_BIG  0x06 ///< spawns two small spiders when killed
#define UTD_CREEP_GHOST       0x07 ///< immune to physical attack
#define UTD_CREEP_OGRE        0x08
#define UTD_CREEP_DINO        0x09
#define UTD_CREEP_DEMON       0x0a ///< only attackable by fire/water

#define UTD_CREEPS            11   ///< total number of creep types
  
void UTD_loadImage(uint8_t index, uint8_t *dest)
{
  const uint8_t *p = UTD_images + 8 * index;

  dest += 2;

  for (uint8_t i = 0; i < 8; ++i)
  {
#if SAF_PLATFORM_HARWARD
    *dest = pgm_read_byte(p);
#else
    *dest = *p;
#endif
     p++;
     dest++;
  }
}

void UTD_drawLogo(void)
{
#if SAF_PLATFORM_HARWARD
  uint8_t img[sizeof(UTD_logo)];

  for (uint8_t i = 0; i < sizeof(UTD_logo); ++i)
    img[i] = pgm_read_byte(UTD_logo + i);
#else
  const uint8_t *img = UTD_logo;
#endif

  SAF_drawImage1Bit(img,19,6,0,SAF_COLOR_WHITE,
#if SAF_PLATFORM_COLOR_COUNT > 2
  SAF_COLOR_RED_DARK,
#else
  SAF_COLOR_BLACK,
#endif
  0);
}

/** Current map description. The format is following:

  Map specifies the layout of creep paths as a series of bytes. The format is
  following:

  - The map is a sequence of one of more path definitions terminated by a 0
    byte.
  - A path definition starts with starting square coordinates in a single byte,
    lower 4 bits are X, upper 4 bits are Y.
  - Path segments follow, each one as a single byte of two parts: the 4 highest
    bits are the direction (0 = up, 1 = right, 2 = down, 3 = left) and the 4
    lowest bits are a square distance towards the next segment.
  - The path definition is either terminated by a 0 byte (end of map definition)
    or 255 byte (end of path definition, another will follow). */
uint8_t UTD_map[UTD_MAX_MAP_SIZE];

/// 2D array of current map tiles computed from the map description.
uint16_t UTD_mapTiles[UTD_MAP_WIDTH * UTD_MAP_HEIGHT];

uint8_t UTD_minU8(uint8_t a, uint8_t b)
{
  return a < b ? a : b;
}

void UTD_playSound(uint8_t index)
{
  if (UTD_sound)
    SAF_playSound(index);
}

uint8_t UTD_buttonPressedOrHeld(uint8_t key)
{
  uint8_t b = SAF_buttonPressed(key);
  return (b == 1) || (b >= 18);
}

void UTD_squarePositionTo2D(uint8_t squarePosition, int8_t *x, int8_t *y)
{
  *x = (squarePosition % UTD_MAP_WIDTH) * 8 + 4;
  *y = (squarePosition / UTD_MAP_WIDTH) * 8 + 4;
}

void UTD_interpolateCoords(int8_t *x1, int8_t *y1, int8_t x2, int8_t y2,
  uint8_t t, uint8_t tMax)
{
  *x1 += (((int16_t) (x2 - *x1)) * t) / tMax;
  *y1 += (((int16_t) (y2 - *y1)) * t) / tMax;
}

uint8_t UTD_canAttack(uint16_t tower, uint8_t creepType)
{
  tower = UTD_TOWER_TYPE(tower);

  return
    // cannon can't attack flying
    !(tower == UTD_TOWER_CANNON && creepType == UTD_CREEP_BAT) &&
    // physical can't attack ghost
    !(creepType == UTD_CREEP_GHOST && 
      (tower == UTD_TOWER_CANNON ||
       tower == UTD_TOWER_GUARD ||
       tower == UTD_TOWER_SNIPER)) &&
    // demon can only be attacked by water or fire
    !(creepType == UTD_CREEP_DEMON &&
       tower != UTD_TOWER_WATER &&
       tower != UTD_TOWER_FIRE);
}

uint8_t UTD_towerColor(uint16_t towerType)
{
#if SAF_PLATFORM_COLOR_COUNT > 2
  switch (towerType)
  {
    case UTD_TOWER_GUARD: return 0x08; break;
    case UTD_TOWER_CANNON: return 0x20; break;
    case UTD_TOWER_ICE: return 0x01; break;
    case UTD_TOWER_ELECTRO: return 0x21; break;
    case UTD_TOWER_SNIPER: return 0x00; break;
    case UTD_TOWER_MAGIC: return 0x25; break;
    case UTD_TOWER_FIRE: return 0x40; break;
    case UTD_TOWER_WATER: return 0x05; break;
    default: break;
  }
#else
  _SAF_UNUSED(towerType);
#endif
  return SAF_COLOR_BLACK;
}

uint8_t UTD_creepColor(uint8_t creepType)
{
#if SAF_PLATFORM_COLOR_COUNT > 2
  switch (creepType)
  {
    case UTD_CREEP_SPIDER: return 0x20; break;
    case UTD_CREEP_LIZARD: return 0x04; break;
    case UTD_CREEP_SNAKE: return 0x01; break;
    case UTD_CREEP_WOLF: return 0x25; break;
    case UTD_CREEP_BAT: return 0x25; break;
    case UTD_CREEP_ENT: return 0x20; break;
    case UTD_CREEP_SPIDER_BIG: return 0x24; break;
    case UTD_CREEP_GHOST: return 0x25; break;
    case UTD_CREEP_OGRE: return 0x20; break;
    case UTD_CREEP_DINO: return 0x04; break;
    case UTD_CREEP_DEMON: return SAF_COLOR_BLACK; break;
    default: break;
  }
#else
  _SAF_UNUSED(creepType);
#endif
  return SAF_COLOR_BLACK;
}

void UTD_getCreepStats(uint8_t creep, uint8_t *hp, uint8_t *speed, 
  uint8_t *reward)
{
  // creep stats in a function (vs a table) avoids dealing with PROGMEM

  switch (creep)
  {
#define C(c,h,s,r) case c: *hp = h; *speed = s; *reward = r; break;
    C(UTD_CREEP_SPIDER,7,2,1)
    C(UTD_CREEP_LIZARD,8,1,1)
    C(UTD_CREEP_SNAKE,12,2,1)
    C(UTD_CREEP_WOLF,20,2,1)
    C(UTD_CREEP_BAT,13,1,1)
    C(UTD_CREEP_ENT,43,4,2)
    C(UTD_CREEP_SPIDER_BIG,20,2,2)
    C(UTD_CREEP_GHOST,30,3,2)
    C(UTD_CREEP_OGRE,58,2,3)
    C(UTD_CREEP_DINO,63,1,3)
    C(UTD_CREEP_DEMON,63,1,3)
#undef C
    default: break;
  }
}

/** Converts a position along a path to 2D pixel position on the map. Returns
  0 if the position is not withing the path finish (before or after),
  otherwise 1. */
uint8_t UTD_pathPositionTo2D(uint8_t pathStart, int16_t pathPosition, 
  int8_t *x, int8_t *y)
{
  uint8_t *s = UTD_map + pathStart;

  *x = (*s & 0x0f) * 8 + 4;
  *y = (*s >> 4) * 8 + 4;

  s++;

  if (pathPosition < 0)
    return 0;

  while (1) // for each segment
  {
    uint8_t segment = *s;

    if (segment == 0 || segment == 255) // no more segments?
      break;

    int8_t dx = 0, dy = 0;

    switch (segment >> 4)
    {
      case 0: dy = -1; break;
      case 1: dx = 1; break;
      case 2: dy = 1; break;
      case 3: dx = -1; break;
      default: break;
    }

    uint8_t segmentLength = (segment & 0x0f) * 8; 

    if (pathPosition > segmentLength)
    {
      *x += dx * segmentLength;
      *y += dy * segmentLength;
      pathPosition -= segmentLength;
    }
    else
    {
      *x += dx * pathPosition;
      *y += dy * pathPosition; 
      return 1;
    }

    s++;
  }

  return 0;
}

/**
  Decreases creep's lives and potentially sends it back to the start.
*/
void UTD_creepEnds(uint8_t creepIndex)
{
  uint8_t cH = 0, cS = 0, cR = 0;

  UTD_getCreepStats(UTD_creeps[creepIndex].typeFreeze & 0x0f,
    &cH,&cS,&cR);

  UTD_Creep *c = UTD_creeps + creepIndex; 

  uint8_t lives = c->healthLives >> 6;

  uint8_t remove = lives == 0;

  if (remove)
  {
    // remove creep

    for (uint8_t i = creepIndex; i < UTD_creepCount - 1; ++i)
      UTD_creeps[i] = UTD_creeps[i + 1];

    UTD_creepCount--;
  }
  else
  {
    // send creep back to start

    c->healthLives = ((lives - 1) << 6) | cH;
    c->pathPosition = -5;
  }

  uint16_t *tile = UTD_mapTiles;

  // correct tower targets:

  for (uint8_t i = 0; i < UTD_MAP_WIDTH * UTD_MAP_HEIGHT; ++i)
  {
    uint16_t t = *tile;

    if (UTD_IS_TOWER(t))
    {
      uint8_t target = UTD_TOWER_TARGET(t);

      if (target == creepIndex)
        *tile = UTD_TOWER_ATTACK_PROGRESS_SET(t,0); // remove target
      else if (remove && target > creepIndex)
        *tile = UTD_TOWER_TARGET_SET(t,target - 1); // because creeps shifted
    }

    tile++;
  }
}

/**
  Checks a square and if it contains a tower, it progresses the tower's
  attack progress (for speed aura).
*/
void UTD_speedUpTower(uint8_t square)
{
  uint16_t t = UTD_mapTiles[square];

  if (UTD_IS_TOWER(t))
  {
    uint8_t attackProgress = UTD_TOWER_ATTACK_PROGRESS(t);

    if (attackProgress > 1)
      UTD_mapTiles[square] = 
        UTD_TOWER_ATTACK_PROGRESS_SET(t,attackProgress - 1);
  }
}

void UTD_applySpeedAura(uint8_t center)
{
  if (UTD_updateCounter % 8 != center % 8)
    return;

  uint8_t x = center % 8;

  uint8_t
    t = center >= UTD_MAP_WIDTH,
    r = x != UTD_MAP_WIDTH - 1,
    b = center < UTD_MAP_WIDTH * (UTD_MAP_HEIGHT - 1),
    l = x != 0;

  if (t)
  {
    UTD_speedUpTower(center - UTD_MAP_WIDTH);

    if (l)
      UTD_speedUpTower(center - UTD_MAP_WIDTH - 1);

    if (r)
      UTD_speedUpTower(center - UTD_MAP_WIDTH + 1);
  }

  if (l)
    UTD_speedUpTower(center - 1);

  if (r)
    UTD_speedUpTower(center + 1);

  if (b)
  {
    UTD_speedUpTower(center + UTD_MAP_WIDTH);

    if (l)
      UTD_speedUpTower(center + UTD_MAP_WIDTH - 1);

    if (r)
      UTD_speedUpTower(center + UTD_MAP_WIDTH + 1);
  }
}

void UTD_creepFinishes(uint8_t index)
{
  uint8_t cH = 0, cS = 0, cR = 0;

  UTD_getCreepStats(UTD_creeps[index].typeFreeze & 0x0f,
    &cH,&cS,&cR);

  UTD_creepEnds(index);

  UTD_lives -= cR;

  if (UTD_lives <= 0)
    UTD_gameState = UTD_GAME_STATE_LOST;

  UTD_playSound(SAF_SOUND_BOOM);
}

void UTD_getTowerStats(uint16_t tower, uint8_t *range, uint8_t *speed, 
  uint8_t *damage, uint8_t *price, uint8_t *upgrades)
{
  // similarly to creep stats we store tower stats as a function code

#define T(t,r,s,d,p,u1,u2) \
  case t: \
    if (range != 0) *range = r; \
    if (speed != 0) *speed = s; \
    if (damage != 0) *damage = d; \
    if (price != 0) *price = p; \
    if (upgrades != 0) *upgrades = u1 | (u2 << 4); break;

  switch (tower)
  {
    T(UTD_TOWER_GUARD,  30,5,2,8,  UTD_UPGRADE_RANGE,UTD_UPGRADE_SPEED)
    T(UTD_TOWER_CANNON, 27,7,2,8,  UTD_UPGRADE_RANGE,UTD_UPGRADE_DAMAGE)
    T(UTD_TOWER_ICE,    26,6,0,17, UTD_UPGRADE_SPEED,UTD_UPGRADE_RANGE)
    T(UTD_TOWER_ELECTRO,35,7,4,30, UTD_UPGRADE_DAMAGE,UTD_UPGRADE_SHOCK)
    T(UTD_TOWER_SNIPER, 60,4,3,45, UTD_UPGRADE_SPEED,UTD_UPGRADE_RANGE)
    T(UTD_TOWER_MAGIC,  25,7,2,60, UTD_UPGRADE_DAMAGE,UTD_UPGRADE_SPEED_AURA)
    T(UTD_TOWER_WATER,  40,3,7,100,UTD_UPGRADE_RANGE,UTD_UPGRADE_NONE)
    T(UTD_TOWER_FIRE,   38,5,8,100,UTD_UPGRADE_RANGE,UTD_UPGRADE_NONE)
    default: break;
  }

#undef T
}

uint8_t UTD_getUpgradeCost(uint16_t tower)
{
  uint8_t tP = 0;
  UTD_getTowerStats(UTD_TOWER_TYPE(tower),0,0,0,&tP,0);
  return (tP * 2) / 3;
}

void UTD_getTowerInstanceStats(uint16_t towerInstance, uint8_t *range,
  uint8_t *damage, uint8_t *speed)
{
  uint8_t upgrades = 0;

  UTD_getTowerStats(UTD_TOWER_TYPE(towerInstance),
    range,speed,damage,0,&upgrades);

  for (uint8_t i = 0; i < 2; ++i)
    if (towerInstance & (i == 0 ? UTD_UPGRADE1 : UTD_UPGRADE2))
      switch (i == 0 ? (upgrades & 0x0f) : (upgrades >> 4))
      {
        case UTD_UPGRADE_RANGE: if (range != 0) *range += *range / 4; break;
        case UTD_UPGRADE_DAMAGE: if (damage != 0) *damage += *damage / 2; break;
        case UTD_UPGRADE_SPEED: if (speed != 0) *speed -= 2; break; 
        default: break;
      }
}

/** Gets the play menu item constant as a return value, its image in the
  pointer parameter (if non-0) and its name in a pointer parameter (if non-0,
  needs to be at least 13 bytes). */
uint16_t UTD_getPlayMenuItem(uint8_t index, uint16_t *imageIndex, char *name)
{
  uint16_t result = index;

  if (index <= 7) // small tower?
    result = (0x8000 | index);
  else if (index == 8)
    result = UTD_UPGRADE1;
  else if (index == 9)
    result = UTD_UPGRADE2;

  if (imageIndex != 0)
    *imageIndex = index <= 5 ?
      (UTD_IMAGE_TOWERS + index * 2 + 1) :
      (UTD_IMAGE_ICONS + index - 6);

  if (name != 0)
  {
    uint8_t end = 0;
    uint8_t price = 0;

    if (UTD_IS_TOWER(result))
      UTD_getTowerStats(result,0,0,0,&price,0);

    switch (result)
    {
#define C(n,c) name[n] = c;
      case UTD_TOWER_GUARD: 
        C(0,'G') C(1,'u') C(2,'a') C(3,'r') C(4,'d') end = 5; break;
      case UTD_TOWER_CANNON: 
        C(0,'C') C(1,'a') C(2,'n') C(3,'n') C(4,'o') C(5,'n') end = 6; break;
      case UTD_TOWER_ICE: 
        C(0,'I') C(1,'c') C(2,'e') end = 3; break;
      case UTD_TOWER_MAGIC: 
        C(0,'M') C(1,'a') C(2,'g') C(3,'i') C(4,'c') end = 5; break;
      case UTD_TOWER_SNIPER: 
        C(0,'S') C(1,'n') C(2,'i') C(3,'p') C(4,'e') C(5,'r') end = 6; break;
      case UTD_TOWER_ELECTRO: 
        C(0,'E') C(1,'l') C(2,'e') C(3,'c') C(4,'t') C(5,'r') end = 6; break;
      case UTD_TOWER_WATER: 
        C(0,'W') C(1,'A') C(2,'T') C(3,'E') C(4,'R') end = 5; break;
      case UTD_TOWER_FIRE: 
        C(0,'F') C(1,'I') C(2,'R') C(3,'E') end = 4; break;
      case UTD_PLAY_MENU_GO: 
        C(0,'G') C(1,'o') C(2,'!') end = 3; break;
      case UTD_PLAY_MENU_SELL: 
        C(0,'S') C(1,'e') C(2,'l') C(3,'l') end = 4; break;
      case UTD_PLAY_MENU_QUIT: 
        C(0,'Q') C(1,'u') C(2,'i') C(3,'t') end = 4; break;

      case UTD_UPGRADE1: 
      case UTD_UPGRADE2:
      {
        uint8_t upgradeType = 0;

        uint16_t t = UTD_TOWER_TYPE(UTD_mapTiles[UTD_cursorPos]);

        price = UTD_getUpgradeCost(t);

        UTD_getTowerStats(t,0,0,0,0,&upgradeType);

        upgradeType = result == UTD_UPGRADE1 ? 
          (upgradeType & 0x0f) : (upgradeType >> 4);

        switch (upgradeType)
        {
          case UTD_UPGRADE_RANGE:
            C(0,'+') C(1,'R') C(2,'N') C(3,'G') end = 4; break;
          case UTD_UPGRADE_DAMAGE:
            C(0,'+') C(1,'D') C(2,'M') C(3,'G') end = 4; break;
          case UTD_UPGRADE_SPEED:
            C(0,'+') C(1,'S') C(2,'P') C(3,'D') end = 4; break;
          case UTD_UPGRADE_SHOCK:
            C(0,'+') C(1,'S') C(2,'H') C(3,'O') C(4,'K') end = 5; break;
          case UTD_UPGRADE_SPEED_AURA:
            C(0,'+') C(1,'A') C(2,'U') C(3,'R') C(4,'A') end = 5; break;
          default: *name = 0; price = 0; break;
        }
 
        break;
      }

      default: *name = 0; break;
#undef CH
    }

    name[end] = 0;

    if (price != 0)
    {
      name += end;

      *name = ' ';
      name++;
      *name = '$';
      name++;

      SAF_intToStr(price,name);
    }
  }

  return result;
}

uint8_t UTD_playMenuItemAvailable(uint16_t item) //  uint8_t index)
{
  uint16_t tile = UTD_mapTiles[UTD_cursorPos];

  if (UTD_IS_TOWER(item))
  {
    if (tile != UTD_TILE_NONE)
      return 0; // can't build on non-empty tile

    uint8_t price;

    UTD_getTowerStats(UTD_TOWER_TYPE(item),0,0,0,&price,0);

    if (price > UTD_money)
      return 0;

    if (
      (UTD_cursorPos >= UTD_MAP_WIDTH) &&
      (UTD_cursorPos % UTD_MAP_WIDTH > 0))
    {
      // big tower blocks the space?
 
      uint16_t t = UTD_mapTiles[UTD_cursorPos - 1];
      if (UTD_IS_TOWER(t) && !UTD_TOWER_IS_SMALL(t)) return 0;
      
      t = UTD_mapTiles[UTD_cursorPos - UTD_MAP_WIDTH];
      if (UTD_IS_TOWER(t) && !UTD_TOWER_IS_SMALL(t)) return 0;
      
      t = UTD_mapTiles[UTD_cursorPos - UTD_MAP_WIDTH - 1];
      if (UTD_IS_TOWER(t) && !UTD_TOWER_IS_SMALL(t)) return 0;
    }

    if (!UTD_TOWER_IS_SMALL(item))
    {
      if ( // big towers need 2x2 squares space
        (UTD_cursorPos % UTD_MAP_WIDTH == UTD_MAP_WIDTH - 1) ||
        (UTD_cursorPos >= (UTD_MAP_HEIGHT - 1) * UTD_MAP_WIDTH) ||
        (UTD_mapTiles[UTD_cursorPos + 1] != UTD_TILE_NONE) ||
        (UTD_mapTiles[UTD_cursorPos + UTD_MAP_WIDTH] != UTD_TILE_NONE) ||
        (UTD_mapTiles[UTD_cursorPos + UTD_MAP_WIDTH + 1] != UTD_TILE_NONE)
        )
        return 0;
    }
  }
  else if (item == UTD_UPGRADE1)
    return (UTD_IS_TOWER(tile) != 0) && ((tile & UTD_UPGRADE1) == 0) &&
      (UTD_getUpgradeCost(UTD_TOWER_TYPE(tile)) <= UTD_money);
  else if (item == UTD_UPGRADE2)
    return UTD_IS_TOWER(tile) && UTD_TOWER_IS_SMALL(tile) &&
      ((tile & UTD_UPGRADE2) == 0) && 
      (UTD_getUpgradeCost(UTD_TOWER_TYPE(tile)) <= UTD_money);
  else if (item == UTD_PLAY_MENU_SELL) // sell
    return (UTD_IS_TOWER(tile) != 0);

  return 1;
}

void UTD_addCreep(uint8_t creep, uint8_t pathIndex, int16_t position, 
  uint8_t lives)
{
  if (UTD_creepCount >= UTD_MAX_CREEPS)
    return;

  UTD_Creep *c = UTD_creeps + UTD_creepCount;

  uint8_t cH = 0, cS = 0, cR = 0;

  UTD_getCreepStats(creep,&cH,&cS,&cR);

  c->typeFreeze = creep;
  c->healthLives = (lives << 6) | cH;
  c->pathPosition = position;

  c->pathStart = 0;

  while (pathIndex > 0)
  {
    while (UTD_map[c->pathStart] != 0 && UTD_map[c->pathStart] != 255)
      c->pathStart++;

    if (UTD_map[c->pathStart] == 255)
      c->pathStart++;
    else
      c->pathStart = 0;

    pathIndex--;
  }

  UTD_creepCount++;
}

uint8_t UTD_cycleCreeps(uint8_t index, uint8_t levelFrom, uint8_t levelTo)
{
  if (levelFrom >= UTD_CREEPS)
    levelFrom = UTD_CREEPS - 1;

  if (levelTo >= UTD_CREEPS)
    levelTo = UTD_CREEPS - 1;

  return levelFrom + (index % (levelTo - levelFrom + 1));
}

void UTD_cyclingSpawn(
    uint8_t total,           // total number of creeps to spawn
    uint8_t levelFrom,       // minimum level of a creep
    uint8_t levelTo,         // maximum level of a creep
    uint8_t maxLives,        // maximum number of lives for a creep
    uint8_t positionOffset,  // gaps between creeps
    uint8_t groupSize        // affects additional gaps and creep lives
    )
{
  total = UTD_minU8(UTD_MAX_CREEPS,total);
  int16_t position = -1 * positionOffset;

  for (uint8_t i = 0; i < total; ++i)
  {
    if (i % groupSize == 0)
      position -= positionOffset;

    UTD_addCreep(
      UTD_cycleCreeps(i,levelFrom,levelTo),
      i,position,i % groupSize == 0 ? UTD_minU8(3,maxLives) : 0);

    position -= positionOffset;
  } 
}

void UTD_goToMenu(void)
{
  UTD_gameState = UTD_GAME_STATE_MENU;
  UTD_menuItem = 0;
  UTD_backColor = 0xff;
}

void UTD_spawnCreeps(void)
{
  UTD_creepCount = 0;

  switch (UTD_mapIndex)
  {
    case 0:
    case 1:
    {
      UTD_cyclingSpawn(1 + UTD_round,UTD_round / 4,
        UTD_round < 15 ? UTD_minU8(UTD_CREEP_DINO - 1,UTD_round) : UTD_round,
        UTD_round / 5,10,5);
      break;
    }

    case 2:
    {
      uint8_t total = UTD_minU8(1 + UTD_round,(UTD_MAX_CREEPS * 2) / 3);

      int16_t pos = -10;

      for (uint8_t i = 0; i < total; ++i)
      {
        uint8_t c = UTD_cycleCreeps(i,UTD_round / 3,(UTD_round * 2) / 3);
        uint8_t l = i % 3 == 0 ? UTD_minU8(3,UTD_round / 7) : 0;

        UTD_addCreep(c,0,pos,l);

        pos -= 10;

        if (i % 2 == 0)
          UTD_addCreep(c,1,pos,l);
      }

      break; 
    }

    case 3:
      UTD_cyclingSpawn(3 + UTD_round *  2,UTD_round / 3,(UTD_round * 4) / 5,
        UTD_round / 8,7,4);
      break;

    case 4:
      UTD_cyclingSpawn(8 + UTD_round *  2,UTD_round / 3,2 + (UTD_round * 4) / 5,
        UTD_round / 8,10,4);
      break;

    case 5:
    {
      UTD_cyclingSpawn((1 + UTD_round / 2) * 2,1 + UTD_round / 3,
        1 + UTD_round,UTD_round / 8,15,5);

      UTD_cyclingSpawn(UTD_round * 2,UTD_round / 4,UTD_round / 2,
        UTD_round / 8,8,4);
      break;
    }
  }
}

void UTD_loadMap(uint8_t index)
{
  for (uint8_t i = 0; i < UTD_MAX_MAP_SIZE; ++i)
    UTD_map[i] = 0;

  UTD_menuItem = 0;
  UTD_cursorPos = UTD_MAP_WIDTH + 1;
  UTD_cameraPos = 0;
  UTD_mapIndex = index;
  UTD_creepCount = 0;
  UTD_round = 0;
  UTD_gameState = UTD_GAME_STATE_PLAYING;

  switch (index)
  {
    // by initializing maps in code we also avoid dealing with PROGMEM

    case 0:
      UTD_map[0] = 0x11;
      UTD_map[1] = 0x25;
      UTD_map[2] = 0x12;
      UTD_map[3] = 0x05;
      UTD_map[4] = 0x18;
      UTD_map[5] = 0x22;
      UTD_map[6] = 0x35;
      UTD_map[7] = 0x23;
      UTD_map[8] = 0x17;
      UTD_map[9] = 0x05;

      UTD_lives = 20;
      UTD_money = 10;
      UTD_backColor = 0xfe;
      break;

    case 1:
      UTD_map[0] =  0x11; 
      UTD_map[1] =  0x22; 
      UTD_map[2] =  0x17; 
      UTD_map[3] =  0x21; 
      UTD_map[4] =  0x12; 
      UTD_map[5] =  0x22; 
      UTD_map[6] =  0x14; 
      UTD_map[7] =  0x03; 
      UTD_map[8] =  0x32; 
      UTD_map[9] =  0x02; 
      UTD_map[10] = 0x34;
      UTD_map[11] = 0xff;

      UTD_map[12] = 0x61;
      UTD_map[13] = 0x03;
      UTD_map[14] = 0x13;
      UTD_map[15] = 0x22;
      UTD_map[16] = 0x11;
      UTD_map[17] = 0x21;
      UTD_map[18] = 0x19;
      UTD_map[19] = 0x03; 
      UTD_map[20] = 0x32; 
      UTD_map[21] = 0x02; 
      UTD_map[22] = 0x34;

      UTD_lives = 25;
      UTD_money = 10;
      UTD_backColor = 0xde;
      break;

    case 2:
      UTD_map[0] =  0x11; 
      UTD_map[1] =  0x25; 
      UTD_map[2] =  0x12; 
      UTD_map[3] =  0x02; 
      UTD_map[4] =  0x12; 
      UTD_map[5] =  0x22; 
      UTD_map[6] =  0x13; 
      UTD_map[7] =  0x03; 
      UTD_map[8] =  0x32; 
      UTD_map[9] =  0x02; 
      UTD_map[10] = 0x33; 
      UTD_map[11] = 0xff;

      UTD_map[12] = 0x1e; 
      UTD_map[13] = 0x34; 
      UTD_map[14] = 0x25; 
      UTD_map[15] = 0x12; 
      UTD_map[16] = 0x03; 
      UTD_map[17] = 0x12; 
      UTD_map[18] = 0x23; 

      UTD_lives = 20;
      UTD_money = 10;
      UTD_backColor = 0xdf;
      break;
    
    case 3:
      UTD_map[0] =  0x72; 
      UTD_map[1] =  0x02; 
      UTD_map[2] =  0x1c; 
      UTD_map[3] =  0x22; 
      UTD_map[4] =  0x37; 
      UTD_map[5] =  0x06; 
      UTD_map[6] =  0x32; 
      UTD_map[7] =  0x22; 
      UTD_map[8] =  0x34; 
      UTD_map[9] =  0x01; 
      UTD_map[10] = 0x13; 
      UTD_map[11] = 0xff; 

      UTD_map[12] =  0x72; 
      UTD_map[13] =  0x02; 
      UTD_map[14] =  0x1c; 
      UTD_map[15] =  0x22; 
      UTD_map[16] =  0x37; 
      UTD_map[17] =  0x06;
      UTD_map[18] =  0x12;
      UTD_map[19] =  0x22;
      UTD_map[20] =  0x14;
      UTD_map[21] =  0x01;
      UTD_map[22] =  0x33;

      UTD_lives = 25;
      UTD_money = 8;
      UTD_backColor = 0xfb;
      break;

    case 4:
      UTD_map[0] =  0x44; 
      UTD_map[1] =  0x02; 
      UTD_map[2] =  0x33; 
      UTD_map[3] =  0x24; 
      UTD_map[4] =  0x15; 
      UTD_map[5] =  0x05; 
      UTD_map[6] =  0x12;
      UTD_map[7] =  0x25;
      UTD_map[8] =  0x13;
      UTD_map[9] =  0x05;
      UTD_map[10] = 0x13;
      UTD_map[11] = 0xff;

      UTD_map[12] = 0x6d; 
      UTD_map[13] = 0x03; 
      UTD_map[14] = 0x35; 
      UTD_map[15] = 0x23; 
      UTD_map[16] = 0x32; 
      UTD_map[17] = 0x03; 
      UTD_map[18] = 0x31; 
      UTD_map[19] = 0x02; 
      UTD_map[20] = 0x33; 
      UTD_map[21] = 0x23; 

      UTD_lives = 30;
      UTD_money = 22;
      UTD_backColor = 0xbe;
      break;
    
    case 5:
      UTD_map[0] =  0x55; 
      UTD_map[1] =  0x34; 
      UTD_map[2] =  0x04; 
      UTD_map[3] =  0x13; 
      UTD_map[4] =  0x21; 
      UTD_map[5] =  0x15;
      UTD_map[6] =  0x01;
      UTD_map[7] =  0x15;
      UTD_map[8] =  0x23;
      UTD_map[9] =  0x11;
      UTD_map[10] = 0x22;
      UTD_map[11] = 0x36;
      UTD_map[12] = 0x02;
      UTD_map[13] = 0x34;
      UTD_map[14] = 0xff;

      UTD_map[15] =  0x55; 
      UTD_map[16] =  0x11; 
      UTD_map[17] =  0x01; 
      UTD_map[18] =  0x13; 
      UTD_map[19] =  0x22; 
      UTD_map[20] =  0x16; 
      UTD_map[21] =  0x02; 
      UTD_map[22] =  0x31; 
      UTD_map[23] =  0x03; 
      UTD_map[24] =  0x35; 
      UTD_map[25] =  0x21; 
      UTD_map[26] =  0x35; 
      UTD_map[27] =  0x01; 
      UTD_map[28] =  0x33; 
      UTD_map[29] =  0x24; 
      UTD_map[30] =  0x13; 
      UTD_map[31] =  0x01; 
      UTD_map[32] =  0x11; 

      UTD_lives = 1;
      UTD_money = 30;
      UTD_backColor = 0xda;
      break;

    default: break;
  }

  // create the tilemap:

  for (uint8_t i = 0; i < UTD_MAP_WIDTH * UTD_MAP_HEIGHT; ++i)
    UTD_mapTiles[i] = UTD_TILE_NONE;

  uint8_t *b = UTD_map;
  uint8_t end = 0;

  while (!end) // for each path
  {
    uint16_t *currentTile = UTD_mapTiles + 
      (*b >> 4) * UTD_MAP_WIDTH + (*b & 0x0f);

    uint16_t *previousTile = currentTile;
    uint16_t *pathStartTile = currentTile;  

    b++;

    while (1) // for each segment
    {
      uint8_t v = *b;

      b++;

      if (v == 0) // end of map?
      {
        end = 1;
        break;
      }

      if (v == 255) // end of segment?
        break;

      int8_t tileShift = 0;

      uint16_t d1 = UTD_TILE_ROAD_U,
               d2 = UTD_TILE_ROAD_D;

      switch (v >> 4)
      {
        case 0: 
          tileShift = -1 * UTD_MAP_WIDTH; 
          break;

        case 1: 
          tileShift = 1; 
          d1 = UTD_TILE_ROAD_R;
          d2 = UTD_TILE_ROAD_L;  
          break;

        case 2: 
          tileShift = UTD_MAP_WIDTH; 
          d1 = UTD_TILE_ROAD_D;
          d2 = UTD_TILE_ROAD_U;
          break;

        case 3: 
          tileShift = -1;
          d1 = UTD_TILE_ROAD_L;
          d2 = UTD_TILE_ROAD_R;
          break;

        default: break;
      }

      for (uint8_t i = 0; i < (v & 0x0f); ++i)
      {
        previousTile = currentTile;
        currentTile += tileShift;

        *previousTile |= d1;
        *currentTile |= d2;
      }
    }

    *currentTile = UTD_TILE_FINISH; 
    *pathStartTile = UTD_TILE_START;
  }

}

uint8_t UTD_isWithinRange(int8_t x, int8_t y, int8_t x2, int8_t y2,
  uint8_t range)
{
  x = (x >= x2) ? (x - x2) : (x2 - x);

  // first quickly test Chebyshev distance:

  if (x <= range)
  {
    y = (y >= y2) ? (y - y2) : (y2 - y);

    // now test proper Euclidean distance:
    if (y <= range && SAF_sqrt(x * x + y * y) <= range) 
      return 1;
  }
 
  return 0; 
}

#define UTD_DRAW_MODE_WHOLE 0
#define UTD_DRAW_MODE_TOP 1
#define UTD_DRAW_MODE_BOTTOM 2

void UTD_drawImage(uint8_t x, uint8_t y, uint8_t imageIndex, uint8_t color1,
  uint8_t color2, uint8_t transform, uint8_t hasMask, uint8_t mode)
{
  UTD_loadImage(imageIndex,UTD_ramImage);

  const uint8_t *mask = 0;

  if (hasMask)
  {
    UTD_loadImage(imageIndex + 1,UTD_ramMask);
    mask = UTD_ramMask;
  }

  if (mode == UTD_DRAW_MODE_TOP)
  {
    UTD_ramImage[1] = 4;
    hasMask = 0;
  }
  else if (mode == UTD_DRAW_MODE_BOTTOM)
  {
    UTD_ramImage[1] = 4;
    y += 4;
    hasMask = 0;

    for (uint8_t i = 2; i < 6; ++i)
      UTD_ramImage[i] = UTD_ramImage[i + 4];
  }

  SAF_drawImage1Bit(UTD_ramImage,x,y,mask,color1,color2,transform);

  UTD_ramImage[1] = 8;
}

void SAF_init(void)
{
  UTD_sound = 1;
  UTD_goToMenu();
}

void UTD_drawTower(uint16_t tower, uint8_t x, uint8_t y)
{
  uint16_t imageIndex = UTD_TOWER_IMAGE(tower);

uint8_t c = UTD_towerColor(UTD_TOWER_TYPE(tower));


  if (UTD_TOWER_IS_SMALL(tower))
  {

    UTD_drawImage(x,y,imageIndex + (tower & UTD_UPGRADE1 ? 1 : 0),
      UTD_backColor,c,0,0,UTD_DRAW_MODE_TOP);

    UTD_drawImage(x,y,imageIndex + (tower & UTD_UPGRADE2 ? 1 : 0),
      UTD_backColor,c,0,0,UTD_DRAW_MODE_BOTTOM);
  }
  else
  {
    if (UTD_TOWER_TYPE(tower) == UTD_TOWER_FIRE)
      imageIndex = UTD_IMAGE_TOWERS_BIG + 8;

    if (tower & UTD_UPGRADE1)
      imageIndex += 4;

    #define drawPart(px,py) \
      UTD_drawImage(x + px,y + py,imageIndex,UTD_backColor,c,\
        0,0,UTD_DRAW_MODE_WHOLE);\
      imageIndex++;

    drawPart(0,0)
    drawPart(8,0)
    drawPart(0,8)
    drawPart(8,8)

    #undef drawPart
  }
}

void UTD_draw(void)
{
  /* Someone once said: "Do not write functions longer than one screen."
     He was a stupid man. */

  SAF_clearScreen(UTD_backColor);

  switch (UTD_gameState)
  {
    case UTD_GAME_STATE_MENU:
    {
      UTD_drawLogo();

      int8_t pos = 0 - (SAF_frame() % 8);

      for (uint8_t i = 0; i < 9; ++i) // draw the wave
      {
        UTD_drawImage(pos,28,UTD_IMAGE_WAVE,
          SAF_COLOR_WHITE,SAF_COLOR_BLUE_DARK,0,0,0);

        pos += 8;
      }

      SAF_drawRect(0,40,SAF_SCREEN_WIDTH,6,SAF_COLOR_GRAY_DARK,1);

      char t[10] = "MAP x";

      if (UTD_menuItem == UTD_MAPS)
      {
        t[0] = 'S'; t[1] = 'N'; t[2] = 'D'; t[3] = ':';
        t[4] = 'o'; 

        if (UTD_sound)
        {
          t[5] = 'n'; t[6] = 0;
        }
        else
        {
          t[5] = 'f'; t[6] = 'f'; t[7] = 0;
        }
      }
      else
        t[4] = '1' + UTD_menuItem;

      if (UTD_menuItem < UTD_MAPS)
      {
        SAF_drawText(t,18,41,SAF_COLOR_YELLOW,1);

        t[0] = 'b'; t[1] = 'e'; t[2] = 's'; t[3] = 't'; t[4] = ':'; t[5] = ' ';
  
        SAF_intToStr(SAF_load(UTD_menuItem),t + 6);
 
        SAF_drawText(t,1,59,SAF_COLOR_GRAY_DARK,1);
      }
      else
        SAF_drawText(t,15,41,SAF_COLOR_YELLOW,1);

      break;
    }

    case UTD_GAME_STATE_LOST:
    {
      char t[15];
 
      // prevent storing text in RAM on Arduino with this one weird trick
      t[0] = 'L'; t[1] = 'O'; t[2] = 'S'; t[3] = 'T'; t[4] = ' '; t[5] = 'i';
      t[6] = 'n'; t[7] = '\n'; t[8] = ' '; t[9] = 'r'; t[10] = 'o';
      t[11] = 'u'; t[12] = 'n'; t[13] = 'd'; t[14] = 0;
 
      SAF_drawText(t,12,14,SAF_COLOR_BLACK,1);

      SAF_drawText(SAF_intToStr(UTD_round,t),UTD_round < 10 ? 29 : 23,31,
        SAF_COLOR_BLACK,2);

      break;
    }

    case UTD_GAME_STATE_CONFIRM_QUIT:
    case UTD_GAME_STATE_PLAYING:
    case UTD_GAME_STATE_PLAYING_MENU:
    case UTD_GAME_STATE_PLAYING_WAVE:
    {
      uint16_t *tile = UTD_mapTiles + UTD_cameraPos;

      // draw tiles:

      for (uint8_t y = 0; y < 64; y += 8)
      {
        for (uint8_t x = 0; x < 64; x += 8)
        {
          uint16_t v = *tile;

          if (v != UTD_TILE_NONE)
          {
            if (UTD_IS_TOWER(v)) // tower?
              UTD_drawTower(v,x,y);
            else // path
            {
              UTD_drawImage(x,y,UTD_IMAGE_TILES - 3 + v,
                UTD_backColor,(v == UTD_TILE_START || v == UTD_TILE_FINISH) ?
                SAF_COLOR_BLACK : SAF_COLOR_GRAY_DARK,0,0,0);         
            }
          } 

          tile++;
        }

        tile += UTD_MAP_WIDTH - 8;
      }

      UTD_Creep *c = UTD_creeps;
      
      // draw creeps:

      int8_t cameraPixelOffset = UTD_cameraPos * 8,
        xMin = (UTD_cameraPos - 1) * 8;
      uint8_t
        xMax = (UTD_cameraPos + 8) * 8; // this might not fit into signed byte

      for (uint8_t i = 0; i < UTD_creepCount; ++i)
      {
        int8_t x, y;

        if (UTD_pathPositionTo2D(c->pathStart,c->pathPosition,&x,&y) &&
          x >= xMin && x <= xMax)
        {
          uint8_t cType = c->typeFreeze & 0x0f;

          UTD_drawImage(
            x - cameraPixelOffset - 4,
            y - 5 + 
            (((c->pathPosition >> 3) & 0x01) == 
              ((UTD_updateCounter >> 2) & 0x01)),
            UTD_IMAGE_CREEPS + cType * 2,
#if SAF_PLATFORM_COLOR_COUNT > 2
            0x77,
#else
            SAF_COLOR_WHITE,
#endif
            UTD_creepColor(cType),0,1,0);
        }

        c++;
      }

      tile = UTD_mapTiles;
      uint8_t squarePos = 0;

      // draw projectiles:

      for (uint8_t y = 0; y < 64; y += 8)
      {
        for (uint8_t x = 0; x < 128; x += 8)
        {
          uint16_t v = *tile;

          if (UTD_IS_TOWER(v)) // tower?
          {
            uint8_t attackProgress = UTD_TOWER_ATTACK_PROGRESS(v);

            if (attackProgress != 0)
            {
              UTD_Creep c = UTD_creeps[UTD_TOWER_TARGET(v)];

              int8_t cX, cY, tX, tY;

              UTD_pathPositionTo2D(c.pathStart,c.pathPosition,&cX,&cY);
              UTD_squarePositionTo2D(squarePos,&tX,&tY);

              cX -= cameraPixelOffset;
              tX -= cameraPixelOffset;

              if (!UTD_TOWER_IS_SMALL(v))
              {
                tX += 4;
                tY += 4;
              }

              uint8_t speed;

              UTD_getTowerInstanceStats(v,0,0,&speed);

              switch (UTD_TOWER_TYPE(v))
              {
                case UTD_TOWER_GUARD:
                  UTD_interpolateCoords(&cX,&cY,tX,tY,attackProgress,speed);
                  SAF_drawPixel(cX,cY,SAF_COLOR_BLACK);

                  if (cY != tY && (cX - tX) / (cY - tY) != 0)
                    cX++;
                  else
                    cY++;

                  SAF_drawPixel(cX,cY,SAF_COLOR_BLACK);
                  break;

                case UTD_TOWER_CANNON:
                  UTD_interpolateCoords(&cX,&cY,tX,tY,attackProgress,speed);
                  SAF_drawPixel(cX,cY,0x00);
                  SAF_drawPixel(cX + 1,cY,0x64);
                  SAF_drawPixel(cX,cY + 1,0x64);
                  SAF_drawPixel(cX + 1,cY + 1,0x6d);
                  break;

                case UTD_TOWER_ICE:
                  UTD_interpolateCoords(&cX,&cY,tX,tY,attackProgress,speed);
                  SAF_drawPixel(cX,cY,SAF_COLOR_BLUE);
                  cX++; cY--;
                  SAF_drawPixel(cX,cY,SAF_COLOR_BLUE);
                  cY += 2;
                  SAF_drawPixel(cX,cY,SAF_COLOR_BLUE);
                  cX -= 2;
                  SAF_drawPixel(cX,cY,SAF_COLOR_BLUE);
                  cY -= 2;
                  SAF_drawPixel(cX,cY,SAF_COLOR_BLUE);
                  break;

                case UTD_TOWER_MAGIC:
                  UTD_interpolateCoords(&cX,&cY,tX,tY,attackProgress,speed);
                  SAF_drawCircle(cX - 2, cY - 2,2,SAF_frame(),0);
                  break;

                case UTD_TOWER_SNIPER:
                {
                  UTD_interpolateCoords(&cX,&cY,tX,tY,attackProgress,speed);
                  UTD_interpolateCoords(&tX,&tY,cX,cY,SAF_frame() % 8,8);
                  SAF_drawLine(cX,cY,tX,tY,0x62);
                  break;
                }
                  
                case UTD_TOWER_ELECTRO:
                  if (UTD_TOWER_ATTACK_PROGRESS(v) > 2)
                    SAF_drawLine(tX,tY,
                      cX - 1 + SAF_frame() % 3,cY - 1 + SAF_frame() % 4,
                    SAF_frame() % 2 ? 0x02 : 0x5f);
                  break;

                case UTD_TOWER_WATER:
                {
                  UTD_interpolateCoords(&cX,&cY,tX,tY,attackProgress,speed);
                  SAF_drawCircle(cX - 2 + SAF_frame() % 2,cY - 2,3,0x01 + attackProgress / 2,1);
                  break;
                }

                case UTD_TOWER_FIRE:
                {
                  UTD_interpolateCoords(&cX,&cY,tX,tY,attackProgress,speed);
                  uint8_t r = (9 - attackProgress);
                  uint8_t shift = SAF_frame() % 2;

                  for (int8_t dX = cX - r / 2 + shift; 
                    dX < cX - r / 2 + r + shift; ++dX)
                    for (int8_t dY = cY - r / 2; dY < cY - r / 2 + r; ++dY)
                      if (SAF_random() & 0x10)
                        SAF_drawPixel(dX,dY,
#if SAF_PLATFORM_COLOR_COUNT == 2
                        SAF_COLOR_BLACK
#else
                        SAF_COLOR_RED
#endif
                        );

                  break;
                }

                default: break;
              }
            }
          }

          squarePos++;
          tile++;
        }
      }

      uint8_t cursorX = ((UTD_cursorPos - UTD_cameraPos) % UTD_MAP_WIDTH) * 8,
              cursorY = (UTD_cursorPos / UTD_MAP_WIDTH) * 8;

      for (uint8_t i = 0; i < 7; ++i) // cursor
      {
        uint8_t c = (((SAF_frame() >> 3) + i) % 3) ?
          SAF_COLOR_BLACK : UTD_backColor;

        SAF_drawPixel(cursorX + i,cursorY,c);
        SAF_drawPixel(cursorX + 7,cursorY + i,c);
        SAF_drawPixel(cursorX + 7 - i,cursorY + 7,c);
        SAF_drawPixel(cursorX,cursorY + 7 - i,c);
      } 

      // draw health bars:

      if (SAF_buttonPressed(SAF_BUTTON_B))
      {
        c = UTD_creeps;

        for (uint8_t i = 0; i < UTD_creepCount; ++i)
        {
          int8_t x, y;
          
          if (UTD_pathPositionTo2D(c->pathStart,c->pathPosition,&x,&y) &&
            x >= xMin && x <= xMax)
          {
            uint8_t cH,cS,cR;
            UTD_getCreepStats(c->typeFreeze & 0x0f,&cH,&cS,&cR);

            x = x - cameraPixelOffset - 4;
            y -= 7;
            SAF_drawRect(x,y,8,3,SAF_COLOR_BLACK,1);

            x++;
            y++;

            uint16_t hp = c->healthLives & 0x3f;
            hp = (hp * 5) / cH;

            SAF_drawLine(x,y,x + hp,y,SAF_COLOR_RED);
          }

          c++;
        }
      }

      // draw bar and menu:

      uint8_t barY = UTD_cursorPos >= UTD_MAP_WIDTH ? 1 : SAF_SCREEN_HEIGHT - 5;

      if (UTD_gameState == UTD_GAME_STATE_PLAYING_MENU ||
        UTD_gameState == UTD_GAME_STATE_CONFIRM_QUIT)
      {
        barY = 1;

        uint8_t menuY = SAF_SCREEN_HEIGHT - 9;

        uint8_t drawRadius = 0;
        uint8_t centerOffset = 4;
        char itemName[13];

        uint16_t item = UTD_getPlayMenuItem(UTD_menuItem,0,itemName);

        if (UTD_playMenuItemAvailable(item))
        {
          if (UTD_IS_TOWER(item))
          {
            if (SAF_frame() & 0x08)
              UTD_drawTower(item,cursorX,cursorY);

            UTD_getTowerStats(item,&drawRadius,0,0,0,0);

            if (!UTD_TOWER_IS_SMALL(item))
              centerOffset = 8;
          }
          else if (item == UTD_UPGRADE1 || item == UTD_UPGRADE2)
          {
            // preview range upgrade
            UTD_getTowerInstanceStats(UTD_mapTiles[UTD_cursorPos] | item,
            &drawRadius,0,0);
          }
        }

        if (drawRadius == 0)
        {
          uint16_t tile = UTD_mapTiles[UTD_cursorPos];

          if (UTD_IS_TOWER(tile))
          {
            uint8_t tD,tS;
            UTD_getTowerInstanceStats(tile,&drawRadius,&tD,&tS);
          }
        }

        if (drawRadius != 0)
        {
          SAF_drawCircle(cursorX + centerOffset,cursorY + centerOffset,drawRadius,
            SAF_COLOR_BLACK,0);
          
          SAF_drawCircle(cursorX + centerOffset,cursorY + centerOffset,
            SAF_frame() % drawRadius,SAF_COLOR_BLACK,0);
        }

        SAF_drawRect(0,menuY,SAF_SCREEN_WIDTH,
          9,SAF_COLOR_BLACK,1);

        SAF_drawLine(0,menuY,SAF_SCREEN_WIDTH,menuY,SAF_COLOR_BLACK);
 
        uint8_t menuOffset = UTD_menuItem > 3 ? UTD_menuItem - 3 : 0;

        if (menuOffset > 5)
          menuOffset = 5;

        menuY++;

        SAF_drawRect(0,SAF_SCREEN_HEIGHT - 15,SAF_SCREEN_WIDTH,6,
          UTD_backColor,1);
        SAF_drawText(itemName,1,SAF_SCREEN_HEIGHT - 14,SAF_COLOR_BLACK,1);

        for (uint8_t i = 0; i < 8; ++i) // play menu items
        {
          uint16_t icon = 0;

          uint8_t itemIndex = i + menuOffset;

          uint16_t item = UTD_getPlayMenuItem(itemIndex,&icon,0);

          uint8_t c1 = SAF_COLOR_WHITE,
                  c2 = SAF_COLOR_BLACK;

          uint8_t highlightPos = UTD_menuItem - menuOffset;

          if (highlightPos == i)
          {
            c1 = SAF_COLOR_BLUE_DARK;
            c2 = SAF_COLOR_GREEN;
          }

          uint8_t drawX = i * 8;

          UTD_drawImage(drawX,menuY,icon,c1,c2,0,0,0);

          if (!UTD_playMenuItemAvailable(item))
            for (uint8_t y = menuY; y < menuY + 8; ++y)
              for (uint8_t x = drawX; x < drawX + 8; ++x)
                if (x % 2 == y % 2)
                  SAF_drawPixel(x,y,SAF_COLOR_BLACK);
        }
      }
      
      if (UTD_gameState == UTD_GAME_STATE_CONFIRM_QUIT)
      {
        SAF_drawRect(12,10,40,20,SAF_COLOR_BLACK,1);
        SAF_drawRect(13,11,38,18,SAF_COLOR_WHITE,1);
        SAF_drawText("QUIT?",19,18,SAF_COLOR_BLACK,1);
      }

      char text[8] = "$...";

      SAF_intToStr(UTD_money,text + 1);
      SAF_drawText(text,1,barY,0x0c,1);

      SAF_intToStr(UTD_round,text);
      SAF_drawText(text,37,barY,0x25,1);

      SAF_intToStr(UTD_lives,text);
      SAF_drawText(text,SAF_SCREEN_WIDTH - 10,barY,
#if SAF_PLATFORM_COLOR_COUNT == 2
        SAF_COLOR_BLACK,
#else
        0x80,
#endif
        1);

      break;
    }
  }
}

void UTD_towerHits(uint16_t tower, uint8_t isSplash)
{
  uint8_t tD = 0;
  UTD_getTowerInstanceStats(tower,0,&tD,0);

  uint8_t creepIndex = UTD_TOWER_TARGET(tower);
  uint8_t creepLives = UTD_creeps[creepIndex].healthLives & 0x3f;
    
  uint16_t towerType = UTD_TOWER_TYPE(tower);

  if (towerType == UTD_TOWER_ICE ||
   (towerType == UTD_TOWER_ELECTRO && (tower & UTD_UPGRADE2) 
    && (SAF_random() % 4 == 0)))
  {
    // freeze / shock
    UTD_creeps[creepIndex].typeFreeze =
      (UTD_creeps[creepIndex].typeFreeze & 0x0f) | 
        ((UTD_creeps[creepIndex].typeFreeze & 0x0f) == UTD_CREEP_WOLF ? 
        0x70 : 0xf0); // ^ wolves get frozen for shorter time
  }
  else if (towerType == UTD_TOWER_WATER && SAF_random() % 10 == 0)
    UTD_creeps[creepIndex].pathPosition -= 10; // a wave can knock creeps back

  if (isSplash)
  {
    tD /= 2; // splash deals half the damage

    if (tD >= creepLives) // splash can't kill, it would complicate the loop
      tD = creepLives - 1;
  }
  else
  {
    if (towerType == UTD_TOWER_CANNON || towerType == UTD_TOWER_FIRE)
    {
      int8_t x, y; 

      UTD_Creep *c = UTD_creeps + creepIndex;

      UTD_pathPositionTo2D(c->pathStart,c->pathPosition,&x,&y);
  
      c = UTD_creeps;
  
      for (uint8_t i = 0; i < UTD_creepCount; ++i)
      {
        if (i != creepIndex && UTD_canAttack(tower,c->typeFreeze & 0x0f))
        {
          int8_t x2, y2;

          UTD_pathPositionTo2D(c->pathStart,c->pathPosition,&x2,&y2);

          if (UTD_isWithinRange(x,y,x2,y2,UTD_SPLASH_RANGE))
            UTD_towerHits(UTD_TOWER_TARGET_SET(tower,i),1);
        }

        c++;
      }
    }
  }

  if (tD >= creepLives)
  {
    uint8_t c,cH,cS,cR;
    UTD_Creep creep = UTD_creeps[creepIndex];

    c = creep.typeFreeze & 0x0f;
    UTD_getCreepStats(c,&cH,&cS,&cR);

    UTD_money += cR;

    UTD_creepEnds(creepIndex); // creep dies
    UTD_playSound(SAF_SOUND_BUMP);

    if (c == UTD_CREEP_SPIDER_BIG)
    {
      // big spider will spawn two small ones

      UTD_addCreep(UTD_CREEP_SPIDER,creep.pathStart,creep.pathPosition,0);
      UTD_addCreep(UTD_CREEP_SPIDER,creep.pathStart,creep.pathPosition - 5,0);
    }
  }
  else
  {
    creepLives -= tD;

    UTD_creeps[creepIndex].healthLives = 
      (UTD_creeps[creepIndex].healthLives & 0xc0) | creepLives;
  }
}

void UTD_gameStep(void)
{
  switch (UTD_gameState)
  {
    case UTD_GAME_STATE_PLAYING_MENU:
    {
      if (SAF_buttonJustPressed(SAF_BUTTON_B))
      {
        UTD_gameState = UTD_GAME_STATE_PLAYING;
        return;
      }
        
      uint16_t item = UTD_getPlayMenuItem(UTD_menuItem,0,0);
   
      if (SAF_buttonJustPressed(SAF_BUTTON_A) &&
          UTD_playMenuItemAvailable(item))
      {
        if (UTD_IS_TOWER(item))
        {
          // build tower
          UTD_mapTiles[UTD_cursorPos] = item;
         
          uint8_t tP;
          UTD_getTowerStats(item,0,0,0,&tP,0);
 
          UTD_money -= tP;

          UTD_gameState = UTD_GAME_STATE_PLAYING;

          UTD_playSound(SAF_SOUND_CLICK);
        }
        else if (item == UTD_UPGRADE1 || item == UTD_UPGRADE2)
        {
          UTD_mapTiles[UTD_cursorPos] |= item;
          UTD_money -= UTD_getUpgradeCost(UTD_mapTiles[UTD_cursorPos]);
          UTD_gameState = UTD_GAME_STATE_PLAYING;
          UTD_playSound(SAF_SOUND_CLICK);
        }
        else if (item == UTD_PLAY_MENU_SELL)
        {
          uint8_t tP = 0;
          UTD_getTowerStats(UTD_TOWER_TYPE(UTD_mapTiles[UTD_cursorPos]),0,0,0,
            &tP,0);

          UTD_mapTiles[UTD_cursorPos] = UTD_TILE_NONE;

          UTD_money += tP / 2;
          UTD_gameState = UTD_GAME_STATE_PLAYING;
          UTD_playSound(SAF_SOUND_BOOM);
        }
        else if (item == UTD_PLAY_MENU_GO)
        {
          UTD_updateCounter = 0;
          UTD_spawnCreeps();
          UTD_gameState = UTD_GAME_STATE_PLAYING_WAVE;
          UTD_playSound(SAF_SOUND_BEEP);
        }
        else if (item == UTD_PLAY_MENU_QUIT)
        {
          UTD_gameState = UTD_GAME_STATE_CONFIRM_QUIT;
        }
      }
 
      if (UTD_buttonPressedOrHeld(SAF_BUTTON_RIGHT))
        UTD_menuItem = (UTD_menuItem < UTD_PLAY_MENU_ITEMS - 1) ? 
          UTD_menuItem + 1 : 0;
      else if (UTD_buttonPressedOrHeld(SAF_BUTTON_LEFT))
        UTD_menuItem = (UTD_menuItem > 0) ? UTD_menuItem - 1 : 
          UTD_PLAY_MENU_ITEMS - 1;

      break;
    }

    case UTD_GAME_STATE_LOST:
      if (SAF_buttonJustPressed(SAF_BUTTON_A) ||
          SAF_buttonJustPressed(SAF_BUTTON_B))
        UTD_goToMenu();

      break;

    case UTD_GAME_STATE_CONFIRM_QUIT:
      if (SAF_buttonJustPressed(SAF_BUTTON_A))
      {
        UTD_goToMenu();
        UTD_playSound(SAF_SOUND_BUMP);
      }
      else if (SAF_buttonJustPressed(SAF_BUTTON_B))
        UTD_gameState = UTD_GAME_STATE_PLAYING_MENU;

      break;

    case UTD_GAME_STATE_MENU:
    {
      uint8_t click = 1;

      if (UTD_buttonPressedOrHeld(SAF_BUTTON_RIGHT))
        UTD_menuItem = (UTD_menuItem + 1) % (UTD_MAPS + 1);
      else if (UTD_buttonPressedOrHeld(SAF_BUTTON_LEFT))
        UTD_menuItem = UTD_menuItem == 0 ? UTD_MAPS : (UTD_menuItem - 1);
      else if (SAF_buttonJustPressed(SAF_BUTTON_A))
      {
        if (UTD_menuItem < UTD_MAPS)
          UTD_loadMap(UTD_menuItem);
        else
          UTD_sound = !UTD_sound;
      }
      else
        click = 0;

      if (click)
        UTD_playSound(SAF_SOUND_CLICK);

      break;
    }

    case UTD_GAME_STATE_PLAYING_WAVE:
    case UTD_GAME_STATE_PLAYING:
    {
      if (UTD_gameState == UTD_GAME_STATE_PLAYING_WAVE)
      {
        UTD_updateCounter++;

        UTD_Creep *c = UTD_creeps;

        // update creeps:

        for (uint8_t i = 0; i < UTD_creepCount; ++i)
        {
          uint8_t cH, cS, cR;

          UTD_getCreepStats(c->typeFreeze & 0x0f,&cH,&cS,&cR);

          uint8_t freeze = c->typeFreeze >> 4;

          if (freeze != 0)
          {
            cS += 15;
            c->typeFreeze = (c->typeFreeze & 0x0f) | ((freeze - 1) << 4);
          }

          if ((UTD_updateCounter % (cS)) == 0)
            c->pathPosition++;

          int8_t dummy1, dummy2;

          if (c->pathPosition >= 0 && !UTD_pathPositionTo2D(
            c->pathStart,c->pathPosition,&dummy1,&dummy2))
            UTD_creepFinishes(i);

          c++;
        }

        uint16_t *t = UTD_mapTiles;

        // update towers:

        if (UTD_updateCounter % 3 == 0)
          for (uint16_t i = 0; i < UTD_MAP_WIDTH * UTD_MAP_HEIGHT; ++i)
          {
            uint16_t tower = *t;

            if (!UTD_IS_TOWER(tower))
            {
              t++;
              continue;
            }

            uint8_t attackProgress = UTD_TOWER_ATTACK_PROGRESS(tower);

            if (UTD_TOWER_TYPE(tower) == UTD_TOWER_MAGIC &&
               (tower & UTD_UPGRADE2))
              UTD_applySpeedAura(i);

            if (attackProgress == 0)
            {
              // look for a new target:

              uint8_t skipFrozen = UTD_TOWER_TYPE(tower) == UTD_TOWER_ICE;
                // ^ ice tower will always look for a new target to freeze

              int8_t towerX, towerY, radius;

              UTD_squarePositionTo2D(i,&towerX,&towerY);

              if (!UTD_TOWER_IS_SMALL(tower))
              {
                towerX += 4;
                towerY += 4;
              }

              uint8_t tR, tS, tD;
              UTD_getTowerInstanceStats(tower,&tR,&tD,&tS);

              radius = tR;

              UTD_Creep *c = UTD_creeps;

              for (uint8_t j = 0; j < UTD_creepCount; ++j)
              {
                int8_t x, y;

                if (UTD_canAttack(tower,c->typeFreeze & 0x0f) &&
                  UTD_pathPositionTo2D(c->pathStart,c->pathPosition,&x,&y) &&
                  UTD_isWithinRange(x,y,towerX,towerY,radius) &&
                  !(skipFrozen && (c->typeFreeze >> 4 != 0)))
                {
                  tower = UTD_TOWER_TARGET_SET(tower,j);
                  attackProgress = tS;
                  break;
                }

                c++;
              } 
            }
            else
            {
              if (attackProgress == 1) // hit
                UTD_towerHits(tower,0);

              attackProgress--;
            }

            tower = UTD_TOWER_ATTACK_PROGRESS_SET(tower,attackProgress);

            *t = tower;

            t++;
          }

        if (UTD_gameState != UTD_GAME_STATE_LOST && UTD_creepCount == 0)
        {
          UTD_money += UTD_WAVE_BASE_REWARD + UTD_round / 4;
          UTD_round++;
          UTD_gameState = UTD_GAME_STATE_PLAYING;
    
          if (UTD_round > SAF_load(UTD_mapIndex))
            SAF_save(UTD_mapIndex,UTD_round); // save high score
        }
      }
      else if (SAF_buttonJustPressed(SAF_BUTTON_A))
      {
        UTD_menuItem = 0;

        if (UTD_mapTiles[UTD_cursorPos] == UTD_TILE_NONE)
        {
          uint8_t cursorNotTop = UTD_cursorPos >= UTD_MAP_WIDTH,
                cursorNotLeft = UTD_cursorPos % UTD_MAP_WIDTH != 0,
                movedLeft = 0;

          if (cursorNotLeft &&  
              UTD_IS_TOWER(UTD_mapTiles[UTD_cursorPos - 1]) &&
              !UTD_TOWER_IS_SMALL(UTD_mapTiles[UTD_cursorPos - 1]))
          {
            UTD_cursorPos--;
            movedLeft = 1;
          }
          else if (cursorNotTop &&
              UTD_IS_TOWER(UTD_mapTiles[UTD_cursorPos - UTD_MAP_WIDTH]) &&
              !UTD_TOWER_IS_SMALL(UTD_mapTiles[UTD_cursorPos - UTD_MAP_WIDTH]))
          {
            UTD_cursorPos -= UTD_MAP_WIDTH;
          }
          else if (cursorNotLeft && cursorNotTop &&
              UTD_IS_TOWER(UTD_mapTiles[UTD_cursorPos - UTD_MAP_WIDTH - 1]) &&
              !UTD_TOWER_IS_SMALL(UTD_mapTiles[UTD_cursorPos - UTD_MAP_WIDTH - 
              1]))
          {
            UTD_cursorPos -= UTD_MAP_WIDTH + 1;
            movedLeft = 1;
          }

          if (movedLeft && UTD_cameraPos > 0)
            UTD_cameraPos--;
        }

        UTD_gameState = UTD_GAME_STATE_PLAYING_MENU;
        return;
      }

      if (UTD_buttonPressedOrHeld(SAF_BUTTON_RIGHT))
      {
        UTD_cursorPos += (UTD_cursorPos % UTD_MAP_WIDTH) < UTD_MAP_WIDTH - 1 ?
          1 : 0;

        if (SAF_buttonPressed(SAF_BUTTON_B) && UTD_cameraPos < 
          UTD_MAP_WIDTH / 2)
          UTD_cameraPos++;
      }
      else if (UTD_buttonPressedOrHeld(SAF_BUTTON_LEFT))
      {
        UTD_cursorPos -= (UTD_cursorPos % UTD_MAP_WIDTH) > 0 ?
          1 : 0;

        if (SAF_buttonPressed(SAF_BUTTON_B) && UTD_cameraPos > 0)
          UTD_cameraPos--;
      }
      else if (UTD_buttonPressedOrHeld(SAF_BUTTON_UP))
        UTD_cursorPos -= UTD_cursorPos >= UTD_MAP_WIDTH ? UTD_MAP_WIDTH : 0;
      else if (UTD_buttonPressedOrHeld(SAF_BUTTON_DOWN))
        UTD_cursorPos += UTD_cursorPos < (UTD_MAP_HEIGHT - 1) *  UTD_MAP_WIDTH ?
          UTD_MAP_WIDTH : 0;

      uint8_t cursorX = UTD_cursorPos % UTD_MAP_WIDTH;

      if (cursorX >= UTD_cameraPos + 8)
        UTD_cameraPos++; 
      if (cursorX < UTD_cameraPos)
        UTD_cameraPos--; 

      break;
    }

    defualt: break;
  }
}

uint8_t SAF_loop(void)
{
  UTD_gameStep();
  UTD_draw();

  return 1;
}
