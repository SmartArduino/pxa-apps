// SPDX-License-Identifier: CC0-1.0
#include "engine.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace calc {
namespace {
constexpr double PI = 3.1415926535897932384626433832795;
struct Parser {
  const char *p;
  bool degrees;
  double answer, memory;
  Error error = Error::None;
  unsigned depth = 0, steps = 0;
  void fail(Error e) {
    if (error == Error::None)
      error = e;
  }
  void spaces() {
    while (*p == ' ')
      ++p;
  }
  bool take(char c) {
    spaces();
    if (*p == c) {
      ++p;
      return true;
    }
    return false;
  }
  double finite(double v) {
    if (!std::isfinite(v) || std::fabs(v) > 1e300)
      fail(Error::Range);
    return error == Error::None ? v : 0;
  }
  double expression() {
    double v = term();
    while (error == Error::None) {
      if (take('+'))
        v = finite(v + term());
      else if (take('-'))
        v = finite(v - term());
      else
        break;
    }
    return v;
  }
  bool primary_start() {
    spaces();
    return *p == '(' || std::isalpha(static_cast<unsigned char>(*p));
  }
  double term() {
    double v = unary();
    while (error == Error::None) {
      if (take('*'))
        v = finite(v * unary());
      else if (take('/')) {
        double d = unary();
        if (d == 0)
          fail(Error::DivideZero);
        else
          v = finite(v / d);
      } else if (primary_start())
        v = finite(v * unary());
      else
        break;
    }
    return v;
  }
  double unary() {
    if (++steps > 160) {
      fail(Error::Limit);
      return 0;
    }
    if (take('+'))
      return unary();
    if (take('-'))
      return -unary();
    return power();
  }
  double power() {
    double v = postfix();
    if (take('^')) {
      double b = unary();
      if (v == 0 && b < 0)
        fail(Error::DivideZero);
      else if (v == 0 && b == 0)
        fail(Error::Domain);
      else if (v < 0 && std::floor(b) != b)
        fail(Error::Domain);
      else
        v = finite(std::pow(v, b));
    }
    return v;
  }
  double postfix() {
    double v = primary();
    while (error == Error::None) {
      if (take('%'))
        v /= 100;
      else if (take('!')) {
        if (v < 0 || v > 170 || std::floor(v) != v) {
          fail(v > 170 ? Error::Range : Error::Domain);
          return 0;
        }
        double n = v;
        v = 1;
        for (int i = 2; i <= n; ++i)
          v = finite(v * i);
      } else
        break;
    }
    return v;
  }
  double primary() {
    if (error != Error::None)
      return 0;
    if (++depth > 24) {
      fail(Error::Limit);
      return 0;
    }
    spaces();
    double v = 0;
    if (take('(')) {
      v = expression();
      if (!take(')'))
        fail(Error::Parentheses);
    } else if (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.') {
      char *end = nullptr;
      const char *begin = p;
      v = std::strtod(p, &end);
      if (end == begin) {
        fail(Error::Syntax);
      } else
        p = end;
      v = finite(v);
    } else if (std::isalpha(static_cast<unsigned char>(*p))) {
      char name[12]{};
      unsigned n = 0;
      while (std::isalpha(static_cast<unsigned char>(*p))) {
        if (n + 1 < sizeof(name))
          name[n++] = *p;
        else
          fail(Error::Syntax);
        ++p;
      }
      if (!std::strcmp(name, "pi"))
        v = PI;
      else if (!std::strcmp(name, "e"))
        v = std::exp(1.0);
      else if (!std::strcmp(name, "Ans"))
        v = answer;
      else if (!std::strcmp(name, "Mem"))
        v = memory;
      else {
        if (!take('(')) {
          fail(Error::Syntax);
          --depth;
          return 0;
        }
        double a = expression();
        if (!take(')'))
          fail(Error::Parentheses);
        const double t = degrees ? a * PI / 180 : a;
        if (!std::strcmp(name, "sin"))
          v = std::sin(t);
        else if (!std::strcmp(name, "cos"))
          v = std::cos(t);
        else if (!std::strcmp(name, "tan")) {
          if (std::fabs(std::cos(t)) < 1e-14)
            fail(Error::Domain);
          else
            v = std::tan(t);
        } else if (!std::strcmp(name, "asin") || !std::strcmp(name, "acos")) {
          if (std::fabs(a) > 1)
            fail(Error::Domain);
          else {
            v = !std::strcmp(name, "asin") ? std::asin(a) : std::acos(a);
            if (degrees)
              v *= 180 / PI;
          }
        } else if (!std::strcmp(name, "atan")) {
          v = std::atan(a);
          if (degrees)
            v *= 180 / PI;
        } else if (!std::strcmp(name, "sqrt")) {
          if (a < 0)
            fail(Error::Domain);
          else
            v = std::sqrt(a);
        } else if (!std::strcmp(name, "ln") || !std::strcmp(name, "log")) {
          if (a <= 0)
            fail(Error::Domain);
          else
            v = !std::strcmp(name, "ln") ? std::log(a) : std::log10(a);
        } else if (!std::strcmp(name, "exp"))
          v = std::exp(a);
        else if (!std::strcmp(name, "abs"))
          v = std::fabs(a);
        else if (!std::strcmp(name, "floor"))
          v = std::floor(a);
        else if (!std::strcmp(name, "ceil"))
          v = std::ceil(a);
        else if (!std::strcmp(name, "round"))
          v = std::round(a);
        else
          fail(Error::Syntax);
        v = finite(v);
      }
    } else
      fail(!*p ? Error::Syntax : Error::Syntax);
    --depth;
    return v;
  }
};
const Unit length[] = {{"m", 1, 0},     {"km", 1000, 0},  {"cm", .01, 0},
                       {"mm", .001, 0}, {"in", .0254, 0}, {"ft", .3048, 0}};
const Unit mass[] = {{"kg", 1, 0},         {"g", .001, 0},
                     {"mg", .000001, 0},   {"市斤", .5, 0},
                     {"lb", .45359237, 0}, {"oz", .028349523125, 0}};
const Unit temp[] = {{"°C", 1, 273.15}, {"°F", 5.0 / 9, 459.67}, {"K", 1, 0}};
const Unit area[] = {{"m²", 1, 0},
                     {"cm²", .0001, 0},
                     {"km²", 1e6, 0},
                     {"ha", 10000, 0},
                     {"市亩", 2000.0 / 3, 0}};
const Unit volume[] = {
    {"L", 1, 0}, {"mL", .001, 0}, {"m³", 1000, 0}, {"美加仑", 3.785411784, 0}};
const Unit speed[] = {{"m/s", 1, 0},
                      {"km/h", 1.0 / 3.6, 0},
                      {"mph", .44704, 0},
                      {"kn", 1852.0 / 3600, 0}};
const Unit duration[] = {{"s", 1, 0},
                         {"min", 60, 0},
                         {"h", 3600, 0},
                         {"day", 86400, 0},
                         {"week", 604800, 0}};
const Unit data[] = {{"B", 1, 0},           {"kB", 1000, 0},
                     {"MB", 1e6, 0},        {"GB", 1e9, 0},
                     {"KiB", 1024, 0},      {"MiB", 1048576, 0},
                     {"GiB", 1073741824, 0}};
const Quantity quantities[] = {
    {"长度", "Length", length, 6},    {"质量", "Mass", mass, 6},
    {"温度", "Temperature", temp, 3}, {"面积", "Area", area, 5},
    {"体积", "Volume", volume, 4},    {"速度", "Speed", speed, 4},
    {"时间", "Time", duration, 5},    {"数据", "Data", data, 7}};
} // namespace
Result evaluate(const char *s, bool degrees, double answer, double memory) {
  if (!s || !*s)
    return {0, Error::Empty};
  if (std::strlen(s) > EXPR)
    return {0, Error::Limit};
  if (!std::isfinite(answer) || !std::isfinite(memory))
    return {0, Error::Range};
  Parser p{s, degrees, answer, memory};
  double v = p.expression();
  p.spaces();
  if (*p.p && p.error == Error::None)
    p.error = Error::Syntax;
  if (p.error == Error::None && !std::isfinite(v))
    p.error = Error::Range;
  return {p.error == Error::None ? (v == 0 ? 0 : v) : 0, p.error};
}
void format(char *out, size_t n, double v) {
  if (!std::isfinite(v)) {
    std::snprintf(out, n, "--");
    return;
  }
  std::snprintf(out, n, "%.12g", v == 0 ? 0 : v);
}
const char *error_text(Error e, bool en) {
  switch (e) {
  case Error::None:
    return "";
  case Error::Empty:
    return en ? "Enter input" : "请输入算式";
  case Error::Syntax:
    return en ? "Check input" : "请检查算式";
  case Error::Parentheses:
    return en ? "Brackets" : "请检查括号";
  case Error::DivideZero:
    return en ? "Zero divisor" : "不能除以零";
  case Error::Domain:
    return en ? "Domain error" : "超出函数定义域";
  case Error::Range:
    return en ? "Out of range" : "数值超出范围";
  case Error::Limit:
    return en ? "Too complex" : "算式过长或层数过多";
  }
  return "";
}
const Quantity &quantity(unsigned c) {
  return quantities[c < QUANTITIES ? c : 0];
}
Result convert(double v, unsigned c, unsigned from, unsigned to) {
  if (c >= QUANTITIES)
    return {0, Error::Range};
  const auto &q = quantity(c);
  if (from >= q.count || to >= q.count || !std::isfinite(v))
    return {0, Error::Range};
  double base = (v + q.units[from].offset) * q.units[from].scale;
  if (c == 2 && base < -.000000001)
    return {0, Error::Domain};
  double value = base / q.units[to].scale - q.units[to].offset;
  if (!std::isfinite(value) || std::fabs(value) > 1e300)
    return {0, Error::Range};
  return {value == 0 ? 0 : value, Error::None};
}
Result practical(double a, double b, unsigned mode) {
  double v = 0;
  if (!std::isfinite(a) || !std::isfinite(b))
    return {0, Error::Range};
  switch (mode) {
  case 0:
    v = a * b / 100;
    break;
  case 1:
    if (b < 0 || b > 10)
      return {0, Error::Domain};
    v = a * b / 10;
    break;
  case 2:
    if (a == 0)
      return {0, Error::DivideZero};
    v = (b - a) / std::fabs(a) * 100;
    break;
  case 3:
    if (b <= 0 || std::floor(b) != b)
      return {0, Error::Domain};
    v = a / b;
    break;
  default:
    return {0, Error::Domain};
  }
  if (!std::isfinite(v) || std::fabs(v) > 1e300)
    return {0, Error::Range};
  return {v == 0 ? 0 : v, Error::None};
}
} // namespace calc
