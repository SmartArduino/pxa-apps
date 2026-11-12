// SPDX-License-Identifier: GPL-3.0-only
#define MINDUSTRY_UI_SUPPORT_ONLY
#include "ui_test.cpp"
#include <array>
using pocket::Phase;
std::array<uint8_t, 3900> saved{};
size_t savedSize = 0;
float targetX = 0, targetY = 0;
void snapshot() {
  savedSize = s.save(saved.data(), saved.size());
  targetX = s.targetX;
  targetY = s.targetY;
  CHECK(savedSize > 0);
}
void unchanged() {
  std::array<uint8_t, 3900> now{};
  auto n = s.save(now.data(), now.size());
  CHECK(n == savedSize && !std::memcmp(saved.data(), now.data(), n));
  CHECK(s.targetX == targetX && s.targetY == targetY);
}
void release(int ax, int bx, int y) {
  game::touch(Phase::UP, 7, ax, y);
  game::touch(Phase::UP, 31, bx, y);
  CHECK(!game::touch_active());
}
int main() {
  CHECK(gfx::attach(
      1, PXA_RASTER_CAP_TEXTURED_QUAD | PXA_RASTER_CAP_PAINTER_POLYGON |
             PXA_RASTER_CAP_AFFINE_UV | PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA));
  for (auto size : {std::pair<int, int>{320, 240},
                    {296, 240},
                    {480, 320},
                    {240, 320},
                    {480, 480}})
    for (int inset : {0, 17}) {
      pocket::WIDTH = size.first;
      pocket::HEIGHT = size.second;
      milky::screen = {
          size.first,
          size.second,
          {inset, inset, size.first - 2 * inset, size.second - 2 * inset}};
      s.generate(154, 0, true);
      resetUI();
      view = World;
      int cx = pocket::WIDTH / 2 + 12, cy = controls().centerY;
      float wx = float(cx - originX()) / zoom,
            wy = float(cy - originY()) / zoom;
      snapshot();
      game::touch(Phase::DOWN, 7, cx - 40, cy);
      game::touch(Phase::DOWN, 31, cx + 40, cy);
      game::touch(Phase::MOVE, 7, cx - 80, cy);
      game::touch(Phase::MOVE, 31, cx + 80, cy);
      CHECK(zoom == 32);
      CHECK(std::abs(originX() + wx * zoom - cx) <= 1.1f &&
            std::abs(originY() + wy * zoom - cy) <= 1.1f);
      // The midpoint can pan without changing the zoom or moving the player.
      game::touch(Phase::MOVE, 7, cx - 55, cy - 12);
      game::touch(Phase::MOVE, 31, cx + 105, cy - 12);
      CHECK(zoom == 32);
      CHECK(std::abs(originX() + wx * zoom - (cx + 25)) <= 1.1f);
      CHECK(std::abs(originY() + wy * zoom - (cy - 12)) <= 1.1f);
      fullFrame();
      float before = cameraX;
      game::touch(Phase::UP, 7, cx - 55, cy - 12);
      game::touch(Phase::MOVE, 31, cx + 115, cy - 12);
      CHECK(std::abs(cameraX - before + 10.f / 32) < .001f);
      game::touch(Phase::UP, 31, cx + 115, cy - 12);
      CHECK(!game::touch_active());
      unchanged();
      // Clamp, crossing fingers, and an initially tiny span stay bounded.
      resetUI();
      snapshot();
      game::touch(Phase::DOWN, 7, cx - 40, cy);
      game::touch(Phase::DOWN, 31, cx + 40, cy);
      game::touch(Phase::MOVE, 7, cx - 600, cy);
      game::touch(Phase::MOVE, 31, cx + 600, cy);
      CHECK(zoom == MaxZoom);
      game::touch(Phase::MOVE, 7, cx - 2, cy);
      game::touch(Phase::MOVE, 31, cx + 2, cy);
      CHECK(zoom == MinZoom);
      release(cx - 2, cx + 2, cy);
      unchanged();
      resetUI();
      game::touch(Phase::DOWN, 7, cx, cy);
      game::touch(Phase::DOWN, 31, cx + 1, cy);
      game::touch(Phase::MOVE, 7, cx - 30, cy);
      game::touch(Phase::MOVE, 31, cx + 30, cy);
      CHECK(std::isfinite(cameraX) && std::isfinite(cameraY) &&
            zoom >= MinZoom && zoom <= MaxZoom);
      release(cx - 30, cx + 30, cy);
      // An unrelated third ID cannot release either of the captured contacts.
      resetUI();
      snapshot();
      game::touch(Phase::DOWN, 7, cx - 40, cy);
      game::touch(Phase::DOWN, 31, cx + 40, cy);
      game::touch(Phase::DOWN, 99, 0, 0);
      game::touch(Phase::UP, 99, 0, 0);
      CHECK(game::touch_active());
      release(cx - 40, cx + 40, cy);
      unchanged();
      // UI-started multi-touch is quarantined, rather than pressing buttons
      // below.
      auto button = controls().dock[0];
      int bx = button.x + button.w / 2, by = button.y + button.h / 2;
      resetUI();
      snapshot();
      game::touch(Phase::DOWN, 7, bx, by);
      game::touch(Phase::DOWN, 31, cx, cy);
      release(bx, cx, cy);
      CHECK(!buildOpen && zoom == 16);
      unchanged();
      game::touch(Phase::DOWN, 7, cx, cy);
      game::touch(Phase::DOWN, 31, bx, by);
      game::touch(Phase::UP, 7, cx, cy);
      game::touch(Phase::UP, 31, bx, by);
      CHECK(!buildOpen && zoom == 16);
      unchanged();
      // A selected line is canceled when two fingers take over; selection
      // remains.
      selected = Wall;
      game::touch(Phase::DOWN, 7, cx - 40, cy);
      CHECK(lineX >= 0);
      game::touch(Phase::DOWN, 31, cx + 40, cy);
      game::touch(Phase::MOVE, 7, cx - 70, cy);
      game::touch(Phase::MOVE, 31, cx + 70, cy);
      CHECK(selected == Wall && previewX < 0 && lineX < 0);
      release(cx - 70, cx + 70, cy);
      unchanged();
      fullFrame();
      // Cancellation disarms old contacts, then a fresh tap works normally.
      resetUI();
      game::touch(Phase::DOWN, 7, cx - 40, cy);
      game::touch(Phase::DOWN, 31, cx + 40, cy);
      game::cancel_touches();
      CHECK(!game::touch_active());
      game::touch(Phase::MOVE, 7, cx + 70, cy);
      release(cx + 70, cx + 40, cy);
      unchanged();
      int tx = 26, ty = 32, px = originX() + tx * zoom + 8,
          py = originY() + ty * zoom + 8;
      game::touch(Phase::DOWN, 7, px, py);
      game::touch(Phase::UP, 7, px, py);
      CHECK(s.mineX < 0); // Empty ground stops mining; the camera drives movement.
      game::tick(50);
      CHECK(s.targetX == cameraX && s.targetY == cameraY);
      // Long hold clones only the type/direction, never builds on release.
      resetUI();
      int id = s.build(Conveyor, 26, 34, 3, true);
      CHECK(id >= 0);
      int count = 0;
      for (auto &b : s.buildings)
        count += b.type != 0;
      px = originX() + 26 * zoom + 8;
      py = originY() + 34 * zoom + 8;
      game::touch(Phase::DOWN, 7, px, py);
      for (int i = 0; i < 5; ++i)
        game::tick(100);
      CHECK(selected == Conveyor && direction == 3 && previewX < 0);
      game::touch(Phase::UP, 7, px, py);
      int after = 0;
      for (auto &b : s.buildings)
        after += b.type != 0;
      CHECK(after == count && selected == Conveyor && previewX < 0);
      fullFrame();
      // Visible ranges remain valid through both zoom extremes and map edges.
      for (int z : {MinZoom, MaxZoom}) {
        zoom = z;
        selected = Duo;
        previewX = 27;
        previewY = 34;
        fullFrame();
        previewX = -1;
        selected = Empty;
        inspect = s.core();
        fullFrame();
      }
    }
  CHECK(glyphRequests == 0);
  gfx::detach();
  std::printf("%d gesture/frame checks passed: anchored pinch, two-finger pan, "
              "release order, UI capture, cancel, bounds and long-hold copy\n",
              checks);
}
