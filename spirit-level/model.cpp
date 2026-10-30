// SPDX-License-Identifier: CC0-1.0
#include "model.hpp"
#include <stdio.h>
#include <string.h>
namespace level {
static uint32_t hash(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
static void put(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    p[i] = uint8_t(v >> (8 * i));
}
static uint32_t get(const uint8_t *p) {
  uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i)
    v |= uint32_t(p[i]) << (8 * i);
  return v;
}
size_t encode(const Preferences &o, uint8_t *p, size_t n) {
  if (!p || n < SAVE_BYTES)
    return 0;
  memset(p, 0, SAVE_BYTES);
  memcpy(p, "SPLV", 4);
  p[4] = 1;
  p[5] = o.mode;
  p[6] = o.unit;
  p[7] = o.tolerance;
  p[8] = o.rotation;
  p[9] = o.axis;
  p[10] = o.calibrated;
  const float v[] = {o.alignment.x, o.alignment.y, o.alignment.z};
  for (unsigned i = 0; i < 3; ++i)
    put(p + 12 + i * 4, uint32_t(int32_t(v[i] * 1073741824.f)));
  put(p + 60, hash(p, 60));
  return SAVE_BYTES;
}
bool decode(Preferences &o, const uint8_t *p, size_t n) {
  if (!p || n != SAVE_BYTES || memcmp(p, "SPLV", 4) || p[4] != 1 || p[5] > 1 ||
      p[6] > 1 || p[7] > 3 || p[8] > 3 || p[9] > 1 || p[10] > 1 || p[11] ||
      get(p + 60) != hash(p, 60))
    return false;
  for (unsigned i = 24; i < 60; ++i)
    if (p[i])
      return false;
  Preferences next;
  next.mode = p[5];
  next.unit = p[6];
  next.tolerance = p[7];
  next.rotation = p[8];
  next.axis = p[9];
  next.calibrated = p[10];
  float *v[] = {&next.alignment.x, &next.alignment.y, &next.alignment.z};
  for (unsigned i = 0; i < 3; ++i) {
    uint32_t b = get(p + 12 + i * 4);
    int64_t signed_value =
        b <= INT32_MAX ? int64_t(b) : int64_t(b) - 4294967296LL;
    *v[i] = float(signed_value) / 1073741824.f;
  }
  float length = dot(next.alignment, next.alignment);
  if (fabsf(length - 1) > .0001f || next.alignment.z < .9658f)
    return false;
  if (!next.calibrated &&
      (next.alignment.x != 0 || next.alignment.y != 0 || next.alignment.z != 1))
    return false;
  o = next;
  return true;
}
float edge_error(float angle, bool relative) {
  return !relative && fabsf(angle) > 45 ? angle - copysignf(90, angle) : angle;
}
void format_value(char *out, size_t size, float angle, unsigned unit,
                  bool valid) {
  if (!valid) {
    snprintf(out, size, "--.-%s", unit ? "%" : "°");
    return;
  }
  float value = unit ? tanf(angle / DEG) * 100.f : angle;
  if (unit && fabsf(angle) >= 89.95f) {
    snprintf(out, size, "%s∞", angle < 0 ? "−" : "+");
    return;
  }
  if (fabsf(value) > 999.9f) {
    snprintf(out, size, "%s999%%", value < 0 ? "<−" : ">");
    return;
  }
  if (fabsf(value) < .05f)
    value = 0;
  snprintf(out, size, "%s%.1f%s", value > 0 ? "+" : "", double(value),
           unit ? "%" : "°");
}
} // namespace level
