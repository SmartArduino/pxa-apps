// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../common/milky-frog/layout.hpp"
namespace md_ui {
using milky::Rect;
struct Layout {
  Rect pause, wave, stock, dock[4], drawer, tabs[7], itemRow;
  Rect map, mapToggle, tools, tool[6], inventory, info;
  int button, centerY;
};
inline Layout layout(Rect safe, Rect systemMenu) {
  Layout l{};
  l.button = std::clamp(std::min(safe.w, safe.h) / 10, 30, 44);
  int x = safe.x + 4, right = safe.right() - 4, w = right - x;
  int y = safe.y + 4, n = l.button;
  l.pause = {x, y, n, n};
  int headerRight = std::min(right, systemMenu.x - 4);
  int content = std::max(90, headerRight - x - n - 8);
  int wave = std::clamp(content * 43 / 100, 66, 122);
  l.wave = {x + n + 4, y, wave, n};
  l.stock = {l.wave.right() + 4, y, headerRight - l.wave.right() - 4, n};
  int cell = (w - 12) / 4, bottom = safe.bottom() - 4;
  for (int i = 0; i < 4; ++i)
    l.dock[i] = {x + i * (cell + 4), bottom - n, cell, n};
  l.dock[3].w = right - l.dock[3].x;
  l.drawer = {x, bottom - n - 4 - (2 * n + 18), w, 2 * n + 18};
  for (int i = 0; i < 7; ++i) {
    int left = x + i * w / 7, end = x + (i + 1) * w / 7;
    l.tabs[i] = {left, l.drawer.y, end - left, n};
  }
  l.itemRow = {x, l.drawer.y + n, w, n + 18};
  int scale = n >= 40 ? 2 : 1;
  l.map = {right - 48 * scale, y + n + 6, 48 * scale, 48 * scale};
  l.mapToggle = {right - n, y + n + 6, n, n};
  int toolH = 3 * n + 12;
  l.tools = {x, bottom - n - toolH - 4, w, toolH};
  for (int i = 0; i < 6; ++i)
    l.tool[i] = {x + 4 + (i % 2) * (w - 4) / 2,
                 l.tools.y + 4 + (i / 2) * (n + 2), (w - 12) / 2, n};
  l.inventory = {x, y + n + 4, w, 100};
  l.info = {x, l.dock[0].y - 26, w, 22};
  l.centerY = (l.pause.bottom() + l.dock[0].y) / 2;
  return l;
}
inline Rect item(const Layout &l, int index, int count) {
  int x = l.itemRow.x + index * l.itemRow.w / std::max(1, count);
  int end = l.itemRow.x + (index + 1) * l.itemRow.w / std::max(1, count);
  return {x, l.itemRow.y, end - x, l.itemRow.h};
}
} // namespace md_ui
