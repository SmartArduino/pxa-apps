// SPDX-License-Identifier: MIT
#include "../damage.hpp"
#include "pxa_canvas.h"
#include <cassert>
#include <cstdio>

static carrot::DamageTracker damage;
static uint8_t bytes[8192];
static pxa_canvas_frame_t frame;
static void begin() {
  pxa_canvas_begin(&frame, bytes, sizeof(bytes));
  pxa_canvas_rect_rgba(&frame, 0, 0, 480, 480, 0x99cc44ff, 0, 0, 0);
}
static void sprite(int x, int y) {
  pxa_canvas_image(&frame, x, y, 48, 48, 255, 1, "sprite.png", 10);
}
static bool update() {
  return damage.analyse(frame.data, frame.length, 480, 480);
}
static bool covers(int x, int y) {
  if (damage.full)
    return true;
  for (unsigned i = 0; i < damage.count; ++i) {
    auto r = damage.regions[i];
    if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h)
      return true;
  }
  return false;
}
int main() {
  begin();
  sprite(100, 100);
  assert(update() && damage.full);
  damage.commit();
  assert(!update());
  begin();
  sprite(140, 100);
  assert(update() && !damage.full);
  assert(covers(101, 101) && covers(185, 145) && !covers(20, 430));
  damage.commit();
  begin();
  assert(update() && covers(160, 120));
  damage.commit();
  // A large image used as an atlas must damage only its actual clip.
  begin();
  pxa_canvas_clip_push(&frame, 220, 180, 40, 40);
  pxa_canvas_image(&frame, -1000, -1000, 4000, 4000, 255, 1, "atlas.png", 9);
  pxa_canvas_clip_pop(&frame);
  assert(update() && !damage.full);
  assert(covers(230, 190) && !covers(40, 40));
  damage.commit();
  // Inserting an unrelated draw must not invalidate unchanged later widgets.
  begin();
  sprite(100, 100);
  sprite(350, 400);
  update();
  damage.commit();
  begin();
  sprite(30, 100);
  sprite(100, 100);
  sprite(350, 400);
  assert(update() && covers(40, 110) && !covers(360, 410));
  damage.commit();
  // Changing clipping can reveal/erase pixels even when image bytes match.
  begin();
  pxa_canvas_clip_push(&frame, 100, 100, 30, 30);
  sprite(95, 95);
  pxa_canvas_clip_pop(&frame);
  update();
  damage.commit();
  begin();
  pxa_canvas_clip_push(&frame, 100, 100, 60, 60);
  sprite(95, 95);
  pxa_canvas_clip_pop(&frame);
  assert(update() && covers(140, 140));
  damage.commit();
  // Layer ordering is visible for overlapping translucent shapes.
  begin();
  pxa_canvas_rect_rgba(&frame, 100, 100, 40, 40, 0xff000080, 0, 0, 0);
  pxa_canvas_rect_rgba(&frame, 110, 110, 40, 40, 0x0000ff80, 0, 0, 0);
  update();
  damage.commit();
  begin();
  pxa_canvas_rect_rgba(&frame, 110, 110, 40, 40, 0x0000ff80, 0, 0, 0);
  pxa_canvas_rect_rgba(&frame, 100, 100, 40, 40, 0xff000080, 0, 0, 0);
  assert(update() && covers(120, 120));
  assert(damage.analyse(frame.data, frame.length, 4096, 2048) && damage.full);
  std::puts("damage: motion, disappearance, atlas clips, insertions, order and "
            "fallback passed");
}
