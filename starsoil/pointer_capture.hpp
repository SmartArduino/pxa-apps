// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace starsoil_touch {
constexpr int MaxPointers = 10;
struct Capture {
  uint32_t id = 0;
  int x = 0, y = 0, role = 0;
  bool active = false, consumed = false, ui_space = false;
};
struct Captures {
  Capture slots[MaxPointers]{};
  Capture *find(uint32_t id) {
    for (auto &p : slots) if (p.active && p.id == id) return &p;
    return nullptr;
  }
  Capture *begin(uint32_t id) {
    if (find(id)) return nullptr; // Duplicate DOWN must not steal a gesture.
    for (auto &p : slots) if (!p.active) {
      p = {}; p.id = id; p.active = true; return &p;
    }
    return nullptr;
  }
  bool owns(int role) const {
    for (auto &p : slots) if (p.active && p.role == role) return true;
    return false;
  }
  void clear() { for (auto &p : slots) p = {}; }
  void block() { for (auto &p : slots) if (p.active) { p.role = 0; p.consumed = true; } }
};
struct Point { int x, y; };
struct Layout {
  int w, h;
  Point stick() const { return {62, h - 65}; }
  Point use() const { return {w - 43, h - 65}; }
  Point jump() const { return {w - 97, h - 51}; }
  Point talk() const { return {w - (h < 256 ? 66 : 52), h - 119 < 116 ? 116 : h - 119}; }
  static bool circle(int x, int y, Point p, int radius) {
    int dx=x-p.x,dy=y-p.y; return dx*dx+dy*dy<=radius*radius;
  }
  int hit(int x,int y) const {
    if (circle(x,y,use(),26)) return 15;
    if (circle(x,y,jump(),23)) return 14;
    if (circle(x,y,talk(),20)) return 13;
    if (x>=24 && x<w*2/5 && y>=h-130 && y>=100 && y<h-24) return 17;
    return -1;
  }
};
} // namespace starsoil_touch
