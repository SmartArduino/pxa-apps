#ifndef JUMP3D_MATH_H
#define JUMP3D_MATH_H

#include <stdint.h>

/* Freestanding float helpers. Guests link without libm, so sqrt/floor are
 * Wasm instructions and sin/cos are short polynomials (accuracy ~1e-6). */

#define J3_PI 3.14159265358979F
#define J3_TWO_PI 6.28318530717959F
#define J3_HALF_PI 1.57079632679490F

static inline float j3_sqrt(float value) { return __builtin_sqrtf(value); }
static inline float j3_floor(float value) { return __builtin_floorf(value); }
static inline float j3_fabs(float value) { return __builtin_fabsf(value); }

static inline float j3_clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

static inline int j3_clampi(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

static inline float j3_sin(float radians) {
    float x = radians - J3_TWO_PI * j3_floor((radians + J3_PI) / J3_TWO_PI);
    float x2;
    if (x > J3_HALF_PI) {
        x = J3_PI - x;
    } else if (x < -J3_HALF_PI) {
        x = -J3_PI - x;
    }
    x2 = x * x;
    return x * (1.0F + x2 * (-1.0F / 6.0F +
                             x2 * (1.0F / 120.0F +
                                   x2 * (-1.0F / 5040.0F +
                                         x2 * (1.0F / 362880.0F +
                                               x2 * (-1.0F / 39916800.0F))))));
}

static inline float j3_cos(float radians) {
    return j3_sin(radians + J3_HALF_PI);
}

#endif
