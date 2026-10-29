// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "companion.hpp"
#include <array>
#include <string_view>
namespace xiaozhi {
enum class SettingAction {
  none,
  back,
  tab,
  details,
  official,
  custom,
  voice,
  listen,
  wake,
  auto_scene,
  awake,
  scene,
  edit_url,
  edit_token,
  edit_endpoint,
  edit_device,
  edit_client,
  edit_text,
  save,
  edit_done,
  edit_cancel
};
struct SettingHit {
  pxa::ui::CanvasRegion box;
  SettingAction action = SettingAction::none;
  unsigned value = 0;
};
struct SettingsData {
  unsigned page = 0, scene = 0;
  bool official = true, voice = true, listen = true, editing = false,
       sending = false, awake = true, saving = false,
       wake_supported = false, wake = false;
  std::string_view url, token, endpoint, device, client, notice, edit_title,
      edit_error, wake_note;
};
struct SettingsUi {
  static constexpr std::uint32_t ink = 0x594760ff, muted = 0x95818fff,
                                 plum = 0x8d739fff, pale = 0xeadfeaff,
                                 border = 0xd6c0ccff, cream = 0xfff8efff;
  pxa::ui::CanvasCommands<4000> commands;
  std::array<SettingHit, 24> hits{};
  unsigned count = 0, focus = 0, content_start = 0;
  bool keyboard_focus = false;
  std::uint64_t previous_hash = 0;
  float scale = 1, ox = 0, oy = 0;
  pxa::ui::CanvasRegion box(int x, int y, int w, int h) const {
    return {int(ox + x * scale), int(oy + y * scale),
            std::max(1, int(w * scale)), std::max(1, int(h * scale))};
  }
  void rect(int x, int y, int w, int h, std::uint32_t color, int radius = 0) {
    auto r = box(x, y, w, h);
    commands.rect(r.x, r.y, r.width, r.height, color, radius);
  }
  void text(int x, int y, int w, int h, std::string_view label, int font = 13,
            std::uint32_t color = ink, unsigned align = 0) {
    commands.text_sized(box(x, y, w, h), color, label,
                        std::max(8, int(font * scale)), align);
  }
  void target(int x, int y, int w, int h, SettingAction action,
              unsigned value = 0) {
    if (count < hits.size())
      hits[count++] = {box(x, y, w, h), action, value};
  }
  void button(int x, int y, int w, int h, std::string_view label,
              SettingAction action, unsigned value = 0, bool active = false) {
    rect(x, y + 2, w, h, border, 3);
    rect(x, y, w, h, active ? plum : pale, 3);
    text(x + 4, y + 5, w - 8, h - 7, label, 13, active ? cream : ink, 1);
    target(x, y, w, h, action, value);
  }
  void arrow(int x, int y) {
    auto a = box(x, y, 1, 1), b = box(x - 5, y + 5, 1, 1),
         c = box(x, y + 10, 1, 1);
    commands.line(a.x, a.y, b.x, b.y, ink, 2).line(b.x, b.y, c.x, c.y, ink, 2);
  }
  static std::string_view brief(std::string_view value) {
    auto n = std::min<std::size_t>(value.size(), 44);
    while (n < value.size() && n &&
           (static_cast<unsigned char>(value[n]) & 0xc0) == 0x80)
      --n;
    return value.substr(0, n);
  }
  void row(int y, std::string_view label, std::string_view value,
           SettingAction action) {
    rect(16, y, 288, 24, cream, 2);
    text(25, y + 4, 88, 18, label, 12);
    text(112, y + 5, 170, 18, brief(value), 11, muted);
    text(284, y + 1, 12, 20, "›", 17, muted, 1);
    target(16, y, 288, 24, action);
  }
  void toggle(int y, std::string_view title, std::string_view note, bool on,
              SettingAction action, bool compact = false) {
    const int height = compact ? 32 : 40, offset = compact ? 5 : 9;
    rect(16, y, 288, height, cream, 3);
    text(26, y + 2, 200, 19, title, compact ? 13 : 14);
    text(26, y + (compact ? 19 : 23), 220, compact ? 12 : 14, note, compact ? 9 : 10, muted);
    rect(250, y + offset, 42, 22, on ? plum : border, 8);
    rect(on ? 272 : 253, y + offset + 3, 17, 16, cream, 4);
    target(16, y, 288, height, action);
  }
  SettingHit hit(int x, int y) const {
    for (unsigned i = count; i > 0; --i) {
      const auto &h = hits[i - 1];
      if (x >= h.box.x && y >= h.box.y && x < h.box.x + h.box.width &&
          y < h.box.y + h.box.height)
        return h;
    }
    return {};
  }
  void move_focus(int direction) {
    if (!count)
      return;
    if (!keyboard_focus)
      focus = std::min(content_start, count - 1);
    else
      focus = unsigned((int(focus) + direction + int(count)) % int(count));
    keyboard_focus = true;
    previous_hash = 0;
  }
  SettingHit selected() const {
    return count && keyboard_focus ? hits[focus % count] : SettingHit{};
  }
  pxa::ui::CanvasRegion editor_bounds() const { return box(20, 65, 280, 37); }
  void draw(pxa::ui::CanvasRef &canvas, const pxa::ui::DisplayMetrics &m,
            const std::optional<pxa::Asset> &background,
            const SettingsData &d) {
    if (!canvas.mounted())
      return;
    std::uint64_t hash = 1469598103934665603ull;
    for (auto s : {d.url, d.token, d.endpoint, d.device, d.client, d.notice,
                   d.edit_title, d.edit_error, d.wake_note})
      for (unsigned char c : s)
        hash = (hash ^ c) * 1099511628211ull;
    hash ^= std::uint64_t(d.page) | std::uint64_t(d.scene) << 4 |
            std::uint64_t(d.official) << 8 | std::uint64_t(d.voice) << 9 |
            std::uint64_t(d.listen) << 10 | std::uint64_t(d.editing) << 11 |
            std::uint64_t(keyboard_focus) << 12 | std::uint64_t(focus) << 16 |
            std::uint64_t(m.width) << 24 | std::uint64_t(m.height) << 40;
    hash ^= background ? background->handle() : 0;
    hash ^= std::uint64_t(d.awake) << 60 | std::uint64_t(d.saving) << 61;
    hash ^= std::uint64_t(d.wake_supported) << 58 | std::uint64_t(d.wake) << 59;
    if (hash == previous_hash)
      return;
    scale = std::min(m.width / 320.f, m.height / 240.f);
    ox = (m.width - 320 * scale) / 2;
    oy = (m.height - 240 * scale) / 2;
    count = 0;
    commands.reset();
    commands.rect(0, 0, m.width, m.height, 0x362b43ff);
    if (background)
      commands.image({0, 0, int(m.width), int(m.height)}, *background,
                     pxa::ui::ImageFit::cover);
    commands.rect(0, 0, m.width, m.height, 0x30273e90);
    rect(9, 10, 304, 224, 0x211b3450, 3);
    rect(8, 8, 304, 224, border, 3);
    rect(10, 10, 300, 220, 0xf8eee6f9, 2);
    button(16, 16, 30, 27, "", SettingAction::back);
    arrow(34, 23);
    text(57, 17, 180, 25, d.page == 3 ? "接入详情" : "小智设置", 19);
    text(58, 40, 224, 12, "把房间调成喜欢的样子", 10, muted);
    if (d.page == 0) {
      text(254, 23, 44, 18, "详情", 12, plum, 1);
      target(248, 16, 56, 30, SettingAction::details);
    } else if (d.page == 1) {
      text(254, 23, 44, 18, "文字", 12, plum, 1);
      target(248, 16, 56, 30, SettingAction::edit_text);
    }
    const auto tab = d.page == 3 ? 0 : d.page;
    const std::string_view tabs[] = {"连接", "对话", "画面"};
    for (unsigned i = 0; i < 3; ++i)
      button(16 + int(i) * 98, 58, 92, 27, tabs[i], SettingAction::tab, i,
             i == tab);
    content_start = count;
    if (d.page == 0) {
      button(16, 99, 140, 29, "官方小智", SettingAction::official, 0,
             d.official);
      button(164, 99, 140, 29, "自定义", SettingAction::custom, 0, !d.official);
      if (d.official) {
        rect(16, 140, 288, 44, cream, 3);
        text(26, 145, 268, 20, "自动获取连接地址和激活信息", 13);
        text(26, 166, 268, 15, "首次见面需要在小智控制台绑定设备", 10, muted);
      } else {
        row(139, "服务器地址",
            d.url.empty() ? "点击填写 ws:// 或 wss://" : d.url,
            SettingAction::edit_url);
        row(167, "访问令牌",
            d.token.empty() ? "点击填写 Token" : "已设置 · 点按修改",
            SettingAction::edit_token);
      }
    } else if (d.page == 1) {
      toggle(96, "语音回复", "让她读出回答", d.voice, SettingAction::voice, true);
      toggle(132, "自动聆听", "回复结束后，继续听你说", d.listen,
             SettingAction::listen, true);
      if (d.wake_supported)
        toggle(168, "语音唤醒", brief(d.wake_note), d.wake, SettingAction::wake, true);
      else {
        rect(16, 168, 288, 30, cream, 3);
        text(26, 169, 220, 17, "语音唤醒", 13, muted);
        text(26, 186, 262, 12, brief(d.wake_note), 9, muted);
      }
    } else if (d.page == 2) {
      toggle(96, "对话时保持亮屏", "聆听、思考与回复时不自动锁屏", d.awake,
             SettingAction::awake, true);
      toggle(132, "跟随系统时间", "按当地时间自动切换房间", d.scene == 0,
             SettingAction::auto_scene, true);
      const std::string_view names[] = {"清晨", "白天", "傍晚", "夜晚"};
      const std::uint32_t colors[] = {0xe5b6a0ff, 0x9fbcd5ff, 0xda9c90ff,
                                      0x77759fff};
      for (unsigned i = 0; i < 4; ++i) {
        int x = 16 + int(i) * 74;
        button(x, 171, 66, 25, names[i], SettingAction::scene, i + 1,
               d.scene == i + 1);
        rect(x + 7, 171, 52, 2, colors[i]);
      }
    } else {
      row(96, "配置服务", d.endpoint, SettingAction::edit_endpoint);
      row(121, "Device-Id", d.device, SettingAction::edit_device);
      row(146, "Client-Id", d.client, SettingAction::edit_client);
      row(171, "文字对话", "写一句话给她", SettingAction::edit_text);
    }
    button(16, 201, 288, 24, d.saving ? "正在保存…" : "保存并返回", SettingAction::save, 0, true);
    if (!d.notice.empty())
      text(20, 87, 280, 12, brief(d.notice), 10, 0xa15662ff, 1);
    if (d.editing) {
      commands.rect(0, 0, m.width, m.height, 0x30273ec0);
      rect(8, 9, 304, 125, border, 3);
      rect(10, 11, 300, 121, cream, 2);
      count = 0;
      button(16, 19, 30, 27, "", SettingAction::edit_cancel);
      arrow(34, 26);
      text(56, 23, 180, 24, d.edit_title, 15);
      button(250, 19, 54, 27, d.sending ? "发送" : "完成",
             SettingAction::edit_done, 0, true);
      rect(19, 64, 282, 39, border, 2);
      rect(20, 65, 280, 37, 0xfffcf6ff, 2);
      text(21, 110, 278, 17,
           d.edit_error.empty() ? "编辑完成后，点右上角确认" : d.edit_error, 11,
           d.edit_error.empty() ? muted : 0xa15662ff);
    }
    if (keyboard_focus && count) {
      auto r = hits[focus % count].box;
      commands.line(r.x, r.y, r.x + r.width, r.y, 0xd69b66ff, 2)
          .line(r.x, r.y + r.height, r.x + r.width, r.y + r.height, 0xd69b66ff,
                2);
    }
    auto shown = canvas.present(commands);
    if (shown)
      previous_hash = hash;
  }
};
} // namespace xiaozhi
