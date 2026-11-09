// SPDX-License-Identifier: MIT
#include "../model.hpp"
#include <cassert>
#include <cstdio>
#include <random>
using namespace compass;
static Vec qrot(Vec v, float x, float y, float z, float w) {
  Vec q{x, y, z};
  return v + cross(q, v) * (2 * w) + cross(q, cross(q, v)) * 2;
}
int main() {
  for (unsigned mapping = 0; mapping < 24; ++mapping) {
    std::mt19937 rng(314159);
    std::normal_distribution<float> random(0, 1);
    Capture c;
    c.begin();
    Vec bias{65, -23, 17};
    unsigned inverse = 0;
    for (unsigned i = 0; i < 24; ++i)
      if (norm(rotate(rotate({1, 2, 3}, i), mapping) - Vec{1, 2, 3}) < .01)
        inverse = i;
    for (unsigned i = 0; i < 256; ++i) {
      float x = random(rng), y = random(rng), z = random(rng), w = random(rng),
            n = std::sqrt(x * x + y * y + z * z + w * w);
      x /= n;
      y /= n;
      z /= n;
      w /= n;
      Vec g = qrot({0, 0, 9.80665}, x, y, z, w),
          m = qrot({0, 24, -38}, x, y, z, w);
      assert(c.add(rotate(m, inverse) + bias, g, 1000000 + i * 100000));
    }
    assert(c.fit());
    assert(c.candidate.rotation == mapping);
    assert(norm(c.candidate.bias - bias) < .001);
    assert(c.faces == 6);
  }
  // An initially stationary capture must recover after its bounded ring fills.
  // Realistic noise is accepted; a large non-spherical distortion is rejected.
  std::mt19937 noise_rng(42);
  std::normal_distribution<float> normal(0, 1);
  Capture rolling, distorted;
  rolling.begin();
  distorted.begin();
  for (unsigned i = 0; i < 300; ++i)
    rolling.add({0, 24, -38}, {0, 0, 9.80665}, 1000000 + i * 100000);
  assert(!rolling.candidate.valid);
  for (unsigned i = 0; i < 300; ++i) {
    float x = normal(noise_rng), y = normal(noise_rng), z = normal(noise_rng),
          w = normal(noise_rng), n = std::sqrt(x * x + y * y + z * z + w * w);
    Vec g = qrot({0, 0, 9.80665}, x / n, y / n, z / n, w / n),
        m = qrot({0, 24, -38}, x / n, y / n, z / n, w / n);
    rolling.add(m + Vec{65 + normal(noise_rng) * .2f,
                        -23 + normal(noise_rng) * .2f,
                        17 + normal(noise_rng) * .2f},
                g, 31000000 + i * 100000);
    distorted.add({m.x * 1.8f, m.y, m.z}, g, 1000000 + i * 100000);
  }
  assert(rolling.count == 256 && valid_cal(rolling.candidate));
  assert(norm(rolling.candidate.bias - Vec{65, -23, 17}) < .2f);
  assert(!distorted.fit());
  assert(!rolling.add({NAN, 0, 0}, {0, 0, 9.8}, 100000000));
  assert(!rolling.add({24, 0, -38}, {NAN, 0, 9.8}, 100000000));
  Capture still;
  still.begin();
  for (unsigned i = 0; i < 256; ++i)
    still.add({0, 24, -38}, {0, 0, 9.80665}, 1000000 + i * 100000);
  assert(!still.fit());
  Model d;
  d.status = Status::Streaming;
  d.gyro = true;
  d.saved[0] = {{}, 45, 0, 0, true};
  auto sample = [&](float heading) {
    d.now += 100000;
    int32_t m[3] = {int32_t(-24000 * std::sin(heading / DEG)),
                    int32_t(24000 * std::cos(heading / DEG)), -38000},
            g[3] = {0, 0, 9806650};
    d.tilt.acceleration(m, d.now, d.now);
    d.tilt.gyroscope(g, d.now);
    d.update();
  };
  for (unsigned i = 0; i < 100; ++i) {
    float x = normal(noise_rng), y = normal(noise_rng), z = normal(noise_rng),
          w = normal(noise_rng), n = std::sqrt(x * x + y * y + z * z + w * w);
    x /= n;
    y /= n;
    z /= n;
    w /= n;
    Vec g = qrot({0, 0, 9.80665}, x, y, z, w),
        m = qrot({0, 24, -38}, x, y, z, w),
        forward = qrot({0, 1, 0}, -x, -y, -z, w);
    const float elevation = std::abs(unit(g).y);
    if (elevation > .707f && elevation < .941f) continue;
    if (elevation >= .941f) forward = qrot({0, 0, -1}, -x, -y, -z, w);
    if (std::hypot(forward.x, forward.y) < .27f) continue;
    d.now += 100000;
    int32_t magnetic[3] = {int32_t(m.x * 1000), int32_t(m.y * 1000),
                           int32_t(m.z * 1000)},
            gravity[3] = {int32_t(g.x * 1000000), int32_t(g.y * 1000000),
                          int32_t(g.z * 1000000)};
    d.has_heading = false;
    d.tilt.acceleration(magnetic, d.now, d.now);
    d.tilt.gyroscope(gravity, d.now);
    d.update();
    assert(d.quality == Quality::Good);
    float expected = wrap(std::atan2(forward.x, forward.y) * DEG);
    assert(std::abs(delta(d.heading, expected)) < .02f);
  }
  for (int h : {0, 90, 180, 270, 359, 1}) {
    d.has_heading = false;
    sample(h);
    assert(d.quality == Quality::Good);
    assert(std::abs(delta(d.heading, h)) < .01);
  }
  d.has_heading = false;
  sample(359);
  sample(1);
  assert(d.heading > 359 || d.heading < 1);
  assert(d.toggle_hold());
  float frozen = d.heading;
  sample(100);
  assert(d.heading == frozen && d.quality == Quality::Held);
  assert(d.toggle_hold());
  d.now += 2000000;
  d.update();
  assert(!d.has_heading && d.quality == Quality::Waiting);
  sample(0);
  d.tilt.raw = {300, 0, 0};
  d.update();
  assert(!d.has_heading && d.quality == Quality::Interference);
  sample(0);
  d.tilt.gravity = {0, 9.80665, 0};
  d.update();
  assert(d.quality == Quality::Good); // Upright now uses the back, not a vanishing top edge.
  // Physically rotate both field and gravity. Raising the top through 90 degrees
  // without turning the device must retain the same north-referenced bearing.
  for (int h : {0,42,90,180,270,359}) {
    for (int pitch=0;pitch<=110;++pitch) {
      const float a=pitch/DEG/2;
      Vec g=qrot({0,0,9.80665},-std::sin(a),0,0,std::cos(a));
      Vec m=qrot({-24*std::sin(h/DEG),24*std::cos(h/DEG),-38},-std::sin(a),0,0,std::cos(a));
      int32_t mv[3]={int32_t(m.x*1000),int32_t(m.y*1000),int32_t(m.z*1000)};
      int32_t gv[3]={int32_t(g.x*1e6),int32_t(g.y*1e6),int32_t(g.z*1e6)};
      d.now+=100000;d.has_heading=false;
      d.tilt.acceleration(mv,d.now,d.now);d.tilt.gyroscope(gv,d.now);d.update();
      assert(d.quality==Quality::Good && std::abs(delta(d.heading,h))<.02f);
    }
  }
  d.capture.begin();
  sample(0);
  unsigned captured = d.capture.count;
  d.now += 100000;
  int32_t raw[3] = {0, 24000, -38000}, gravity[3] = {0, 0, 9806650};
  d.tilt.acceleration(raw, d.now, d.now);
  d.update();
  assert(d.capture.count ==
         captured); // The old gravity belongs to another pose.
  d.tilt.gyroscope(gravity, d.now);
  d.update();
  assert(d.capture.count == captured + 1);
  uint8_t bytes[SAVE_SIZE];
  assert(encode(d, bytes, sizeof(bytes)) == sizeof(bytes));
  Model restored;
  assert(decode(restored, bytes, sizeof(bytes)));
  assert(valid_cal(restored.saved[0]));
  // Old mounting calibrations target a different device frame; never load them.
  bytes[3] = '1';
  uint32_t old_checksum = checksum(bytes, SAVE_SIZE - 4);
  std::memcpy(bytes + SAVE_SIZE - 4, &old_checksum, 4);
  assert(!decode(restored, bytes, sizeof(bytes)));
  assert(encode(d, bytes, sizeof(bytes)));
  bytes[12] ^= 1;
  assert(!decode(restored, bytes, sizeof(bytes)));
  assert(!decode(restored, bytes, 4));
  assert(!decode(restored, nullptr, SAVE_SIZE));
  assert(!encode(d, nullptr, SAVE_SIZE));
  d.channel = 1;
  d.saved[1] = {{15, 2, -1}, 44, .01, 17, true};
  d.declination = -12;
  assert(encode(d, bytes, sizeof(bytes)));
  assert(decode(restored, bytes, sizeof(bytes)));
  assert(restored.channel == 1 && restored.declination == -12 &&
         restored.saved[1].rotation == 17 && !restored.held);
  Model system; system.system_heading=true; system.status=Status::Streaming; system.now=1000000;
  system.heading_sample(359000000,system.now); system.update();
  assert(system.has_heading && system.quality==Quality::Good && system.strength==0);
  for(int i=0;i<30;++i){system.now+=100000;system.heading_sample(1000000,system.now);system.update();}
  assert(std::abs(delta(system.heading,1))<.01);
  system.heading_sample(360000000,system.now+1); system.heading_sample(200000000,system.now-1);
  assert(system.absolute_heading==1);
  assert(system.toggle_hold()); system.now+=2000000; system.update(); assert(system.quality==Quality::Held);
  system.toggle_hold(); system.update(); assert(!system.has_heading);
  system.suspend(); assert(system.heading_stamp==0);
  system.saved[0]=d.saved[0]; system.saved[1]=d.saved[1]; system.channel=d.channel;system.declination=d.declination;
  uint8_t original[SAVE_SIZE], compat[SAVE_SIZE]; encode(d,original,sizeof(original));encode(system,compat,sizeof(compat));
  assert(!memcmp(original,compat,sizeof(compat))); // Switching source preserves raw calibrations byte-for-byte.
  puts("Compass: all 24 mounting rotations, spherical calibration, stationary "
       "rejection, cardinal headings, wraparound, hold, stale data, "
       "interference, vertical pose and persistent-state corruption passed");
}
