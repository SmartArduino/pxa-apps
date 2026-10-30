// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "estimator.hpp"
#include <stddef.h>
namespace level {
// Visual interpolation only. Exponential smoothing has the same time constant
// at variable frame intervals and never snaps to the target after a long frame.
inline float bubble_blend(uint32_t elapsed_ms) {
  return -expm1f(-float(elapsed_ms) / 80.f);
}
enum class Status {
  Unavailable,
  Connecting,
  Permission,
  Streaming,
  Denied,
  Error
};
struct Preferences {
  uint8_t mode = 0, unit = 0, tolerance = 1, rotation = 0, axis = 0;
  bool calibrated = false;
  Vector alignment{};
};
struct Reading {
  bool valid = false, stable = false, held = false, relative = false;
  float x = 0, y = 0, total = 0;
  Vector gravity{};
};
struct Calibration {
  enum Stage { Intro, Collecting, Complete } stage = Intro;
  uint64_t start = 0, last = 0;
  unsigned count = 0;
  Vector mean = {0, 0, 0};
  float variance = 0;
  Vector candidate{};
  void begin() {
    stage = Collecting;
    reset_window();
  }
  void reset_window() {
    start = last = 0;
    count = 0;
    mean = {0, 0, 0};
    variance = 0;
  }
  bool sample(Vector value, uint64_t stamp, bool stable) {
    if (stage != Collecting || stamp == last)
      return false;
    if (!stable || value.z < .9659f) {
      reset_window();
      return false;
    }
    if (!count)
      start = stamp;
    last = stamp;
    ++count;
    Vector delta = {value.x - mean.x, value.y - mean.y, value.z - mean.z};
    mean = {mean.x + delta.x / count, mean.y + delta.y / count,
            mean.z + delta.z / count};
    variance += delta.x * (value.x - mean.x) + delta.y * (value.y - mean.y) +
                delta.z * (value.z - mean.z);
    if (count >= 32 && stamp - start >= 2000000) {
      if (variance / count > .000025f) {
        reset_window();
        return false;
      }
      candidate = normalized(mean);
      stage = Complete;
      return true;
    }
    return false;
  }
  float progress() const {
    return stage == Complete ? 1.f
           : count           ? fminf(1, float(last - start) / 2000000.f)
                             : 0.f;
  }
};
struct Model {
  Estimator tilt;
  Preferences prefs;
  Status status = Status::Unavailable;
  uint64_t now = 0;
  uint32_t retry = 0;
  bool wanted = true, gyro = false, held = false, relative = false;
  Vector reference{}, frozen{};
  bool frozen_stable = false;
  void request(bool on) { wanted = on; }
  bool ready() const { return status == Status::Streaming && tilt.fresh(now); }
  Reading read() const {
    Reading r;
    r.held = held;
    r.relative = relative;
    if (!held && !ready())
      return r;
    Vector g = held ? frozen : align(tilt.gravity, prefs.alignment);
    if (relative)
      g = align(g, reference);
    g = screen_vector(g, prefs.rotation);
    r.valid = true;
    r.gravity = g;
    r.stable = held ? frozen_stable : tilt.stable(now);
    r.x = atan2f(g.x, hypotf(g.y, g.z)) * DEG;
    r.y = atan2f(g.y, hypotf(g.x, g.z)) * DEG;
    r.total = atan2f(hypotf(g.x, g.y), fabsf(g.z)) * DEG;
    return r;
  }
  bool toggle_hold() {
    if (held) {
      held = false;
      return true;
    }
    if (!ready())
      return false;
    frozen = align(tilt.gravity, prefs.alignment);
    frozen_stable = tilt.stable(now);
    held = true;
    return true;
  }
  bool toggle_relative() {
    if (relative) {
      relative = false;
      return true;
    }
    if (!held && !ready())
      return false;
    reference = held ? frozen : align(tilt.gravity, prefs.alignment);
    relative = true;
    return true;
  }
  void absolute() { held = relative = false; }
  float tolerance() const {
    const float values[] = {.2f, .3f, .5f, 1.f};
    return values[prefs.tolerance];
  }
  bool apply(Calibration &c) {
    if (c.stage != Calibration::Complete)
      return false;
    prefs.alignment = c.candidate;
    prefs.calibrated = true;
    absolute();
    return true;
  }
  void clear_calibration() {
    prefs.alignment = {};
    prefs.calibrated = false;
    absolute();
  }
};
inline Model device;
constexpr unsigned SAVE_BYTES = 64;
size_t encode(const Preferences &, uint8_t *, size_t);
bool decode(Preferences &, const uint8_t *, size_t);
float edge_error(float angle, bool relative);
void format_value(char *out, size_t size, float angle, unsigned unit,
                  bool valid);
} // namespace level
