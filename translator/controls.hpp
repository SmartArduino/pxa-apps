// SPDX-License-Identifier: MIT
#pragma once
#include "../common/media/canvas.hpp"

namespace translate {
// Decorative Canvas and real native controls: the Host owns focus, IME and
// three-key navigation. Never capture the product's physical keys as game
// input.
struct NativeCanvas : media::Canvas {
  struct Control {
    media::Hit hit{};
    char label[96]{};
  } previous[64]{};
  unsigned previous_count = 0;
  bool present() {
    if (content_clip) {
      unclip();
      content_clip = false;
    }
    if (!initialized) {
      pxa_ui_transaction_t tx{};
      uint32_t next = ++generation;
      bool ok = pxa_ui_transaction_begin(&tx, next,
                                         PXA_UI_TRANSACTION_REPLACE_SURFACE,
                                         scratch, sizeof(scratch)) &&
                pxa_ui_create(&tx, 1, 0, 0, PXA_UI_NODE_ROOT) &&
                pxa_ui_create(&tx, 2, 1, 0, PXA_UI_NODE_CANVAS);
      for (unsigned node : {1u, 2u})
        ok = ok &&
             pxa_ui_set_length(&tx, node, PXA_UI_PROPERTY_WIDTH,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_length(&tx, node, PXA_UI_PROPERTY_HEIGHT,
                               PXA_UI_LENGTH_FILL, 0) &&
             pxa_ui_set_padding(&tx, node, 0, 0, 0, 0) &&
             pxa_ui_set_dp(&tx, node, PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
             pxa_ui_set_dp(&tx, node, PXA_UI_PROPERTY_RADIUS, 0) &&
             pxa_ui_set_event_mask(&tx, node, 0);
      ok = ok && pxa_ui_set_u8(&tx, 2, PXA_UI_PROPERTY_POSITION, 1) &&
           pxa_ui_set_rgba(&tx, 1, PXA_UI_PROPERTY_BACKGROUND,
                           pxa_canvas_rgba(background)) &&
           pxa_ui_transaction_commit(&tx);
      if (!ok) {
        if (tx.active)
          pxa_ui_transaction_cancel(&tx);
        return false;
      }
      initialized = 1;
      ui_generation = next;
      previous_count = 0;
    }
    uint8_t ready = 1;
    return pxa_canvas_present_with_root_event_mask(
        2, &frame, &generation, &ready, 0, scratch, sizeof(scratch), scratch,
        sizeof(scratch));
  }
  bool control(const media::Hit &hit, const char *label, unsigned index) {
    if (index >= std::size(previous))
      return false;
    auto &old = previous[index];
    bool create = index >= previous_count;
    if (!create && old.hit.id != hit.id)
      return false; // Rebuild when control sets change.
    if (!create && old.hit.x == hit.x && old.hit.y == hit.y &&
        old.hit.w == hit.w && old.hit.h == hit.h &&
        !std::strcmp(old.label, label))
      return true;
    pxa_ui_transaction_t tx{};
    uint32_t next = ++generation;
    bool ok = pxa_ui_transaction_begin(&tx, next, PXA_UI_TRANSACTION_PATCH,
                                       scratch, sizeof(scratch));
    if (create)
      ok = ok && pxa_ui_create_typed(&tx, hit.id, 1, 0, PXA_UI_NODE_CONTROL,
                                     PXA_UI_CONTROL_BUTTON);
    ok = ok && pxa_ui_set_u8(&tx, hit.id, PXA_UI_PROPERTY_POSITION, 1) &&
         pxa_ui_set_length(&tx, hit.id, PXA_UI_PROPERTY_X, PXA_UI_LENGTH_PX,
                           hit.x + viewport.content.x) &&
         pxa_ui_set_length(&tx, hit.id, PXA_UI_PROPERTY_Y, PXA_UI_LENGTH_PX,
                           hit.y + viewport.content.y) &&
         pxa_ui_set_length(&tx, hit.id, PXA_UI_PROPERTY_WIDTH, PXA_UI_LENGTH_PX,
                           hit.w) &&
         pxa_ui_set_length(&tx, hit.id, PXA_UI_PROPERTY_HEIGHT,
                           PXA_UI_LENGTH_PX, hit.h) &&
         pxa_ui_set_padding(&tx, hit.id, 0, 0, 0, 0) &&
         pxa_ui_set_dp(&tx, hit.id, PXA_UI_PROPERTY_BORDER_WIDTH, 0) &&
         pxa_ui_set_dp(&tx, hit.id, PXA_UI_PROPERTY_RADIUS, 8) &&
         pxa_ui_set_rgba(&tx, hit.id, PXA_UI_PROPERTY_BACKGROUND, 0) &&
         pxa_ui_set_rgba(&tx, hit.id, PXA_UI_PROPERTY_FOREGROUND, 0) &&
         pxa_ui_set_text(&tx, hit.id, label, std::strlen(label)) &&
         pxa_ui_set_event_mask(&tx, hit.id, PXA_UI_EVENT_MASK_CLICK) &&
         pxa_ui_transaction_commit(&tx);
    if (!ok) {
      if (tx.active)
        pxa_ui_transaction_cancel(&tx);
      return false;
    }
    old.hit = hit;
    std::snprintf(old.label, sizeof(old.label), "%s", label);
    previous_count = std::max(previous_count, index + 1);
    ui_generation = next;
    return true;
  }
};
} // namespace translate
