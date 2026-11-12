// SPDX-License-Identifier: MIT
#include "game_blit.hpp"
#include "../common/pocket-play/runtime.hpp"
#include "pxa_game_render_rgb565.h"
#include <stdint.h>
#include <stdio.h>
namespace {
constexpr uint64_t CREATE = 0x55450001;
uint64_t handle = 0;
bool requested = false;
alignas(4) uint16_t target[16];
uint8_t packet[64];
} // namespace
namespace game_blit {
void update(bool fg) {
  if (!fg || requested)
    return;
  requested = true;
  pxa_game_render_create_raster(CREATE, 512);
}
bool event(const pxa_event_t &e) {
  if (e.service != PXA_GAME_RENDER_SERVICE || e.token != CREATE)
    return false;
  pxa_game_render_raster_result_t r{};
  int status = -15;
  if (pxa_game_render_parse_raster(&e, CREATE, &r)) {
    status = r.status;
    if (!status) {
      pxa_game_render_rgb565_header_wire_t h{};
      h.magic = PXA_GAME_RENDER_RGB565_MAGIC;
      h.version = 1;
      h.count = 1;
      h.target_offset = uint32_t(uintptr_t(target));
      h.target_bytes = sizeof(target);
      h.target_width = 4;
      h.target_height = 4;
      h.target_pitch = 8;
      pxa_game_render_rgb565_command_wire_t c{};
      c.light = 255;
      c.width = 4;
      c.height = 4;
      c.color = 0xf800;
      pxa_game_render_rgb565_header_encode(packet, 32, &h);
      pxa_game_render_rgb565_command_encode(packet + 32, 32, &c);
      status = pxa_game_render_execute_rgb565(r.handle, packet, sizeof(packet));
      if (status == 64) {
        bool ok = true;
        for (auto v : target)
          if (v != 0xf800)
            ok = false;
        if (ok) {
          handle = r.handle;
          status = 0;
        } else
          status = -15;
      }
      if (!handle)
        pxa_close_handle(r.handle);
    }
  }
  char text[128];
  snprintf(text, sizeof(text),
           "Unciv: GameRender RGB565 probe status=%d fallback=%s", status,
           handle ? "available" : "unavailable");
  pocket::log(text);
  return true;
}
uint64_t context() {
#ifdef UNCIV_LOCAL_BLIT
  return 0;
#else
  return handle;
#endif
}
} // namespace game_blit
