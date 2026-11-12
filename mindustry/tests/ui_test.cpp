// SPDX-License-Identifier: GPL-3.0-only
// Exercise real game gestures and complete frames, using the bounded Host stub.
#include "../main.cpp"
#define MINDUSTRY_RENDER_SUPPORT_ONLY
#include "render_test.cpp"
namespace pocket {
void clear(uint16_t c) { gfx::clear(c); }
void rect(int x, int y, int w, int h, uint16_t c) { gfx::rect(x, y, w, h, c); }
void line(int x, int y, int xx, int yy, uint16_t c) {
  gfx::line(x, y, xx, yy, c);
}
void outline(int x, int y, int w, int h, uint16_t c) {
  rect(x, y, w, 1, c);
  rect(x, y + h - 1, w, 1, c);
  rect(x, y, 1, h, c);
  rect(x + w - 1, y, 1, h, c);
}
void redraw() {}
void save() {}
uint32_t random() { return 154; }
void log(const char *) {}
} // namespace pocket
namespace milky {
void sounds(const char *const *, unsigned) {}
void play(unsigned, unsigned, bool) {}
void volume(unsigned, unsigned) {}
} // namespace milky
namespace md_audio {
void scene(Scene, unsigned) {}
void wave() {}
void effect(md::Sound, unsigned, unsigned) {}
void loop(md::Sound, unsigned, unsigned) {}
void stop(unsigned) {}
}
namespace gfx {
void toggle_language() { pocket::set_english(!pocket::english()); }
} // namespace gfx
void tapAt(int x, int y) {
  game::pointer(pocket::Phase::DOWN, x, y);
  game::pointer(pocket::Phase::UP, x, y);
}
void tapRect(milky::Rect r) { tapAt(r.x + r.w / 2, r.y + r.h / 2); }
void checkInside(milky::Rect r, milky::Rect safe) {
  CHECK(r.w > 0 && r.h > 0);
  CHECK(r.x >= safe.x && r.y >= safe.y && r.right() <= safe.right() &&
        r.bottom() <= safe.bottom());
}
void fullFrame() {
  for (int i = 0; i < 2; ++i) {
    CHECK(gfx::begin(1, pocket::WIDTH, pocket::HEIGHT));
    game::draw();
    CHECK(gfx::finish() > 0);
    warmFonts();
  }
  CHECK(pxa_load_u32(frame.data() + 16) < 1024);
  CHECK(glyphRequests ==
        0); // Known UI text must be complete without Host round trips.
}
#ifndef MINDUSTRY_UI_SUPPORT_ONLY
int main() {
  CHECK(gfx::attach(
      1, PXA_RASTER_CAP_TEXTURED_QUAD | PXA_RASTER_CAP_PAINTER_POLYGON |
             PXA_RASTER_CAP_AFFINE_UV | PXA_RASTER_CAP_SPRITE_TEXEL_ALPHA));
  game::init();
  CHECK(glyphRequests == 0);
  CHECK(&blockArt(Drill) == &art::drill_frame0);
  CHECK(&blockArt(Pneumatic) == &art::pneumatic_frame0);
  for (auto pair : {std::pair<const art::Sprite *,const art::Sprite *>{
                        &art::drill_frame0,&art::drilltop},
                    {&art::pneumatic_frame0,&art::pneumatictop}}) {
    int center = 16 * 32 + 16;
    CHECK(pair.first->p[center] == pair.second->p[center]);
  }
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
      auto safe = milky::bounds();
      auto l = controls();
      for (auto r : {l.pause, l.wave, l.stock, l.drawer, l.map, l.tools,
                     l.inventory, l.info})
        checkInside(r, safe);
      for (auto r : l.dock) {
        checkInside(r, safe);
        CHECK(r.h >= 28 && r.w >= 38);
      }
      for (auto r : l.tabs)
        checkInside(r, safe);
      for (auto r : l.tool)
        checkInside(r, safe);
      int permanent =
          l.pause.w * l.pause.h + l.wave.w * l.wave.h + l.stock.w * l.stock.h;
      for (auto r : l.dock)
        permanent += r.w * r.h;
      CHECK(permanent * 100 < safe.w * safe.h * 30);
      s.generate(154, 0, true);
      resetUI();
      view = World;
      CHECK(!buildOpen && !toolsOpen && !stockOpen && !mapOpen);
      fullFrame();
      tapRect(l.dock[0]);
      CHECK(buildOpen);
      fullFrame();
      tapRect(l.tabs[2]);
      int entries[md::Blocks], n = buildList(entries), wall = -1;
      for (int i = 0; i < n; ++i)
        if (entries[i] == md::Wall)
          wall = i;
      CHECK(wall >= 0);
      tapRect(md_ui::item(l, wall, n));
      CHECK(selected == md::Wall && !buildOpen);
      tapAt(originX() + 22 * zoom + zoom / 2, originY() + 34 * zoom + zoom / 2);
      CHECK(previewX == 22 && previewY == 34);
      fullFrame();
      tapRect(l.dock[2]);
      CHECK(s.at(22, 34) >= 0);
      tapRect(l.dock[3]);
      CHECK(selected == md::Empty);
      tapAt(originX() + 22 * zoom + zoom / 2, originY() + 34 * zoom + zoom / 2);
      CHECK(inspect >= 0);
      fullFrame();
      tapRect(l.dock[2]);
      CHECK(s.at(22, 34) < 0);
      tapRect(l.stock);
      CHECK(stockOpen);
      fullFrame();
      auto targetX = s.targetX;
      tapAt(pocket::WIDTH / 2, l.centerY);
      CHECK(!stockOpen && s.targetX == targetX);
      tapRect(l.dock[2]);
      CHECK(mapOpen);
      fullFrame();
      tapAt(l.map.x + 27 * l.map.w / 48, l.map.y + 29 * l.map.h / 48);
      CHECK(!mapOpen && std::abs(cameraX - 27) < .6f &&
            std::abs(cameraY - 29) < .6f);
      tapRect(l.dock[3]);
      CHECK(toolsOpen);
      fullFrame();
      tapRect(l.tool[3]);
      CHECK(zoom == 12 && toolsOpen);
      tapRect(l.tool[2]);
      CHECK(zoom == 16);
      tapRect(l.tool[0]);
      CHECK(view == Research && !toolsOpen);
      fullFrame();
      tapRect(l.pause);
      CHECK(view == World);
      tapRect(l.pause);
      CHECK(view == Paused);
      fullFrame();
      tapRect(menuRow(0));
      CHECK(view == World);
      view = Options;
      auto oldMute = s.muted;
      auto oldMusic = s.musicMode;
      for (int row = 0; row < 4; ++row)
        checkInside({safe.x + 4, menuTop() + row * (l.button + 12), safe.w - 8, l.button}, safe);
      tapAt(pocket::WIDTH / 2, menuTop() + 2 * (l.button + 12) + l.button / 2);
      CHECK(s.musicMode == (oldMusic + 1) % 3 && s.muted == oldMute);
      tapAt(pocket::WIDTH / 2, menuTop() + (l.button + 12) + l.button / 2);
      CHECK(s.muted != oldMute && s.musicMode == (oldMusic + 1) % 3);
      tapAt(pocket::WIDTH / 2, menuTop() + 3 * (l.button + 12) + l.button / 2);
      CHECK(view == About);
      fullFrame();
      tapRect(l.pause);
      CHECK(view == Options);
      tapRect(l.pause);
      CHECK(view == Title);
      for (auto page : {Guide, About, Options, Sectors, Paused, Units}) {
        view = page;
        fullFrame();
      }
      view = World;
    }
  // Maximum population of damaged, wet drills at the widest map view must
  // still fit the Host's 1024-command ceiling.
  pocket::WIDTH = pocket::HEIGHT = 480;
  milky::screen = {480,480,{0,0,480,480}};
  s.generate(154,0,true);
  for (auto &tile:s.tiles) tile={Stone,Copper};
  s.rebuild();
  for (int y=2;y<44;y+=2)
    for (int x=2;x<44;x+=2)
      s.build(Pneumatic,x,y,0,true);
  int count=0;
  for (auto &b:s.buildings) if(b.type) {
    ++count;
    if(b.type==Pneumatic || b.type==Drill) {b.water=50;b.hp=1;}
  }
  CHECK(count==MaxBuildings);
  resetUI();view=World;zoom=8;cameraX=cameraY=24;
  fullFrame();
  std::printf("Dense drill frame: %u commands\n",pxa_load_u32(frame.data()+16));
  selected=Conveyor;lineX=20;lineY=30;previewX=24;previewY=30;rallyPick=true;
  game::touch(pocket::Phase::DOWN,31,240,240);
  game::key(pocket::Key::B);
  CHECK(!game::touch_active() && !selected && !rallyPick);
  CHECK(previewX==-1 && previewY==-1 && lineX==-1 && lineY==-1);
  gfx::detach();
  std::printf("%d UI/frame checks passed: five sizes, safe areas, folded HUD, "
              "build/inspect, drawers and map gestures\n",
              checks);
}

#endif
