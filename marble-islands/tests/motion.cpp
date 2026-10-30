// SPDX-License-Identifier: CC0-1.0
#include "../motion.hpp"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
using namespace marble_motion;
static uint64_t steady(Tilt &t, uint64_t now = 1000000,
                       const int32_t *pose = nullptr) {
  const int32_t flat[] = {0, 0, 9806650}, still[] = {0, 0, 0};
  if (!pose)
    pose = flat;
  for (unsigned i = 0; i < 20; ++i) {
    now += 50000;
    t.gyroscope(still, now);
    assert(t.acceleration(pose, now, now));
  }
  return now;
}
int main() {
  Options o;
  Tilt t;
  uint64_t now = steady(t);
  assert(t.calibrated && !t.calibrating && t.fresh(now));
  assert(!t.fresh(now + 400001));
  Vec v = t.input(o);
  assert(v.x == 0 && v.y == 0);
  const int32_t right[] = {0, -3500000, 9150000};
  for (int i = 0; i < 20; ++i) {
    now += 50000;
    t.acceleration(right, now, now);
  }
  v = t.input(o);
  assert(v.x > .7f && fabsf(v.y) < .001f);
  o.rotation = 1;
  Vec rotated = t.input(o);
  assert(fabsf(rotated.x) < .001f && rotated.y > .7f);
  o.rotation = 0;
  o.mirror = true;
  assert(t.input(o).x < -.7f);
  o.mirror = false;
  o.sensitivity = 0;
  float low = t.input(o).x;
  o.sensitivity = 1;
  float medium = t.input(o).x;
  o.sensitivity = 2;
  float high = t.input(o).x;
  assert(low < medium && medium < high && high <= 1);
  const int32_t extreme[] = {INT_MIN, INT_MAX, INT_MIN};
  uint64_t last = t.acc_time;
  assert(!t.acceleration(extreme, now + 50000, now + 50000) &&
         t.acc_time == last);
  assert(!t.acceleration(right, last, last));
  assert(!t.acceleration(right, last - 1, last));
  assert(!t.gyroscope(extreme, now + 50000));
  // Calibrate at a comfortable, non-flat grip. Its neutral output is still
  // zero.
  t.suspend();
  now = steady(t, now + 1000000, right);
  assert(t.calibrated);
  v = t.input(o);
  assert(fabsf(v.x) < .01f && fabsf(v.y) < .01f);
  // A long signal gap restarts neutral calibration instead of jumping to a
  // tilt.
  assert(t.acceleration(right, now + 1000000, now + 1000000));
  assert(t.calibrating && !t.calibrated);
  // Moving or shaking during setup cannot establish a misleading neutral point.
  t.suspend();
  now += 2000000;
  const int32_t left[] = {0, 3500000, 9150000};
  for (int i = 0; i < 100; ++i) {
    now += 50000;
    t.acceleration(i % 2 ? left : right, now, now);
    assert(!t.calibrated);
  }
  // Gyroscope improves response between acceleration frames; yaw on a flat grip
  // does not tilt the board, and an old sample cannot apply a second rotation.
  t.suspend();
  now = steady(t, now + 1000000);
  const int32_t yaw[] = {0, 0, 2000000}, roll[] = {2000000, 0, 0};
  now += 50000;
  t.gyroscope(yaw, now);
  v = t.input(o);
  assert(fabsf(v.x) < .001f && fabsf(v.y) < .001f);
  now += 50000;
  t.gyroscope(roll, now);
  v = t.input(o);
  assert(v.x < -.2f);
  float x = v.x;
  assert(!t.gyroscope(roll, now));
  assert(t.input(o).x == x);
  const int32_t upside[] = {0, 0, -9806650};
  t.acceleration(upside, now + 120000, now + 120000);
  for (float value : t.gravity)
    assert(isfinite(value));
  assert(t.calibrating);
  t.suspend();
  assert(!t.fresh(now) && !t.calibrated);
  puts("PASS motion: stable/non-flat calibration, movement rejection, "
       "deadzone, sensitivity, rotation, mirror, stale/duplicate/outlier "
       "samples and gyro fusion");
}
