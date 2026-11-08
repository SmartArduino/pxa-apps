// SPDX-License-Identifier: MIT
// Exercise the real page compositor and input handlers on a guarded canvas.
#include "../main.cpp"
#include "../resample.hpp"
#include "../text.hpp"
#include "../hud_compositor.hpp"
#include <cassert>
#include <vector>
namespace {
uint16_t pixels[480 * 480 + 32];
bool controller_mode;
bool held_keys[4]{};
int shade_calls, saves, redraw_calls;
std::vector<const sprites::Sprite *> ui_sprites;
dewfield_hud::Backdrop hud_backdrop;
} // namespace
namespace pocket {
void begin_ui() {}
void begin_hud(int x,int y,int w,int h,int opacity) {
  hud_backdrop.begin(pixels+16,WIDTH,HEIGHT,x,y,x+w,y+h,opacity);
}
void end_hud() { hud_backdrop.end(pixels+16,WIDTH); }
void scene_surface(bool) {}
void switch_language() { set_english(!english()); }
void music_enabled(bool) {}
void effects_enabled(bool) {}
void sound(dewfield_audio::Sfx) {}
void soundscape(int,int,int) {}
void cover_background(const uint16_t *source, int width, int height) {
  dewfield_raster::cover(source, width, height, pixels + 16, WIDTH, HEIGHT);
}
bool using_controller() { return controller_mode; }
NavigationInsets navigation_insets() { return {}; }
uint16_t *framebuffer() { return pixels + 16; }
void copy_background(const uint16_t *source, int sw, int sh, int x, int y,
                     uint16_t outside) {
  for (int row = 0; row < HEIGHT; ++row)
    for (int col = 0; col < WIDTH; ++col)
      framebuffer()[row * WIDTH + col] = row + y < sh && col + x < sw
          ? source[(row + y) * sw + col + x] : outside;
}
void rect(int x, int y, int w, int h, uint16_t c) {
  art::Raster r{framebuffer(), WIDTH, HEIGHT};
  r.rect(x, y, w, h, c);
}
void clear(uint16_t c) { rect(0, 0, WIDTH, HEIGHT, c); }
void outline(int x, int y, int w, int h, uint16_t c) {
  art::Raster r{framebuffer(), WIDTH, HEIGHT};
  r.outline(x, y, w, h, c);
}
void line(int x, int y, int xx, int yy, uint16_t c) {
  art::Raster r{framebuffer(), WIDTH, HEIGHT};
  r.line(x, y, xx, yy, c);
}
void pixel(int x, int y, uint16_t c) { rect(x, y, 1, 1, c); }
void sprite(int x, int y, const sprites::Sprite &s) {
  ui_sprites.push_back(&s);
  art::Raster r{framebuffer(), WIDTH, HEIGHT};
  art::sprite(r, s, x, y);
}
int text_width(const char *s, int scale) {
  return dewfield_text::width(s, scale);
}
void text(int x, int y, const char *s, uint16_t c, int scale) {
  assert(x >= 0 && y >= 0 && x + text_width(s, scale) <= WIDTH &&
         y + 12 * scale <= HEIGHT);
  const char *at = s;
  while (*at) {
    uint32_t cp = pocket_ui::next_utf8(at);
    if (cp != '\n' && !dewfield_text::glyph(cp))
      std::fprintf(stderr, "Missing glyph U+%04x in %s\n", unsigned(cp), s);
    assert(cp == '\n' || dewfield_text::glyph(cp));
  }
  dewfield_text::spans(x, y, s, [&](int xx, int yy, int w, int) {
    rect(x + (xx - x) * scale, y + (yy - y) * scale, w * scale, scale, c);
  });
}
void shade() {
  ++shade_calls;
  for (int i = 0; i < WIDTH * HEIGHT; ++i)
    framebuffer()[i] = art::blend(framebuffer()[i], rgb(36, 49, 39), 4);
}
void redraw() { ++redraw_calls; }
void save() { ++saves; }
bool held(Key k) { return unsigned(k) < 4 && held_keys[unsigned(k)]; }
uint32_t millis() { return 0; }
uint32_t random() { return 123; }
void open_menu() {}
void log(const char *) {}
void profile(DrawStage) {}
} // namespace pocket
static void tap(int x, int y) {
  game::pointer(pocket::Phase::DOWN, x, y);
  game::pointer(pocket::Phase::UP, x, y);
}
static void cached_cultivation() {
  draw_base();
  assert(!plots_changed());
  const std::vector<uint16_t> original(base, base + WorldPixelsW * WorldPixelsH);
  for (farm::Plot plot : {farm::Plot{1, 0, 0, 0}, {1, 0, 0, 1},
                          {1, 3, 0, 1}, {1, 0, 0, 1}, {}}) {
    state.plots[11] = plot;
    assert(plots_changed());
    draw_plots(true);
    assert(!plots_changed());
    std::vector<uint16_t> expected = original;
    for (int y = PlotY * Tile; y < (PlotY + PlotH) * Tile; ++y)
      for (int x = PlotX * Tile; x < (PlotX + PlotW) * Tile; ++x)
        expected[y * WorldPixelsW + x] = terrain_map.pixel(x, y, season());
    art::Raster r{expected.data(), WorldPixelsW, WorldPixelsH};
    terrain::plots(r, state);
    assert(std::equal(expected.begin(), expected.end(), base));
  }
  state.plots[11] = {1, 2, 0, 1};
  draw_plots(true);
  state.plots[11].growth = 2;
  assert(!plots_changed()); // Leaf growth never regenerates static soil.
}
int main() {
  // Mosaico reports a 57-pixel strip on its 480-pixel panel. Scaling it by
  // density a second time used to push the belt almost 120 pixels upward.
  assert(farm_ui::canvas_inset(57, 480, 240) == 29);
  farm_ui::Layout mosaico{240, 240, 0, 0,
                        farm_ui::canvas_inset(57, 480, 240)};
  auto belt = mosaico.belt();
  assert(2 * (240 - belt.y - belt.h) == 66);
  assert(farm_ui::canvas_inset(0, 480, 240) == 0);
  assert(farm_ui::canvas_inset(36, 480, 240) == 18);
  assert(farm_ui::canvas_inset(57, 480, 480) == 57);
  assert(farm_ui::canvas_inset(18, 320, 240) == 14);
  assert(farm_ui::canvas_inset(57, 0, 240) == 0);
  farm_ui::Layout round{412,412,134,0,42};
  auto enter=round.title_enter();
  assert(enter.y+enter.h<=round.height-round.navigation_bottom-4);
  assert(round.title_caption_y()+12<round.height);
  game::init();
  cached_cultivation();
  game::init();
  title_screen = false;
  for (auto size :
       {std::pair<int, int>{240, 240}, {320, 240}, {240, 320}, {480, 320}}) {
    WIDTH = size.first;
    HEIGHT = size.second;
    auto ui = layout();
    auto within = [&](farm_ui::Rect r) {
      assert(r.x >= 0 && r.y >= 0 && r.x + r.w <= WIDTH && r.y + r.h <= HEIGHT);
    };
    within(ui.calendar());
    within(ui.vitals());
    within(ui.belt());
    within(ui.modal());
    within(ui.title_logo());
    within(ui.title_enter());
    within(ui.joystick());
    within(ui.action());
    assert(ui.joystick().x >= 20);
    assert(ui.belt().x >= 20);
    assert(ui.belt().y + ui.belt().h <= HEIGHT - 14);
    for (int i = 0; i < 3; ++i)
      within(ui.header_button(i));
    for (int i = 0; i < 7; ++i)
      within(ui.tool(i));
    for (bool english_mode : {false, true})
      for (bool controller : {false, true}) {
        pocket::set_english(english_mode);
        controller_mode = controller;
        title_screen = true;
        for (bool pressed : {false, true}) {
          touch_down = pressed;
          touchx = ui.title_enter().center_x();
          touchy = ui.title_enter().center_y();
          std::fill(pixels, pixels + WIDTH * HEIGHT + 32, uint16_t(0xabcd));
          game::draw();
          for (int i = 0; i < 16; ++i) {
            assert(pixels[i] == 0xabcd);
            assert(pixels[16 + WIDTH * HEIGHT + i] == 0xabcd);
          }
        }
        touch_down = false;
        title_screen = false;
        for (Page p :
             {Page::Farm, Page::Shop, Page::Bag, Page::Guide, Page::Sleep,
              Page::Morning, Page::Fishing, Page::Letter, Page::Settings, Page::World, Page::People, Page::Story, Page::Dialogue}) {
          page = p;talking_npc=0;
          state.gold = 999999;
          state.seeds[0] = 999;
          state.produce[0] = 999;
          state.shipping = 999999;
          state.fish = 999;
          state.wood = 999;
          state.stone = 999;
          std::snprintf(
              letter, sizeof(letter), "%s",
              pocket::tr(
                  "Please bring fresh vegetables. Thank you for helping our "
                  "village!",
                  "谢谢你，让晴露谷热闹起来！\n去田里享受慢慢生长的日子吧。"));
          std::fill(pixels, pixels + WIDTH * HEIGHT + 32, uint16_t(0xabcd));
          ui_sprites.clear();
          shade_calls = 0;
          toast_ms = 0;
          game::draw();
          for (int i = 0; i < 16; ++i) {
            assert(pixels[i] == 0xabcd);
            assert(pixels[16 + WIDTH * HEIGHT + i] == 0xabcd);
          }
          assert(shade_calls == (p == Page::Farm ? 0 : 1));
          if (p != Page::Farm && p != Page::Bag)
            for (auto *s : ui_sprites)
              assert(s != &sprites::hoe && s != &sprites::axe);
        }
      }
  }
  WIDTH = 240;
  HEIGHT = 240;
  for (auto insets : {pocket::NavigationInsets{16, 0, 24},
                      pocket::NavigationInsets{24, 8, 28}}) {
    farm_ui::Layout safe{WIDTH, HEIGHT, insets.left, insets.right,
                         insets.bottom};
    assert(safe.joystick().x >= insets.left + 6);
    assert(safe.belt().x >= insets.left + 6);
    assert(safe.belt().x + safe.belt().w <= WIDTH - insets.right - 6);
    assert(safe.belt().y + safe.belt().h <= HEIGHT - insets.bottom - 4);
  }
  std::fill(pixels, pixels + WIDTH * HEIGHT + 32, uint16_t(0xabcd));
  rounded_rect(20, 20, 24, 24, Text, 8);
  assert(framebuffer()[20 * WIDTH + 20] == 0xabcd &&
         framebuffer()[20 * WIDTH + 32] == Text);
  for (int yy = 0; yy < 24; ++yy)
    for (int xx = 0; xx < 24; ++xx) {
      auto c = framebuffer()[(20 + yy) * WIDTH + 20 + xx];
      assert(c == framebuffer()[(20 + yy) * WIDTH + 43 - xx]);
      assert(c == framebuffer()[(43 - yy) * WIDTH + 20 + xx]);
    }
  // No decoration may leak outside the rounded panel and its shadow.
  for (auto size : {std::pair<int, int>{20, 20},
                    {78, 30},
                    {110, 34},
                    {228, 34},
                    {228, 210}}) {
    std::fill(pixels, pixels + WIDTH * HEIGHT + 32, uint16_t(0xabcd));
    rounded_rect(6, 7, size.first, size.second, 1, 8);
    rounded_rect(7, 9, size.first, size.second, 1, 8);
    std::vector<uint16_t> mask(framebuffer(), framebuffer() + WIDTH * HEIGHT);
    clear(0xabcd);
    panel(6, 7, size.first, size.second);
    for (int at = 0; at < WIDTH * HEIGHT; ++at)
      if (mask[at] == 0xabcd)
        assert(framebuffer()[at] == 0xabcd);
  }
  page = Page::Shop;
  toast_ms = 10;
  redraw_calls = 0;
  game::tick(20);
  assert(toast_ms == 0 && redraw_calls > 0);
  WIDTH = 240;
  HEIGHT = 240;
  pocket::set_english(false);
  farm::fresh(state);state.region=unsigned(farm::Region::SeedShop);state.visited|=1u<<state.region;state.minute=600;
  page = Page::Shop;
  auto ui = layout();
  int gold = state.gold;
  tap(ui.modal().x + 227, HEIGHT - 1);
  assert(page == Page::Shop && state.gold == unsigned(gold));
  tap(ui.shop_buy(0).center_x(), ui.shop_buy(0).y + 24);
  assert(state.gold == unsigned(gold)); // Gap below the button.
  tap(ui.shop_buy(0).center_x(), ui.shop_buy(0).center_y());
  assert(state.gold == unsigned(gold - 5));state.region=0;
  page = Page::Bag;
  tap(0, HEIGHT - 1);
  assert(page == Page::Bag);
  controller_mode = true;
  state.wood = 8;
  state.stone = 6;
  game::key(Key::RIGHT);
  assert(selected_row == 1);
  game::key(Key::A);
  assert(page == Page::Farm);
  int i = farm::plot_index(state.px / 16, state.py / 16);
  assert(state.sprinklers[i / 8] & (1u << (i % 8)));
  page = Page::Guide;
  selected_row = 0;
  game::key(Key::RIGHT);
  assert(selected_row == 1);
  game::key(Key::DOWN);
  assert(selected_row == 3);
  game::key(Key::LEFT);
  assert(selected_row == 2);
  selected_row = 1;
  game::key(Key::UP);
  assert(selected_row == 1);
  selected_row = 4;
  game::key(Key::DOWN);
  assert(selected_row == 6);
  game::key(Key::DOWN);
  assert(selected_row == 6);
  page = Page::Shop;
  state.gold = 0;
  selected_row = 0;
  game::key(Key::A);
  assert(toast_ms > 0 && state.gold == 0);
  page = Page::Farm;
  state.px = 232;
  state.py = 232;
  camx = 112;
  camy = 116;
  msg("点地块自动操作，背包里有手册", "Tap tiles to farm. Journal in bag");
  auto notification_state = notice();
  assert(notification_state.box.y >= ui.action().y + ui.action().h);
  assert(notification_state.box.y + notification_state.box.h <= ui.belt().y);
  tap(notification_state.box.center_x(), notification_state.box.center_y());
  assert(toast_ms == 0);
  page = Page::Farm;
  tap(ui.header_button(0).center_x(), ui.header_button(0).center_y());
  assert(page == Page::Bag);
  tap(ui.close().center_x(), ui.close().center_y());
  assert(page == Page::Farm);
  assert(game::menu_hit(ui.header_button(2).center_x(),
                        ui.header_button(2).center_y()));
  assert(!game::menu_hit(ui.header_button(1).center_x(),
                         ui.header_button(1).center_y()));
  for (Page panel : {Page::Bag, Page::Guide, Page::Fishing, Page::Morning}) {
    page = panel;
    assert(game::back() && page == Page::Farm);
  }
  assert(!game::back());
  // Starting a captured joystick gesture is safely away from the Back strip.
  auto stick_box = layout().joystick();
  game::pointer(Phase::DOWN, stick_box.center_x(), stick_box.center_y());
  assert(stick);
  game::pointer(Phase::MOVE, 0, stick_box.center_y());
  assert(stick && stickx == stick_box.center_x() - 20);
  game::pointer(Phase::CANCEL, 0, stick_box.center_y());
  assert(!stick && !touch_down);
  char lines[3][128]{};
  int n = wrapped("A long notice should wrap whole words while preserving a "
                  "readable line length.",
                  lines, 3, 100);
  for (int row = 0; row < n; ++row)
    assert(text_width(lines[row]) <= 100);
  page = Page::Farm;
  swing = 360;
  for (int i = 0; i < 11; ++i) {
    redraw_calls = 0;
    game::tick(33);
    assert(redraw_calls > 0);
  }
  assert(swing == 0);
  // A held direction remains animated across tile boundaries, then stops
  // on the exact foot anchor. Phase advances only for actual movement.
  farm::fresh(state);
  std::memset(state.cleared, 1, sizeof(state.cleared));
  page = Page::Farm;
  stop_motion();
  walk_distance = 0;
  held_keys[1] = true;
  unsigned phase_mask = 0;
  int start_x = state.px;
  for (int i = 0; i < 30; ++i) {
    game::tick(33);
    assert(walking);
    assert(walk_distance == unsigned(state.px - start_x));
    phase_mask |= 1u << ((walk_distance / 4) % 8);
  }
  assert(phase_mask == 255 && state.px > start_x + 48);
  held_keys[1] = false;
  for (int i = 0; i < 12; ++i)
    game::tick(33);
  assert(!moving && !walking && state.px % 16 == 8);
  auto stationary_phase = walk_distance;
  game::tick(100);
  assert(walk_distance == stationary_phase && !walking);
  // Exercise real captured pointer gestures, including cancellation and gaps.
  farm::fresh(state);
  controller_mode = false;
  page = Page::Farm;
  auto drag = [&](farm_ui::Rect from, farm_ui::Rect to, bool cancel=false) {
    game::pointer(Phase::DOWN, from.center_x(), from.center_y());
    game::pointer(Phase::MOVE, to.center_x(), to.center_y());
    game::pointer(cancel ? Phase::CANCEL : Phase::UP, to.center_x(), to.center_y());
  };
  tap(ui.tool(0).center_x(), ui.tool(0).center_y());
  assert(state.active_slot == 255 && held_item() == 0);
  tap(ui.tool(0).center_x(), ui.tool(0).center_y());
  assert(state.active_slot == 0 && held_item() == 1);
  drag(ui.tool(0), ui.tool(4));
  assert(state.slots[0] == 5 && state.slots[4] == 1);
  auto stable = state;
  drag(ui.tool(0), ui.tool(1), true);
  assert(std::equal(std::begin(stable.slots), std::end(stable.slots), state.slots));
  page = Page::Bag;
  drag(ui.bag_slot(1), ui.bag_slot(0));
  assert(state.slots[0] == 8 && state.slots[1] == 5);
  assert(state.seeds[0] == stable.seeds[0]);
  stable = state;
  drag(ui.bag_slot(0), {0, 0, 1, 1});
  assert(std::equal(std::begin(stable.slots), std::end(stable.slots), state.slots));
  game::settings();
  for (int row : {0, 1, 2})
    tap(ui.setting(row).center_x(), ui.setting(row).center_y());
  assert(!state.show_joystick && !state.show_action && !state.music);
  page = Page::Farm;
  game::pointer(Phase::DOWN, ui.joystick().center_x(), ui.joystick().center_y());
  assert(!stick);
  game::pointer(Phase::CANCEL, 0, 0);
  game::pointer(Phase::DOWN, ui.action().center_x(), ui.action().center_y());
  assert(!action_hold && !action_touch);
  game::pointer(Phase::CANCEL, 0, 0);
  page = Page::Bag;
  bag_selection = 1;
  state.gold = 100; state.wood = 8; state.stone = 6;
  tap(ui.bag_upgrade().center_x(), ui.bag_upgrade().center_y());
  assert(state.tool_grade[unsigned(farm::Tool::Axe)] == 1 && state.gold == 20);
  puts("PASS: guarded page rendering, Chinese/English layout, modal isolation, "
       "exact button hits, inventory controller actions and notification "
       "wrapping");
  return 0;
}
