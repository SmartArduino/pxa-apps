/* SPDX-License-Identifier: MIT.
 * Original ArduDash entry and read-only, padding-free diagnostic state.
 * Original game source remains in vendor/original; two platform changes
 * bound the jump table and resume the sole delay through the event loop.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>
#include <Arduboy2.h>
#include "../common/retro-games/arduboy-batch-20261010/backend.h"

static uint8_t death_continuation;
static void ardudash_mark_death_continuation(void) { death_continuation=1; }
#define ARDUDASH_EVENT_DRIVEN 1
/* Scope the access change to the original Game declaration. It enables
 * read-only diagnostics and the original reset() delay continuation. */
#define private public
#include "vendor/platform/game.h"
#undef private

alignas(Game) static uint8_t game_storage[sizeof(Game)];
static Game *game;
static constexpr size_t GAME_WIRE_BYTES=571;

extern "C" void arduboy_game_boot(void) {
    if(game) game->~Game();
    /* Original global Game has zero-initialised storage before its ctor. */
    memset(game_storage,0,sizeof game_storage);
    death_continuation=0;
    game=new(game_storage) Game(24);
    game->init();
}
extern "C" void arduboy_game_step(void) {
    /* The common scheduler calls this only once the original 250ms delay
     * expires. The next original statement is reset(), followed by return. */
    if(death_continuation) {
        death_continuation=0;
        game->reset();
        return;
    }
    game->run();
}
extern "C" const char *arduboy_game_name(void) { return "ArduDash"; }

static void put8(uint8_t **p,uint8_t v) { *(*p)++=v; }
static void put16(uint8_t **p,uint16_t v) { put8(p,(uint8_t)v);put8(p,(uint8_t)(v>>8)); }
static void put_obstacle(uint8_t **p,const Obstacle &o) {
    put16(p,(uint16_t)o.bounds.x);put16(p,(uint16_t)o.bounds.y);
    put8(p,o.bounds.width);put8(p,o.bounds.height);
    put8(p,(uint8_t)o._type);put8(p,o.anim_idx);put8(p,o.enabled?1:0);
}
extern "C" size_t arduboy_game_pack(uint8_t *out,size_t capacity) {
    if(!out) return GAME_WIRE_BYTES;
    if(capacity<GAME_WIRE_BYTES || !game) return 0;
    uint8_t *p=out;
    put8(&p,game->fps);
    for(const Point &s:game->stars) { put16(&p,(uint16_t)s.x);put16(&p,(uint16_t)s.y); }
    const Player &v=game->player;
    put16(&p,(uint16_t)v.x);put16(&p,(uint16_t)v.y);
    put8(&p,v.width);put8(&p,v.height);put8(&p,v.jump_idx);put8(&p,v.animation_idx);
    put8(&p,v.is_jumping?1:0);put8(&p,v.is_grounded?1:0);put8(&p,v.is_dead?1:0);
    put8(&p,(uint8_t)game->obstacle_under_idx);
    put_obstacle(&p,game->floor);
    put8(&p,(uint8_t)game->last_added_segment_idx);
    put16(&p,(uint16_t)game->init_offset);
    put8(&p,(uint8_t)game->obstacle_pool_free_size);
    put16(&p,game->player_distance);put16(&p,game->max_score);
    put8(&p,(uint8_t)game->load_cnt);
    put16(&p,game->level_cnt);put16(&p,game->new_level_cnt);
    put8(&p,game->do_preload?1:0);put8(&p,game->on_pause?1:0);
    put8(&p,(uint8_t)game->state);
    for(const Obstacle &o:game->obstacle_pool) put_obstacle(&p,o);
    put8(&p,death_continuation);
    return (size_t)(p-out)==GAME_WIRE_BYTES?GAME_WIRE_BYTES:0;
}
