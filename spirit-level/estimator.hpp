// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <math.h>
#include <stdint.h>
namespace level {
constexpr float PI = 3.14159265358979323846f, DEG = 180.f / PI;
struct Vector {
  float x = 0, y = 0, z = 1;
};
inline float dot(Vector a, Vector b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vector cross(Vector a, Vector b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vector normalized(Vector v) {
  float n = sqrtf(dot(v, v));
  return n > .00001f ? Vector{v.x / n, v.y / n, v.z / n} : Vector{};
}
// Rotate a measured reference onto screen-up without removing real-world tilt
// implicitly. Used only for explicitly requested alignment/reference changes.
inline Vector align(Vector value, Vector reference) {
  reference = normalized(reference);
  const float c = reference.z, sine = hypotf(reference.x, reference.y);
  if (sine < .0000001f)
    return c < 0 ? normalized(Vector{value.x, -value.y, -value.z})
                 : normalized(value);
  // Axis/angle form avoids dividing by 1 + cos(theta), which loses
  // precision for an explicitly chosen reference close to screen-down.
  const Vector axis = {reference.y / sine, -reference.x / sine, 0};
  const Vector turn = cross(axis, value);
  const float along = dot(axis, value) * (1 - c);
  return normalized({value.x * c + turn.x * sine + axis.x * along,
                     value.y * c + turn.y * sine + axis.y * along,
                     value.z * c + turn.z * sine + axis.z * along});
}
inline Vector screen_vector(Vector raw, unsigned rotation) {
  // Project the Host's device frame to the meter; board mounting belongs in
  // the sensor provider, before both acceleration and angular velocity arrive.
  Vector v = {-raw.y, raw.x, raw.z};
  for (unsigned i = 0; i < rotation % 4; ++i)
    v = {-v.y, v.x, v.z};
  return v;
}
struct Estimator {
  Vector gravity{}, previous{};
  uint64_t acc_time = 0, gyro_time = 0, received = 0, stable_since = 0;
  float angular_speed = 0, magnitude = 0;
  unsigned samples = 0;
  bool calibrated =
      false; // initialized; never means an arbitrary pose was zeroed
  void suspend() {
    acc_time = gyro_time = received = stable_since = 0;
    angular_speed = 0;
    samples = 0;
    calibrated = false;
    gravity = previous = {};
  }
  bool acceleration(const int32_t *values, uint64_t timestamp,
                    uint64_t arrival) {
    if (!timestamp || timestamp <= acc_time)
      return false;
    Vector next = {values[0] / 9806650.f, values[1] / 9806650.f,
                   values[2] / 9806650.f};
    magnitude = sqrtf(dot(next, next));
    if (magnitude < .75f || magnitude > 1.25f) {
      stable_since = 0;
      return false;
    }
    next = normalized(next);
    bool gap = !acc_time || timestamp - acc_time > 400000;
    float change = (next.x - previous.x) * (next.x - previous.x) +
                   (next.y - previous.y) * (next.y - previous.y) +
                   (next.z - previous.z) * (next.z - previous.z);
    if (gap || dot(next, gravity) < 0) {
      gravity = next;
      samples = 0;
      stable_since = 0;
    } else {
      float dt = float(timestamp - acc_time) * .000001f;
      bool gyro_fresh =
          gyro_time && timestamp >= gyro_time && timestamp - gyro_time < 200000;
      float blend = dt / ((gyro_fresh ? .3f : .12f) + dt);
      gravity = normalized({gravity.x + (next.x - gravity.x) * blend,
                            gravity.y + (next.y - gravity.y) * blend,
                            gravity.z + (next.z - gravity.z) * blend});
    }
    bool turning = gyro_time && timestamp >= gyro_time &&
                   timestamp - gyro_time < 200000 && angular_speed > .0025f;
    if (gap || change > .000025f || fabsf(magnitude - 1) > .06f || turning)
      stable_since = 0;
    else if (!stable_since)
      stable_since = arrival;
    previous = next;
    acc_time = timestamp;
    received = arrival;
    ++samples;
    calibrated = samples >= 3;
    return true;
  }
  bool gyroscope(const int32_t *values, uint64_t timestamp) {
    if (!timestamp || timestamp <= gyro_time)
      return false;
    Vector w = {values[0] * .000001f, values[1] * .000001f,
                values[2] * .000001f};
    angular_speed = dot(w, w);
    if (angular_speed > 400.f)
      return false;
    if (gyro_time && timestamp - gyro_time <= 100000 && acc_time &&
        timestamp >= acc_time && timestamp - acc_time < 400000) {
      float dt = float(timestamp - gyro_time) * .000001f;
      Vector delta = cross(gravity, w);
      gravity = normalized({gravity.x + delta.x * dt, gravity.y + delta.y * dt,
                            gravity.z + delta.z * dt});
    }
    gyro_time = timestamp;
    return true;
  }
  bool fresh(uint64_t now) const {
    return calibrated && received && now >= received &&
           now - received <= 400000;
  }
  bool stable(uint64_t now) const {
    return fresh(now) && stable_since && now >= stable_since &&
           now - stable_since >= 500000;
  }
};
} // namespace level
