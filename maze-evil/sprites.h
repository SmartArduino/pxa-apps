#ifndef MAZE_EVIL_SPRITES_H
#define MAZE_EVIL_SPRITES_H

#include <stdint.h>

#include "raster.h"

enum {
    SPR_IMP_WALK_A = 0,
    SPR_IMP_WALK_B,
    SPR_IMP_ATTACK,
    SPR_IMP_PAIN,
    SPR_IMP_DEAD,
    SPR_FIREBALL_A,
    SPR_FIREBALL_B,
    SPR_MEDKIT,
    SPR_AMMO,
    SPR_TORCH_A,
    SPR_TORCH_B,
    SPR_BARREL,
    SPR_SHOTGUN,
    SPR_MUZZLE_FLASH,
    SPR_TORCH_C,
    SPR_TORCH_D,
    SPR_COUNT,
};

extern const sprite_t kSprites[SPR_COUNT];

static inline uint8_t torch_frame(int index) {
    static const uint8_t frames[4] = {SPR_TORCH_A, SPR_TORCH_B, SPR_TORCH_C,
                                      SPR_TORCH_D};
    return frames[index & 3];
}

static inline int sprite_is_torch(int id) {
    return id == SPR_TORCH_A || id == SPR_TORCH_B || id == SPR_TORCH_C ||
           id == SPR_TORCH_D;
}

#endif
