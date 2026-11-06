// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
namespace farm_ui {
struct Rect {
  int x, y, w, h;
  bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
  int center_x() const { return x + w / 2; }
  int center_y() const { return y + h / 2; }
};
struct Layout {
  int width, height;
  int navigation_left = 0, navigation_right = 0, navigation_bottom = 0;
  int left_margin() const { return std::max(20, navigation_left + 6); }
  int right_margin() const { return std::max(20, navigation_right + 6); }
  int footer_margin() const { return std::max(14, navigation_bottom + 4); }
  int tool_width() const {
    return std::min(32, (width - left_margin() - right_margin() - 4) / 7);
  }
  Rect title_logo() const { return {(width - 174) / 2, 18, 174, 62}; }
  Rect title_enter() const { return {(width - 164) / 2, height - 54, 164, 30}; }
  Rect calendar() const { return {6, 5, 110, 34}; }
  Rect header_button(int index) const {
    return {width - 84 + index * 27, 5, 24, 25};
  }
  Rect vitals() const { return {width - 84, 35, 78, 30}; }
  Rect belt() const {
    int w = tool_width() * 7 + 4;
    int left = left_margin(), usable = width - left - right_margin();
    return {left + (usable - w) / 2, height - footer_margin() - 34, w, 34};
  }
  Rect tool(int index) const {
    auto r = belt();
    return {r.x + 2 + index * tool_width(), r.y + 2, tool_width(), 30};
  }
  Rect caption(int w) const { return {belt().x + 2, belt().y - 19, w, 18}; }
  Rect joystick() const { return {left_margin(), belt().y - 83, 48, 50}; }
  Rect action() const {
    return {width - right_margin() - 42, belt().y - 72, 42, 32};
  }
  Rect modal() const {
    return {(width - 228) / 2, (height - 210) / 2, 228, 210};
  }
  Rect close() const {
    auto r = modal();
    return {r.x + 199, r.y + 7, 20, 20};
  }
  Rect shop_buy(int index) const {
    auto r = modal();
    return {r.x + 164, r.y + 39 + index * 27, 51, 23};
  }
  Rect upgrade() const {
    auto r = modal();
    return {r.x + 12, r.y + 174, 202, 24};
  }
  Rect bag_action(int index) const {
    auto r = modal();
    return {r.x + (index ? 115 : 12), r.y + 174, index ? 100 : 98, 24};
  }
  Rect bag_slot(int index) const {
    auto r = modal();
    return {r.x + 12 + (index % 7) * 29, r.y + 36 + (index / 7) * 28, 27, 27};
  }
  Rect bag_upgrade() const {
    auto r = modal(); return {r.x + 160, r.y + 151, 55, 20};
  }
  Rect setting(int index) const {
    auto r = modal(); return {r.x + 12, r.y + 38 + index * 32, 202, 27};
  }
  Rect destination(int index) const {
    auto r = modal();
    return {r.x + 12 + (index % 2) * 105, r.y + 40 + (index / 2) * 29, 100, 25};
  }
  Rect confirm(int top = 169, int h = 28) const {
    auto r = modal();
    return {r.x + 24, r.y + top, 180, h};
  }
};
} // namespace farm_ui
