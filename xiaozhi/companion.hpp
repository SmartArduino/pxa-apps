// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "captions.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <optional>
#include <pxa/app.hpp>
#include <pxa/ui_display.hpp>
#include <string>
#include <string_view>
namespace xiaozhi {
inline unsigned emotion_frame(std::string_view emotion) {
  if (emotion == "happy" || emotion == "laughing" || emotion == "funny" ||
      emotion == "loving" || emotion == "confident" || emotion == "relaxed" ||
      emotion == "delicious" || emotion == "kiss")
    return 2;
  if (emotion == "sad" || emotion == "crying")
    return 3;
  if (emotion == "thinking" || emotion == "confused" ||
      emotion == "embarrassed")
    return 4;
  if (emotion == "surprised" || emotion == "shocked")
    return 5;
  if (emotion == "angry" || emotion == "determined")
    return 6;
  if (emotion == "sleepy" || emotion == "winking")
    return 7;
  return 0;
}
enum class RoomMode : unsigned { arriving, offline, activation, present };
struct Companion {
  pxa::ui::CanvasRef canvas;
  std::optional<pxa::Asset> background, character;
  unsigned loaded_frame = 0, loaded_scene = 1;
  bool loading = true;
  pxa::ui::CanvasCommands<3072> commands;
  std::uint64_t previous_hash = 0;
  static constexpr std::string_view frames[] = {
      "neutral",  "speaking",  "happy", "sad",
      "thinking", "surprised", "angry", "blink"};
  static constexpr std::string_view scenes[] = {"morning", "day", "evening",
                                                "night"};
  pxa::Task<pxa::Asset> image(pxa::Context &context, std::string path) {
    timespec started{}, ended{};
    clock_gettime(CLOCK_MONOTONIC, &started);
    auto result = co_await context.assets().load(pxa::AssetKind::image,
                                                 "assets/companion/" + path);
    clock_gettime(CLOCK_MONOTONIC, &ended);
    const auto elapsed = (ended.tv_sec-started.tv_sec)*1000000LL +
                         (ended.tv_nsec-started.tv_nsec)/1000;
    char diagnostic[192];
    std::snprintf(diagnostic, sizeof(diagnostic),
                  "Xiaozhi image: path=%s us=%lld status=%d resident=%u",
                  path.c_str(), elapsed, result ? 0 : int(result.error()),
                  result ? result->descriptor().resident_bytes : 0);
    (void)context.log().write(pxa::LogLevel::info, diagnostic);
    co_return std::move(result);
  }
  pxa::Task<void> load(pxa::Context &context, unsigned scene) {
    scene = std::min(scene, 3u);
    auto room = co_await image(context, "room-" + std::string(scenes[scene]) + ".pxr");
    if (!room)
      co_return std::unexpected(room.error());
    background.emplace(std::move(*room));
    loaded_scene = scene;
    previous_hash = 0;
    loading = false;
    co_return pxa::Result<void>{};
  }
  pxa::Task<void> change(pxa::Context &context, unsigned next, unsigned scene) {
    // Replace one layer per frame. Presenting it releases the old Canvas
    // binding before another layer can load: at most three full images live
    // in the default 512 KiB cache. Whole character frames retain body
    // gestures.
    if (scene != loaded_scene) {
      auto room = co_await image(context,
                                 "room-" + std::string(scenes[scene]) + ".pxr");
      if (!room) {
        loading = false;
        co_return std::unexpected(room.error());
      }
      background.emplace(std::move(*room));
      loaded_scene = scene;
    } else if (!character || next != loaded_frame) {
      auto girl = co_await image(
          context, "frames/" + std::string(frames[next]) + ".png");
      if (!girl) {
        loading = false;
        co_return std::unexpected(girl.error());
      }
      character.emplace(std::move(*girl));
      loaded_frame = next;
    }
    previous_hash = 0;
    loading = false;
    co_return pxa::Result<void>{};
  }
  void update(pxa::Context &context, unsigned requested, unsigned scene,
              bool speaking, std::uint64_t now, bool visible) {
    requested = std::min(requested, 7u);
    scene = std::min(scene, 3u);
    // A complete talking pose includes the small conversational hand gesture.
    if (requested == 0) {
      if (speaking && (now / 1000) % 3 != 0)
        requested = 1;
      else if (!speaking && now % 5600 < 300)
        requested = 7;
    }
    if (!visible && !loading)
      character.reset();
    if (!loading && (scene != loaded_scene ||
                     (visible && (!character || requested != loaded_frame)))) {
      loading = true;
      auto started = context.tasks().start(change(context, requested, scene));
      if (!started)
        loading = false;
    }
  }
  static std::array<std::string, 3> lines(std::string_view text, int width,
                                          int font, unsigned max_lines) {
    return wrap_caption(text, width, font, max_lines);
  }
  void caption(std::string_view text, int width, int height, int margin,
               int font, bool top) {
    if (text.empty())
      return;
    auto wrapped = lines(text, width - 2 * margin - 20, font, top ? 2 : 3);
    unsigned count = caption_line_count(wrapped);
    if (!count)
      return;
    int box_height = int(count) * (font + 5) + 16;
    int y = top ? margin : height - margin - box_height;
    commands.rect(margin + 2, y + 2, width - 2 * margin, box_height, 0x100d1c60)
        .rect(margin, y, width - 2 * margin, box_height, 0x171322c8, 2);
    for (unsigned i = 0; i < count; i++)
      commands.text_sized({margin + 10, y + 7 + int(i) * (font + 5),
                           width - 2 * margin - 20, font + 6},
                          top ? 0xe0d9f0ff : 0xfff4e2ff, wrapped[i], font, 0);
  }
  void draw(const pxa::ui::DisplayMetrics &metrics, std::string_view status,
            std::string_view stt, std::string_view tts, bool speaking,
            bool ready, bool listening, bool waiting, std::uint64_t now,
            RoomMode room, std::string_view code,
            std::string_view activation_message, bool text_reply = false,
            bool idle = false) {
    if (!canvas.mounted())
      return;
    int w = int(metrics.width), h = int(metrics.height);
    if (w <= 0 || h <= 0)
      return;
    unsigned base = loaded_frame;
    const bool visible = room == RoomMode::present;
    const int breathe = visible ? int((now / (speaking ? 450 : 1600)) % 2) : 0;
    std::uint64_t hash = 1469598103934665603ull;
    auto mix = [&](std::string_view s) {
      for (unsigned char c : s)
        hash = (hash ^ c) * 1099511628211ull;
    };
    mix(status);
    mix(stt);
    mix(tts);
    mix(code);
    mix(activation_message);
    hash ^= std::uint64_t(room) << 48 | std::uint64_t(text_reply) << 56 |
            std::uint64_t(idle) << 57;
    hash ^= std::uint64_t(w) << 32 | std::uint64_t(h) << 16 | base << 8 |
            breathe << 3 | ready << 2 | listening << 1;
    hash ^= background ? background->handle() : 0;
    hash ^= character ? character->handle() << 1 : 0;
    if (hash == previous_hash)
      return;
    commands.reset();
    commands.rect(0, 0, w, h, 0x2b253bff);
    const pxa::ui::CanvasRegion full{0, 0, w, h};
    if (background)
      commands.image(full, *background, pxa::ui::ImageFit::cover);
    const float scale = std::max(w / 320.f, h / 240.f);
    const float x = (w - 320 * scale) / 2, y = (h - 240 * scale) / 2;
    auto region = [&](int left, int top, int width, int height) {
      return pxa::ui::CanvasRegion{int(x + left * scale), int(y + top * scale),
                                   std::max(1, int(width * scale)),
                                   std::max(1, int(height * scale))};
    };
    if (character && visible)
      commands.image(region(72, 20 + breathe, 176, 220), *character,
                     pxa::ui::ImageFit::stretch);
    int margin = std::max(10, w / 32),
        font = std::clamp(std::min(w / 21, h / 15), 13, 22);
    if (!visible) {
      if (room == RoomMode::activation) {
        const int left = margin + 6, top = (h - 152) / 2, cw = w - 2 * left;
        commands.rect(left + 3, top + 4, cw, 152, 0x16112470)
            .rect(left, top, cw, 152, 0xc8a99bff)
            .rect(left + 2, top + 2, cw - 4, 148, 0xfff3e5ff)
            .text_sized({left + 12, top + 12, cw - 24, 23}, 0x5b4058ff,
                        "还有一步，就能见面", 15, 1)
            .text_sized({left + 12, top + 42, cw - 24, 42}, 0x5b4058ff, code,
                        code.size() <= 10 ? 32 : 18, 1);
        auto notes = lines(activation_message.empty() ? "在小智控制台绑定设备"
                                                      : activation_message,
                           cw - 24, 13, 2);
        for (unsigned i = 0; i < 2; ++i)
          commands.text_sized({left + 12, top + 86 + int(i) * 18, cw - 24, 19},
                              0x765c69ff, notes[i], 13, 1);
        commands.text_sized({left + 8, top + 126, cw - 16, 20}, 0x765c69ff,
                            "绑定后轻触继续 · 长按设置", 12, 1);
      } else {
        auto message = room == RoomMode::offline
                           ? "房间准备好了\n请先在系统设置中连接 "
                             "Wi-Fi\n连好网络后，会自动继续"
                           : "等一位新朋友\n" + std::string(status);
        caption(message, w, h, margin, font, false);
      }
      auto shown = canvas.present(commands);
      if (shown)
        previous_hash = hash;
      return;
    }
    if (idle) {
      const int y = h - margin - 53;
      commands.rect(margin, y, w - margin * 2, 53, 0x2d203ca8, 2)
          .text_sized({margin + 10, y + 7, w - margin * 2 - 20, 23},
                      0xfff4e2ff, "我在这里，轻触和我聊聊", font, 1)
          .text_sized({margin + 10, y + 32, w - margin * 2 - 20, 15},
                      0xd5c8deff, "长按打开设置", std::max(10, font - 3), 1);
      auto shown = canvas.present(commands);
      if (shown)
        previous_hash = hash;
      return;
    }
    caption(stt, w, h, margin, font - 1, true);
    std::string_view bottom = tts;
    if (!ready || waiting || tts.empty() || (listening && !text_reply))
      bottom = status;
    if (!ready && bottom.empty())
      bottom = "正在连接小智…";
    caption(bottom, w, h, margin, font, false);
    auto shown = canvas.present(commands);
    if (shown)
      previous_hash = hash;
  }
};
} // namespace xiaozhi
