// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "../common/pocket-ui/i18n.hpp"
#include "palette.hpp"
#include "sound_design.hpp"
#include "pixel_sprite.hpp"
#include <stddef.h>
#include <stdint.h>

namespace pocket {
inline int WIDTH = 240, HEIGHT = 240;
// PXA SDK 2.1's maximum single storage value; keep wire headers separate.
constexpr size_t SAVE_CAPACITY = 2048;
enum class Key : uint8_t {
  UP,
  RIGHT,
  DOWN,
  LEFT,
  A,
  B,
  X,
  Y,
  L,
  R,
  START,
  SELECT
};
enum class Phase : uint8_t { DOWN, MOVE, UP, CANCEL };
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r & 248u) << 8) | ((g & 252u) << 3) | (b >> 3));
}
constexpr uint16_t BG = rgb(15, 23, 42), PANEL = rgb(30, 41, 59);
constexpr uint16_t INK = farm_palette::Ink, MUTED = rgb(148, 163, 184);
constexpr uint16_t ACCENT = rgb(56, 189, 248), GOOD = rgb(74, 222, 128);
constexpr uint16_t WARN = rgb(251, 191, 36), BAD = rgb(248, 113, 113);
// Drawing is clipped to the canvas. Call these only from game::draw().
void begin_ui();
void begin_hud(int x,int y,int width,int height,int opacity);
void end_hud();
// Keep title art at full resolution; game scenes use integer Host scaling.
void scene_surface(bool enabled);
void switch_language();
void music_enabled(bool enabled);
void effects_enabled(bool enabled);
void sound(dewfield_audio::Sfx effect);
void soundscape(int region,int weather,int minute);
// Draw directly to output pixels, preserving the source aspect ratio and
// cropping centrally to fill the entire surface. Also enters UI mode.
void cover_background(const uint16_t *pixels, int width, int height);
void shade();
bool using_controller();
struct NavigationInsets {
  int left = 0, right = 0, bottom = 0;
};
NavigationInsets navigation_insets();
void sprite(int x, int y, const sprites::Sprite &sprite);
void clear(uint16_t color = BG);
void rect(int x, int y, int w, int h, uint16_t color);
void outline(int x, int y, int w, int h, uint16_t color);
void line(int x0, int y0, int x1, int y1, uint16_t color);
void text(int x, int y, const char *value, uint16_t color = INK, int scale = 1);
int text_width(const char *value, int scale = 1);
void redraw();
// Coalesces writes; serialize is called later by the runtime, not here.
void save();
uint32_t random();
uint32_t millis(); // foreground time only
void log(const char *message);
enum class DrawStage : uint8_t {
  Begin, Ground, Water, Queue, Shadows, Bodies, Effects, Upscale, UI, Count
};
// Aggregated timing only; no on-screen overlay or per-frame log traffic.
void profile(DrawStage stage);
// Continuous controller state; false while paused/backgrounded. Touch is
// delivered via pointer(), so games can combine their captured gesture with
// held(A).
bool held(Key key);
bool portrait_display();
// Convert display-independent pixels to this canvas's fitted logical units.
int dp(int value);
// System typography units, including the Host's font scale.
int sp(int value);
void open_menu();
// Draw-only access. framebuffer() is nullptr outside game::draw().
void pixel(int x, int y, uint16_t color);
uint16_t *framebuffer();
// Copy a cropped, pitched world cache through the existing native RGB565
// operation when available. The software fallback produces identical pixels.
void copy_background(const uint16_t *source, int sw, int sh, int x, int y,
                     uint16_t outside);
// Optional, quiet synthesized tones. when_seconds is absolute foreground time
// (millis()/1000); zero means now. At most 16 queued tones, duration <= 4
// seconds.
void tone(float frequency, float duration_seconds, float when_seconds = 0.0f);
void silence();
} // namespace pocket

namespace game {
// The game translation unit implements all these hooks. init precedes load.
const char *title();
const char *app_id();
void init();
bool input_active();
// Handle page navigation locally before the shared pause menu consumes Back.
bool back();
void reset();
void settings();
// Press edges and repeated arrows are delivered individually, never flattened
// into final state. START is reserved for the common pause menu.
void key(pocket::Key key);
// Captured first pointer, coordinates relative to the actual canvas. Outside
// releases/cancels still arrive, with coordinates allowed outside the canvas.
void pointer(pocket::Phase phase, int x, int y);
#ifdef PXA_POCKET_CALCULATOR
// Keep calculator and its shared menu in the same input mode. Connections and
// idle controller reports do not enable the navigation focus.
bool navigation_visible();
void set_navigation(bool visible);
#endif
// Elapsed foreground milliseconds, capped at 100. Use bounded work per call.
void tick(uint32_t elapsed_ms);
void draw();
#ifdef PXA_POCKET_CUSTOM_MENU
// App-defined menu button geometry, with shared pause, language and save/exit
// handling.
bool menu_hit(int x, int y);
void draw_menu(bool paused, int row, bool exiting);
#endif
// Return 0 to omit saving. Validate size/version/ranges before changing state.
size_t serialize(uint8_t *data, size_t capacity);
void load(const uint8_t *data, size_t size);
} // namespace game
