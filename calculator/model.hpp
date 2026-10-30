// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "engine.hpp"
#include <stdint.h>
namespace calc {
struct Entry {
  char expression[EXPR + 1]{};
  double value = 0;
  bool degrees = true;
};
struct Model {
  char input[EXPR + 1]{}, previous[EXPR + 1]{};
  double answer = 0, memory = 0;
  bool degrees = true, completed = false, has_memory = false;
  Entry history[8]{};
  unsigned count = 0;
  unsigned category = 0, from = 0, to = 1, practical_mode = 0;
  void clear();
  bool insert(const char *token);
  void erase();
  bool negate();
  Result calculate();
  Result current() const;
  bool memory_store();
  bool memory_add();
  void memory_clear();
  void recall(unsigned index);
  size_t encode(uint8_t *out, size_t capacity) const;
  bool decode(const uint8_t *data, size_t size);
};
constexpr size_t SAVE_BYTES = 1008;
} // namespace calc
