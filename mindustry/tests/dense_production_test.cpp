// SPDX-License-Identifier: GPL-3.0-only
#define MINDUSTRY_UI_SUPPORT_ONLY
#include "ui_test.cpp"
int main() {
  CHECK(gfx::attach(
      1, PXA_RASTER_CAP_TEXTURED_QUAD | PXA_RASTER_CAP_PAINTER_POLYGON |
             PXA_RASTER_CAP_AFFINE_UV | PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA));
  pocket::WIDTH = pocket::HEIGHT = 480;
  milky::screen = {480, 480, {0, 0, 480, 480}};
  for (int z : {8, 12, 16}) {
    s.generate(154, 0, true);
    for (int i = 0; i < MaxBuildings; ++i)
      if (s.buildings[i].type > Core)
        s.remove(i);
    for (auto &t : s.tiles)
      t = {Stone, None};
    s.rebuild();
    for (int y = 5; y < 31; y += 2)
      for (int x = 5; x < 44; x += 2)
        s.build(Press, x, y, 0, true);
    int buildings = 0;
    for (auto &b : s.buildings)
      if (b.type) {
        ++buildings;
        if (b.type == Press) {
          b.hp = defs[b.type].hp / 2;
          b.inv[Coal] = 10;
          b.progress = 700;
        }
      }
    CHECK(buildings == MaxBuildings);
    resetUI();
    view = World;
    zoom = z;
    cameraX = 24.5f;
    cameraY = 16;
    fullFrame();
    std::printf("Dense production at zoom %d: %u commands\n", z,
                pxa_load_u32(frame.data() + 16));
    selected = Conveyor;
    lineX = 5;
    lineY = 25;
    previewX = 43;
    previewY = 25;
    fullFrame();
    std::printf("Dense line preview at zoom %d: %u commands\n", z,
                pxa_load_u32(frame.data() + 16));
  }
  gfx::detach();
  std::printf("%d dense production frame checks passed\n", checks);
}
