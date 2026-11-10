// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace compass {
constexpr float PI = 3.14159265358979323846f, DEG = 180.f / PI;
struct Vec {
  float x = 0, y = 0, z = 0;
  float &operator[](int i) { return i == 0 ? x : i == 1 ? y : z; }
  float operator[](int i) const { return i == 0 ? x : i == 1 ? y : z; }
};
inline Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec operator*(Vec a, float b) { return {a.x * b, a.y * b, a.z * b}; }
inline float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float norm(Vec a) { return std::sqrt(dot(a, a)); }
inline Vec unit(Vec a) {
  float n = norm(a);
  return n > 1e-6f ? a * (1 / n) : Vec{};
}
inline Vec cross(Vec a, Vec b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline bool finite(Vec v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline float wrap(float x) {
  x = std::fmod(x, 360.f);
  return x < 0 ? x + 360 : x;
}
inline float delta(float a, float b) { return wrap(a - b + 180) - 180; }
inline float needle_blend(uint32_t ms) {
  return -std::expm1(-float(ms) / 100.f);
}
// All 24 proper rotations of orthogonal sensor mountings. No reflections.
inline Vec rotate(Vec v, unsigned id) {
  static const int p[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                              {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  static const int parity[6] = {1, -1, -1, 1, 1, -1};
  if (id >= 24)
    return {};
  unsigned k = id / 4, s = id % 4;
  int x = s & 1 ? -1 : 1, y = s & 2 ? -1 : 1, z = x * y * parity[k];
  return {v[p[k][0]] * x, v[p[k][1]] * y, v[p[k][2]] * z};
}
// Above 45 degrees of top-edge elevation, ease the reference toward the
// back of the display. At 70 degrees it is fully rear-facing, so 90 degrees
// has no top-edge projection singularity and crossing it cannot flip north.
inline float forward_blend(float top_up) {
  float t = std::clamp((std::abs(top_up) - .70710678f) / (.93969262f - .70710678f), 0.f, 1.f);
  return t * t * (3 - 2 * t);
}
inline Vec forward_reference(Vec up) {
  float w = forward_blend(up.y);
  Vec v{0, 1 - w, -w};
  return v - up * dot(v, up);
}
struct Calibration {
  Vec bias{};
  float radius = 0, residual = 0;
  uint8_t rotation = 0;
  bool valid = false;
};
inline bool valid_cal(const Calibration &c) {
  return c.valid && finite(c.bias) && norm(c.bias) < 2000 && c.radius >= 15 &&
         c.radius <= 85 && std::isfinite(c.residual) && c.residual >= 0 &&
         c.residual <= .10f && c.rotation < 24;
}
struct Capture {
  struct Sample {
    Vec m, g;
  };
  Sample data[256]{};
  unsigned count = 0, next = 0, since_fit = 0;
  uint64_t last = 0, started = 0;
  bool collecting = false;
  Calibration candidate{};
  unsigned faces = 0;
  float fit_error = 1, alignment_error = 1;
  void begin() {
    count = next = faces = since_fit = 0;
    last = started = 0;
    candidate = {};
    collecting = true;
  }
  bool add(Vec m, Vec gravity, uint64_t stamp) {
    float gn = norm(gravity);
    if (!collecting || stamp <= last || !finite(m) || !finite(gravity) ||
        norm(m) > 2000 || gn < 8.5f || gn > 11.2f)
      return false;
    if (!started)
      started = stamp;
    last = stamp;
    data[next] = {m, unit(gravity)};
    next = (next + 1) % 256;
    count = std::min(256u, count + 1);
    if (++since_fit >= 10) {
      since_fit = 0;
      fit();
    }
    return true;
  }
  bool fit() {
    candidate = {};
    faces = 0;
    unsigned bins[6]{};
    double a[4][5]{};
    Vec low{1e6f, 1e6f, 1e6f}, high{-1e6f, -1e6f, -1e6f};
    for (unsigned i = 0; i < count; ++i) {
      const auto &s = data[i];
      for (int k = 0; k < 3; ++k) {
        if (s.g[k] > .8f)
          ++bins[k * 2];
        if (s.g[k] < -.8f)
          ++bins[k * 2 + 1];
        low[k] = std::min(low[k], s.m[k]);
        high[k] = std::max(high[k], s.m[k]);
      }
      double v[4] = {s.m.x / 100., s.m.y / 100., s.m.z / 100., 1};
      double rhs = -(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
      for (int j = 0; j < 4; ++j) {
        for (int k = 0; k < 4; ++k)
          a[j][k] += v[j] * v[k];
        a[j][4] += v[j] * rhs;
      }
    }
    for (unsigned n : bins)
      if (n >= 4)
        ++faces;
    if (count < 96 || last - started < 8000000 || faces < 6)
      return false;
    for (int k = 0; k < 4; ++k) {
      int p = k;
      for (int j = k + 1; j < 4; ++j)
        if (std::abs(a[j][k]) > std::abs(a[p][k]))
          p = j;
      if (std::abs(a[p][k]) < 1e-7)
        return false;
      for (int j = k; j < 5; ++j)
        std::swap(a[k][j], a[p][j]);
      double d = a[k][k];
      for (int j = k; j < 5; ++j)
        a[k][j] /= d;
      for (int i = 0; i < 4; ++i)
        if (i != k) {
          double f = a[i][k];
          for (int j = k; j < 5; ++j)
            a[i][j] -= f * a[k][j];
        }
    }
    Vec bias{float(-50 * a[0][4]), float(-50 * a[1][4]), float(-50 * a[2][4])};
    float r2 = dot(bias, bias) - float(10000 * a[3][4]);
    if (!std::isfinite(r2) || r2 < 225 || r2 > 7225 || norm(bias) > 2000)
      return false;
    float radius = std::sqrt(r2);
    for (int k = 0; k < 3; ++k)
      if (high[k] - low[k] < radius * 1.25f)
        return false;
    double residual = 0;
    for (unsigned i = 0; i < count; ++i) {
      float e = norm(data[i].m - bias) / radius - 1;
      residual += e * e;
    }
    fit_error = std::sqrt(residual / count);
    if (fit_error > .10f)
      return false;
    float best = 1e9f, second = 1e9f;
    unsigned rotation = 0;
    for (unsigned k = 0; k < 24; ++k) {
      double sum = 0, square = 0;
      for (unsigned i = 0; i < count; ++i) {
        float v = dot(unit(rotate(data[i].m - bias, k)), data[i].g);
        sum += v;
        square += v * v;
      }
      float variance =
          std::max(0., square / count - (sum / count) * (sum / count));
      if (variance < best) {
        second = best;
        best = variance;
        rotation = k;
      } else
        second = std::min(second, variance);
    }
    alignment_error = std::sqrt(best);
    if (alignment_error > .08f || second - best < .015f)
      return false;
    candidate = {bias, radius, fit_error, uint8_t(rotation), true};
    return true;
  }
};
struct Estimator {
  Vec raw{}, gravity{}, field{};
  uint64_t received = 0, gyro_time = 0, stamp = 0;
  bool calibrated = false;
  void suspend() {
    received = gyro_time = stamp = 0;
    calibrated = false;
  }
  void acceleration(const int32_t *v, uint64_t timestamp, uint64_t now) {
    if (timestamp <= stamp)
      return;
    raw = {v[0] / 1000.f, v[1] / 1000.f, v[2] / 1000.f};
    stamp = timestamp;
    received = now;
    calibrated = true;
  }
  void gyroscope(const int32_t *v, uint64_t timestamp) {
    if (timestamp <= gyro_time)
      return;
    gravity = {v[0] / 1000000.f, v[1] / 1000000.f, v[2] / 1000000.f};
    gyro_time = timestamp;
  }
};
enum class Status {
  Unavailable,
  Connecting,
  Permission,
  Streaming,
  Denied,
  Error,
  NoGravity
};
enum class Quality {
  Waiting,
  Calibrate,
  Moving,
  Vertical,
  Interference,
  Good,
  Held
};
struct Model {
  Estimator tilt;
  Status status = Status::Unavailable;
  uint64_t now = 0;
  uint32_t retry = 0;
  bool wanted = true, gyro = false, held = false, system_heading = false;
  uint64_t heading_stamp = 0;
  float absolute_heading = 0;
  unsigned channel = 0;
  Calibration saved[2]{};
  Capture capture;
  float heading = 0, shown = 0, strength = 0;
  bool has_heading = false;
  Quality quality = Quality::Waiting;
  uint64_t last_compute = 0;
  int declination = 0;
  void suspend() { tilt.suspend(); heading_stamp = 0; last_compute = 0; }
  void heading_sample(int32_t value, uint64_t stamp) {
    if (value < 0 || value >= 360000000 || stamp <= heading_stamp || stamp > now)
      return;
    absolute_heading = value / 1000000.f;
    heading_stamp = stamp;
  }
  void compute(float target, uint64_t stamp) {
    quality = Quality::Good;
    if (last_compute != stamp) {
      float dt = last_compute && stamp > last_compute
                     ? std::min(.5f, (stamp - last_compute) / 1000000.f) : 1;
      heading = has_heading ? wrap(heading + delta(target, heading) *
                                  (-std::expm1(-dt / .24f))) : target;
      last_compute = stamp;
    }
    has_heading = true;
  }
  bool fresh() const {
    if (system_heading)
      return status == Status::Streaming && heading_stamp && now >= heading_stamp &&
             now - heading_stamp < 1500000;
    return status == Status::Streaming && tilt.received &&
           now >= tilt.received && now - tilt.received < 1500000 &&
           tilt.gyro_time && now >= tilt.gyro_time &&
           now - tilt.gyro_time < 1500000;
  }
  bool paired() const {
    uint64_t a = tilt.stamp, b = tilt.gyro_time;
    return (a > b ? a - b : b - a) <= 50000;
  }
  void update() {
    if (held) {
      quality = Quality::Held;
      return;
    }
    if (!fresh()) {
      quality = Quality::Waiting;
      has_heading = false;
      return;
    }
    if (system_heading) {
      strength = 0;
      compute(wrap(absolute_heading + declination), heading_stamp);
      return;
    }
    auto &c = saved[channel];
    strength = norm(tilt.raw - (valid_cal(c) ? c.bias : Vec{}));
    // Pair time-aligned readings; never fit an earlier pose's gravity with
    // a newer magnetic vector. Capture keeps fitting after its ring is full.
    if (capture.collecting && paired())
      capture.add(tilt.raw, tilt.gravity, tilt.stamp);
    if (!valid_cal(c)) {
      quality = Quality::Calibrate;
      has_heading = false;
      return;
    }
    // Both subscriptions are asynchronous. Keep the last display for this
    // fraction of a sample period, then compute when its matching vector lands.
    if (!paired()) {
      if (!has_heading)
        quality = Quality::Waiting;
      return;
    }
    float gn = norm(tilt.gravity);
    if (gn < 8.5f || gn > 11.2f) {
      quality = Quality::Moving;
      has_heading = false;
      return;
    }
    if (strength < c.radius * .7f || strength > c.radius * 1.3f) {
      quality = Quality::Interference;
      has_heading = false;
      return;
    }
    Vec g = unit(tilt.gravity), top = forward_reference(g);
    if (norm(top) < .26f) {
      quality = Quality::Vertical;
      has_heading = false;
      return;
    }
    top = unit(top);
    Vec left = cross(g, top), m = rotate(tilt.raw - c.bias, c.rotation);
    float x = dot(m, top), y = dot(m, left);
    if (std::hypot(x, y) < 5) {
      quality = Quality::Interference;
      has_heading = false;
      return;
    }
    float target = wrap(std::atan2(y, x) * DEG + declination);
    compute(target, tilt.stamp);
  }
  bool toggle_hold() {
    if (held) {
      held = false;
      last_compute = 0;
      return true;
    }
    if (!has_heading || quality != Quality::Good)
      return false;
    held = true;
    return true;
  }
  void select(unsigned c) {
    channel = c % 2;
    held = false;
    has_heading = false;
    capture.collecting = false;
    suspend();
    ++retry;
  }
  bool apply() {
    if (system_heading || !valid_cal(capture.candidate))
      return false;
    saved[channel] = capture.candidate;
    capture.collecting = false;
    held = false;
    has_heading = false;
    last_compute = 0;
    return true;
  }
};
inline Model device;
constexpr unsigned SAVE_SIZE = 80;
inline uint32_t checksum(const uint8_t *p, unsigned n) {
  uint32_t h = 2166136261u;
  for (unsigned i = 0; i < n; ++i)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
inline size_t encode(const Model &d, uint8_t *p, size_t n) {
  if (!p || n < SAVE_SIZE)
    return 0;
  std::memset(p, 0, SAVE_SIZE);
  std::memcpy(p, "CMP2", 4);
  p[4] = d.channel;
  p[5] = uint8_t(d.declination + 30);
  for (int i = 0; i < 2; ++i) {
    const auto &c = d.saved[i];
    auto *v = p + 8 + i * 28;
    v[0] = valid_cal(c);
    v[1] = c.rotation;
    float a[5] = {c.bias.x, c.bias.y, c.bias.z, c.radius, c.residual};
    std::memcpy(v + 4, a, sizeof(a));
  }
  uint32_t h = checksum(p, SAVE_SIZE - 4);
  std::memcpy(p + SAVE_SIZE - 4, &h, 4);
  return SAVE_SIZE;
}
inline bool decode(Model &d, const uint8_t *p, size_t n) {
  if (!p || n != SAVE_SIZE || std::memcmp(p, "CMP2", 4) || p[4] > 1 ||
      p[5] > 60)
    return false;
  uint32_t h;
  std::memcpy(&h, p + n - 4, 4);
  if (h != checksum(p, n - 4))
    return false;
  Calibration c[2];
  for (int i = 0; i < 2; ++i) {
    const auto *v = p + 8 + i * 28;
    float a[5];
    std::memcpy(a, v + 4, sizeof(a));
    c[i] = {{a[0], a[1], a[2]}, a[3], a[4], v[1], v[0] != 0};
    if (v[0] > 1 || (v[0] && !valid_cal(c[i])))
      return false;
  }
  d.saved[0] = c[0];
  d.saved[1] = c[1];
  d.declination = int(p[5]) - 30;
  d.select(p[4]);
  return true;
}
} // namespace compass
