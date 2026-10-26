/* SPDX-License-Identifier: MIT.
 * Original Guess Dots engine and full canonical field observation.
 * Upstream bytes are retained in vendor/original; owner platform.patch
 * exposes the process-lifetime flash flag and bounds simultaneous choices.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>
#include <Arduboy2.h>
#include "backend.h"

/* Like the original function static, this survives NEW/RESTART and Game
 * reconstruction. It starts true only once per fresh module image. */
static bool guessdots_title_flash = true;
/* Arduino's original u32 alias is absent from the portable provider. */
using u32 = uint32_t;
#define private public
#include "vendor/platform/game.h"
#undef private

alignas(Game) static uint8_t game_storage[sizeof(Game)];
static Game *game;
static constexpr size_t GAME_WIRE_BYTES = 494;

extern "C" void arduboy_game_boot(void) {
    if(game) game->~Game();
    /* Original sketch defines a global Game, whose storage is zeroed
     * before its default constructor. Avoid an uninitialised temporary. */
    memset(game_storage,0,sizeof game_storage);
    game=new(game_storage) Game();
    /* The original global Game is zero-initialized before its constructor.
     * Placement-new must establish this enum explicitly: byte clearing alone
     * leaves the new uninitialized member indeterminate to an optimizer. */
    game->chosen_difficulty=lvl::Difficulty::EASY;
    game->init();
}
extern "C" void arduboy_game_step(void) { game->run(); }
extern "C" const char *arduboy_game_name(void) { return "Guess Dots"; }

static void put8(uint8_t **p,uint8_t v) { *(*p)++=v; }
static void put16(uint8_t **p,uint16_t v) { put8(p,(uint8_t)v);put8(p,(uint8_t)(v>>8)); }
static void put32(uint8_t **p,uint32_t v) { put16(p,(uint16_t)v);put16(p,(uint16_t)(v>>16)); }

extern "C" size_t arduboy_game_pack(uint8_t *out,size_t capacity) {
    if(!out) return GAME_WIRE_BYTES;
    if(capacity<GAME_WIRE_BYTES || !game) return 0;
    uint8_t *p=out;
    put8(&p,game->fps);put8(&p,game->lm_idx);
    put32(&p,game->level);put32(&p,game->solution);
    put16(&p,(uint16_t)game->player_pos);
    put16(&p,(uint16_t)game->offset_x);put16(&p,(uint16_t)game->offset_y);
    put16(&p,(uint16_t)game->new_offset_x);put16(&p,(uint16_t)game->new_offset_y);
    put8(&p,(uint8_t)game->chosen_difficulty);
    put8(&p,game->adjust_offsets?1:0);put8(&p,game->force_offsets?1:0);put8(&p,game->level_start?1:0);
    put16(&p,game->score);put8(&p,game->score_saved?1:0);put8(&p,(uint8_t)game->state);
    put8(&p,guessdots_title_flash?1:0);
    /* lm is an original non-const global array. Include every capacity
     * field, including inactive entries; encode no padding or pointers. */
    for(const lvl::LevelMetadata &m: lvl::lm) {
        put8(&p,m.use_offsets?1:0);put8(&p,m.tile_size);put8(&p,m.dots_cnt);put8(&p,m.numbers_cnt);
        put32(&p,m.lvl_limit);put8(&p,m.score_value);
        for(const PointS8 &v:m.dots){put8(&p,(uint8_t)v.x);put8(&p,(uint8_t)v.y);}
        for(const PointS8 &v:m.numbers){put8(&p,(uint8_t)v.x);put8(&p,(uint8_t)v.y);}
        for(uint32_t mask:m.cnt_masks) put32(&p,mask);
    }
    return (size_t)(p-out)==GAME_WIRE_BYTES?GAME_WIRE_BYTES:0;
}
