// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_game_render_rgb565.h"
#include "resample.hpp"

#include <algorithm>
#include <cstdint>

namespace dewfield_raster {
// Uses the existing logical and mapped Surface buffers: no extra framebuffer
// or palette conversion. Older hosts reject the new operation before writes.
class NativeRaster {
  uint64_t context = 0;
  bool announced = false;
  static constexpr unsigned Capacity = 256;
  uint8_t fills[32 + Capacity * 32];
  pxa_game_render_rgb565_command_wire_t pending{};
  uint16_t *target = nullptr;
  int width = 0, height = 0;
  unsigned count = 0;
  bool active = false;
  void append_pending() {
    if (!pending.width) return;
    pxa_game_render_rgb565_command_encode(fills + 32 + count * 32, 32, &pending);
    ++count; pending.width = 0;
  }
  void header(uint8_t *packet, unsigned commands, uint16_t *out, int w, int h) {
    pxa_game_render_rgb565_header_wire_t value{};
    value.magic = PXA_GAME_RENDER_RGB565_MAGIC; value.version = 1;
    value.count = commands; value.target_offset = uint32_t(uintptr_t(out));
    value.target_bytes = unsigned(w * h * 2);
    value.target_width = w; value.target_height = h; value.target_pitch = w * 2;
    pxa_game_render_rgb565_header_encode(packet, 32, &value);
  }
  bool execute(uint8_t *packet, unsigned bytes) {
    if (!context) return false;
    const auto result = pxa_game_render_execute_rgb565(context, packet, bytes);
    if (result == int32_t(bytes)) return true;
    if (result != PXA_STATUS_WOULD_BLOCK) {
      pxa_game_render_close(context); context = 0;
      pxa_log_write(2, "Dewfield: software RGB565 fallback");
    }
    return false;
  }
public:
  bool copy(const uint16_t *source, int pitch, int w, int h,
            uint16_t *destination, int dw, int dh) {
    if (!context || !source || !destination || w < 1 || h < 1) return false;
    uint8_t packet[64];
    header(packet, 1, destination, dw, dh);
    pxa_game_render_rgb565_command_wire_t command{};
    command.kind = PXA_GAME_RENDER_RGB565_BLIT; command.light = 255;
    command.width = w; command.height = h;
    command.source_offset = uint32_t(uintptr_t(source));
    command.source_bytes = unsigned(((h - 1) * pitch + w) * 2);
    command.source_width = w; command.source_height = h;
    command.source_pitch = pitch * 2;
    pxa_game_render_rgb565_command_encode(packet + 32, 32, &command);
    return execute(packet, sizeof(packet));
  }
  bool batching() const { return active; }
  void created(const pxa_game_render_raster_result_t &result) {
    if (!result.status) context = result.handle;
  }
  // Background geometry is emitted as ordered rectangles. Merge adjacent
  // equal spans; keep bounded storage and preserve the read-after-write order.
  void begin(uint16_t *pixels, int w, int h) {
    target = pixels; width = w; height = h; active = context && pixels;
  }
  bool fill(int x, int y, int w, int h, uint16_t color) {
    if (!active) return false;
    int right = std::min(width, x + w), bottom = std::min(height, y + h);
    x = std::max(x, 0); y = std::max(y, 0); w = right - x; h = bottom - y;
    if (w <= 0 || h <= 0) return true;
    if (pending.width && pending.color == color) {
      if (pending.x == x && pending.width == w && pending.y + pending.height == y) {
        pending.height += h; return true;
      }
      if (pending.y == y && pending.height == h && pending.x + pending.width == x) {
        pending.width += w; return true;
      }
    }
    append_pending();
    if (count == Capacity) flush();
    pending = {}; pending.light = 255; pending.x = x; pending.y = y;
    pending.width = w; pending.height = h; pending.color = color;
    return true;
  }
  void flush() {
    append_pending();
    if (!count) return;
    header(fills, count, target, width, height);
    if (!execute(fills, 32 + count * 32)) {
      for (unsigned i = 0; i < count; ++i) {
        pxa_game_render_rgb565_command_wire_t c{};
        pxa_game_render_rgb565_command_decode(fills + 32 + i * 32, 32, &c);
        for (int row = c.y; row < c.y + c.height; ++row)
          std::fill_n(target + row * width + c.x, c.width, c.color);
      }
    }
    count = 0;
  }
  void end() { flush(); active = false; }
  void resample(const uint16_t *source, int sw, int sh,
                uint16_t *destination, int w, int h) {
    if (source == destination && sw == w && sh == h) return;
    if (context) {
      uint8_t packet[64];
      header(packet, 1, destination, w, h);
      pxa_game_render_rgb565_command_wire_t command{};
      command.kind = PXA_GAME_RENDER_RGB565_BLIT; command.light = 255;
      command.width = w; command.height = h;
      command.source_offset = uint32_t(uintptr_t(source));
      command.source_bytes = unsigned(sw * sh * 2);
      command.source_width = sw; command.source_height = sh; command.source_pitch = sw * 2;
      pxa_game_render_rgb565_command_encode(packet + 32, 32, &command);
      if (execute(packet, sizeof(packet))) {
        if (!announced) { pxa_log_write(2, "Dewfield: native RGB565 resample"); announced = true; }
        return;
      }
    }
    dewfield_raster::resample(source, sw, sh, destination, w, h);
  }
};
}
