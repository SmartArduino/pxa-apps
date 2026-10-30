// SPDX-License-Identifier: CC0-1.0
// PXA's layer size must match its surface size. Draw in logical coordinates,
// resample into bounded output buffers, and let the Host apply only its exact
// integer 1x/2x/4x scale. All origins below are physical display coordinates.
pocket_ui::Viewport viewport;
uint32_t frame_bytes;
bool surface_inflight;
int surface_display_w, surface_display_h;
int request_surface() {
  surface_ready = false;
  prepared = false;
  staged = false;
  if (surface) {
    int32_t status = pxa_close_handle(surface);
    if (status)
      return status;
    surface = 0;
  }
  // Full-bleed surface. Expand the logical scene instead of letterboxing or
  // stretching its character sprites. Rounded display corners are host masks.
  viewport = {};
  int divisor = 1;
  while ((display_w / divisor > 480 || display_h / divisor > 480) &&
         divisor < 4)
    divisor *= 2;
  viewport.width = std::clamp(display_w / divisor, 1, 480);
  viewport.height = std::clamp(display_h / divisor, 1, 480);
  viewport.host_scale = divisor;
  viewport.x = viewport.y = 0;
  if (int64_t(display_w) * BASE_HEIGHT >= int64_t(display_h) * BASE_WIDTH) {
    HEIGHT = BASE_HEIGHT;
    WIDTH = std::clamp(
        int((int64_t(display_w) * HEIGHT + display_h / 2) / display_h),
        BASE_WIDTH, 480);
  } else {
    WIDTH = BASE_WIDTH;
    HEIGHT = std::clamp(
        int((int64_t(display_h) * WIDTH + display_w / 2) / display_w),
        BASE_HEIGHT, 480);
  }
  surface_display_w = display_w;
  surface_display_h = display_h;
  frame_bytes = viewport.pixels() * 2;
  frame_id = 0;
  pxa_surface_desc_t desc{uint16_t(viewport.width), uint16_t(viewport.height),
                          PXA_SURFACE_RGB565, 2,
                          uint8_t(PXA_SURFACE_PREFER_DIRECT_SCANOUT |
                                  (mapped ? PXA_SURFACE_GUEST_MAPPED : 0))};
  uint32_t n;
  if (!pxa_surface_build_create(packet, sizeof(packet), CREATE, desc, &n))
    return PXA_STATUS_INTERNAL;
  int32_t status = pxa_control_packet(packet, n);
  surface_inflight = status == 0;
  return status;
}
int32_t handle_surface(const pxa_event_t &e) {
  if (e.service != PXA_SURFACE_SERVICE)
    return PXA_EVENT_UNHANDLED;
  if (e.token == CREATE) {
    pxa_surface_create_result_t r;
    if (!pxa_surface_parse_create(&e, CREATE, &r))
      return PXA_STATUS_INTERNAL;
    if (r.status == PXA_STATUS_UNSUPPORTED && mapped) {
      mapped = false;
      return request_surface();
    }
    if (r.status || r.stride_bytes != uint32_t(viewport.width * 2) ||
        r.frame_bytes != frame_bytes || r.buffer_count != 2)
      return PXA_STATUS_INTERNAL;
    surface = r.handle;
    if (mapped && pxa_surface_register_buffers(surface, frames, frame_bytes,
                                               2) != int32_t(frame_bytes * 2))
      return PXA_STATUS_INTERNAL;
    uint32_t n;
    if (!pxa_surface_build_configure_layer(
            packet, sizeof(packet), CONFIGURE, surface, viewport.x, viewport.y,
            uint16_t(viewport.width), uint16_t(viewport.height), 0, 1, &n))
      return PXA_STATUS_INTERNAL;
    int32_t status = pxa_control_packet(packet, n);
    return status ? status : PXA_EVENT_HANDLED;
  }
  if (e.token == CONFIGURE) {
    int32_t status;
    if (!pxa_surface_parse_status(&e, CONFIGURE, PXA_SURFACE_CONFIGURE_LAYER,
                                  &status) ||
        status)
      return PXA_STATUS_INTERNAL;
    surface_inflight = false;
    // Resize notifications may arrive during asynchronous creation. Finish
    // the outstanding exchange before reusing its token and buffer memory.
    if (surface_display_w != display_w || surface_display_h != display_h) {
      status = request_surface();
      return status ? status : PXA_EVENT_HANDLED;
    }
    surface_ready = true;
    pocket::redraw();
    char message[176];
    snprintf(
        message, sizeof(message),
        "Pocket: viewport canvas=%dx%d origin=%d,%d size=%dx%d display=%dx%d",
        WIDTH, HEIGHT, viewport.x, viewport.y, viewport.physical_width(),
        viewport.physical_height(), display_w, display_h);
    pxa_log_write(2, message);
    return PXA_EVENT_HANDLED;
  }
  return PXA_EVENT_UNHANDLED;
}
