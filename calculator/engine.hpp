// SPDX-License-Identifier: CC0-1.0
#pragma once
#include <stddef.h>
namespace calc {
constexpr size_t EXPR = 96;
enum class Error {
  None,
  Empty,
  Syntax,
  Parentheses,
  DivideZero,
  Domain,
  Range,
  Limit
};
struct Result {
  double value = 0;
  Error error = Error::None;
  bool ok() const { return error == Error::None; }
};
Result evaluate(const char *expression, bool degrees = true, double answer = 0,
                double memory = 0);
const char *error_text(Error error, bool english);
void format(char *out, size_t size, double value);
struct Unit {
  const char *name;
  double scale, offset;
};
struct Quantity {
  const char *zh, *en;
  const Unit *units;
  unsigned count;
};
constexpr unsigned QUANTITIES = 8;
const Quantity &quantity(unsigned category);
Result convert(double value, unsigned category, unsigned from, unsigned to);
Result practical(double a, double b, unsigned mode);
} // namespace calc
