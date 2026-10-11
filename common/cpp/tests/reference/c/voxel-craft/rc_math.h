#ifndef VOXEL_CRAFT_RC_MATH_H
#define VOXEL_CRAFT_RC_MATH_H

#include <stdint.h>

/* Freestanding float helpers. Guests link without libm, so sqrt/floor/abs are
 * Wasm instructions and sin/cos are short polynomials (accuracy ~1e-6). */

#define RC_PI 3.14159265358979F
#define RC_TWO_PI 6.28318530717959F
#define RC_HALF_PI 1.57079632679490F

static inline float rc_sqrt(float value) { return __builtin_sqrtf(value); }
static inline float rc_floor(float value) { return __builtin_floorf(value); }
static inline float rc_fabs(float value) { return __builtin_fabsf(value); }
static inline int rc_floor_int(float value) {
    return (int)__builtin_floorf(value);
}

static inline float rc_clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

static inline int rc_clampi(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

static inline float rc_sin(float radians) {
    float x = radians - RC_TWO_PI * rc_floor((radians + RC_PI) / RC_TWO_PI);
    float x2;
    if (x > RC_HALF_PI) {
        x = RC_PI - x;
    } else if (x < -RC_HALF_PI) {
        x = -RC_PI - x;
    }
    x2 = x * x;
    return x * (1.0F + x2 * (-1.0F / 6.0F +
                             x2 * (1.0F / 120.0F +
                                   x2 * (-1.0F / 5040.0F +
                                         x2 * (1.0F / 362880.0F +
                                               x2 * (-1.0F / 39916800.0F))))));
}

static inline float rc_cos(float radians) {
    return rc_sin(radians + RC_HALF_PI);
}

#endif
