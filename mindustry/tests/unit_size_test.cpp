// SPDX-License-Identifier: GPL-3.0-only
#define MINDUSTRY_UI_SUPPORT_ONLY
#include "ui_test.cpp"
int main() {
  CHECK(gfx::attach(
      1, PXA_RASTER_CAP_TEXTURED_QUAD | PXA_RASTER_CAP_PAINTER_POLYGON |
             PXA_RASTER_CAP_AFFINE_UV | PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA));
  pocket::WIDTH = pocket::HEIGHT = 480;
  milky::screen = {480, 480, {0, 0, 480, 480}};
  // Golden source dimensions: pinned Alpha/Dagger/Flare/Crawler are 48 px;
  // pinned conveyors are 32 px per world tile. At 100% a body is 24 px.
  for (auto pair :
       {std::pair<int, int>{8, 12}, {16, 24}, {24, 36}, {32, 48}, {40, 60}}) {
    zoom = pair.first;
    for (const auto *unit : {&art::alpha_unit, &art::dagger_unit,
                             &art::flare_unit, &art::crawler_unit}) {
      CHECK(unit->sourceWidth == 48 && unit->sourceHeight == 48);
      auto r = unitRect(*unit, 240, 180);
      CHECK(r.w == pair.second && r.h == pair.second);
      CHECK(r.x + r.w / 2 == 240 && r.y + r.h / 2 == 180);
      CHECK(unit->body.w == 24 && unit->body.h == 24);
      CHECK(gfx::begin(1, 480, 480));
      drawUnit(*unit, 240, 180);
      CHECK(gfx::finish() > 0);
      CHECK(pxa_load_u32(frame.data() + 16) == 1);
    }
  }
  gfx::detach();
  std::printf("%d unit scale/frame checks passed: original 1.5-tile body, "
              "native aspect ratio, independent of zoom\n",
              checks);
}
