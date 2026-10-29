// SPDX-License-Identifier: MIT
#pragma once
#include <functional>
#include <pxa/app.hpp>
#include <pxa/ui_environment.hpp>
#include <pxa/ui_layout.hpp>

namespace sibo {
using namespace pxa;
using namespace pxa::ui;
constexpr uint32_t paper = 0xfff8eaff, ink = 0x28433fff, muted = 0x6a7e77ff;
constexpr uint32_t green = 0x20866bff, mint = 0xe0f1e5ff, white = 0xffffffff;
struct Flag {
  State<bool> value{false};
};
struct Label {
  State<std::string> value{""};
};
inline bool visibility(Transaction<> &tx, uint32_t n, const bool &v) {
  return tx.u8(n, protocol::visible, v);
}

// Keep the native controls and their callback addresses stable. Tree order is
// also the three-key focus order; every page places its header after its body.
template <class Page> struct Painter {
  Page &page;
  Transaction<> &tx;
  std::array<std::function<void()>, 128> &callbacks;
  unsigned callback_count = 0;
  bool ok = true;
  Painter(Page &p, std::array<std::function<void()>, 128> &c)
      : page(p), tx(p.transaction()), callbacks(c) {}
  void fill(uint32_t n, bool height = false) {
    ok &= tx.fill(n, protocol::width);
    if (height)
      ok &= tx.fill(n, protocol::height);
  }
  void padding(uint32_t n, int l, int t, int r, int b) {
    std::array<std::byte, 16> bytes{};
    int values[] = {l, t, r, b};
    for (unsigned i = 0; i < 4; ++i)
      wire::put32(bytes.data() + 4 * i, unsigned(values[i] * 64));
    ok &= tx.property(n, 268, bytes);
  }
  uint32_t box(uint32_t parent, bool row = false, int gap = 8) {
    auto n = page.create(parent, protocol::box);
    ok &= n &&
          tx.u8(n, protocol::layout, row ? protocol::row : protocol::column) &&
          tx.dp(n, protocol::gap, gap);
    fill(n);
    return n;
  }
  void visible(uint32_t n, State<bool> &state) {
    ok &= visibility(tx, n, state.get()) &&
          page.template bind<bool, visibility>(state, n);
  }
  uint32_t text(uint32_t parent, std::string_view value, Font font = Font::body,
                uint32_t color = ink) {
    auto n = page.create(parent, protocol::text);
    fill(n);
    ok &= tx.text(n, value) && tx.u16(n, protocol::font_role, uint16_t(font)) &&
          tx.rgba(n, protocol::foreground, color);
    return n;
  }
  uint32_t text(uint32_t parent, State<std::string> &value,
                Font font = Font::body, uint32_t color = ink) {
    auto n = text(parent, value.get(), font, color);
    ok &= page.template bind<std::string, write_string_text>(value, n);
    return n;
  }
  uint32_t button_root(uint32_t parent, std::function<void()> fn,
                       bool primary = false) {
    auto n = page.create(parent, protocol::control, protocol::button);
    fill(n);
    ok &= tx.u8(n, protocol::layout, protocol::column) &&
          tx.u64(n, protocol::event_mask, 1) &&
          tx.rgba(n, protocol::background, primary ? green : mint) &&
          tx.rgba(n, protocol::foreground, primary ? white : ink) &&
          tx.dp(n, protocol::radius, 13) &&
          tx.dp(n, protocol::border_width, 0) &&
          tx.u8(n, protocol::justify, 1) && tx.dp(n, protocol::gap, 3);
    padding(n, 10, 9, 10, 9);
    if (callback_count == callbacks.size()) {
      ok = false;
      return n;
    }
    auto &cb = callbacks[callback_count++];
    cb = std::move(fn);
    ok &= page.on_click(n, cb);
    return n;
  }
  template <class Text>
  uint32_t button(uint32_t parent, Text &label, std::function<void()> fn,
                  bool primary = false) {
    auto n = button_root(parent, std::move(fn), primary);
    auto t = text(n, label, Font::title, primary ? white : ink);
    ok &= tx.u8(t, protocol::text_align, 1);
    return n;
  }
  template <size_t N>
  uint32_t button(uint32_t parent, const char (&label)[N],
                  std::function<void()> fn, bool primary = false) {
    std::string_view s(label, N - 1);
    return button(parent, s, std::move(fn), primary);
  }
  uint32_t scroll(uint32_t parent) {
    auto n = page.create(parent, 3);
    fill(n, true);
    ok &= tx.u16(n, protocol::grow, 1) && tx.u8(n, protocol::scroll_axis, 2) &&
          tx.u64(n, protocol::event_mask, UINT64_C(1) << 3);
    return box(n);
  }
  uint32_t frame(uint32_t parent, State<bool> &shown) {
    auto n = box(parent);
    fill(n, true);
    visible(n, shown);
    return n;
  }
  uint32_t body(uint32_t frame) {
    auto n = box(frame);
    fill(n, true);
    padding(n, 0, 41, 0, 0);
    return n;
  }
  template <class Text>
  void header(uint32_t parent, Text &title, std::function<void()> back,
              bool exit = false) {
    auto n = box(parent, true, 6);
    ok &= tx.u8(n, protocol::position, 1) && tx.logical_px(n, protocol::x, 0) &&
          tx.logical_px(n, protocol::y, 0) &&
          tx.logical_px(n, protocol::height, 34) &&
          tx.rgba(n, protocol::background, paper) &&
          tx.u8(n, protocol::align, 1);
    auto t = text(n, title, Font::body, green);
    ok &= tx.u16(t, protocol::grow, 1);
    auto label = exit ? std::string_view("退出") : std::string_view("返回");
    auto b = button(n, label, std::move(back));
    ok &= tx.logical_px(b, protocol::width, 54) &&
          tx.logical_px(b, protocol::height, 34);
    padding(b, 3, 2, 3, 2);
  }
  void canvas(uint32_t parent, CanvasRef &ref, int width, int height,
              State<bool> *show = nullptr) {
    auto n = page.create(parent, protocol::canvas);
    ok &= tx.logical_px(n, protocol::width, width) &&
          tx.logical_px(n, protocol::height, height) &&
          tx.u64(n, protocol::event_mask, 0) && page.attach_canvas(ref, n);
    if (show)
      visible(n, *show);
  }
};
} // namespace sibo
