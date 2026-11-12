// SPDX-License-Identifier: GPL-3.0-only
// Included inside the app runtime. Input coordinates match the actual renderer.
pocket_ui::Viewport viewport;
bool surface_inflight = false;
int surface_display_w = 0, surface_display_h = 0;
void apply_render_layout(int width, int height) {
  auto &screen = milky::screen;
  auto safe = screen.safe;
  int oldW = std::max(1, screen.width), oldH = std::max(1, screen.height);
  screen.safe = {safe.x * width / oldW, safe.y * height / oldH,
                 safe.w * width / oldW, safe.h * height / oldH};
  pocket::WIDTH = screen.width = width;
  pocket::HEIGHT = screen.height = height;
}
int32_t request_surface() {
  if (surface) {
    gfx::detach();
    pxa_game_render_close(surface);
    surface = 0;
  }
  surface_ready = false;
  surface_display_w = display_w;
  surface_display_h = display_h;
  pxa_game_render_options_t options{};
  options.buffer_count = 3;
  options.flags = PXA_GAME_RENDER_FLAG_PREFER_DIRECT_SCANOUT |
                  PXA_GAME_RENDER_FLAG_BORROW_COMMANDS;
  options.scratch_mode = PXA_GAME_RENDER_SCRATCH_NONE;
  options.max_draw_bytes = 49152;
  // Product Hosts can disable CREATE_AUTO. Explicit sizing uses the same exact
  // integer fit as Surface input, and does not depend on the auto-target
  // profile.
  viewport = pocket_ui::fill_viewport(display_w, display_h);
  options.width = uint16_t(viewport.width);
  options.height = uint16_t(viewport.height);
  options.requested_scale = 0;
  uint32_t size = 0;
  if (!pxa_game_render_build_create(packet, sizeof(packet), CREATE, &options,
                                    &size))
    return PXA_STATUS_INTERNAL;
  int32_t status = pxa_control_packet(packet, size);
  surface_inflight = status == 0;
  return status;
}
int32_t handle_surface(const pxa_event_t &e) {
  pxa_game_render_create_result_t created{};
  if (!pxa_game_render_parse_create(&e, CREATE, &created))
    return PXA_EVENT_UNHANDLED;
  surface_inflight = false;
  if (created.status)
    return created.status;
  surface = created.handle;
  if (surface_display_w != display_w || surface_display_h != display_h)
    return request_surface();
  int width = viewport.width, height = viewport.height,
      scale = viewport.host_scale;
  if (!width || !height || scale < 1 || scale > 4)
    return PXA_STATUS_PROTOCOL_ERROR;
  if (!gfx::attach(surface, created.capabilities))
    return PXA_STATUS_UNSUPPORTED;
  apply_render_layout(width, height);
  viewport.x = (display_w - width * scale) / 2;
  viewport.y = (display_h - height * scale) / 2;
  viewport.width = width;
  viewport.height = height;
  viewport.host_scale = scale;
  frame_id = 0;
  surface_ready = true;
  pocket::redraw();
  char message[180];
  snprintf(
      message, sizeof(message),
      "Pocket: viewport canvas=%dx%d origin=%d,%d size=%dx%d display=%dx%d",
      width, height, viewport.x, viewport.y, width * scale, height * scale,
      display_w, display_h);
  pxa_log_write(2, message);
  pxa_log_write(2, "Mindustry: GameRender direct-scanout requested; HUD uses "
                   "cached system glyphs");
  return PXA_EVENT_HANDLED;
}
