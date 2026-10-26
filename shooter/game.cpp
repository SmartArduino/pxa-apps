/* SPDX-License-Identifier: MIT.
 * Original Shooter entry, game-local zero-interval guard and read-only state.
 * Upstream gameplay, rendering, input order and frame phase remain in the sketch.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <new>
#include <type_traits>
#include <Arduboy2.h>
#include "backend.h"
#include "vendor/platform/tinyfont/Tinyfont.h"

extern Arduboy2 arduboy;
static bool shooterEveryXFrames(long sampled) {
    /* The original argument is uint8_t. Keep that conversion and RNG call,
       then skip only the previously undefined zero-divisor case. */
    const uint8_t frames=static_cast<uint8_t>(sampled);
    return frames!=0 && arduboy.everyXFrames(frames);
}
/* Arduino's sketch preprocessor normally provides these declarations. */
uint8_t findUnusedBullet(void);
void summonExplosion(int16_t x,int16_t y);
void displayScore(void);
#include "vendor/platform/shooter.ino"

extern "C" void arduboy_game_boot(void) {
    /* Reproduce the original cold global initializers before original setup().
       setup() itself does not reset these globals on a second call. */
    gunAngle=0.0f;frame=0;score=0;selectedIndex=0;groundX=0;tankY=32;
    ground[0]=GroundType::Flat;ground[1]=GroundType::Flat;
    ground[2]=GroundType::Hole;ground[3]=GroundType::Flat;ground[4]=GroundType::Flat;
    for(Bullet &item:bullet)item=Bullet{};
    for(Enemy &item:enemy)item=Enemy{};
    for(Explosion &item:explosion)item=Explosion{};
    for(Pothole &item:pothole)item=Pothole{};
    gameState=GameState::Menu;
    /* A statically folded common constructor counter can skip non-folded
       sketch construction on this Host. Copy assignment resets value fields
       but does not install Tinyfont's vptr. Reconstruct the original object
       with exactly its original arguments, also for the existing NEW reset.
       No reference to this object escapes between callbacks; destruction is
       trivial, so same-type placement construction safely replaces it. */
    static_assert(std::is_trivially_destructible<Tinyfont>::value,
                  "Tinyfont has no destructor side effects");
    new (static_cast<void *>(&tinyfont)) Tinyfont(arduboy.sBuffer,Arduboy2::width(),Arduboy2::height());
    setup();
}
extern "C" void arduboy_game_step(void) { loop(); }
extern "C" const char *arduboy_game_name(void) { return "Shooter"; }

static constexpr size_t GAME_WIRE_BYTES=127;
static void put8(uint8_t **p,uint8_t value){*(*p)++=value;}
static void put16(uint8_t **p,uint16_t value){put8(p,(uint8_t)value);put8(p,(uint8_t)(value>>8));}
static void put32(uint8_t **p,uint32_t value){put16(p,(uint16_t)value);put16(p,(uint16_t)(value>>16));}
static void putFloat(uint8_t **p,float value){static_assert(sizeof(float)==4,"32-bit original floats");uint32_t bits;memcpy(&bits,&value,4);put32(p,bits);}
struct TinyfontPxaState {
    static void pack(uint8_t **p,const Tinyfont &font) {
        /* Buffer pointer is structurally fixed to the separately packed sBuffer.
           Serialize all value state explicitly, including untouched constants. */
        put16(p,(uint16_t)font.sWidth);put16(p,(uint16_t)font.sHeight);
        put16(p,(uint16_t)font.cursorX);put16(p,(uint16_t)font.baseX);put16(p,(uint16_t)font.cursorY);
        put8(p,font.textColor);put8(p,font.letterSpacing);put8(p,font.lineHeight);put8(p,font.maskText?1:0);
    }
};
extern "C" size_t arduboy_game_pack(uint8_t *out,size_t capacity) {
    if(!out)return GAME_WIRE_BYTES;
    if(capacity<GAME_WIRE_BYTES)return 0;
    uint8_t *p=out;
    put8(&p,(uint8_t)gameState);putFloat(&p,gunAngle);
    put8(&p,frame);put8(&p,score);put8(&p,selectedIndex);put8(&p,groundX);put8(&p,tankY);
    for(GroundType type:ground)put8(&p,(uint8_t)type);
    for(const Bullet &item:bullet){put16(&p,(uint16_t)item.x);put16(&p,(uint16_t)item.y);put16(&p,(uint16_t)item.lifetime);putFloat(&p,item.initialAngle);put8(&p,item.isOnScreen?1:0);}
    for(const Enemy &item:enemy){put16(&p,(uint16_t)item.x);put16(&p,(uint16_t)item.y);putFloat(&p,item.dx);putFloat(&p,item.dy);put8(&p,item.width);put8(&p,item.height);put8(&p,item.isOnScreen?1:0);}
    for(const Explosion &item:explosion){put16(&p,(uint16_t)item.x);put16(&p,(uint16_t)item.y);put8(&p,item.isOnScreen?1:0);}
    for(const Pothole &item:pothole){put16(&p,(uint16_t)item.x);put16(&p,(uint16_t)item.y);put8(&p,item.isOnScreen?1:0);}
    TinyfontPxaState::pack(&p,tinyfont);
    return (size_t)(p-out)==GAME_WIRE_BYTES?GAME_WIRE_BYTES:0;
}
