// SPDX-License-Identifier: MIT
#include "../../common/pocket-play/runtime.hpp"
#include "../model.hpp"
#include <cassert>
#include <cstdio>
#include <utility>
namespace test_runtime {
void begin();
void finish();
bool visible(const char *);
} // namespace test_runtime
using namespace compass;
static void draw() {
  test_runtime::begin();
  game::draw();
  test_runtime::finish();
}
static void tap(int x, int y) {
  game::pointer(pocket::Phase::DOWN, x, y);
  game::pointer(pocket::Phase::UP, x, y);
}
static void feed(float heading) {
  device.now += 100000;
  int32_t m[3] = {int32_t(24000 * std::cos(heading / DEG)),
                  int32_t(24000 * std::sin(heading / DEG)), -38000};
  int32_t g[3] = {0, 0, 9806650};
  device.tilt.acceleration(m, device.now, device.now);
  device.tilt.gyroscope(g, device.now);
  game::tick(100);
}
int main() {
  for (unsigned ms : {0u, 16u, 20u, 33u, 50u, 100u, 1000u}) {
    float a = needle_blend(ms);
    assert(std::isfinite(a) && a >= 0 && a <= 1);
  }
  for (auto size : {std::pair<int, int>{284, 284},
                    {336, 224},
                    {288, 224},
                    {240, 284},
                    {240, 320},
                    {288, 234},
                    {240, 360}}) {
    pocket::WIDTH = size.first;
    pocket::HEIGHT = size.second;
    for (bool en : {false, true}) {
      pocket::set_english(en);
      game::init();
      draw();
      assert(test_runtime::visible(en ? "NO MAGNETOMETER" : "未检测到磁力计"));
      unsigned retry = device.retry;
      tap(pocket::WIDTH / 6, pocket::HEIGHT - 22);
      assert(device.retry > retry);
      device.status = Status::Streaming;
      device.gyro = true;
      device.saved[0] = {{}, 45, 0, 0, true};
      for (float h : {0.f, 45.f, 90.f, 180.f, 270.f, 359.f, 1.f}) {
        for (unsigned n = 0; n < 20; ++n)
          feed(h);
        assert(device.has_heading);
        draw();
      }
      tap(pocket::WIDTH / 6, pocket::HEIGHT - 22);
      assert(device.held);
      draw();
      tap(pocket::WIDTH / 6, pocket::HEIGHT - 22);
      assert(!device.held);
      tap(pocket::WIDTH / 2, pocket::HEIGHT - 22);
      draw();
      tap(pocket::WIDTH * 3 / 4, pocket::HEIGHT - 24);
      assert(device.capture.collecting);
      draw();
      tap(pocket::WIDTH / 4, pocket::HEIGHT - 24);
      assert(!device.capture.collecting && valid_cal(device.saved[0]));
      tap(pocket::WIDTH * 5 / 6, pocket::HEIGHT - 22);
      draw();
      tap(pocket::WIDTH - 29, 94);
      assert(device.declination == 1);
      draw();
      tap(pocket::WIDTH / 2, pocket::HEIGHT - 24);
      feed(45);
      draw();
      tap(pocket::WIDTH * 5 / 6, pocket::HEIGHT - 22);
      int sy = std::min(146, pocket::HEIGHT - 104);
      tap(pocket::WIDTH / 2, sy + 12);
      draw();
      game::pointer(pocket::Phase::DOWN, pocket::WIDTH * 3 / 4,
                    pocket::HEIGHT - 24);
      game::pointer(pocket::Phase::CANCEL, 0, 0);
      game::pointer(pocket::Phase::UP, pocket::WIDTH * 3 / 4,
                    pocket::HEIGHT - 24);
      assert(valid_cal(device.saved[0]));
      tap(pocket::WIDTH / 4, pocket::HEIGHT - 24);
      draw();
      tap(pocket::WIDTH / 2, sy + 44);
      draw();
      tap(pocket::WIDTH / 2, pocket::HEIGHT - 24);
      tap(pocket::WIDTH / 2, sy + 12);
      tap(pocket::WIDTH * 3 / 4, pocket::HEIGHT - 24);
      assert(!valid_cal(device.saved[0]));
      draw();
      tap(pocket::WIDTH / 2, 62);
      assert(device.channel == 1);
      draw();
      game::key(pocket::Key::B);
      feed(0);
      draw();
      game::key(pocket::Key::RIGHT);
      game::key(pocket::Key::A);
      draw();
      game::key(pocket::Key::RIGHT);
      game::key(pocket::Key::A);
      assert(device.capture.collecting);
      game::key(pocket::Key::B);
      assert(!device.capture.collecting);
      device.status = Status::Denied;
      draw();
      device.status = Status::NoGravity;
      draw();
    }
  }
  puts("Compass native: finite pointer animation, bilingual responsive text "
       "bounds/overlaps, real input hooks, hold, cancel, clear, settings and "
       "retry passed");
}
