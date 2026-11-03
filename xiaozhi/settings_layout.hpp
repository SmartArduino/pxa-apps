// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cmath>

namespace xiaozhi {
struct SettingsRect { int x, y, width, height; };
struct SettingsLayout {
    int width, height, pad, gap, body_top, body_bottom;
    float unit;
    bool compact;
    SettingsRect header, tabs, save;
    SettingsLayout(int w, int h) : width(w), height(h) {
        unit = std::clamp(std::min(w / 240.f, h / 284.f), .65f, 2.5f);
        compact = h < 270 || w > h;
        pad = px(6); gap = px(compact ? 4 : 6);
        header = {pad, pad, w - pad * 2, px(compact ? 28 : 34)};
        tabs = {pad, header.y + header.height + gap, header.width, px(compact ? 26 : 30)};
        save = {pad, h - pad - px(compact ? 30 : 34), header.width, px(compact ? 30 : 34)};
        body_top = tabs.y + tabs.height + gap * 2;
        body_bottom = save.y - gap;
    }
    int px(float value) const { return std::max(1, int(std::lround(value * unit))); }
    int font(int value) const { return std::max(8, px(value)); }
    SettingsRect row(unsigned index, unsigned count) const {
        int available = std::max(int(count), body_bottom - body_top - gap * (int(count) - 1));
        int top = body_top + int(index) * available / int(count) + int(index) * gap;
        int bottom = body_top + int(index + 1) * available / int(count) + int(index) * gap;
        return {pad, top, width - pad * 2, bottom - top};
    }
};
}
