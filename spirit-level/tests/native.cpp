// SPDX-License-Identifier: CC0-1.0
#include "../../common/pocket-play/runtime.hpp"
#include "../model.hpp"
#include <assert.h>
#include <initializer_list>
#include <stdio.h>
#include <utility>
#include <string.h>
namespace test_runtime {
extern uint32_t clock;
void begin();
void finish();
extern int focus_id, screen_id, menu_requests;
bool visible(const char *);
} // namespace test_runtime
using namespace level;
static unsigned checks = 0;
#define CHECK(v)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(v)) {                                                                \
      fprintf(stderr, "CHECK %u: %s\n", __LINE__, #v);                         \
      assert(v);                                                               \
    }                                                                          \
  } while (0)
static bool near(float a, float b, float eps = .003f) {
  return fabsf(a - b) < eps;
}
static Vector pose(float x, float y = 0) {
  float sx = sinf(x / DEG), sy = sinf(y / DEG);
  return {sy, -sx, sqrtf(fmaxf(0, 1 - sx * sx - sy * sy))};
}
static void feed(Model &m, Vector g, unsigned n = 30) {
  m.status = Status::Streaming;
  for (unsigned i = 0; i < n; ++i) {
    m.now += 50000;
    int32_t v[] = {int32_t(g.x * 9806650), int32_t(g.y * 9806650),
                   int32_t(g.z * 9806650)};
    m.tilt.acceleration(v, m.now, m.now);
  }
}
static void draw() {
  test_runtime::begin();
  game::draw();
  test_runtime::finish();
}
static void tap(int x, int y) {
  game::pointer(pocket::Phase::DOWN, x, y);
  game::pointer(pocket::Phase::UP, x, y);
}
static void prefs(uint8_t *b) { CHECK(game::serialize(b, 64) == 64); }
static void seal(uint8_t *p) {
  uint32_t h = 2166136261;
  for (unsigned i = 0; i < 60; ++i)
    h = (h ^ p[i]) * 16777619;
  for (unsigned i = 0; i < 4; ++i)
    p[60 + i] = uint8_t(h >> (8 * i));
}
int main() {
  CHECK(bubble_blend(0) == 0);
  CHECK(bubble_blend(100) > 0 && bubble_blend(100) < .99f);
  for (unsigned step : {16u, 20u, 33u, 50u, 80u, 100u}) {
    float value = 0;
    for (unsigned elapsed = 0; elapsed < 800;) {
      const unsigned dt = step < 800 - elapsed ? step : 800 - elapsed;
      value += (1 - value) * bubble_blend(dt);
      elapsed += dt;
      CHECK(value >= 0 && value < 1);
    }
    CHECK(near(value, -expm1f(-10.f), .000001f));
  }
  // Exercise the unchanged Host-frame projection, with no board compensation.
  for (unsigned axis = 0; axis < 2; ++axis) {
    Model axis_meter;
    feed(axis_meter, axis == 0 ? Vector{0, -.5f, .8660254f}
                              : Vector{.5f, 0, .8660254f});
    const auto r = axis_meter.read();
    CHECK(near(axis == 0 ? r.x : r.y, 30));
    CHECK(near(axis == 0 ? r.y : r.x, 0));
    // Host -X tilts the displayed X; Host -Y tilts the displayed Y.
    Model gyro_meter;
    feed(gyro_meter, {0, 0, 1});
    int32_t rate[] = {axis == 0 ? -523599 : 0, axis == 1 ? -523599 : 0, 0};
    CHECK(gyro_meter.tilt.gyroscope(rate, gyro_meter.now + 1));
    gyro_meter.now += 50001;
    CHECK(gyro_meter.tilt.gyroscope(rate, gyro_meter.now));
    const auto turning = gyro_meter.read();
    CHECK((axis == 0 ? turning.x : turning.y) > 1.4f);
    CHECK(near(axis == 0 ? turning.y : turning.x, 0));
  }
  Model reverse_y;
  feed(reverse_y, {-.5f, 0, .8660254f});
  CHECK(near(reverse_y.read().x, 0) && near(reverse_y.read().y, -30));
  for (float angle :
       {-89.9f, -80.f, -45.f, -10.f, -.1f, 0.f, .1f, 10.f, 45.f, 80.f, 89.9f}) {
    Model m;
    feed(m, pose(angle));
    auto r = m.read();
    CHECK(r.valid && r.stable);
    CHECK(near(r.x, angle) && near(r.y, 0) && near(r.total, fabsf(angle)));
    m.tilt.suspend();
    feed(m, pose(angle));
    CHECK(near(m.read().x, angle)); // Resume never implicitly zeroes a slope.
  }
  Model tiny;
  feed(tiny, pose(.1f));
  CHECK(tiny.read().x > .09f && tiny.read().x < .11f);
  Model m;
  feed(m, pose(12, 8));
  auto original = m.read();
  CHECK(m.toggle_relative() && m.read().relative && m.read().total < .001f);
  CHECK(m.toggle_relative() && near(m.read().x, original.x));
  CHECK(m.toggle_hold());
  auto held = m.read();
  feed(m, pose(-10, 5), 80);
  CHECK(m.read().held && near(m.read().x, held.x));
  m.now += 1000000;
  CHECK(m.read().valid);
  CHECK(m.toggle_hold() && !m.read().valid);
  feed(m, pose(10), 80);
  m.prefs.rotation = 1;
  CHECK(near(m.read().y, 10) && near(m.read().x, 0));
  m.prefs.rotation = 0;
  // Alignment maps a known level pose to Z; vectors remain unit length.
  for (unsigned i = 0; i < 1000; ++i) {
    float a = float(i) * .17f, b = float(i) * .113f;
    Vector v = normalized({sinf(a), cosf(b), cosf(a + b)});
    Vector aligned = align(v, v);
    CHECK(near(aligned.x, 0) && near(aligned.y, 0) && near(aligned.z, 1));
  }
  for (float degrees : {179.f, 179.5f, 179.9f, 179.99f, 180.f, -179.99f}) {
    const float a = degrees / DEG;
    for (float bearing : {0.f, .7f, 1.57f, 2.4f}) {
      Vector v = {sinf(a) * cosf(bearing), sinf(a) * sinf(bearing), cosf(a)};
      const Vector zero = align(v, v);
      CHECK(fabsf(zero.x) < .000001f && fabsf(zero.y) < .000001f &&
            near(zero.z, 1));
      const Vector other = normalized({.2f, .3f, .8f});
      CHECK(near(dot(align(other, v), zero), dot(other, v)));
    }
  }

  Calibration c;
  c.begin();
  uint64_t stamp = 1000000;
  Vector tilted = pose(2);
  for (unsigned i = 0; i < 45; ++i) {
    stamp += 50000;
    c.sample(tilted, stamp, true);
  }
  CHECK(c.stage == Calibration::Complete && c.progress() == 1);
  Model meter;
  feed(meter, tilted);
  CHECK(near(meter.read().x, 2));
  CHECK(meter.apply(c) && meter.read().total < .001f);
  meter.clear_calibration();
  CHECK(near(meter.read().x, 2));
  c = {};
  c.begin();
  for (unsigned i = 0; i < 100; ++i) {
    stamp += 50000;
    c.sample(i % 2 ? pose(2) : pose(4), stamp, true);
  }
  CHECK(c.stage != Calibration::Complete);
  c.sample(tilted, stamp + 50000, false);
  CHECK(c.count == 0);
  c.sample(pose(20), stamp + 100000, true);
  CHECK(c.count == 0);
  CHECK(!meter.apply(c));
  // Serialization is independent of UI and transient readings.
  Preferences p;
  uint8_t good[64], bad[65];
  CHECK(encode(p, good, 64) == 64 && encode(p, bad, 63) == 0);
  Preferences target;
  CHECK(decode(target, good, 64));
  for (unsigned bit = 0; bit < 512; ++bit) {
    memcpy(bad, good, 64);
    bad[bit / 8] ^= uint8_t(1u << (bit % 8));
    Preferences unchanged = target;
    CHECK(!decode(target, bad, 64));
    CHECK(target.rotation == unchanged.rotation &&
          target.alignment.z == unchanged.alignment.z);
  }
  for (unsigned size = 0; size < 64; ++size)
    CHECK(!decode(target, good, size));
  CHECK(!decode(target, nullptr, 64) && !decode(target, good, 65));
  for (unsigned offset : {5u, 6u, 7u, 8u, 9u, 10u, 11u, 24u, 59u}) {
    memcpy(bad, good, 64);
    bad[offset] = 255;
    seal(bad);
    CHECK(!decode(target, bad, 64));
  }
  char text[40];
  format_value(text, sizeof(text), .04f, 0, true);
  CHECK(!strcmp(text, "0.0°"));
  format_value(text, sizeof(text), 45, 1, true);
  CHECK(!strcmp(text, "+100.0%"));
  format_value(text, sizeof(text), 90, 1, true);
  CHECK(strstr(text, "∞"));
  CHECK(near(edge_error(89, false), -1) && near(edge_error(-89, false), 1) &&
        near(edge_error(89, true), 89));
  // Real app hooks, both locales, no-sensor states, gesture cancellation and
  // settings.
  for (bool en : {false, true}) {
    pocket::set_english(en);
    game::init();
    draw();
    CHECK(test_runtime::visible(en ? "NO MOTION SENSOR" : "未检测到体感"));
    tap(55, 208);
    CHECK(!device.held);
    feed(device, pose(.2f));
    game::tick(100);
    draw();
    tap(55, 208);
    CHECK(device.held);
    tap(145, 208);
    CHECK(device.relative);
    draw();
    tap(55, 208);
    CHECK(!device.held);
    auto before = device.prefs;
    game::pointer(pocket::Phase::DOWN, 230, 208);
    game::pointer(pocket::Phase::CANCEL, 230, 208);
    game::pointer(pocket::Phase::UP, 230, 208);
    CHECK(device.prefs.mode == before.mode);
    tap(230, 208);
    CHECK(device.prefs.mode == 1);
    draw();
    tap(220, 37);
    CHECK(device.prefs.axis == 1);
    draw();
    tap(210, 13);
    draw();
    tap(120, 64);
    tap(120, 88);
    tap(120, 112);
    draw();
    uint8_t saved[64];
    prefs(saved);
    CHECK(saved[6] == 1 && saved[7] == 2 && saved[8] == 1);
    tap(120, 136);
    draw();
    tap(82, 191);
    draw();
    for (unsigned i = 0; i < 70; ++i) {
      feed(device, pose(2), 1);
      game::tick(50);
    }
    draw();
    CHECK(!device.prefs.calibrated);
    tap(208, 191);
    CHECK(!device.prefs.calibrated);
    draw();
    tap(120, 160);
    draw();
    tap(144, 201);
    draw();
    game::key(pocket::Key::B);
    draw();
    game::reset();
    CHECK(!device.relative && !device.held);
    game::init();
    game::load(saved, 64);
    CHECK(device.prefs.rotation == 1 && device.prefs.unit == 1 &&
          !device.held && !device.relative);
    draw();
  }
  // Gen4's three raw keys reach every page in its native portrait layout.
  for (auto size : {std::pair<int,int>{240,284}, {240,320}, {336,224}, {284,284}, {288,234}, {240,360}})
  for (bool en : {false, true}) {
    pocket::WIDTH = size.first;
    pocket::HEIGHT = size.second;
    pocket::set_english(en);
    game::init();
    feed(device, pose(0));
    game::tick(50);
    draw();
    auto choose = [](int id) {
      for (unsigned i = 0; i < 12 && test_runtime::focus_id != id; ++i) {
        game::key(pocket::Key::DOWN);
        game::tick(20);
      }
      CHECK(test_runtime::focus_id == id);
      game::key(pocket::Key::A);
      game::tick(20);
    };
    choose(0);
    CHECK(device.held);
    choose(0);
    CHECK(!device.held);
    choose(1);
    CHECK(device.relative);
    choose(1);
    CHECK(!device.relative);
    choose(2);
    CHECK(device.prefs.mode == 1);
    draw();
    choose(9);
    CHECK(device.prefs.axis == 1);
    draw();
    choose(3);
    draw();
    choose(20);
    choose(21);
    choose(22);
    draw();
    choose(23);
    draw();
    choose(30);
    for (unsigned i = 0; i < 70; ++i) {
      feed(device, pose(2), 1);
      game::tick(50);
    }
    draw();
    choose(31);
    CHECK(!device.prefs.calibrated);
    choose(23);
    choose(30);
    for (unsigned i = 0; i < 70; ++i) {
      feed(device, pose(2), 1);
      game::tick(50);
    }
    choose(30);
    CHECK(device.prefs.calibrated);
    draw();
    choose(24);
    draw();
    choose(52);
    draw();
    choose(50);
    choose(25);
    draw();
    choose(6);
    CHECK(test_runtime::menu_requests > 0);
    game::key(pocket::Key::UP);
    game::tick(20);
    draw();
  }
  pocket::WIDTH = 288;
  pocket::HEIGHT = 224;
  pocket::set_english(false);
  printf("PASS Spirit Level %u checks: absolute angles including 0.1°, resume, "
         "references/hold, 3D alignment, stable capture, "
         "persistence/corruption, bilingual layout and actual input hooks\n",
         checks);
}
