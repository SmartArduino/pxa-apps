#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct jump_timing_snapshot {
    float charge, body_scale, px, py, pz, vx, vy, vz, land_x, land_z;
    uint32_t score;
    uint8_t state, land_result;
};
uint8_t jump_reference_steps(uint64_t *previous, uint64_t now);
void jump_reference_reset(uint32_t seed);
void jump_reference_press(void);
void jump_reference_release(void);
void jump_reference_tick(void);
struct jump_timing_snapshot jump_reference_snapshot(void);
#ifdef __cplusplus
}
#endif
