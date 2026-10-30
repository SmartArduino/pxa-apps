// SPDX-License-Identifier: CC0-1.0
#pragma once
#include "../common/pocket-ui/i18n.hpp"
#include <stddef.h>
#include <stdint.h>

namespace pocket {
constexpr int BASE_WIDTH = 288, BASE_HEIGHT = 224;
// Preserve square artwork pixels while extending the canvas to the display.
extern int WIDTH, HEIGHT;
// Largest supported world save, stored through the package-private FS service.
constexpr size_t SAVE_CAPACITY = 160000;
constexpr int WorldSlots = 8;
struct WorldInfo {
  bool occupied = false, valid = false;
  uint16_t width = 0, height = 0;
  uint32_t seed = 0, day = 0, played = 0;
};
const WorldInfo &world_info(int slot);
int active_world_slot();
bool worlds_busy();
int world_error(); // 0 none, 1 corrupt, 2 IO, 3 no free world slot
void clear_world_error();
bool load_world(int slot);
bool delete_world(int slot);
bool create_world(uint32_t seed, uint8_t size);
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
constexpr uint16_t BG = rgb(23, 34, 44), PANEL = rgb(36, 51, 59);
constexpr uint16_t INK = rgb(225, 226, 204), MUTED = rgb(143, 163, 162);
constexpr uint16_t ACCENT = rgb(229, 192, 120), GOOD = rgb(74, 222, 128);
constexpr uint16_t WARN = rgb(251, 191, 36), BAD = rgb(248, 113, 113);
// Drawing is clipped to the canvas. Call these only from game::draw().
void clear(uint16_t color = BG);
void rect(int x, int y, int w, int h, uint16_t color);
void outline(int x, int y, int w, int h, uint16_t color);
void line(int x0, int y0, int x1, int y1, uint16_t color);
void text(int x, int y, const char *value, uint16_t color = INK, int scale = 1);
int text_width(const char *value, int scale = 1);
int text_height(int scale = 1);
int text_line_height();
// Finish the scene resample, then paint UI and native fonts on output pixels.
void begin_ui(bool preserve_scene = false);
void redraw();
// Coalesces writes; serialize is called later by the runtime, not here.
void save();
int save_status(); // -1 error, 0 idle, 1 writing, 2 committed
uint32_t random();
uint32_t millis(); // foreground time only
void log(const char *message);
// Continuous controller state; false while paused/backgrounded. Touch is
// delivered via pointer(), so games can combine their captured gesture with
// held(A).
bool held(Key key);
// Last active input was a controller press; connection alone is insufficient.
// A new touch or controller disconnect returns to the default touch hints.
bool using_controller();
// Draw-only access. framebuffer() is nullptr outside game::draw().
void pixel(int x, int y, uint16_t color);
uint16_t *framebuffer();
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
void reset();
// Press edges and repeated arrows are delivered individually, never flattened
// into final state. START is reserved for the common pause menu.
void key(pocket::Key key);
// Captured first pointer, coordinates relative to the actual canvas. Outside
// releases/cancels still arrive, with coordinates allowed outside the canvas.
void pointer(pocket::Phase phase, int x, int y);
// Elapsed foreground milliseconds, capped at 100. Use bounded work per call.
void tick(uint32_t elapsed_ms);
void draw();
// Return 0 to omit saving. Validate size/version/ranges before changing state.
size_t serialize(uint8_t *data, size_t capacity);
bool load(const uint8_t *data, size_t size);
bool inspect_world(const uint8_t *data, size_t size, pocket::WorldInfo &info);
void create_world(uint32_t seed, uint8_t size);
void world_deleted();
} // namespace game
