// SPDX-License-Identifier: CC0-1.0
#include "model.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace calc {
void Model::clear() {
  input[0] = previous[0] = 0;
  completed = false;
}
bool Model::insert(const char *s) {
  if (!s)
    return false;
  const bool fresh =
      completed && (std::isdigit(static_cast<unsigned char>(*s)) || *s == '.' ||
                    std::isalpha(static_cast<unsigned char>(*s)) || *s == '(');
  size_t n = fresh ? 0 : std::strlen(input), z = std::strlen(s);
  if (n + z > EXPR)
    return false;
  if (fresh)
    clear();
  std::memcpy(input + n, s, z + 1);
  completed = false;
  return true;
}
void Model::erase() {
  size_t n = std::strlen(input);
  if (n)
    input[n - 1] = 0;
  completed = false;
}
bool Model::negate() {
  const size_t n = std::strlen(input);
  if (!n)
    return insert("-");
  size_t end = n;
  while (end && (input[end - 1] == '%' || input[end - 1] == '!'))
    --end;
  if (!end || std::strchr("+-*/^(", input[end - 1]))
    return insert("-");
  size_t start = end;
  if (input[end - 1] == ')') {
    int balance = 0;
    while (start) {
      --start;
      if (input[start] == ')')
        ++balance;
      if (input[start] == '(' && --balance == 0)
        break;
    }
    while (start && std::isalpha(static_cast<unsigned char>(input[start - 1])))
      --start;
  } else {
    for (size_t i = 0; i < end;) {
      if (std::isdigit(static_cast<unsigned char>(input[i])) ||
          input[i] == '.') {
        char *tail = nullptr;
        std::strtod(input + i, &tail);
        size_t stop = size_t(tail - input);
        if (stop == end) {
          start = i;
          break;
        }
        i = stop > i ? stop : i + 1;
      } else
        ++i;
    }
  }
  if (start == end) {
    start = 0;
  }
  if (start && input[start - 1] == '-' &&
      (start == 1 || std::strchr("+-*/^(", input[start - 2]))) {
    std::memmove(input + start - 1, input + start, n - start + 1);
    completed = false;
    return true;
  }
  if (n + 3 > EXPR)
    return false;
  char out[EXPR + 1];
  std::memcpy(out, input, start);
  out[start] = 0;
  std::strcat(out, "(-");
  std::strcat(out, input + start);
  std::strcat(out, ")");
  std::strcpy(input, out);
  completed = false;
  return true;
}
Result Model::current() const {
  return evaluate(input, degrees, answer, memory);
}
Result Model::calculate() {
  auto r = current();
  if (!r.ok())
    return r;
  if (completed) {
    answer = r.value;
    return r;
  }
  if (!count || std::strcmp(history[0].expression, input) ||
      history[0].value != r.value) {
    for (unsigned i = 7; i > 0; --i)
      history[i] = history[i - 1];
    std::strcpy(history[0].expression, input);
    history[0].value = r.value;
    history[0].degrees = degrees;
    if (count < 8)
      ++count;
  }
  std::strcpy(previous, input);
  answer = r.value;
  std::snprintf(input, sizeof(input), "%.17g", answer);
  completed = true;
  return r;
}
bool Model::memory_store() {
  auto r = current();
  if (!r.ok())
    return false;
  memory = r.value;
  has_memory = true;
  return true;
}
bool Model::memory_add() {
  auto r = current();
  if (!r.ok() || !std::isfinite(memory + r.value) ||
      std::fabs(memory + r.value) > 1e300)
    return false;
  memory += r.value;
  has_memory = true;
  return true;
}
void Model::memory_clear() {
  memory = 0;
  has_memory = false;
}
void Model::recall(unsigned i) {
  if (i >= count)
    return;
  if (degrees == history[i].degrees)
    std::strcpy(previous, history[i].expression);
  else
    previous[0] = 0;
  std::snprintf(input, sizeof(input), "%.17g", history[i].value);
  completed = true;
}
namespace {
uint32_t hash(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
void put32(uint8_t *p, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = uint8_t(n >> (i * 8));
}
uint32_t get32(const uint8_t *p) {
  uint32_t n = 0;
  for (unsigned i = 0; i < 4; ++i)
    n |= uint32_t(p[i]) << (i * 8);
  return n;
}
void putdouble(uint8_t *p, double v) {
  static_assert(sizeof(double) == 8, "IEEE754 double");
  uint64_t n;
  std::memcpy(&n, &v, 8);
  for (unsigned i = 0; i < 8; ++i)
    p[i] = uint8_t(n >> (i * 8));
}
bool getdouble(const uint8_t *p, double &v) {
  uint64_t n = 0;
  for (unsigned i = 0; i < 8; ++i)
    n |= uint64_t(p[i]) << (i * 8);
  std::memcpy(&v, &n, 8);
  return std::isfinite(v) && std::fabs(v) <= 1e300;
}
bool text(const uint8_t *p) {
  bool ended = false;
  for (unsigned i = 0; i < 97; ++i) {
    unsigned c = p[i];
    if (!c)
      ended = true;
    else if (ended || c > 127 ||
             !(std::isalnum(c) || std::strchr(" +-*/^.!%()", int(c))))
      return false;
  }
  return ended;
}
} // namespace
size_t Model::encode(uint8_t *p, size_t n) const {
  if (!p || n < SAVE_BYTES)
    return 0;
  std::memset(p, 0, SAVE_BYTES);
  std::memcpy(p, "CALC", 4);
  p[4] = 1;
  p[5] = degrees;
  p[6] = has_memory;
  p[7] = uint8_t(count);
  p[8] = uint8_t(category);
  p[9] = uint8_t(from);
  p[10] = uint8_t(to);
  p[11] = uint8_t(practical_mode);
  p[12] = completed;
  putdouble(p + 16, answer);
  putdouble(p + 24, memory);
  std::memcpy(p + 32, input, std::strlen(input));
  for (unsigned i = 0; i < count; ++i) {
    unsigned at = 132 + i * 105;
    std::memcpy(p + at, history[i].expression,
                std::strlen(history[i].expression));
    putdouble(p + at + 97, history[i].value);
    if (history[i].degrees)
      p[13] |= uint8_t(1u << i);
  }
  put32(p + 1004, hash(p, 1004));
  return SAVE_BYTES;
}
bool Model::decode(const uint8_t *p, size_t n) {
  if (!p || n != SAVE_BYTES || std::memcmp(p, "CALC", 4) || p[4] != 1 ||
      p[5] > 1 || p[6] > 1 || p[7] > 8 || p[8] >= QUANTITIES ||
      p[9] >= quantity(p[8]).count || p[10] >= quantity(p[8]).count ||
      p[11] > 3 || p[12] > 1 || (p[7] < 8 && (p[13] >> p[7])) || p[14] ||
      p[15] || get32(p + 1004) != hash(p, 1004) || !text(p + 32))
    return false;
  for (unsigned i = 129; i < 132; ++i)
    if (p[i])
      return false;
  for (unsigned i = 972; i < 1004; ++i)
    if (p[i])
      return false;
  Model next;
  next.degrees = p[5];
  next.has_memory = p[6];
  next.count = p[7];
  next.category = p[8];
  next.from = p[9];
  next.to = p[10];
  next.practical_mode = p[11];
  next.completed = p[12];
  std::memcpy(next.input, p + 32, 97);
  if (!getdouble(p + 16, next.answer) || !getdouble(p + 24, next.memory) ||
      (!next.has_memory && next.memory != 0))
    return false;
  for (unsigned i = 0; i < 8; ++i) {
    unsigned at = 132 + i * 105;
    if (!text(p + at) || !getdouble(p + at + 97, next.history[i].value))
      return false;
    std::memcpy(next.history[i].expression, p + at, 97);
    next.history[i].degrees = (p[13] & (1u << i)) != 0;
    if (i >= next.count && (p[at] || next.history[i].value != 0))
      return false;
  }
  if (next.completed)
    for (unsigned i = 0; i < next.count; ++i)
      if (next.history[i].degrees == next.degrees &&
          next.history[i].value == next.current().value) {
        std::strcpy(next.previous, next.history[i].expression);
        break;
      }
  *this = next;
  return true;
}
} // namespace calc
