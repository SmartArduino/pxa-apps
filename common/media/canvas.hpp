// SPDX-License-Identifier: MIT
#pragma once
#include "pxa_canvas.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
namespace media {
struct Hit {
  int x, y, w, h;
  unsigned id;
};
struct Canvas {
  uint8_t data[16384]{}, scratch[4096]{};
  pxa_canvas_frame_t frame{};
  uint32_t generation = 0, ui_generation = 0;
  uint8_t initialized = 0;
  Hit hits[64]{};
  unsigned count = 0;
  int w = 320, h = 240;
  uint32_t density = 65536, background = 0;
  int pixel() const { return int((65536u + density - 1) / density); }
  bool input_mode = false;
  void invalidate() { initialized = 0; }
  void begin(int width, int height, uint32_t bg) {
    w = std::max(160, width);
    h = std::max(120, height);
    count = 0;
    background = bg;
    pxa_canvas_begin(&frame, data, sizeof(data));
    int bleed = pixel();
    rect(-bleed, -bleed, w + 2 * bleed, h + 2 * bleed, bg);
  }
  void rect(int x, int y, int width, int height, uint32_t c, int radius = 0) {
    if (width > 0 && height > 0)
      pxa_canvas_rect(&frame, x, y, width, height, c, radius);
  }
  void line(int x, int y, int x2, int y2, uint32_t c, int thickness = 1) {
    pxa_canvas_line(&frame, x, y, x2, y2, c, std::max(pixel(), thickness));
  }
  void circle(int x, int y, int r, uint32_t c) {
    pxa_canvas_circle(&frame, x, y, r, c);
  }
  void arc(int x, int y, int r, int from, int to, uint32_t c,
           int thickness = 1) {
    pxa_canvas_arc(&frame, x, y, r, from, to, pxa_canvas_rgba(c),
                   std::max(pixel(), thickness));
  }
  void text(int x, int y, int width, const char *value, uint32_t c,
            uint8_t role = 0, uint8_t align = 0) {
    if (width > 0) {
      int height = role == 2 ? 27 : role == 1 ? 21 : 16;
      pxa_canvas_clip_push(&frame, x, y, width, height);
      pxa_canvas_text_role(&frame, x, y, width, pxa_canvas_rgba(c), role, align,
                           value, std::strlen(value));
      pxa_canvas_clip_pop(&frame);
    }
  }
  void image(int x, int y, int width, int height, uint64_t handle) {
    if (handle)
      pxa_canvas_image_handle(&frame, x, y, width, height, 255, 2, handle);
  }
  void hit(unsigned id, int x, int y, int width, int height) {
    if (count < std::size(hits))
      hits[count++] = {x, y, width, height, id};
  }
  unsigned find(int x, int y) const {
    for (unsigned i = count; i--;)
      if (x >= hits[i].x && y >= hits[i].y && x < hits[i].x + hits[i].w &&
          y < hits[i].y + hits[i].h)
        return hits[i].id;
    return 0;
  }
  void pill(unsigned id, int x, int y, int width, const char *title, bool on,
            uint32_t accent, uint32_t ink, uint32_t muted) {
    rect(x, y, width, 25, on ? accent : muted, 12);
    text(x, y + 5, width, title, on ? 0xffffff : ink, 0, 1);
    hit(id, x, y, width, 25);
  }
  void icon(const char *name, int x, int y, uint32_t color, int size = 24) {
    // Match native physical pixels on 120/160/205 dpi displays. Alpha edges are
    // supersampled offline; this sends one cached image instead of line strips.
    unsigned physical = uint64_t(size) * density / 65536;
    unsigned source =
        (physical == 18 || physical == 24 || physical == 30) ? physical : 48;
    char path[96];
    std::snprintf(path, sizeof(path), "assets/ui/%s-%06x-%u.png", name,
                  unsigned(color & 0xffffff), source);
    pxa_canvas_image(&frame, x - size / 2, y - size / 2, size, size, 255, 2,
                     path, std::strlen(path));
  }
  void play_icon(int x, int y, bool playing, uint32_t c, int scale = 1) {
    icon(playing ? "pause" : "play", x, y, c, 24 * scale);
  }
  void skip_icon(int x, int y, int direction, uint32_t c) {
    icon(direction < 0 ? "previous" : "next", x, y, c);
  }
  void heart(int x, int y, bool on, uint32_t c) {
    icon(on ? "heart-filled" : "heart", x, y, c);
  }
  void back(int x, int y, uint32_t c) { icon("back", x, y, c); }
  void search(int x, int y, uint32_t c) { icon("search", x, y, c); }
  void disc(int x, int y, int radius, uint32_t center, uint32_t bg) {
    circle(x + 2, y + 4, radius, 0xd9d0bf);
    circle(x, y, radius, 0x24282c);
    for (int r = radius - 5; r > radius / 3; r -= 5)
      arc(x, y, r, 0, 359, 0x383c40);
    circle(x, y, radius / 3, center);
    circle(x, y, 2, bg);
  }
  void orbit(int x, int y, int r, uint64_t milliseconds, uint32_t color) {
    int angle = (milliseconds / 30) % 360;
    arc(x, y, r, angle, angle + 38, color, 2);
    arc(x, y, r, angle + 180, angle + 218, color, 2);
  }
  void activity(int x, int y, uint64_t milliseconds, uint32_t color) {
    for (int i = 0; i < 3; ++i) {
      int h = 4 + int((std::sin(milliseconds / 180.0 + i * 1.8) + 1) * 4);
      rect(x + i * 5, y + 12 - h, 3, h, color, 1);
    }
  }
  __attribute__((noinline)) bool initialize_surface() {

    pxa_ui_transaction_t tx{};
    uint32_t next = ++generation;
    bool ok =
        pxa_ui_transaction_begin(&tx, next, PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                 scratch, sizeof(scratch)) &&
        pxa_ui_create(&tx, 1, 0, 0, PXA_UI_NODE_ROOT) &&
        pxa_ui_create(&tx, 2, 1, 0, PXA_UI_NODE_CANVAS);
    for (unsigned node : {1u, 2u}) {
      ok = ok &&
           pxa_ui_set_length(&tx, node, PXA_UI_PROPERTY_WIDTH,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_length(&tx, node, PXA_UI_PROPERTY_HEIGHT,
                             PXA_UI_LENGTH_FILL, 0) &&
           pxa_ui_set_padding(&tx, node, 0, 0, 0, 0) &&
           pxa_ui_set_dp(&tx, node, PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
           pxa_ui_set_dp(&tx, node, PXA_UI_PROPERTY_RADIUS, 0);
      if (node == 1)
        ok = ok && pxa_ui_set_rgba(&tx, node, PXA_UI_PROPERTY_BACKGROUND,
                                   pxa_canvas_rgba(background));
    }
    ok = ok && pxa_ui_set_u8(&tx, 2, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&tx, 2, PXA_UI_PROPERTY_X, PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_length(&tx, 2, PXA_UI_PROPERTY_Y, PXA_UI_LENGTH_PX, 0) &&
         pxa_ui_set_event_mask(&tx, 1, PXA_UI_EVENT_MASK_CONTROLLER_STATE) &&
         pxa_ui_set_event_mask(&tx, 2, PXA_UI_EVENT_MASK_POINTER) &&
         pxa_ui_transaction_commit(&tx);
    if (!ok) {
      if (tx.active)
        pxa_ui_transaction_cancel(&tx);
      return false;
    }
    initialized = 1;
    ui_generation = next;

    return true;
  }
  bool present() {
    if (!initialized && !initialize_surface())
      return false;
    uint8_t frame_initialized = 1;
    return pxa_canvas_present_with_root_event_mask(
        2, &frame, &generation, &frame_initialized,
        PXA_UI_EVENT_MASK_CONTROLLER_STATE, scratch, sizeof(scratch), scratch,
        sizeof(scratch));
  }
  bool input(unsigned id, int x, int y, int width, const char *value,
             unsigned max = 96, uint32_t bg = 0x1b2533,
             uint32_t fg = 0xf5f1ea) {
    pxa_ui_transaction_t tx{};
    uint32_t next = ++generation;
    bool ok =
        pxa_ui_transaction_begin(&tx, next, PXA_UI_TRANSACTION_PATCH, scratch,
                                 sizeof(scratch)) &&
        pxa_ui_create_typed(&tx, id, 1, 0, PXA_UI_NODE_CONTROL,
                            PXA_UI_CONTROL_TEXT_INPUT) &&
        pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_POSITION, 1) &&
        pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_X, PXA_UI_LENGTH_PX, x) &&
        pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_Y, PXA_UI_LENGTH_PX, y) &&
        pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX,
                          width) &&
        pxa_ui_set_length(&tx, id, PXA_UI_PROPERTY_HEIGHT, PXA_UI_LENGTH_PX,
                          32) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_BACKGROUND,
                        pxa_canvas_rgba(bg)) &&
        pxa_ui_set_rgba(&tx, id, PXA_UI_PROPERTY_FOREGROUND,
                        pxa_canvas_rgba(fg)) &&
        pxa_ui_set_dp(&tx, id, PXA_UI_PROPERTY_RADIUS, 8) &&
        pxa_ui_set_u8(&tx, id, PXA_UI_PROPERTY_TEXT_SINGLE_LINE, 1) &&
        pxa_ui_set_u32(&tx, id, PXA_UI_PROPERTY_TEXT_MAX_BYTES, max) &&
        pxa_ui_set_text(&tx, id, value, std::strlen(value)) &&
        pxa_ui_set_event_mask(&tx, id, PXA_UI_EVENT_MASK_TEXT) &&
        pxa_ui_transaction_commit(&tx);
    if (ok)
      ui_generation = next;
    else if (tx.active)
      pxa_ui_transaction_cancel(&tx);
    return ok;
  }
};
} // namespace media
