#ifndef TOMB_MATH_H
#define TOMB_MATH_H

/* Freestanding float helpers: the Guest links no libm. Ported from
 * micropixel guest/apps/tomb-explorer/game/math.hpp. */

#define TOMB_PI 3.14159265358979f
#define TOMB_TWO_PI 6.28318530717959f
#define TOMB_HALF_PI 1.57079632679490f

static inline float tomb_sqrt(float value) {
    return value > 0.0f ? __builtin_sqrtf(value) : 0.0f;
}

static inline float tomb_fabs(float value) { return __builtin_fabsf(value); }

static inline float tomb_floor(float value) { return __builtin_floorf(value); }

static inline float tomb_clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

/* Wraps to [-pi, pi). */
static inline float tomb_wrap_angle(float radians) {
    return radians - TOMB_TWO_PI * tomb_floor((radians + TOMB_PI) / TOMB_TWO_PI);
}

/* Odd polynomial on [-pi/2, pi/2] after range reduction; error below 1e-6. */
static inline float tomb_sin(float radians) {
    float x = tomb_wrap_angle(radians);
    float x2;
    if (x > TOMB_HALF_PI) {
        x = TOMB_PI - x;
    } else if (x < -TOMB_HALF_PI) {
        x = -TOMB_PI - x;
    }
    x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6.0f +
                             x2 * (1.0f / 120.0f +
                                   x2 * (-1.0f / 5040.0f +
                                         x2 * (1.0f / 362880.0f +
                                               x2 * (-1.0f / 39916800.0f))))));
}

static inline float tomb_cos(float radians) {
    return tomb_sin(radians + TOMB_HALF_PI);
}

/* atan2 accurate to about 1e-4 rad; (0, 0) yields 0. */
static inline float tomb_atan2(float y, float x) {
    const float ax = tomb_fabs(x);
    const float ay = tomb_fabs(y);
    const float larger = ax > ay ? ax : ay;
    float ratio;
    float r2;
    float angle;
    if (larger == 0.0f) return 0.0f;
    ratio = (ax < ay ? ax : ay) / larger;
    r2 = ratio * ratio;
    angle = ratio * (0.99997726f +
                     r2 * (-0.33262347f +
                           r2 * (0.19354346f +
                                 r2 * (-0.11643287f +
                                       r2 * (0.05265332f - r2 * 0.01172120f)))));
    if (ay > ax) angle = TOMB_HALF_PI - angle;
    if (x < 0.0f) angle = TOMB_PI - angle;
    return y < 0.0f ? -angle : angle;
}

/* Moves `angle` towards `target` by at most `step` along the shorter arc. */
static inline float tomb_approach_angle(float angle, float target, float step) {
    const float delta = tomb_wrap_angle(target - angle);
    if (tomb_fabs(delta) <= step) return target;
    return tomb_wrap_angle(angle + (delta > 0.0f ? step : -step));
}

#endif
