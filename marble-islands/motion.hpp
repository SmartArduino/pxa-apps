// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <math.h>
#include <stdint.h>
namespace marble_motion {
struct Vec {
  float x = 0, y = 0;
};
struct Options {
  bool enabled = true;
  uint8_t sensitivity = 1, rotation = 0, navigation = 0;
  bool mirror = false;
};
enum class Status {
  Unavailable,
  Connecting,
  Permission,
  Streaming,
  Denied,
  Error
};
struct Tilt {
  float gravity[3] = {0, 0, 1}, zero[2] = {}, mean[2] = {}, variance[2] = {};
  uint64_t acc_time = 0, gyro_time = 0, received = 0, calibration_start = 0;
  float angular_speed = 0;
  unsigned count = 0;
  bool calibrated = false, calibrating = true;
  void calibrate() {
    count = 0;
    calibration_start = 0;
    calibrated = false;
    calibrating = true;
    mean[0] = mean[1] = variance[0] = variance[1] = 0;
  }
  void suspend() {
    acc_time = gyro_time = received = 0;
    angular_speed = 0;
    gravity[0] = gravity[1] = 0;
    gravity[2] = 1;
    calibrate();
  }
  static float angle(float a) {
    while (a > 3.14159265f)
      a -= 6.2831853f;
    while (a < -3.14159265f)
      a += 6.2831853f;
    return a;
  }
  static Vec angles(const float *g) {
    return {-atan2f(g[1], sqrtf(g[0] * g[0] + g[2] * g[2])),
            atan2f(g[0], g[2])};
  }
  void normalize() {
    float length = sqrtf(gravity[0] * gravity[0] + gravity[1] * gravity[1] +
                         gravity[2] * gravity[2]);
    for (float &v : gravity)
      v /= length;
  }
  bool acceleration(const int32_t *values, uint64_t timestamp,
                    uint64_t arrival) {
    if (!timestamp || timestamp <= acc_time)
      return false;
    float next[3], magnitude = 0, jerk = 0;
    for (unsigned i = 0; i < 3; ++i) {
      next[i] = float(values[i]) / 9806650.f;
      magnitude += next[i] * next[i];
      float d = next[i] - gravity[i];
      jerk += d * d;
    }
    if (magnitude < .49f || magnitude > 1.69f) {
      if (calibrating)
        calibrate();
      return false;
    }
    float length = sqrtf(magnitude);
    for (float &v : next)
      v /= length;
    bool gap = !acc_time || timestamp - acc_time > 400000;
    float alignment = 0;
    for (unsigned i = 0; i < 3; ++i)
      alignment += next[i] * gravity[i];
    // A flipped device needs a new neutral, never normalization of two
    // cancelling gravity vectors.
    if (gap || alignment < 0) {
      for (unsigned i = 0; i < 3; ++i)
        gravity[i] = next[i];
      calibrate();
    } else {
      float dt = float(timestamp - acc_time) * .000001f;
      bool gyro_fresh =
          gyro_time && timestamp >= gyro_time && timestamp - gyro_time < 200000;
      float blend = dt / ((gyro_fresh ? .65f : .12f) + dt);
      for (unsigned i = 0; i < 3; ++i)
        gravity[i] += blend * (next[i] - gravity[i]);
      normalize();
    }
    acc_time = timestamp;
    received = arrival;
    if (calibrating) {
      if ((!gap && jerk > .0064f) ||
          (gyro_time && timestamp >= gyro_time &&
           timestamp - gyro_time < 200000 && angular_speed > .0144f)) {
        calibrate();
        return true;
      }
      Vec raw = angles(next);
      if (!count)
        calibration_start = timestamp;
      ++count;
      const float reading[2] = {raw.x, raw.y};
      for (unsigned i = 0; i < 2; ++i) {
        float delta = angle(reading[i] - mean[i]);
        mean[i] += delta / count;
        variance[i] += delta * angle(reading[i] - mean[i]);
      }
      if (count >= 16 && timestamp - calibration_start >= 750000) {
        if (variance[0] / count > .0003f || variance[1] / count > .0003f)
          calibrate();
        else {
          zero[0] = mean[0];
          zero[1] = mean[1];
          calibrated = true;
          calibrating = false;
        }
      }
    }
    return true;
  }
  bool gyroscope(const int32_t *values, uint64_t timestamp) {
    if (!timestamp || timestamp <= gyro_time)
      return false;
    float w[3];
    angular_speed = 0;
    for (unsigned i = 0; i < 3; ++i) {
      w[i] = float(values[i]) * .000001f;
      angular_speed += w[i] * w[i];
    }
    if (angular_speed > 400.f)
      return false;
    if (gyro_time && timestamp - gyro_time <= 100000 && acc_time &&
        timestamp >= acc_time && timestamp - acc_time <= 400000) {
      float dt = float(timestamp - gyro_time) * .000001f;
      float next[3] = {
          gravity[0] + (gravity[1] * w[2] - gravity[2] * w[1]) * dt,
          gravity[1] + (gravity[2] * w[0] - gravity[0] * w[2]) * dt,
          gravity[2] + (gravity[0] * w[1] - gravity[1] * w[0]) * dt};
      for (unsigned i = 0; i < 3; ++i)
        gravity[i] = next[i];
      normalize();
    }
    gyro_time = timestamp;
    return true;
  }
  bool fresh(uint64_t now) const {
    return received && now >= received && now - received <= 400000;
  }
  Vec input(const Options &options) const {
    Vec v = angles(gravity);
    v = {angle(v.x - zero[0]), angle(v.y - zero[1])};
    for (unsigned i = 0; i < options.rotation; ++i)
      v = {-v.y, v.x};
    if (options.mirror)
      v.x = -v.x;
    const float full[] = {.55f, .38f, .25f};
    float limit = full[options.sensitivity < 3 ? options.sensitivity : 1];
    auto clean = [&](float value) {
      float a = fabsf(value);
      return a <= .03f
                 ? 0.f
                 : copysignf(fminf(1, (a - .03f) / (limit - .03f)), value);
    };
    v = {clean(v.x), clean(v.y)};
    float length = sqrtf(v.x * v.x + v.y * v.y);
    if (length > 1) {
      v.x /= length;
      v.y /= length;
    }
    return v;
  }
};
struct Device {
  Tilt tilt;
  Options options;
  Status status = Status::Unavailable;
  uint64_t now = 0;
  uint32_t retry = 0;
  bool wanted = false, gyro = false;
  void request(bool value) { wanted = value && options.enabled; }
  bool ready() const {
    return wanted && status == Status::Streaming && tilt.calibrated &&
           tilt.fresh(now);
  }
  bool waiting() const {
    return wanted &&
           (status == Status::Connecting || status == Status::Permission ||
            (status == Status::Streaming && !ready()));
  }
};
inline Device device;
} // namespace marble_motion
