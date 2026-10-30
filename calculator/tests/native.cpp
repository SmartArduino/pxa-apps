// SPDX-License-Identifier: CC0-1.0
#include "../../common/pocket-play/runtime.hpp"
#include "../model.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>
namespace test_runtime {
void begin();
void finish();
extern int focus_id, screen_id, menu_requests;
extern uint16_t *pixels;
} // namespace test_runtime
unsigned checks = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                     \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
bool near(double a, double b, double eps = 1e-11) {
  return std::fabs(a - b) <= eps * std::fmax(1, std::fabs(b));
}
void value(const char *s, double expected, bool deg = true) {
  auto r = calc::evaluate(s, deg);
  if (!r.ok())
    std::fprintf(stderr, "bad %s error %u\n", s, unsigned(r.error));
  CHECK(r.ok() && near(r.value, expected));
}
void draw() {
  test_runtime::begin();
  game::draw();
  test_runtime::finish();
}
unsigned focus_pixels() {
  unsigned n = 0;
  for (int i = 0; i < pocket::WIDTH * pocket::HEIGHT; ++i)
    n += test_runtime::pixels[i] == pocket::rgb(239, 193, 119);
  return n;
}
void choose(int id) {
  for (int i = 0; i < 40 && test_runtime::focus_id != id; ++i)
    game::key(pocket::Key::DOWN);
  CHECK(test_runtime::focus_id == id);
  game::key(pocket::Key::A);
  draw();
}
calc::Model state() {
  uint8_t b[calc::SAVE_BYTES];
  CHECK(game::serialize(b, sizeof(b)) == sizeof(b));
  calc::Model m;
  CHECK(m.decode(b, sizeof(b)));
  return m;
}
void install(const calc::Model &m) {
  uint8_t b[calc::SAVE_BYTES];
  CHECK(m.encode(b, sizeof(b)) == sizeof(b));
  game::load(b, sizeof(b));
}
int main() {
  using namespace calc;
  value("2+3*4", 14);
  value("(2+3)*4", 20);
  value("2^3^2", 512);
  value("-2^2", -4);
  value("(-2)^2", 4);
  value("2^-3", .125);
  value("200+10%", 200.1);
  value("200*10%", 20);
  value("5!", 120);
  value("0!", 1);
  value("sqrt(81)+abs(-7)", 16);
  value("sin(30)", .5);
  value("cos(60)", .5);
  value("tan(45)", 1);
  value("asin(0.5)", 30);
  value("acos(0.5)", 60);
  value("atan(1)", 45);
  value("sin(pi/2)", 1, false);
  value("ln(e)", 1);
  value("log(1000)", 3);
  value("exp(0)", 1);
  value("floor(-2.2)+ceil(2.2)", 0);
  value("round(-2.5)", -3);
  value("2(pi)", 2 * std::acos(-1));
  value("2(3+4)", 14);
  value("1e-12+2e-12", 3e-12);
  value("2++3", 5);
  value("(1/3)*3", 1);
  CHECK(evaluate("Ans+Mem", true, 7, 3).value == 10);
  CHECK(evaluate("1/0").error == Error::DivideZero);
  CHECK(evaluate("0^-1").error == Error::DivideZero);
  CHECK(evaluate("sqrt(-1)").error == Error::Domain);
  CHECK(evaluate("log(0)").error == Error::Domain);
  CHECK(evaluate("asin(2)").error == Error::Domain);
  CHECK(evaluate("tan(90)").error == Error::Domain);
  CHECK(evaluate("(-2)^.5").error == Error::Domain);
  CHECK(evaluate("0^0").error == Error::Domain);
  CHECK(evaluate("1e309").error == Error::Range);
  CHECK(evaluate("171!").error == Error::Range);
  CHECK(evaluate("2.5!").error == Error::Domain);
  CHECK(!evaluate(".").ok());
  CHECK(!evaluate("1+").ok());
  CHECK(!evaluate("sin(").ok());
  CHECK(!evaluate("oops(2)").ok());
  CHECK(!evaluate("2 3").ok());
  CHECK(!evaluate("2)").ok());
  CHECK(evaluate("").error == Error::Empty);
  char deep[97]{};
  for (int i = 0; i < 30; ++i)
    deep[i] = '(';
  deep[30] = '1';
  for (int i = 31; i < 61; ++i)
    deep[i] = ')';
  CHECK(evaluate(deep).error == Error::Limit);
  for (int i = 1; i <= 800; ++i) {
    char expr[96];
    std::snprintf(expr, sizeof(expr), "(%d+%d)*.125-%d/7", i, i + 3, i);
    value(expr, (2 * i + 3) * .125 - i / 7.0);
  }
  uint32_t seed = 9;
  for (int i = 0; i < 5000; ++i) {
    char b[97]{};
    unsigned n = unsigned(i) % 97;
    for (unsigned j = 0; j < n; ++j) {
      seed = seed * 1664525 + 1013904223;
      b[j] = "0123456789+-*/^()%.! sinq"[(seed >> 24) % 24];
    }
    auto r = evaluate(b);
    CHECK(!r.ok() || std::isfinite(r.value));
  }
  CHECK(near(convert(1, 0, 4, 0).value, .0254));
  CHECK(near(convert(1, 1, 3, 1).value, 500));
  CHECK(near(convert(0, 2, 0, 1).value, 32));
  CHECK(near(convert(212, 2, 1, 0).value, 100));
  CHECK(near(convert(-273.15, 2, 0, 2).value, 0));
  CHECK(convert(-274, 2, 0, 2).error == Error::Domain);
  CHECK(near(convert(1, 3, 4, 0).value, 2000.0 / 3));
  CHECK(near(convert(1, 4, 3, 0).value, 3.785411784));
  CHECK(near(convert(36, 5, 1, 0).value, 10));
  CHECK(near(convert(1, 6, 3, 2).value, 24));
  CHECK(near(convert(1, 7, 2, 0).value, 1000000));
  CHECK(near(convert(1, 7, 5, 0).value, 1048576));
  for (unsigned c = 0; c < QUANTITIES; ++c)
    for (unsigned a = 0; a < quantity(c).count; ++a)
      for (unsigned b = 0; b < quantity(c).count; ++b) {
        auto there = convert(100, c, a, b),
             back = convert(there.value, c, b, a);
        CHECK(there.ok() && back.ok() && near(back.value, 100));
      }
  CHECK(practical(200, 15, 0).value == 30);
  CHECK(practical(200, 8, 1).value == 160);
  CHECK(practical(100, 120, 2).value == 20);
  CHECK(near(practical(100, 3, 3).value, 100.0 / 3));
  CHECK(!practical(100, 2.5, 3).ok());
  CHECK(!practical(0, 1, 2).ok());
  CHECK(!practical(100, 11, 1).ok());
  Model sign;
  sign.insert("1+2");
  CHECK(sign.negate() && sign.current().value == -1);
  sign.clear();
  sign.insert("5-3");
  CHECK(sign.negate() && sign.current().value == 8);
  sign.clear();
  sign.insert("sin(30)");
  CHECK(sign.negate() && near(sign.current().value, -.5));
  CHECK(evaluate("1e-12").value == 1e-12);
  Model m;
  CHECK(m.insert("1/3"));
  CHECK(m.calculate().ok());
  CHECK(m.insert("*3"));
  CHECK(m.calculate().ok() && m.answer == 1);
  unsigned count = m.count;
  CHECK(m.calculate().ok() && m.count == count);
  CHECK(m.memory_store());
  CHECK(m.memory_add() && m.memory == 2);
  m.recall(1);
  CHECK(near(m.current().value, 1.0 / 3));
  m.memory_clear();
  CHECK(!m.has_memory);
  for (int i = 0; i < 12; ++i) {
    m.clear();
    char b[20];
    std::snprintf(b, sizeof(b), "%d*7", i);
    CHECK(m.insert(b) && m.calculate().ok());
  }
  CHECK(m.count == 8 && m.history[0].value == 77);
  m.degrees = false;
  m.category = 7;
  m.from = 5;
  m.to = 2;
  uint8_t saved[SAVE_BYTES], other[SAVE_BYTES];
  CHECK(m.encode(saved, sizeof(saved)) == SAVE_BYTES);
  Model restored;
  CHECK(restored.decode(saved, sizeof(saved)));
  CHECK(restored.answer == m.answer && restored.count == 8 &&
        !restored.degrees && restored.category == 7);
  CHECK(restored.encode(other, sizeof(other)) == sizeof(other) &&
        !std::memcmp(saved, other, sizeof(saved)));
  for (unsigned bit = 0; bit < SAVE_BYTES * 8; ++bit) {
    std::memcpy(other, saved, sizeof(other));
    other[bit / 8] ^= uint8_t(1 << (bit % 8));
    CHECK(!restored.decode(other, sizeof(other)));
  }
  for (unsigned n = 0; n < SAVE_BYTES; ++n)
    CHECK(!restored.decode(saved, n));
  CHECK(!restored.decode(nullptr, 0));
  for (auto size : {std::pair<int, int>{240, 284},
                    {336, 224},
                    {240, 320},
                    {240, 360},
                    {284, 284},
                    {288, 226}})
    for (bool en : {false, true}) {
      pocket::WIDTH = size.first;
      pocket::HEIGHT = size.second;
      pocket::set_english(en);
      game::init();
      draw();
      CHECK(focus_pixels() == 0); // Opening the app never assumes a controller.
      game::key(pocket::Key::DOWN);
      draw();
      CHECK(focus_pixels() > 0);
      game::pointer(pocket::Phase::DOWN, -20, -20);
      game::pointer(pocket::Phase::UP, -20, -20);
      draw();
      CHECK(focus_pixels() == 0); // Returning to touch removes keyboard focus.
      choose(113);
      choose(115);
      choose(114);
      choose(107);
      choose(108);
      choose(119);
      CHECK(state().answer == 14);
      choose(2);
      CHECK(test_runtime::screen_id == 2);
      choose(500);
      CHECK(state().current().value == 14);
      choose(1);
      choose(200);
      choose(219);
      choose(106);
      choose(117);
      choose(119);
      CHECK(near(state().answer, 1)); // sin(90) closes itself
      choose(1);
      choose(218);
      choose(200);
      CHECK(state().has_memory);
      choose(203);
      CHECK(!state().has_memory);
      choose(219);
      choose(3);
      choose(30);
      draw();
      choose(301);
      choose(301);
      choose(302);
      choose(303);
      choose(700);
      choose(713);
      choose(705);
      draw();
      choose(714);
      draw();
      choose(715);
      choose(31);
      choose(401);
      choose(402);
      choose(700);
      choose(703);
      choose(708);
      choose(403);
      choose(701);
      draw();
      choose(401);
      choose(401);
      draw();
      choose(715);
      choose(32);
      draw();
      choose(552);
      draw();
      choose(9);
      choose(2);
      choose(522);
      CHECK(test_runtime::focus_id == 550);
      choose(550);
      CHECK(state().count > 0);
      choose(522);
      choose(551);
      CHECK(state().count == 0);
      choose(9);
      choose(6);
      CHECK(test_runtime::menu_requests > 0);
      Model longvalue;
      longvalue.insert("12345678901234567890/3");
      install(longvalue);
      draw();
      choose(119);
      draw();
    }
  std::printf("PASS calculator: %u checks; precedence, scientific domains, "
              "conversion roundtrips, persistence corruption, bilingual "
              "responsive UI and Gen4 keys\n",
              checks);
}
